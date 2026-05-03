#pragma once

// Individual test functions
void test_platform_heap(void);
void test_math(void);
void test_buddy(void);
void test_arena(void);
void test_scratch(void);
void test_format(void);
void test_io(void);
void test_file_flags(void);
void test_hashtable_int_string(void);
void test_hashtable_string_int(void);
void test_vector_int(void);
void test_vector_int_ptr(void);
void test_string(void);
void test_std_fds(void);
void test_stdin(void);
void test_args(void);

// Argument parsing helper
int check_test_input_flag(void);

// Main test runner
void test_base(void);
