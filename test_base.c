#include <base/base_io.h>
#include <platform/platform.h>
#include <base/arena.h>
#include <base/scratch.h>
#include <base/buddy.h>
#include <base/format.h>
#include <base/io.h>
#include <base/hashtable.h>
#include <base/vector.h>
#include <base/base_string.h>
#include <base/mem.h>
#include <base/assert.h>
#include <test_base.h>

// Define hashtable and vector types for tests
#define MapIntString_HASH(key) ((size_t)(key))
#define MapIntString_EQUAL(key1, key2) ((key1) == (key2))
DEFINE_HASHTABLE_FOR_TYPES(int, string, MapIntString)

#define MapStringInt_HASH(key) (str_hash(key))
#define MapStringInt_EQUAL(key1, key2) (str_eq((key1), (key2)))
DEFINE_HASHTABLE_FOR_TYPES(string, int, MapStringInt)

DEFINE_VECTOR_FOR_TYPE(int, VecInt)
DEFINE_VECTOR_FOR_TYPE(int*, VecIntP)

// Simple print function for base tests
static void print(const char *str) {
    ciovec_t iov = {str, base_strlen(str)};
    write_all(PLATFORM_STDOUT_FD, &iov, 1);
}

// Helper function for inner scratch scope
static char* test_nested_scratch_inner(Arena *outer_arena, bool avoid_conflict) {
    Scratch inner;
    if (avoid_conflict) {
        inner = scratch_begin_avoid_conflict(outer_arena);
    } else {
        inner = scratch_begin();
    }

    // Fill result using outer_arena AFTER inner scratch begins
    char *result = arena_alloc(outer_arena, 50);
    base_strcpy(result, "ABC");
    print("  ARENAS: inner=");
    // Simple pointer printing - just show it's set
    print(inner.arena ? "set" : "null");
    print(", outer=");
    print(outer_arena ? "set" : "null");
    print("\n");

    char *inner_temp = arena_alloc(inner.arena, 50);
    base_strcpy(inner_temp, "Inner temp");
    print("  In inner scratch: ");
    print(inner_temp);
    print("\n");

    if (avoid_conflict) {
        assert(inner.arena != outer_arena);
    } else {
        assert(inner.arena == outer_arena);
    }
    scratch_end(inner);

    return result;
}

// Helper function for outer scratch scope
static void test_nested_scratch_outer(bool avoid_conflict) {
    Scratch outer = scratch_begin();
    char *outer_temp = test_nested_scratch_inner(outer.arena, avoid_conflict);
    char *outer_temp2 = arena_alloc(outer.arena, 50);
    base_strcpy(outer_temp2, "XXX");

    if (avoid_conflict) {
        print("  In outer scratch after inner: ");
        print(outer_temp);
        print("\n");

        // Values are different (correct)
        assert(outer_temp[0] == 'A');
        assert(outer_temp[1] == 'B');
        assert(outer_temp[2] == 'C');

        assert(outer_temp2[0] == 'X');
        assert(outer_temp2[1] == 'X');
        assert(outer_temp2[2] == 'X');

        // and the pointers are different (correct)
        assert(outer_temp != outer_temp2);
    } else {
        print("  In outer scratch after inner: ");
        print(outer_temp);
        print(" (corrupted!)\n");

        // This demonstrates the bug: scratch_begin() without conflict avoidance allows
        // both scopes to share the same arena, and scratch_end(inner) invalidates outer_temp
        // The values are the same (bug)
        assert(outer_temp[0] == 'X');
        assert(outer_temp[1] == 'X');
        assert(outer_temp[2] == 'X');

        assert(outer_temp2[0] == 'X');
        assert(outer_temp2[1] == 'X');
        assert(outer_temp2[2] == 'X');

        // and the pointers are the same (bug)
        assert(outer_temp == outer_temp2);
    }
    scratch_end(outer);
}

void test_platform_heap(void) {
    print("## Testing WASI heap operations...\n");
    void* hb = platform_heap_base();
    print("heap_base set\n");

    size_t ms1 = platform_heap_size();
    print("Initial heap size obtained\n");

    void* mg = platform_heap_grow(4 * PLATFORM_WASM_PAGE_SIZE);
    assert((size_t)hb + ms1 == (size_t)mg);

    size_t ms2 = platform_heap_size();
    assert(ms1 + 4*PLATFORM_WASM_PAGE_SIZE == ms2);

    mg = platform_heap_grow(8 * PLATFORM_WASM_PAGE_SIZE);
    assert((size_t)hb + ms2 == (size_t)mg);

    ms2 = platform_heap_size();
    assert(ms1 + (4+8)*PLATFORM_WASM_PAGE_SIZE == ms2);
    print("WASI heap tests passed\n");
}

void test_buddy(void) {
    print("## Testing buddy allocator...\n");
    buddy_init();

    // Allocate a small block (will round up to MIN_PAGE_SIZE)
    void* p1 = buddy_alloc(100, NULL);
    if (!p1) {
        print("Allocation failed\n");
        platform_exit(1);
    }
    print("Allocated p1\n");

    // Allocate a larger block
    void* p2 = buddy_alloc(8192, NULL);
    if (!p2) {
        print("Allocation failed\n");
        platform_exit(1);
    }
    print("Allocated p2\n");

    // Free the first block
    buddy_free(p1);
    print("Freed p1\n");

    // Allocate again to demonstrate reuse
    void* p3 = buddy_alloc(200, NULL);
    if (!p3) {
        print("Allocation failed\n");
        platform_exit(1);
    }
    print("Allocated p3\n");

    // Free remaining
    buddy_free(p2);
    buddy_free(p3);
    print("Buddy allocator tests passed\n");
}

void test_arena(void) {
    print("## Testing arena allocator...\n");
    print("Creating a new arena with an initial size of 4KB...\n");
    Arena *main_arena = arena_new(4096);
    if (!main_arena) {
        print("Error: Failed to create the arena.\n");
        platform_exit(1);
    }
    arena_pos_t saved_pos_0 = arena_get_pos(main_arena);

    print("Allocating three strings in the arena...\n");
    char s1[] = "Hello from the Arena!\n";
    char *p_s1 = arena_alloc(main_arena, base_strlen(s1) + 1);
    base_strcpy(p_s1, s1);

    char s2[] = "This is a standalone C program. ";
    char *p_s2 = arena_alloc(main_arena, base_strlen(s2) + 1);
    base_strcpy(p_s2, s2);

    char s3[] = "It works on WASM, Linux, macOS, and Windows.\n";
    char *p_s3 = arena_alloc(main_arena, base_strlen(s3) + 1);
    base_strcpy(p_s3, s3);

    print("Strings allocated. Printing from the arena:\n");
    print(p_s1);
    print(p_s2);
    print(p_s3);

    print("Saving position and making temporary allocations...\n");
    arena_pos_t saved_pos = arena_get_pos(main_arena);

    char s_temp[] = "[--THIS IS A TEMPORARY ALLOCATION THAT WILL BE ROLLED BACK--]";
    char *p_temp = arena_alloc(main_arena, base_strlen(s_temp) + 1);
    base_strcpy(p_temp, s_temp);
    print("Allocated temporary string: ");
    print(p_temp);
    print("\n");

    print("Resetting to saved position...\n");
    arena_reset(main_arena, saved_pos);

    print("Allocating again from the saved position...\n");
    char s4[] = "String 3, allocated after reset.\n";
    char *p_s4 = arena_alloc(main_arena, base_strlen(s4) + 1);
    base_strcpy(p_s4, s4);
    print("Allocated: ");
    print(p_s4);

    print("Final content of the arena (first two strings are still valid):\n");
    print("-> ");
    print(p_s1);
    print(p_s2);
    print(p_s3);
    print(p_s4);

    print("Resetting the arena...\n");
    arena_reset(main_arena, saved_pos_0);
    print("Arena has been reset. Previous pointers are now invalid.\n");
    print("Allocating a new string to show that memory is being reused:\n");

    char s5[] = "This new string overwrites the old data after the reset!\n";
    char *p_s5 = arena_alloc(main_arena, base_strlen(s5) + 1);
    base_strcpy(p_s5, s5);
    print(p_s5);

    print("Freeing the arena...\n");
    arena_free(main_arena);
    print("Arena has been completely deallocated and memory returned to the system.\n");

    // Test arena expansion
    print("Testing arena expansion...\n");
    Arena *expand_arena = arena_new(1024); // Small initial size (will be rounded to MIN_CHUNK_SIZE=4096)
    if (!expand_arena) {
        print("Error: Failed to create the arena.\n");
        platform_exit(1);
    }

    // Verify initial state: 1 chunk, at index 0
    assert(arena_chunk_count(expand_arena) == 1);
    assert(arena_current_chunk_index(expand_arena) == 0);
    print("Initial state: 1 chunk, current index 0\n");

    // Allocate data that fits in first chunk
    char *block1 = arena_alloc(expand_arena, 2048);
    base_strcpy(block1, "Block 1 in first chunk");
    print("Allocated block 1: ");
    print(block1);
    print("\n");

    // Still in first chunk
    assert(arena_chunk_count(expand_arena) == 1);
    assert(arena_current_chunk_index(expand_arena) == 0);

    // Force expansion by allocating more than remaining space in first chunk
    // With actual_size optimization, buddy rounds up MIN_CHUNK_SIZE+header to 8192
    // So after 2048, we have ~6000 bytes left. Allocating 7000 forces expansion.
    char *block2 = arena_alloc(expand_arena, 7000);
    if (!block2) {
        print("Error: Failed to expand arena.\n");
        platform_exit(1);
    }
    base_strcpy(block2, "Block 2 forces expansion to second chunk");
    print("Allocated block 2 (forces expansion): ");
    print(block2);
    print("\n");

    // Verify expansion: now 2 chunks, at index 1
    assert(arena_chunk_count(expand_arena) == 2);
    assert(arena_current_chunk_index(expand_arena) == 1);
    print("After expansion: 2 chunks, current index 1\n");

    // Verify both blocks are still valid (proves expansion worked)
    print("Verifying block 1 is still valid: ");
    print(block1);
    print("\n");

    // Allocate another block to confirm arena still works after expansion
    char *block3 = arena_alloc(expand_arena, 256);
    base_strcpy(block3, "Block 3 after expansion");
    print("Allocated block 3: ");
    print(block3);
    print("\n");

    // Still in second chunk
    assert(arena_chunk_count(expand_arena) == 2);
    assert(arena_current_chunk_index(expand_arena) == 1);

    // Verify all blocks are still valid
    assert(block1[0] == 'B');
    assert(block2[0] == 'B');
    assert(block3[0] == 'B');
    print("Arena expansion verified - 2 chunks created, currently at chunk index 1\n");

    // Test reset of expanded arena (multiple chunks)
    print("Testing reset of expanded arena with multiple chunks...\n");
    arena_pos_t pos_after_block1 = arena_get_pos(expand_arena);
    // Note: we need to save position after block1, before expansion

    Arena *reset_test_arena = arena_new(512); // Will be rounded to MIN_CHUNK_SIZE=4096, buddy gives 8192
    char *r1 = arena_alloc(reset_test_arena, 2048);
    base_strcpy(r1, "R1");
    arena_pos_t pos_after_r1 = arena_get_pos(reset_test_arena);

    // Verify we're in chunk 0 with 1 total chunk
    assert(arena_chunk_count(reset_test_arena) == 1);
    assert(arena_current_chunk_index(reset_test_arena) == 0);

    // Force expansion - allocate more than remaining space in first chunk (~6000 bytes left)
    char *r2 = arena_alloc(reset_test_arena, 7000);
    base_strcpy(r2, "R2 in second chunk");

    // Verify expansion occurred: 2 chunks, at index 1
    assert(arena_chunk_count(reset_test_arena) == 2);
    assert(arena_current_chunk_index(reset_test_arena) == 1);

    // Allocate more in expanded chunk
    char *r3 = arena_alloc(reset_test_arena, 512);
    base_strcpy(r3, "R3 also in second chunk");

    // Still in chunk 1
    assert(arena_chunk_count(reset_test_arena) == 2);
    assert(arena_current_chunk_index(reset_test_arena) == 1);

    print("Before reset: r1=");
    print(r1);
    print(", r2=");
    print(r2);
    print(", r3=");
    print(r3);
    print(" (2 chunks, at index 1)\n");

    // Reset to position after r1 (back to first chunk)
    arena_reset(reset_test_arena, pos_after_r1);

    // Verify reset: still 2 chunks total, but back at index 0
    assert(arena_chunk_count(reset_test_arena) == 2);
    assert(arena_current_chunk_index(reset_test_arena) == 0);
    print("After reset: 2 chunks still exist, back at chunk index 0\n");

    // Allocate again - should reuse the space from r2/r3
    char *r4 = arena_alloc(reset_test_arena, 256);
    base_strcpy(r4, "R4 after reset");

    // Still in chunk 0
    assert(arena_current_chunk_index(reset_test_arena) == 0);

    print("After reset and new allocation: r1=");
    print(r1);
    print(", r4=");
    print(r4);
    print("\n");

    assert(r1[0] == 'R' && r1[1] == '1');
    assert(r4[0] == 'R' && r4[1] == '4');
    print("Reset of expanded arena verified - correctly returned to chunk 0\n");

    arena_free(expand_arena);
    arena_free(reset_test_arena);
    print("Arena allocator tests passed\n");
}

void test_scratch(void) {
    print("## Testing scratch arena...\n");
    print("Creating a new arena for scratch tests...\n");
    Arena *scratch_test_arena = arena_new(4096);
    if (!scratch_test_arena) {
        print("Error: Failed to create the arena.\n");
        platform_exit(1);
    }

    print("Test 1: Basic scratch allocation and cleanup\n");
    char *persistent = arena_alloc(scratch_test_arena, 100);
    base_strcpy(persistent, "This persists");

    {
        Scratch scratch = scratch_begin();
        char *temp1 = arena_alloc(scratch.arena, 50);
        base_strcpy(temp1, "Temporary 1");
        char *temp2 = arena_alloc(scratch.arena, 50);
        base_strcpy(temp2, "Temporary 2");
        print("  Inside scratch: ");
        print(persistent);
        print(", ");
        print(temp1);
        print(", ");
        print(temp2);
        print("\n");
        assert(temp1 != temp2);
        scratch_end(scratch);
    }

    char *after_scratch = arena_alloc(scratch_test_arena, 100);
    base_strcpy(after_scratch, "After scratch");
    print("  After scratch end: ");
    print(persistent);
    print(", ");
    print(after_scratch);
    print("\n");

    print("Test 2: Nested scratch scopes with conflict avoidance\n");
    test_nested_scratch_outer(true);

    print("Test 2b: Nested scratch scopes WITHOUT conflict avoidance\n");
    test_nested_scratch_outer(false);

    print("Test 3: Multiple sequential scratch scopes\n");
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        base_strcpy(temp, "Iteration 0");
        print("  ");
        print(temp);
        print("\n");
        scratch_end(scratch);
    }
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        base_strcpy(temp, "Iteration 1");
        print("  ");
        print(temp);
        print("\n");
        scratch_end(scratch);
    }
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        base_strcpy(temp, "Iteration 2");
        print("  ");
        print(temp);
        print("\n");
        scratch_end(scratch);
    }

    print("Test 4: Verify memory reuse after scratch_end\n");
    arena_pos_t before_reuse = arena_get_pos(scratch_test_arena);
    {
        Scratch scratch = scratch_begin();
        arena_alloc(scratch.arena, 1000);
        scratch_end(scratch);
    }
    arena_pos_t after_reuse = arena_get_pos(scratch_test_arena);
    assert(before_reuse.ptr == after_reuse.ptr);
    print("  Memory position restored correctly\n");

    print("Freeing scratch test arena...\n");
    arena_free(scratch_test_arena);

    // Test scratch arena expansion
    print("Test 5: Scratch arena expansion\n");
    {
        Scratch scratch = scratch_begin();

        // Verify we're at the beginning (previous tests may have expanded the arena but we should be reset)
        size_t initial_chunks = arena_chunk_count(scratch.arena);
        assert(arena_current_chunk_index(scratch.arena) == 0);

        // Scratch arenas start with 1024 bytes which gets rounded to MIN_CHUNK_SIZE=4096
        // With actual_size optimization, buddy provides 8192 bytes
        // After alignment, we have ~8100 usable bytes in first chunk
        // Allocate to fill most of it, then force expansion
        char *fill1 = arena_alloc(scratch.arena, 4000);
        char *fill2 = arena_alloc(scratch.arena, 4000);

        // Now allocate more to force expansion
        char *large1 = arena_alloc(scratch.arena, 1000);
        if (!large1) {
            print("Error: Failed to expand scratch arena.\n");
            platform_exit(1);
        }
        large1[0] = 'L';
        large1[1] = '1';
        large1[2] = '\0';

        // Verify expansion: should have more chunks than before
        size_t after_chunks = arena_chunk_count(scratch.arena);
        assert(after_chunks > initial_chunks);
        assert(arena_current_chunk_index(scratch.arena) > 0);

        // Allocate more to confirm expanded scratch still works
        char *large2 = arena_alloc(scratch.arena, 500);
        large2[0] = 'L';
        large2[1] = '2';
        large2[2] = '\0';

        // Verify both allocations are valid
        assert(large1[0] == 'L' && large1[1] == '1');
        assert(large2[0] == 'L' && large2[1] == '2');
        print("  Scratch arena expansion verified\n");

        scratch_end(scratch);
    }

    // Test reset of expanded scratch arena
    print("Test 6: Reset of expanded scratch arena\n");
    {
        Scratch scratch = scratch_begin();

        // After Test 5, the scratch arena may already be expanded
        // We're at index 0 but may have multiple chunks already
        size_t initial_chunks = arena_chunk_count(scratch.arena);
        size_t initial_index = arena_current_chunk_index(scratch.arena);
        assert(initial_index == 0);
        print("  Starting state: ");
        print(initial_chunks == 1 ? "1 chunk" : "2+ chunks");
        print(", at index 0\n");

        // Allocate enough to move through chunks (if multi-chunk) or expand (if single-chunk)
        // With actual_size optimization, we have ~8KB per chunk, so allocate more to force progression
        for (int i = 0; i < 3; i++) {
            arena_alloc(scratch.arena, 3500);
        }

        // Verify we moved forward in chunks
        size_t current_index = arena_current_chunk_index(scratch.arena);
        assert(current_index > initial_index);
        print("  After allocations: moved to chunk index > 0\n");

        // scratch_end should reset back to the initial saved position (chunk 0)
        scratch_end(scratch);
    }

    // Use scratch again - should start fresh at chunk 0
    {
        Scratch scratch = scratch_begin();

        // Back at chunk 0 (chunks still exist but we're at the start)
        assert(arena_current_chunk_index(scratch.arena) == 0);
        print("  After scratch_end: back at chunk index 0\n");

        char *new_alloc = arena_alloc(scratch.arena, 100);
        new_alloc[0] = 'R';
        new_alloc[1] = '\0';
        assert(new_alloc[0] == 'R');
        print("  New allocation after reset: verified\n");
        scratch_end(scratch);
    }

    print("  Reset of expanded scratch arena verified - correctly resets to chunk 0\n");

    print("Scratch arena tests passed\n");
}


void test_format(void) {
    println(str_lit("")); // Test empty string / line
    println(str_lit("## Testing format..."));
    Arena* arena = arena_new(1024*10);
    double pi = 3.1415926535;

    // Example with no arguments
    string fmt = str_lit("Hello!");
    string result = format(arena, fmt);
    assert(str_eq(result, str_lit("Hello!")));
    println(str_lit("No args: {}"), str_to_cstr_copy(arena, result));

    // Example with one argument
    fmt = str_lit("Hello, {}!");
    result = format(arena, fmt, str_lit("world"));
    assert(str_eq(result, str_lit("Hello, world!")));
    println(str_lit("One arg: {}"), str_to_cstr_copy(arena, result));

    fmt = str_lit("Hello, {}!");
    result = format(arena, fmt, 5);
    assert(str_eq(result, str_lit("Hello, 5!")));
    println(str_lit("One arg: {}"), str_to_cstr_copy(arena, result));

    // Example with formatted double
    fmt = str_lit("Value: {:10.5f}");
    result = format(arena, fmt, pi);
    // Note: Double formatting may have slight differences, so we just print it
    println(str_lit("Formatted double: {}"), str_to_cstr_copy(arena, result));

    // Example with formatted char
    fmt = str_lit("Char: |{:^5}|");
    result = format(arena, fmt, 'x');
    assert(str_eq(result, str_lit("Char: | 120 |")));
    println(str_lit("Formatted char: {}"), str_to_cstr_copy(arena, result));

    // Example with multiple arguments
    fmt = str_lit("Hello, {}, {}, {}, {}!");
    result = format(arena, fmt, "world", 35.5, str_lit("XX"), 3);
    println(str_lit("Multiple args: {}"), str_to_cstr_copy(arena, result));
    println(str_lit("Multiple args: {}"), result);

    arena_free(arena);
    println(str_lit("Format tests passed"));
}

void test_io(void) {
    println(str_lit("## Testing io..."));
    Arena* arena = arena_new(1024*20);

    string text;
    bool ok = read_file(arena, str_lit("does not exist"), &text);
    assert(!ok);

    text.size = 0;
    assert(text.size == 0);
    ok = read_file(arena, str_lit("README.md"), &text);
    assert(ok);
    assert(text.size > 100);
    println(str_lit("Read README.md: {} bytes"), text.size);
    println(str_lit("Initial text in README.md:\n{}"), str_substr(text, 0, 100));
    println(str_lit("---"));

    println(str_lit("Hello from io."));

    arena_free(arena);
    println(str_lit("I/O tests passed"));
}

void test_file_flags(void) {
    println(str_lit("## Testing file open flags..."));

    const char* test_file = "test_flags.txt";
    const char* test_content = "Hello, World!";
    const char* new_content = "Updated!";

    // Test 1: WRONLY | CREAT - Create new file and write
    println(str_lit("Test 1: PLATFORM_O_WRONLY | PLATFORM_O_CREAT"));
    platform_fd_t fd = platform_path_open(test_file, base_strlen(test_file), PLATFORM_RIGHTS_WRITE, PLATFORM_O_CREAT);
    assert(fd >= 0);
    ciovec_t iov = {.buf = test_content, .buf_len = base_strlen(test_content)};
    size_t nwritten;
    uint32_t ret = platform_fd_write(fd, &iov, 1, &nwritten);
    assert(ret == 0);
    assert(nwritten == base_strlen(test_content));
    assert(platform_fd_close(fd) == 0);
    println(str_lit("  Created and wrote to file"));

    // Test 2: RDONLY - Read from existing file
    println(str_lit("Test 2: PLATFORM_O_RDONLY"));
    fd = platform_path_open(test_file, base_strlen(test_file), PLATFORM_RIGHTS_READ, 0);
    assert(fd >= 0);
    char read_buf[100] = {0};
    iovec_t read_iov = {.iov_base = read_buf, .iov_len = 99};
    size_t nread;
    int read_ret = platform_fd_read(fd, &read_iov, 1, &nread);
    assert(read_ret == 0);
    assert(nread == base_strlen(test_content));
    assert(base_memcmp(read_buf, test_content, nread) == 0);
    assert(platform_fd_close(fd) == 0);
    println(str_lit("  Read file successfully: {}"), read_buf);

    // Test 3: WRONLY | TRUNC - Truncate and write new content
    println(str_lit("Test 3: PLATFORM_O_WRONLY | PLATFORM_O_TRUNC"));
    fd = platform_path_open(test_file, base_strlen(test_file), PLATFORM_RIGHTS_WRITE, PLATFORM_O_TRUNC);
    assert(fd >= 0);
    ciovec_t trunc_iov = {.buf = new_content, .buf_len = base_strlen(new_content)};
    ret = platform_fd_write(fd, &trunc_iov, 1, &nwritten);
    assert(ret == 0);
    assert(nwritten == base_strlen(new_content));
    assert(platform_fd_close(fd) == 0);
    println(str_lit("  Truncated and wrote new content"));

    // Test 4: RDONLY - Verify truncation worked
    println(str_lit("Test 4: Verify truncation"));
    fd = platform_path_open(test_file, base_strlen(test_file), PLATFORM_RIGHTS_READ, 0);
    assert(fd >= 0);
    base_memset(read_buf, 0, sizeof(read_buf));
    read_iov.iov_base = read_buf;
    read_iov.iov_len = 99;
    read_ret = platform_fd_read(fd, &read_iov, 1, &nread);
    assert(read_ret == 0);
    assert(nread == base_strlen(new_content));
    assert(base_memcmp(read_buf, new_content, nread) == 0);
    assert(platform_fd_close(fd) == 0);
    println(str_lit("  Verified truncated content: {}"), read_buf);

    // Test 5: RDWR - Read and write with same fd
    println(str_lit("Test 5: PLATFORM_O_RDWR"));
    fd = platform_path_open(test_file, base_strlen(test_file), PLATFORM_RIGHTS_RDWR, 0);
    assert(fd >= 0);

    // Read current content
    base_memset(read_buf, 0, sizeof(read_buf));
    read_iov.iov_base = read_buf;
    read_iov.iov_len = 99;
    read_ret = platform_fd_read(fd, &read_iov, 1, &nread);
    assert(read_ret == 0);
    println(str_lit("  Read: {}"), read_buf);

    // Seek back to start
    uint64_t newoffset;
    int seek_ret = platform_fd_seek(fd, 0, PLATFORM_SEEK_SET, &newoffset);
    assert(seek_ret == 0);
    assert(newoffset == 0);

    // Write over it
    const char* rdwr_content = "RDWR!";
    ciovec_t rdwr_iov = {.buf = rdwr_content, .buf_len = base_strlen(rdwr_content)};
    ret = platform_fd_write(fd, &rdwr_iov, 1, &nwritten);
    assert(ret == 0);
    assert(nwritten == base_strlen(rdwr_content));
    assert(platform_fd_close(fd) == 0);
    println(str_lit("  Wrote with RDWR"));

    // Test 6: RDWR | CREAT - Create if doesn't exist
    println(str_lit("Test 6: PLATFORM_O_RDWR | PLATFORM_O_CREAT"));
    const char* new_file = "test_rdwr_creat.txt";
    fd = platform_path_open(new_file, base_strlen(new_file), PLATFORM_RIGHTS_RDWR, PLATFORM_O_CREAT);
    assert(fd >= 0);
    const char* creat_content = "Created with RDWR|CREAT";
    ciovec_t creat_iov = {.buf = creat_content, .buf_len = base_strlen(creat_content)};
    ret = platform_fd_write(fd, &creat_iov, 1, &nwritten);
    assert(ret == 0);
    assert(platform_fd_close(fd) == 0);
    println(str_lit("  Created new file with RDWR|CREAT"));

    // Test 7: WRONLY | CREAT | TRUNC - All flags combined
    println(str_lit("Test 7: PLATFORM_O_WRONLY | PLATFORM_O_CREAT | PLATFORM_O_TRUNC"));
    fd = platform_path_open(test_file, base_strlen(test_file), PLATFORM_RIGHTS_WRITE, PLATFORM_O_CREAT | PLATFORM_O_TRUNC);
    assert(fd >= 0);
    const char* final_content = "Final!";
    ciovec_t final_iov = {.buf = final_content, .buf_len = base_strlen(final_content)};
    ret = platform_fd_write(fd, &final_iov, 1, &nwritten);
    assert(ret == 0);
    assert(platform_fd_close(fd) == 0);
    println(str_lit("  Combined flags work correctly"));

    println(str_lit("File open flags tests passed"));
}

void test_hashtable_int_string(void) {
    println(str_lit("## Testing hashtable (int->string)..."));
    Arena* arena = arena_new(1024*10);

    MapIntString ht;
    MapIntString_init(arena, &ht, 16);
    MapIntString_insert(arena, &ht, 42, str_lit("forty-two"));
    string *value = MapIntString_get(&ht, 42);
    assert(value);
    println(str_lit("Value for key 42: {}"), str_to_cstr_copy(arena, *value));

    arena_free(arena);
    println(str_lit("Hashtable (int->string) tests passed"));
}

void test_hashtable_string_int(void) {
    println(str_lit("## Testing hashtable (string->int)..."));
    Arena* arena = arena_new(1024*10);

    MapStringInt ht;
    MapStringInt_init(arena, &ht, 16);
    MapStringInt_insert(arena, &ht, str_lit("forty-two"), 42);
    int *value = MapStringInt_get(&ht, str_lit("forty-two"));
    assert(value);
    println(str_lit("Value for key \"forty-two\": {}"), *value);

    arena_free(arena);
    println(str_lit("Hashtable (string->int) tests passed"));
}

void test_vector_int(void) {
    println(str_lit("## Testing vector (int)..."));
    Arena* arena = arena_new(1024*10);

    VecInt v;
    VecInt_reserve(arena, &v, 1);
    assert(v.size == 0);
    VecInt_push_back(arena, &v, 1);
    assert(v.size == 1);
    VecInt_push_back(arena, &v, 2);
    assert(v.size == 2);
    VecInt_push_back(arena, &v, 3);
    assert(v.size == 3);
    assert(v.data[0] == 1);
    assert(v.data[1] == 2);
    assert(v.data[2] == 3);

    arena_free(arena);
    println(str_lit("Vector (int) tests passed"));
}

void test_vector_int_ptr(void) {
    println(str_lit("## Testing vector (int*)..."));
    Arena* arena = arena_new(1024*10);

    VecIntP v;
    int i=1, j=2, k=3;
    VecIntP_reserve(arena, &v, 1);
    assert(v.size == 0);
    VecIntP_push_back(arena, &v, &i);
    assert(v.size == 1);
    VecIntP_push_back(arena, &v, &j);
    assert(v.size == 2);
    VecIntP_push_back(arena, &v, &k);
    assert(v.size == 3);
    assert(*v.data[0] == 1);
    assert(*v.data[1] == 2);
    assert(*v.data[2] == 3);
    k = 4;
    assert(*v.data[2] == 4);

    arena_free(arena);
    println(str_lit("Vector (int*) tests passed"));
}

void test_string(void) {
    print("## Testing base string functions...\n");
    Arena *arena = arena_new(4096);

    // Test str_from_cstr_view
    string s1 = str_from_cstr_view("hello");
    assert(s1.size == 5);
    assert(s1.str[0] == 'h');

    // Test str_lit macro
    string s2 = str_lit("world");
    assert(s2.size == 5);

    // Test str_eq
    string s3 = str_lit("hello");
    assert(str_eq(s1, s3));
    assert(!str_eq(s1, s2));

    // Test str_concat
    string s4 = str_concat(arena, s1, str_lit(" "));
    string s5 = str_concat(arena, s4, s2);
    assert(s5.size == 11);
    assert(str_eq(s5, str_lit("hello world")));

    // Test int_to_string
    string s6 = int_to_string(arena, 42);
    assert(str_eq(s6, str_lit("42")));

    string s7 = int_to_string(arena, -123);
    assert(str_eq(s7, str_lit("-123")));

    // Test char_to_string
    string s8 = char_to_string(arena, 'X');
    assert(s8.size == 1);
    assert(s8.str[0] == 'X');

    // Test str_to_cstr_copy
    char *cstr = str_to_cstr_copy(arena, s5);
    assert(cstr[11] == '\0');
    assert(base_strlen(cstr) == 11);

    print("String function tests passed\n");
    arena_free(arena);
}

void test_std_fds(void) {
    print("## Testing standard file descriptors...\n");

    // Test that PLATFORM_STDOUT_FD works
    const char *msg_stdout = "Testing PLATFORM_STDOUT_FD\n";
    ciovec_t iov_stdout = {.buf = msg_stdout, .buf_len = base_strlen(msg_stdout)};
    size_t nwritten;
    uint32_t ret = platform_fd_write(PLATFORM_STDOUT_FD, &iov_stdout, 1, &nwritten);
    assert(ret == 0);
    assert(nwritten == base_strlen(msg_stdout));
    print("PLATFORM_STDOUT_FD works\n");

    // Test that PLATFORM_STDERR_FD works
    const char *msg_stderr = "Testing PLATFORM_STDERR_FD\n";
    ciovec_t iov_stderr = {.buf = msg_stderr, .buf_len = base_strlen(msg_stderr)};
    ret = platform_fd_write(PLATFORM_STDERR_FD, &iov_stderr, 1, &nwritten);
    assert(ret == 0);
    assert(nwritten == base_strlen(msg_stderr));
    print("PLATFORM_STDERR_FD works\n");

    // Test that file operations don't interfere with standard streams
    // Open a file and verify the returned FD is not 0, 1, or 2
    const char* test_file = "test_std_fds.txt";
    platform_fd_t fd = platform_path_open(test_file, base_strlen(test_file), PLATFORM_RIGHTS_WRITE, PLATFORM_O_CREAT | PLATFORM_O_TRUNC);
    assert(fd >= 0);
    assert(fd != PLATFORM_STDIN_FD);
    assert(fd != PLATFORM_STDOUT_FD);
    assert(fd != PLATFORM_STDERR_FD);
    print("File descriptor is not 0, 1, or 2: ");
    Scratch scratch = scratch_begin();
    println(str_lit("{}"), (int)fd);
    scratch_end(scratch);

    // Write to the file
    const char *file_content = "Test content";
    ciovec_t iov_file = {.buf = file_content, .buf_len = base_strlen(file_content)};
    ret = platform_fd_write(fd, &iov_file, 1, &nwritten);
    assert(ret == 0);
    assert(nwritten == base_strlen(file_content));
    assert(platform_fd_close(fd) == 0);
    print("File write successful\n");

    // Verify stdout/stderr still work after file operations
    const char *msg_after = "stdout works after file ops\n";
    ciovec_t iov_after = {.buf = msg_after, .buf_len = base_strlen(msg_after)};
    ret = platform_fd_write(PLATFORM_STDOUT_FD, &iov_after, 1, &nwritten);
    assert(ret == 0);
    assert(nwritten == base_strlen(msg_after));

    // Test opening multiple files to verify fcntl F_DUPFD works correctly
    // This ensures that even if FD 3 is taken, we can still open more files
    const char* file1 = "test_multi_fd1.txt";
    const char* file2 = "test_multi_fd2.txt";
    const char* file3 = "test_multi_fd3.txt";

    platform_fd_t fd1 = platform_path_open(file1, base_strlen(file1), PLATFORM_RIGHTS_WRITE, PLATFORM_O_CREAT | PLATFORM_O_TRUNC);
    assert(fd1 >= 3);  // Should be >= 3 (not 0, 1, 2)

    platform_fd_t fd2 = platform_path_open(file2, base_strlen(file2), PLATFORM_RIGHTS_WRITE, PLATFORM_O_CREAT | PLATFORM_O_TRUNC);
    assert(fd2 >= 3);  // Should be >= 3
    assert(fd2 != fd1);  // Should be different from fd1

    platform_fd_t fd3 = platform_path_open(file3, base_strlen(file3), PLATFORM_RIGHTS_WRITE, PLATFORM_O_CREAT | PLATFORM_O_TRUNC);
    assert(fd3 >= 3);  // Should be >= 3
    assert(fd3 != fd1 && fd3 != fd2);  // Should be different from both

    // Write to each file to verify they work
    const char* content1 = "File 1";
    const char* content2 = "File 2";
    const char* content3 = "File 3";

    ciovec_t iov1 = {.buf = content1, .buf_len = base_strlen(content1)};
    ciovec_t iov2 = {.buf = content2, .buf_len = base_strlen(content2)};
    ciovec_t iov3 = {.buf = content3, .buf_len = base_strlen(content3)};

    ret = platform_fd_write(fd1, &iov1, 1, &nwritten);
    assert(ret == 0 && nwritten == base_strlen(content1));

    ret = platform_fd_write(fd2, &iov2, 1, &nwritten);
    assert(ret == 0 && nwritten == base_strlen(content2));

    ret = platform_fd_write(fd3, &iov3, 1, &nwritten);
    assert(ret == 0 && nwritten == base_strlen(content3));

    // Close all files
    assert(platform_fd_close(fd1) == 0);
    assert(platform_fd_close(fd2) == 0);
    assert(platform_fd_close(fd3) == 0);

    print("Multiple file opens work correctly\n");

    print("Standard file descriptors tests passed\n");
}

void test_stdin(void) {
    print("## Testing stdin (PLATFORM_STDIN_FD)...\n");

    char buffer[256];
    iovec_t iov = {.iov_base = buffer, .iov_len = sizeof(buffer) - 1};
    size_t nread;
    int ret = platform_fd_read(PLATFORM_STDIN_FD, &iov, 1, &nread);
    assert(ret == 0);
    assert(nread > 0);
    buffer[nread] = '\0';

    // Normalize: strip trailing whitespace (CR, LF, spaces, tabs)
    // to handle platform differences (Windows CRLF vs Unix LF)
    while (nread > 0 && (buffer[nread-1] == '\r' || buffer[nread-1] == '\n' ||
                         buffer[nread-1] == ' ' || buffer[nread-1] == '\t')) {
        nread--;
    }
    buffer[nread] = '\0';

    // Verify we read the expected test data
    const char* expected = "test input data";
    size_t expected_len = base_strlen(expected);

    // Debug output
    print("Read from stdin (length ");
    Arena *debug_arena = arena_new(1024);
    println(str_lit("{}"), (int)nread);
    print("): '");
    print(buffer);
    print("'\n");
    print("Expected (length ");
    println(str_lit("{}"), (int)expected_len);
    print("): '");
    print(expected);
    print("'\n");
    arena_free(debug_arena);

    assert(nread == expected_len);
    assert(base_memcmp(buffer, expected, expected_len) == 0);

    print("Read from stdin: ");
    print(buffer);
    print("\n");
    print("stdin test passed\n");
}

void test_args(void) {
    print("## Testing command line arguments...\n");

    // Get argument sizes
    size_t argc, argv_buf_size;
    int ret = platform_args_sizes_get(&argc, &argv_buf_size);
    assert(ret == 0);

    print("argc=");
    Arena *arena = arena_new(4096);
    println(str_lit("{}"), (int)argc);
    print("argv_buf_size=");
    println(str_lit("{}"), (int)argv_buf_size);

    // Allocate buffers
    char** argv = (char**)buddy_alloc(argc * sizeof(char*), NULL);
    char* argv_buf = (char*)buddy_alloc(argv_buf_size, NULL);
    assert(argv != NULL);
    assert(argv_buf != NULL);

    // Get arguments
    ret = platform_args_get(argv, argv_buf);
    assert(ret == 0);

    // Print all arguments
    print("Arguments:\n");
    for (size_t i = 0; i < argc; i++) {
        Scratch scratch = scratch_begin();
        string idx_str = int_to_string(scratch.arena, (int)i);
        print("  argv[");
        print(str_to_cstr_copy(scratch.arena, idx_str));
        print("] = \"");
        print(argv[i]);
        print("\"\n");
        scratch_end(scratch);
    }

    // Verify argc is at least 1 (program name)
    assert(argc >= 1);

    // Free buffers
    buddy_free(argv);
    buddy_free(argv_buf);
    arena_free(arena);

    print("Command line arguments tests passed\n");
}

int check_test_input_flag(void) {
    // Get command line arguments to check for --test-input flag
    size_t argc, argv_buf_size;
    int ret = platform_args_sizes_get(&argc, &argv_buf_size);

    if (ret == 0 && argc > 1) {
        char** argv = (char**)buddy_alloc(argc * sizeof(char*), NULL);
        char* argv_buf = (char*)buddy_alloc(argv_buf_size, NULL);

        if (argv && argv_buf) {
            platform_args_get(argv, argv_buf);

            // Check if first argument is --test-input
            if (base_strcmp(argv[1], "--test-input") == 0) {
                test_stdin();
                buddy_free(argv);
                buddy_free(argv_buf);
                return 1;
            }

            buddy_free(argv);
            buddy_free(argv_buf);
        }
    }

    return 0;
}

void test_base(void) {
    print("=== base tests ===\n");

    test_platform_heap();
    test_buddy();
    test_arena();
    test_scratch();
    test_format();
    test_io();
    test_file_flags();
    test_hashtable_int_string();
    test_hashtable_string_int();
    test_vector_int();
    test_vector_int_ptr();
    test_string();
    test_std_fds();
    test_args();

    print("base tests passed\n\n");
}
