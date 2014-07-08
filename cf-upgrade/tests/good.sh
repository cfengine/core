#!/bin/sh

# Exit with error on any failed command
set -e


echo running inside $0, arguments are: $@

echo good.sh STDOUT
echo good.sh STDERR >&2


return 0
