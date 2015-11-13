#!/bin/sh
# WARNING keep this /bin/sh compatible!

# Outputs to file $1 all the rest of the arguments


if [ $# = 0 ]
then
    echo "ERROR: At least one argument OUTPUT_FILE expected!"
    exit 1
fi

OUTFILE="$1"
if [ -f "$OUTFILE" ]
then
    echo "ERROR: already exists, I refuse to overwrite file: $OUTFILE"
    exit 1
fi


shift
echo "$*" > "$OUTFILE"
