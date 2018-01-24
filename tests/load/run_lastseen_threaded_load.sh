#!/bin/sh

if [ "x$label" = "xPACKAGES_x86_64_solaris_10" ] ;
then
    echo "Skipping lastseen_threaded_load on $label"
    exit 0;
fi

./lastseen_threaded_load -c 1   4 1 1
