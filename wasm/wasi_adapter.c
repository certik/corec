// WASI adapter for the wasm -> native (LLVM / Mach-O / ELF) lift path.
//
// When a linked wasm module is lifted back to the LLVM dialect
// (`tinyc --from-wasm ...`), its WASI imports (fd_write, path_open, args_*, ...)
// are left as ordinary external calls. This file *defines* those imports in
// plain C so they no longer have to be synthesised by the compiler
// (mlir_wasmssa_to_llvm.c). It is compiled LP64 by tinyC and spliced into the
// lifted module by the driver, alongside the host platform code.
//
// A WASI argument that is a "pointer" is really an i32 byte offset into the
// wasm linear memory. `__wasm_linmem_base` (set by the backend crt0/_start to
// the base of the linear-memory mapping) turns such an offset into a host
// pointer. The actual I/O is delegated to the host platform, whose public
// `platform_*` entry points the driver renames to `__host_platform_*` (the
// lifted module already contains the wasm-side `platform_*` from
// platform_wasm.c, so the host copies must not collide).

typedef unsigned char      u8;
typedef unsigned int       u32;
typedef int                i32;
typedef long long          i64;
typedef unsigned long long u64;
typedef unsigned long      usize;   // LP64 size_t

// I/O vectors as the host platform sees them ({ptr, size_t}); layout matches
// corec's ciovec_t / iovec_t. Declared locally to avoid pulling in platform.h.
typedef struct { const void *buf; usize buf_len; } host_ciovec_t;
typedef struct { void *iov_base; usize iov_len; } host_iovec_t;

// Globals filled by the backend's _start (crt0).
extern u64 __wasm_linmem_base;   // base address of the wasm linear-memory image
extern i32 __wasm_argc;
extern u64 __wasm_argv;          // host `char **argv`, as an integer

// Host platform primitives (corec platform_*.c, renamed __host_platform_* by
// the driver). Signatures mirror corec/platform/platform.h.
extern u32 __host_platform_fd_write(int fd, const host_ciovec_t *iovs, usize n, usize *nwritten);
extern int __host_platform_fd_read(int fd, const host_iovec_t *iovs, usize n, usize *nread);
extern int __host_platform_fd_close(int fd);
extern int __host_platform_fd_seek(int fd, i64 offset, int whence, u64 *newoffset);
extern int __host_platform_fd_tell(int fd, u64 *offset);
extern int __host_platform_path_open(const char *path, usize path_len, u64 rights, int oflags);

// Darwin: a freshly O_CREAT'd file gets a garbage mode because open()'s mode is
// a variadic arg; fix it explicitly. fchmod is non-variadic. (No-op-safe
// elsewhere.) Renamed to the underscored libSystem symbol by the driver.
extern int fchmod(int fd, int mode);

static void *wasm_ptr(u32 off) { return (void *)(__wasm_linmem_base + (u64)off); }

// i32 fd_write(fd, iovs, iovs_len, nwritten): walk (buf_ofs, len) iovec pairs in
// linmem, write each chunk, accumulate the byte count into linmem[nwritten].
u32 fd_write(u32 fd, u32 iovs, u32 iovs_len, u32 nwritten) {
    u64 total = 0;
    for (u32 i = 0; i < iovs_len; i = i + 1) {
        u32 *iov = (u32 *)wasm_ptr(iovs + i * 8);
        host_ciovec_t cv;
        cv.buf = wasm_ptr(iov[0]);
        cv.buf_len = (usize)iov[1];
        usize nw = 0;
        __host_platform_fd_write((int)fd, &cv, 1, &nw);
        total = total + (u64)nw;
    }
    *(u32 *)wasm_ptr(nwritten) = (u32)total;
    return 0;
}

// i32 fd_read(fd, iovs, iovs_len, nread): walk iovecs, read each, accumulate.
// Stops on short read; returns 8 (WASI EBADF-ish) on platform error.
u32 fd_read(u32 fd, u32 iovs, u32 iovs_len, u32 nread) {
    u64 total = 0;
    for (u32 i = 0; i < iovs_len; i = i + 1) {
        u32 *iov = (u32 *)wasm_ptr(iovs + i * 8);
        host_iovec_t cv;
        cv.iov_base = wasm_ptr(iov[0]);
        cv.iov_len = (usize)iov[1];
        usize nr = 0;
        if (__host_platform_fd_read((int)fd, &cv, 1, &nr) != 0) return 8;
        total = total + (u64)nr;
        if ((u32)nr < iov[1]) break;   // short read: stop the whole call
    }
    *(u32 *)wasm_ptr(nread) = (u32)total;
    return 0;
}

u32 fd_close(u32 fd) { __host_platform_fd_close((int)fd); return 0; }

u32 fd_seek(u32 fd, i64 offset, u32 whence, u32 newoffset) {
    u64 no = 0;
    if (__host_platform_fd_seek((int)fd, offset, (int)whence, &no) != 0) return 8;
    *(u64 *)wasm_ptr(newoffset) = no;
    return 0;
}

u32 fd_tell(u32 fd, u32 offset_out) {
    u64 o = 0;
    if (__host_platform_fd_tell((int)fd, &o) != 0) return 8;
    *(u64 *)wasm_ptr(offset_out) = o;
    return 0;
}

// path_open(dirfd, dirflags, path, path_len, oflags, rights, rights_inh,
//           fdflags, opened_fd): copy the path out of linmem, open it via the
// host platform (which does the WASI rights/oflags -> POSIX translation), and
// store the resulting fd into linmem[opened_fd].
u32 path_open(u32 dirfd, u32 dirflags, u32 path, u32 path_len, u32 oflags,
              u64 rights, u64 rights_inh, u32 fdflags, u32 opened_fd) {
    (void)dirfd; (void)dirflags; (void)rights_inh; (void)fdflags;
    char buf[1056];
    if (path_len >= 1055) return 44;   // WASI ENOENT
    for (u32 i = 0; i < path_len; i = i + 1)
        buf[i] = *(char *)wasm_ptr(path + i);
    buf[path_len] = 0;
    int fd = __host_platform_path_open(buf, (usize)path_len, rights, (int)oflags);
    if (fd < 0) return 44;
    *(u32 *)wasm_ptr(opened_fd) = (u32)fd;
    if (oflags & 1) fchmod(fd, 0644);  // O_CREAT: fix the created-file mode
    return 0;
}

// args_sizes_get(out_argc, out_buf_size): argc and total argv byte size
// (including nul terminators).
u32 args_sizes_get(u32 out_argc, u32 out_buf_size) {
    *(u32 *)wasm_ptr(out_argc) = (u32)__wasm_argc;
    char **argv = (char **)__wasm_argv;
    u64 total = 0;
    for (i32 i = 0; i < __wasm_argc; i = i + 1) {
        char *s = argv[i];
        while (*s) { total = total + 1; s = s + 1; }
        total = total + 1;   // nul
    }
    *(u32 *)wasm_ptr(out_buf_size) = (u32)total;
    return 0;
}

// args_get(argv_ofs_arr, argv_buf): fill argv_ofs_arr[i] with the linmem offset
// of argv[i] and copy each string (incl. nul) into linmem at argv_buf.
u32 args_get(u32 argv_ofs_arr, u32 argv_buf) {
    char **argv = (char **)__wasm_argv;
    u32 cur = argv_buf;
    for (i32 i = 0; i < __wasm_argc; i = i + 1) {
        *(u32 *)wasm_ptr(argv_ofs_arr + (u32)i * 4) = cur;
        char *s = argv[i];
        for (;;) {
            char c = *s;
            s = s + 1;
            *(char *)wasm_ptr(cur) = c;
            cur = cur + 1;
            if (c == 0) break;
        }
    }
    return 0;
}

u32 environ_sizes_get(u32 out_count, u32 out_buf_size) {
    *(u32 *)wasm_ptr(out_count) = 0;
    *(u32 *)wasm_ptr(out_buf_size) = 0;
    return 0;
}

u32 environ_get(u32 environ, u32 buf) { (void)environ; (void)buf; return 0; }
