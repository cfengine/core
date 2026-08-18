#!/bin/sh

# Tests that every short option advertised in a struct option table also
# appears in the option string passed alongside it to getopt_long().
#
# How it can happen: the long option table and the short option string are two
# separate lists that have to agree, and nothing enforces that. Adding an
# option means editing both. Forgetting the option string is silent -- the code
# compiles, the long form works, the handler exists, and only the short form is
# broken.
#
# What effects does it have: getopt_long() rejects the short form as an
# unrecognized option even though --help advertises it. If the character is
# present but its argument specification disagrees with the table, the option
# instead consumes the wrong thing: an option string entry with no colon
# against a required_argument table entry leaves the value to be misparsed as a
# positional argument.
#
# This was CFE-4736, where cf-check -V, cf-net -t, cf-net -c, cf-secret -v,
# cf-secret -g, cf-secret -I and cf-testd -r were all rejected despite being
# declared and handled, and cf-serverd's -L silently consumed nothing.
#
# How do we test for it: by parsing each getopt_long() call site out of the
# sources, pairing it with the struct option table it actually names, and
# checking that every table entry's character is present in that call's option
# string. Duplicate characters in an option string are reported too --
# getopt_long() matches the first occurrence, so a second one is dead and means
# the string disagrees with itself.
#
# This test is deliberately conservative. It reports only the two unambiguous
# faults above. It does not check argument specifications, because reconciling
# those is sometimes a judgement call about the intended interface rather than
# a mechanical fix. Anything it cannot parse it skips rather than failing on,
# so a construct it does not understand can never break the build.

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

# Locate the source tree. srcdir is set by automake when running the test
# suite; fall back to the build layout for a manual run from tests/unit.
if [ -n "$srcdir" ]; then
    TOP="$srcdir/../.."
else
    TOP="../.."
fi

if [ ! -d "$TOP/libpromises" ]; then
    echo "Could not locate the source tree (tried '$TOP')"
    exit 2
fi

if ! command -v awk >/dev/null 2>&1; then
    echo "Could not find awk"
    exit 2
fi

# The shipped binaries. Test helpers are excluded on purpose:
# tests/acceptance/mock_package_manager.c passes a literal "" and is
# intentionally long-options-only.
FILES="
cf-agent/cf-agent.c
cf-check/cf-check.c
cf-execd/cf-execd.c
cf-key/cf-key.c
cf-monitord/cf-monitord.c
cf-net/cf-net.c
cf-promises/cf-promises.c
cf-runagent/cf-runagent.c
cf-secret/cf-secret.c
cf-serverd/cf-serverd-functions.c
cf-testd/cf-testd.c
cf-upgrade/cf-upgrade.c
"

FOUND=0
CHECKED_TOTAL=0

for f in $FILES; do
    [ -f "$TOP/$f" ] || continue

    OUT=`awk -v FILENAME_SHORT="$f" '
    # POSIX awk does not guarantee \x escapes; build the apostrophe by code.
    BEGIN { Q = sprintf("%c", 39) }

    # ---- collect struct option tables, one entry per line ----------------
    /struct[ \t]+option[ \t]+[A-Za-z_][A-Za-z0-9_]*\[\]/ {
        line = $0
        sub(/.*struct[ \t]+option[ \t]+/, "", line)
        sub(/\[\].*/, "", line)
        table = line
        # A name can be declared more than once in a file -- cf-net has two
        # separate "longopts" tables for different subcommands, each with its
        # own option string. Reset so a call site pairs with the nearest
        # preceding definition rather than the union of all of them.
        tab_n[table] = 0
        intable = 1
        next
    }
    intable {
        # {"name", <argspec>, <flag>, <char>}
        if (match($0, /\{[ \t]*"[^"]+"[ \t]*,[^,]*,[^,]*,[ \t]*(.)[^,}]*\}/)) {
            entry = substr($0, RSTART, RLENGTH)
            name = entry
            sub(/^\{[ \t]*"/, "", name)
            sub(/".*/, "", name)
            # the character literal is the last apostrophe-quoted field
            if (match(entry, Q "." Q)) {
                ch = substr(entry, RSTART + 1, 1)
                tab_char[table, ++tab_n[table]] = ch
                tab_name[table, tab_n[table]] = name
            }
        }
        # Ends on "};" and equally on a sentinel sharing the line, as in
        # cf-testd:  {NULL, 0, 0, sentinel}};
        if ($0 ~ /\}[ \t]*;/) { intable = 0 }
        next
    }

    # ---- remember option string variables --------------------------------
    /[A-Za-z_][A-Za-z0-9_]*[ \t]*=[ \t]*"[^"]*"[ \t]*;/ {
        v = $0
        sub(/[ \t]*=.*/, "", v)
        sub(/.*[^A-Za-z0-9_]/, "", v)
        s = $0
        sub(/^[^"]*"/, "", s)
        sub(/".*/, "", s)
        var_val[v] = s
        next
    }

    # ---- getopt_long() call sites ----------------------------------------
    /getopt_long[ \t]*\(/ { acc = $0; collecting = 1; held = 0 }
    collecting {
        if (acc != $0) { acc = acc " " $0 }

        call = acc
        sub(/.*getopt_long[ \t]*\(/, "", call)
        n = split(call, a, ",")

        optexpr = a[3]; tname = a[4]
        gsub(/^[ \t]+|[ \t]+$/, "", optexpr)
        gsub(/^[ \t]+|[ \t]+$/, "", tname)
        gsub(/[^A-Za-z0-9_].*/, "", tname)

        # A call may wrap, and the option string is often the last thing on
        # the first line, leaving a trailing comma and an empty 4th field.
        # Keep reading rather than giving up on it.
        if (n < 4 || tname == "") {
            if (++held > 4) { collecting = 0 }   # runaway guard
            next
        }
        collecting = 0

        if (optexpr ~ /^"/) {
            lit = optexpr
            sub(/^"/, "", lit); sub(/".*/, "", lit)
        } else {
            key = optexpr
            gsub(/[^A-Za-z0-9_]/, "", key)
            if (!(key in var_val)) { next }   # cannot resolve: skip
            lit = var_val[key]
        }
        if (!(tname in tab_n)) { next }       # cannot resolve: skip

        # Record that this call site really was inspected. A parser that
        # silently stops understanding a file would otherwise report success.
        printf "CHECKED\n"

        # index the option string: char -> present
        delete present
        delete seen_twice
        i = 1
        while (i <= length(lit)) {
            c = substr(lit, i, 1)
            if (c == "+" || c == "-" || (c == ":" && i == 1)) { i++; continue }
            colons = 0
            if (substr(lit, i+1, 1) == ":") {
                colons = 1
                if (substr(lit, i+2, 1) == ":") { colons = 2 }
            }
            if (c in present) { seen_twice[c] = 1 } else { present[c] = 1 }
            i = i + 1 + colons
        }

        for (c in seen_twice) {
            printf "%s: option string \"%s\" lists %s%s%s more than once (the second is dead)\n", FILENAME_SHORT, lit, Q, c, Q
        }
        for (k = 1; k <= tab_n[tname]; k++) {
            c = tab_char[tname, k]
            if (!(c in present)) {
                printf "%s: --%s declares -%s, but %s%s%s is missing from option string \"%s\"\n", \
                       FILENAME_SHORT, tab_name[tname, k], c, Q, c, Q, lit
            }
        }
        next
    }
    ' "$TOP/$f"`

    NCHECKED=`echo "$OUT" | grep -c "^CHECKED$"`
    OUT=`echo "$OUT" | grep -v "^CHECKED$"`

    # Every listed file calls getopt_long(). If none of its call sites could be
    # paired with a table, this test has stopped understanding the source and
    # is no longer testing anything -- say so instead of passing quietly.
    if [ "$NCHECKED" -eq 0 ] && grep -q "getopt_long" "$TOP/$f"; then
        echo "$f: calls getopt_long() but no call site could be paired with"
        echo "  its struct option table. This test can no longer parse this"
        echo "  file and would pass without checking it. Fix the parser in"
        echo "  $0 rather than removing the file from its list."
        exit 2
    fi
    CHECKED_TOTAL=`expr "$CHECKED_TOTAL" + "$NCHECKED"`

    if [ -n "$OUT" ]; then
        echo "$OUT"
        FOUND=1
    fi
done

if [ "$CHECKED_TOTAL" -eq 0 ]; then
    echo "No getopt_long() call sites were checked at all."
    exit 2
fi

if [ "$FOUND" -ne 0 ]; then
    echo ""
    echo "A short option is advertised in a struct option table but is missing"
    echo "from the option string handed to getopt_long() beside it, or a"
    echo "character is listed twice. The short form will be rejected as an"
    echo "unrecognized option. Add the character (with ':' if it takes an"
    echo "argument, '::' if the argument is optional) to the option string."
    exit 1
fi

exit 0
