#!/bin/sh

# Works like "cat <filename>", except that it will consume everything on stdin
# before outputting the file.

cat > /dev/null
cat "$@"
