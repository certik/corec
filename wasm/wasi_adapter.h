#pragma once

// Public interface of the wasm->native lift WASI adapter (wasm/wasi_adapter.c).
//
// The adapter *defines* the wasi_snapshot_preview1 imports as plain C functions
// that operate on i32 linear-memory offsets (see __wasm_linmem_base). This
// header declares those entry points and the crt0-provided globals they read,
// so the adapter and its tests share a single set of declarations.
//
// The types below are spelled with built-in C types (not <base/types.h> /
// <stdint.h>) so this header stays dependency-free and can be included both by
// the freestanding adapter (-nostdinc) and by host-side tests that use libc.

typedef unsigned int       u32;
typedef int                i32;
typedef long long          i64;
typedef unsigned long long u64;

// Globals filled by the backend's _start (crt0): the base of the wasm
// linear-memory image, and the host argc/argv (argv as an integer to be cast to
// `char **`).
extern u64 __wasm_linmem_base;
extern i32 __wasm_argc;
extern u64 __wasm_argv;

// The wasi_snapshot_preview1 imports defined by the adapter. Every "pointer"
// argument is an i32 byte offset into the wasm linear memory.
u32 fd_write(u32 fd, u32 iovs, u32 iovs_len, u32 nwritten);
u32 fd_read(u32 fd, u32 iovs, u32 iovs_len, u32 nread);
u32 fd_close(u32 fd);
u32 fd_seek(u32 fd, i64 offset, u32 whence, u32 newoffset);
u32 fd_tell(u32 fd, u32 offset_out);
u32 path_open(u32 dirfd, u32 dirflags, u32 path, u32 path_len, u32 oflags,
              u64 rights, u64 rights_inh, u32 fdflags, u32 opened_fd);
u32 args_sizes_get(u32 out_argc, u32 out_buf_size);
u32 args_get(u32 argv_ofs_arr, u32 argv_buf);
u32 environ_sizes_get(u32 out_count, u32 out_buf_size);
u32 environ_get(u32 environ, u32 buf);
