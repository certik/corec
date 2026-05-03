// Node.js host for Core C wasm programs. Behaves like `wasmtime --dir .`:
// real argv, real stdin (when piped), real stdout/stderr, and real
// filesystem access via node:fs.
//
// Usage:
//   node platform/js/run_node.js path/to/program.wasm [args...]
//   echo "x" | node platform/js/run_node.js path/to/program.wasm --test-input

import { readFileSync, openSync, closeSync, readSync, writeSync,
         fstatSync, constants as FS } from "node:fs";
import { basename } from "node:path";
import { makeWasi, ProcExit } from "./wasi.js";

const wasmPath = process.argv[2];
if (!wasmPath) {
    process.stderr.write("usage: node run_node.js <program.wasm> [args...]\n");
    process.exit(2);
}

const argv = [basename(wasmPath), ...process.argv.slice(3)];

let stdinBuf = new Uint8Array();
try {
    if (!process.stdin.isTTY) stdinBuf = new Uint8Array(readFileSync(0));
} catch {}
let stdinPos = 0;

const handles = new Map(); // wasiFd -> { nodeFd, position }
let nextFd = 3;

const RIGHT_FD_READ  = 0x2n;
const RIGHT_FD_WRITE = 0x40n;

const PLATFORM_O_CREAT = 0x1;
const PLATFORM_O_TRUNC = 0x8;

const io = {
    argv,
    stdin:  { read(max) {
        const slice = stdinBuf.subarray(stdinPos, stdinPos + max);
        stdinPos += slice.length;
        return slice;
    }},
    stdout: { write(b) { process.stdout.write(b); } },
    stderr: { write(b) { process.stderr.write(b); } },
    fs: {
        open(path, oflags, rights) {
            const wantRead  = (BigInt(rights) & RIGHT_FD_READ)  !== 0n;
            const wantWrite = (BigInt(rights) & RIGHT_FD_WRITE) !== 0n;
            let flag = wantRead && wantWrite ? FS.O_RDWR
                     : wantWrite             ? FS.O_WRONLY
                                             : FS.O_RDONLY;
            if (oflags & PLATFORM_O_CREAT) flag |= FS.O_CREAT;
            if (oflags & PLATFORM_O_TRUNC) flag |= FS.O_TRUNC;
            try {
                const nodeFd = openSync(path, flag, 0o644);
                const fd = nextFd++;
                handles.set(fd, { nodeFd, position: 0 });
                return fd;
            } catch {
                return -1;
            }
        },
        close(fd) {
            const h = handles.get(fd);
            if (!h) return 8;
            try { closeSync(h.nodeFd); } catch {}
            handles.delete(fd);
            return 0;
        },
        read(fd, max) {
            const h = handles.get(fd); if (!h) return null;
            const buf = Buffer.allocUnsafe(max);
            const n = readSync(h.nodeFd, buf, 0, max, h.position);
            h.position += n;
            return new Uint8Array(buf.buffer, buf.byteOffset, n);
        },
        write(fd, bytes) {
            const h = handles.get(fd); if (!h) return -1;
            const n = writeSync(h.nodeFd, bytes, 0, bytes.length, h.position);
            h.position += n;
            return n;
        },
        seek(fd, offsetBig, whence) {
            const h = handles.get(fd); if (!h) return null;
            const offset = Number(offsetBig);
            let pos;
            if (whence === 0)      pos = offset;
            else if (whence === 1) pos = h.position + offset;
            else if (whence === 2) pos = fstatSync(h.nodeFd).size + offset;
            else return null;
            if (pos < 0) return null;
            h.position = pos;
            return BigInt(pos);
        },
        tell(fd) {
            const h = handles.get(fd); if (!h) return null;
            return BigInt(h.position);
        },
    },
};

const wasi = makeWasi(io);
const bytes = readFileSync(wasmPath);
const { instance } = await WebAssembly.instantiate(bytes, wasi.imports);
wasi.setMemory(instance.exports.memory);

try {
    instance.exports._start();
    process.exit(0);
} catch (e) {
    if (e instanceof ProcExit) process.exit(e.status);
    throw e;
}
