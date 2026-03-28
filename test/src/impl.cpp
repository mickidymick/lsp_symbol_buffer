#include "test.h"
#include <cstdio>
#include <cstring>

// -----------------------------------------------------------------------
// Case 1: definitions for functions declared in test.h
// -----------------------------------------------------------------------
int add(int a, int b) {
    return a + b;
}

void process_string(const char *str) {
    printf("Processing: %s\n", str);
}


// -----------------------------------------------------------------------
// Case 3: Calculator method definitions
// -----------------------------------------------------------------------
Calculator::Calculator(int initial_value) : value(initial_value) {}

int Calculator::get_value() const {
    return value;
}

void Calculator::add_to_value(int n) {
    value += n;
}


// -----------------------------------------------------------------------
// Case 5: ForwardDeclared -- defined only here, declared in test.h
// -----------------------------------------------------------------------
class ForwardDeclared {
public:
    int data;
    explicit ForwardDeclared(int d) : data(d) {}
};

ForwardDeclared *create_forward() {
    return new ForwardDeclared(42);
}

void destroy_forward(ForwardDeclared *p) {
    delete p;
}


// -----------------------------------------------------------------------
// Case 6: Static function -- only a definition, no header declaration.
// Declaration and definition should point to the same place (or only
// definition is returned).
// -----------------------------------------------------------------------
static int square(int x) {
    return x * x;
}

// Exposed wrapper so main.cpp can exercise square indirectly via references.
int run_impl_tests() {
    int a = add(10, 20);
    int b = add(a, square(3));
    process_string("impl self-test");
    return b;
}
