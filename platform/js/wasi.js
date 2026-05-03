// Pure-JS implementation of the small subset of `wasi_snapshot_preview1`
// imports that Core C programs actually use:
//
//   args_sizes_get, args_get,
//   fd_write, fd_read, fd_close, fd_seek, fd_tell,
//   path_open,
//   proc_exit
//
// This file is the Core C "browser/JS platform". It contains no DOM, no
// filesystem, and no Node-specific code. A host runner (run_node.js,
// run_browser.js, ...) plugs in its own argv / stdin / stdout / stderr /
// filesystem implementations through the `io` object and reuses the same
// marshalling logic here.
//
// All multi-byte values are little-endian (the WASM ABI). Strings are UTF-8.
// 64-bit integer arguments arrive as JS BigInt (the default since Node 16
// and modern browsers).

const ENCODER = new TextEncoder();
const DECODER = new TextDecoder();

// Subset of WASI errno values we use. 0 means success.
export const ERRNO = {
    SUCCESS:    0,
    BADF:       8,
    INVAL:      28,
    IO:         29,
    NOENT:      44,
    NOTCAPABLE: 76,
};

// Thrown by proc_exit() to unwind out of the WASM call.
export class ProcExit extends Error {
    constructor(status) {
        super("proc_exit:" + status);
        this.status = status;
    }
}

// WASI whence values (matching PLATFORM_SEEK_* in platform/platform.h).
const WHENCE_SET = 0;
const WHENCE_CUR = 1;
const WHENCE_END = 2;

// Build the imports object for a given host IO implementation.
//
//   io.argv    : string[]
//   io.stdin   : { read(maxBytes) -> Uint8Array }   // empty = EOF
//   io.stdout  : { write(bytes:Uint8Array) }
//   io.stderr  : { write(bytes:Uint8Array) }
//   io.fs      : { open(path, oflags, rights) -> fd|-1,
//                  close(fd) -> errno,
//                  read(fd, max) -> Uint8Array | null,
//                  write(fd, bytes) -> bytesWritten | -1,
//                  seek(fd, offsetBig, whence) -> bigint | null,
//                  tell(fd) -> bigint | null }
//
// `setMemory(memory)` must be called once after instantiating the wasm
// module so the imports can read/write linear memory.
export function makeWasi(io) {
    let memory = null;

    function dv() { return new DataView(memory.buffer); }
    function u8() { return new Uint8Array(memory.buffer); }
    function readStr(ptr, len) {
        return DECODER.decode(new Uint8Array(memory.buffer, ptr, len));
    }
    function writeBytes(ptr, bytes) { u8().set(bytes, ptr); }
    function writeU32(ptr, v) { dv().setUint32(ptr, v >>> 0, true); }
    function writeU64(ptr, big) { dv().setBigUint64(ptr, BigInt(big), true); }

    const imports = {
        proc_exit(status) { throw new ProcExit(status); },

        args_sizes_get(argc_ptr, argv_buf_size_ptr) {
            let bufSize = 0;
            for (const a of io.argv) bufSize += ENCODER.encode(a).length + 1;
            writeU32(argc_ptr, io.argv.length);
            writeU32(argv_buf_size_ptr, bufSize);
            return ERRNO.SUCCESS;
        },

        args_get(argv_ptr, argv_buf_ptr) {
            let p = argv_buf_ptr;
            for (let i = 0; i < io.argv.length; i++) {
                writeU32(argv_ptr + i * 4, p);
                const bytes = ENCODER.encode(io.argv[i]);
                writeBytes(p, bytes);
                u8()[p + bytes.length] = 0;
                p += bytes.length + 1;
            }
            return ERRNO.SUCCESS;
        },

        fd_write(fd, iovs_ptr, iovs_len, nwritten_ptr) {
            let total = 0;
            const view = dv();
            for (let i = 0; i < iovs_len; i++) {
                const buf = view.getUint32(iovs_ptr + i * 8, true);
                const len = view.getUint32(iovs_ptr + i * 8 + 4, true);
                const bytes = new Uint8Array(memory.buffer, buf, len);
                let written;
                if (fd === 1) {
                    io.stdout.write(bytes); written = len;
                } else if (fd === 2) {
                    io.stderr.write(bytes); written = len;
                } else {
                    const r = io.fs.write(fd, bytes);
                    if (r < 0) return ERRNO.BADF;
                    written = r;
                }
                total += written;
            }
            writeU32(nwritten_ptr, total);
            return ERRNO.SUCCESS;
        },

        fd_read(fd, iovs_ptr, iovs_len, nread_ptr) {
            let total = 0;
            const view = dv();
            for (let i = 0; i < iovs_len; i++) {
                const buf = view.getUint32(iovs_ptr + i * 8, true);
                const len = view.getUint32(iovs_ptr + i * 8 + 4, true);
                let bytes;
                if (fd === 0)      bytes = io.stdin.read(len);
                else               bytes = io.fs.read(fd, len);
                if (bytes === null || bytes === undefined) return ERRNO.BADF;
                writeBytes(buf, bytes);
                total += bytes.length;
                if (bytes.length < len) break; // short read / EOF
            }
            writeU32(nread_ptr, total);
            return ERRNO.SUCCESS;
        },

        fd_close(fd) {
            if (fd <= 2) return ERRNO.SUCCESS;
            return io.fs.close(fd);
        },

        fd_seek(fd, offsetBig, whence, newoffset_ptr) {
            const r = io.fs.seek(fd, offsetBig, whence);
            if (r === null || r === undefined) return ERRNO.BADF;
            writeU64(newoffset_ptr, r);
            return ERRNO.SUCCESS;
        },

        fd_tell(fd, offset_ptr) {
            const r = io.fs.tell(fd);
            if (r === null || r === undefined) return ERRNO.BADF;
            writeU64(offset_ptr, r);
            return ERRNO.SUCCESS;
        },

        path_open(dirfd, dirflags, path_ptr, path_len,
                  oflags, rights_base, rights_inheriting, fdflags, fd_ptr) {
            const path = readStr(path_ptr, path_len);
            const fd = io.fs.open(path, oflags, rights_base);
            if (fd < 0) return ERRNO.NOENT;
            writeU32(fd_ptr, fd);
            return ERRNO.SUCCESS;
        },
    };

    return {
        imports: { wasi_snapshot_preview1: imports },
        setMemory(m) { memory = m; },
        WHENCE_SET, WHENCE_CUR, WHENCE_END,
        ProcExit,
    };
}

// Helper: build a minimal in-memory filesystem suitable for io.fs. Used by
// the browser runner (which has no real filesystem).
export function makeMemoryFS(initial = {}) {
    const files = new Map();
    for (const [name, data] of Object.entries(initial)) {
        files.set(name, {
            data: data instanceof Uint8Array ? data : ENCODER.encode(data),
            position: 0,
        });
    }
    const handles = new Map();
    let nextFd = 3;

    const O_CREAT = 0x1;
    const O_TRUNC = 0x8;

    return {
        files,
        open(path, oflags, _rights) {
            let f = files.get(path);
            if (!f) {
                if (!(oflags & O_CREAT)) return -1;
                f = { data: new Uint8Array(), position: 0 };
                files.set(path, f);
            } else if (oflags & O_TRUNC) {
                f.data = new Uint8Array();
            }
            f.position = 0;
            const fd = nextFd++;
            handles.set(fd, path);
            return fd;
        },
        close(fd) { return handles.delete(fd) ? 0 : 8; },
        read(fd, max) {
            const name = handles.get(fd); if (!name) return null;
            const f = files.get(name);
            const slice = f.data.subarray(f.position,
                Math.min(f.data.length, f.position + max));
            f.position += slice.length;
            return slice;
        },
        write(fd, bytes) {
            const name = handles.get(fd); if (!name) return -1;
            const f = files.get(name);
            const need = f.position + bytes.length;
            if (need > f.data.length) {
                const grown = new Uint8Array(need);
                grown.set(f.data);
                f.data = grown;
            }
            f.data.set(bytes, f.position);
            f.position += bytes.length;
            return bytes.length;
        },
        seek(fd, offsetBig, whence) {
            const name = handles.get(fd); if (!name) return null;
            const f = files.get(name);
            const offset = Number(offsetBig);
            let pos;
            if (whence === WHENCE_SET) pos = offset;
            else if (whence === WHENCE_CUR) pos = f.position + offset;
            else if (whence === WHENCE_END) pos = f.data.length + offset;
            else return null;
            if (pos < 0) return null;
            f.position = pos;
            return BigInt(pos);
        },
        tell(fd) {
            const name = handles.get(fd); if (!name) return null;
            return BigInt(files.get(name).position);
        },
    };
}
