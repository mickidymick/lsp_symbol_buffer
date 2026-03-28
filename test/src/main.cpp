#include "test.h"
#include <cstdio>

int run_impl_tests();

int main() {
    // -----------------------------------------------------------------------
    // Case 1: Multiple references to add() and process_string()
    // Good targets for lsp-search-symbol and lsp-find-references.
    // -----------------------------------------------------------------------
    int r1 = add(1, 2);
    int r2 = add(3, 4);
    int r3 = add(r1, r2);

    process_string("hello");
    process_string("world");
    process_string("test");

    // -----------------------------------------------------------------------
    // Case 2: multiply() -- inline, declaration == definition
    // -----------------------------------------------------------------------
    int m = multiply(r1, r2);
    int m2 = multiply(m, 2);

    // -----------------------------------------------------------------------
    // Case 3: Calculator -- member function references
    // -----------------------------------------------------------------------
    Calculator calc(10);
    calc.add_to_value(5);
    calc.add_to_value(r1);
    int val = calc.get_value();
    calc.reset();
    calc.add_to_value(val);

    // -----------------------------------------------------------------------
    // Case 4: Template -- max_val with different types
    // -----------------------------------------------------------------------
    int   imax = max_val(val, r3);
    float fmax = max_val(1.5f, 2.5f);

    // -----------------------------------------------------------------------
    // Case 5: Forward declared class via factory functions
    // -----------------------------------------------------------------------
    ForwardDeclared *fd = create_forward();
    destroy_forward(fd);

    // -----------------------------------------------------------------------
    // Cross-file references
    // -----------------------------------------------------------------------
    int impl_result = run_impl_tests();

    printf("r3=%d m2=%d imax=%d fmax=%.1f impl=%d\n",
           r3, m2, imax, fmax, impl_result);

    return 0;
}
