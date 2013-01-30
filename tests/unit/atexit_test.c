#include "cf3.defs.h"
#include "atexit.h"

bool FN1;
bool FN2;
bool FN3;

void fn1(void)
{
    if (FN1)
    {
        fprintf(stderr, "fn1 is called twice");
        _exit(255);
    }

    if (!FN2)
    {
        fprintf(stderr, "fn2 is not called fn1");
        _exit(255);
    }

    if (!FN3)
    {
        fprintf(stderr, "fn3 is not called before fn1");
        _exit(255);
    }

    FN1 = true;
}

void fn2(void)
{
    if (FN1)
    {
        fprintf(stderr, "fn1 is called before fn2");
        _exit(255);
    }

    if (FN2)
    {
        fprintf(stderr, "fn2 is called twice");
        _exit(255);
    }

    if (!FN3)
    {
        fprintf(stderr, "fn3 is not called before fn2");
        _exit(255);
    }

    FN2 = true;
}

void fn3(void)
{
    if (FN1)
    {
        fprintf(stderr, "fn1 is called before fn3");
        _exit(255);
    }

    if (FN2)
    {
        fprintf(stderr, "fn2 is called before fn3");
        _exit(255);
    }

    if (FN3)
    {
        fprintf(stderr, "fn3 is called twice");
        _exit(255);
    }

    FN3 = true;
}


int main()
{
    RegisterAtExitFunction(&fn1);
    atexit(&fn2);
    RegisterAtExitFunction(&fn3);
    return 0;
}
