#!/bin/sh

OS=$(uname -s)

case "$OS" in
  Linux)
    NM_ARGS='--defined-only'
    FILTER=' [Ttr] '
    ;;
  Darwin)
    NM_ARGS='-U -m'
    FILTER=',(__eh_frame|__cstring|__text|__const)'
    ;;
  *)
    echo "Unknown operating system: $OS" >&2
    exit 1;;
esac

for dotlibs_dir in $(find . -name .libs)
do
  dir=${dotlibs_dir%/.libs}
  dir=${dir#./}
  echo '----------------------------------------------------------------------'
  echo "$dir"
  echo '----------------------------------------------------------------------'
  echo
  (cd $dir/.libs
    nm $NM_ARGS *.o 2>/dev/null | egrep -v "$FILTER")
done
