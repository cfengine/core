#ifndef __CONDITION_MACROS_H__
#define __CONDITION_MACROS_H__

#include <assert.h>

// This file contains macros to use for error checking, assertions,
// abort, return, etc. Each macro should have a comment about when to use it.
// The normal assert() macro should only be used to catch programmer mistakes,
// things which should never happen, even for weird file/network inputs.
// For example, a function which doesn't accept NULL pointer arguments,
// should assert that the parameter is not NULL.

// Used when you want to assert a precondtion (catch programmer mistake)
// but also want to handle the error if it ever happens in a release build.
// Useful if you are unsure if you need to handle the error, for example
// if you are adding assertions to older code. Also useful in cases where
// you know you want both, for example when network or file input can trigger
// the condition
#ifndef assert_or_return
#define assert_or_return(expr, val) { \
    assert(expr);                     \
    if (!(expr))                      \
    {                                 \
        return val;                   \
    }                                 \
}
#endif

// Similar to assert_or_return, except you put it inside the if
// body which handles the error in release builds
#ifndef debug_abort_if_reached
#define debug_abort_if_reached() { \
    assert(false);                 \
}
#endif

#endif
