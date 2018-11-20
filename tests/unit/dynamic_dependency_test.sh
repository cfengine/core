#!/bin/sh

# Tests that the symbols in our static libraries do not occur twice in the
# output binaries. Most platforms don't warn about this, but it has potential
# ill effects.
#
# How it can happen: It can happen if we list a static library as a dependency
# for libpromises, and then we list the same static library as a dependency for
# a binary that depends on libpromises. Then the symbol will be included both
# in the library and in the binary.
#
# What effects does it have: Exactly how the symbols are resolved appears to be
# platform dependent, but what can happen is this: At any point, when you call
# a duplicate symbol, depending on where you call it from, you will either call
# the version in the binary or in the library. This is fine, since they both
# contain exactly the same code. However, they do not refer to the same global
# symbols. They each have their own set. This means that something that was
# initialized in the binary may not be initialized in the library, even though
# the symbol name is the same. This has weird effects, like a symbol suddenly
# switching from a valid value to zero when the stack trace crosses a library
# boundary.
#
# The problem has been observed on AIX, where the log level randomly switches
# between verbose and non-verbose, depending on where the Log() function was
# called from. It has also been observed on certain Linux versions (Ubuntu
# 12.04).
#
# How do we test for it: By making sure that functions that are known to be in
# the static libraries are undefined in the binaries, which means that they
# will link to the shared library version, instead of using their own version.

#
# Detect and replace non-POSIX shell
#
try_exec() {
    type "$1" > /dev/null 2>&1 && exec "$@"
}

broken_posix_shell()
{
    unset foo
    local foo=1
    test "$foo" != "1"
}

if broken_posix_shell >/dev/null 2>&1; then
    try_exec /usr/xpg4/bin/sh "$0" "$@"
    echo "No compatible shell script interpreter found."
    echo "Please find a POSIX shell for your system."
    exit 42
fi


cd ../..

# Sanity check that nm works.
if ! which nm | grep '^/' >/dev/null; then
    echo "Could not find nm"
    exit 2
fi

#                libutils.a         libenv.a         libcfnet.a
#                    v                 v                 v
for symbol in LogSetGlobalLevel GetInterfacesInfo ConnectionInfoNew; do
    for binary in cf-*; do
        if test "$binary" = "cf-check" ; then
            continue
        fi
        if test -e "$binary/.libs/$binary"; then
            LOC="$binary/.libs/$binary"
        else
            LOC="$binary/$binary"
        fi
        if nm "$LOC" | grep "$symbol" >/dev/null 2>&1 && ! nm -u "$LOC" | grep "$symbol" >/dev/null 2>&1; then
            echo "$symbol is defined in $binary, but should be undefined."
            echo "Most likely a static library is listed in the Makefile.am which shouldn't be."
            echo "Check the *_LDADD statements in $binary/Makefile.am."
            exit 1
        fi
    done
done

exit 0
