#!/bin/sh

if [ "`uname`" = "HP-UX" ]; then
  echo "$0: Known to hang on HPUX, see Redmine #6907. We will simply fail without attempting."
  exit 1
fi

./lastseen_threaded_load -c 1   4 1 1
