#pragma once

// -----------------------------------------------------------------------
// Case 1: Separate declaration (here) and definition (impl.cpp)
// Cursor on call site in main.cpp should show both locations distinctly.
// -----------------------------------------------------------------------
int  add(int a, int b);
void process_string(const char *str);


// -----------------------------------------------------------------------
// Case 2: Declaration == definition (inline in header)
// Both declaration and definition should point to the same location.
// -----------------------------------------------------------------------
inline int multiply(int a, int b) {
    return a * b;
}


// -----------------------------------------------------------------------
// Case 3: Class -- methods declared here, defined in impl.cpp
// Testing declaration/definition split on member functions.
// -----------------------------------------------------------------------
class Calculator {
public:
    Calculator(int initial_value);
    int  get_value() const;
    void add_to_value(int n);

    // Inline method: declaration == definition
    void reset() { value = 0; }

private:
    int value;
};


// -----------------------------------------------------------------------
// Case 4: Template -- declaration == definition (must live in header)
// -----------------------------------------------------------------------
template<typename T>
T max_val(T a, T b) {
    return a > b ? a : b;
}


// -----------------------------------------------------------------------
// Case 5: Forward declaration only -- ForwardDeclared defined in impl.cpp
// Declaration (here) and definition (impl.cpp) are different.
// -----------------------------------------------------------------------
class ForwardDeclared;
ForwardDeclared *create_forward();
void             destroy_forward(ForwardDeclared *p);
