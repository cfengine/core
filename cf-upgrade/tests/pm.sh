#!/bin/sh

# Exit with error on any failed command
set -e


echo running inside $0, arguments are: $@

echo pm.sh STDOUT
echo pm.sh STDERR >&2

FOLDER=$1
# mv $FOLDER/cf-upgrade-test $FOLDER/cf-upgrade-done


exit 0
