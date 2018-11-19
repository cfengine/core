#ifndef __CONDITION_MACROS_H__
#define __CONDITION_MACROS_H__

#include <assert.h>

// Used when you want to assert a precondtion (catch programmer mistake)
// but also want to handle the error if it ever happens in a release build.
// This is mainly for tightening existing functions used in many places
// New functions should usually choose one or the other (handle error or asssert)
#ifndef assert_or_return
#define assert_or_return(expr, val) { \
    assert(expr);                     \
    if (!(expr))                      \
    {                                 \
        return val;                   \
    }                                 \
}
#endif

#endif
