#pragma once

#include <base/arena.h>
#include <base/base_string.h>
#include <base/format.h>

// Returns the file contents as a null-terminated string in `text`.
// Returns `true` on success, otherwise `false`.
bool read_file(Arena *arena, const string filename, string *text);
string read_file_ok(Arena *arena, const string filename);

void println_explicit(string fmt, size_t arg_count, ...);

#define println(fmt, ...) \
    println_explicit(fmt, COUNT_ARGS(__VA_ARGS__) __VA_OPT__(,) APPLY_WITH_COUNT(COUNT_ARGS(__VA_ARGS__) __VA_OPT__(,) __VA_ARGS__))

#define PRINT_LOG(x) println(str_lit("{}:{} in {}(): {}"), str_lit(__FILE__), __LINE__, str_lit(__func__), str_lit(x))
