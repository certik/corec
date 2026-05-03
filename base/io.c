#include <base/base_types.h>
#include <base/io.h>
#include <base/exit.h>
#include <base/base_io.h>
#include <base/mem.h>
#include <base/scratch.h>
#include <platform/platform.h>

// Returns the file contents as a null-terminated string in `text`.
// Returns `true` on success, otherwise `false`.
// The size of `text` includes the null character, which is inserted
// to allow tokenizing the text and use a null character as a "file end"
// condition.
bool read_file(Arena *arena, const string filename, string *text) {
    Scratch scratch = scratch_begin_avoid_conflict(arena);
    // Open file using WASI interface
    platform_fd_t fd = platform_path_open(str_to_cstr_copy(scratch.arena, filename),
            filename.size, PLATFORM_RIGHTS_READ, 0);
    if (fd < 0) {
        scratch_end(scratch);
        return false;
    }

    // Get file size by seeking to end
    uint64_t filesize_u64;
    if (platform_fd_seek(fd, 0, PLATFORM_SEEK_END, &filesize_u64) != 0) {
        platform_fd_close(fd);
        scratch_end(scratch);
        return false;
    }

    // Seek back to beginning
    uint64_t dummy;
    if (platform_fd_seek(fd, 0, PLATFORM_SEEK_SET, &dummy) != 0) {
        platform_fd_close(fd);
        scratch_end(scratch);
        return false;
    }

    size_t filesize = (size_t)filesize_u64;

    // Allocate buffer
    char *bytes = arena_alloc_array(arena, char, filesize+1);

    // Read file contents using iovec
    iovec_t iov = { .iov_base = bytes, .iov_len = filesize };
    size_t nread;
    int ret = platform_fd_read(fd, &iov, 1, &nread);
    platform_fd_close(fd);

    if (ret != 0 || nread != filesize) {
        scratch_end(scratch);
        return false;
    }
    bytes[nread] = '\0';
    text->str = bytes;
    text->size = filesize+1;
    scratch_end(scratch);
    return true;
}


string read_file_ok(Arena *arena, const string filename) {
    string text;
    if (read_file(arena, filename, &text)) {
        return text;
    } else {
        FATAL_ERROR("File cannot be opened.");
        return text;
    }
}

void println_explicit(string fmt, size_t arg_count, ...) {
    Scratch scratch = scratch_begin();
    va_list varg;
    va_start(varg, arg_count);

    string text = format_explicit_varg(scratch.arena, fmt, arg_count, varg);
    va_end(varg);
    text = str_concat(scratch.arena, text, str_lit("\n"));
    ciovec_t iov = {.buf = text.str, .buf_len = text.size};
    write_all(PLATFORM_STDOUT_FD, &iov, 1);

    scratch_end(scratch);
}
