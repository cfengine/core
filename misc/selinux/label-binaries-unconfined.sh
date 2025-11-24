#!/usr/bin/env bash
set -ex # for visibility
# This script is intended to be used by the package scriptlets in case the selinux module fails to install
_prefix=${1:-/var/cfengine}
# find all executables in prefix(/var/cfengine default) which do not include a period(.) as that happens to work for us, catching cf-* in bin and things like apachectl.
find "$_prefix" -type f -executable -not -name '*.*' | while IFS='' read -r binary
do
  semanage fcontext -a -t bin_t "$binary"
done
restorecon -iR "$_prefix"
