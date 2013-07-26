#include <stdint.h>

// Pretend we are an extension.
#define BUILDING_CORE_EXTENSION
#include <enterprise_extension.h>

ENTERPRISE_FUNC_2ARG_DECLARE(int64_t, extension_function, int32_t, short_int, int64_t, long_int);
ENTERPRISE_FUNC_3ARG_DECLARE(int64_t, extension_function_broken, int32_t, short_int, char, unwanted_par, int64_t, long_int);

ENTERPRISE_FUNC_2ARG_DEFINE(int64_t, extension_function, int32_t, short_int, int64_t, long_int)
{
    return short_int * long_int;
}

// Notice that this function has a different signature from the one in the test .c file.
ENTERPRISE_FUNC_3ARG_DEFINE(int64_t, extension_function_broken, int32_t, short_int, char, unwanted_par, int64_t, long_int)
{
    return short_int * long_int;
}
