#!/bin/sh

# Outputs to file $1 all the rest of the arguments


if [ $# = 0 ]
then
    echo "ERROR: At least one argument OUTPUT_FILE expected!"
    exit 1
fi
if [ -e "$OUTFILE" ]
then
    echo "ERROR: output file $1 already exists, I refuse to overwrite it!"
    exit 1
fi


OUTFILE="$1"
shift
echo "$*" > "$OUTFILE"
