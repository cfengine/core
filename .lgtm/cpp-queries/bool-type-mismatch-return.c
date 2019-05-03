#include <stdbool.h>

int good_int()
{
    return 0;
}

int bad_int()
{
    return false;
}

bool good_with_variable()
{
    bool r = true;
    return r;
}

bool bad_with_variable()
{
    int r = true;
    return r;
}

bool good_with_constant()
{
    return false;
}

bool bad_with_constant()
{
    return 0;
}

bool good_with_function()
{
    return good_with_constant();
}

bool bad_with_function()
{
    return good_int();
}

bool good_with_comparison()
{
    return good_int() != 0;
}

int main(void)
{
    good_int();
    bad_int();
    good_with_variable();
    bad_with_variable();
    good_with_constant();
    bad_with_constant();
    good_with_function();
    bad_with_function();
    good_with_comparison();
    return 0;
}
