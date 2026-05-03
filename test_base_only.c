#include <base/io.h>
#include <test_base.h>

// Test runner that only depends on base/ - no stdlib/ dependencies
// This proves that base/ is completely self-contained

int app_main() {
    // Check for --test-input flag and run stdin test if present
    if (check_test_input_flag()) {
        return 0;
    }

    // Normal test suite
    test_base();

    println(str_lit("=== All tests passed ==="));
    return 0;
}
