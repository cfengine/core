#include "misc_lib.h"

unsigned long UnsignedModulus(long dividend, long divisor)
{
    return ((dividend % divisor) + divisor) % divisor;
}
