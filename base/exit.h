#pragma once

#include <base/base_io.h>

#define FATAL_ERROR(x) do { PRINT_ERR(x); base_abort(); } while (0)

// Process exit for base/
void base_exit(int status);
void base_abort(void);
