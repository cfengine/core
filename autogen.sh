#!/bin/sh
#
#  Copyright 2019 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#

if [ ! -f libntech/libutils/sequence.h ] ; then
    echo "Error: libntech/libutils/sequence.h is missing"
    echo "       You probably forgot to use the --recursive option when cloning"
    echo "       To fix it now, run:"
    echo "       git submodule init && git submodule update"
    exit 1
fi

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

# Valid only after having switched to POSIX shell.
set -e


srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"

echo "$0: Running determine-version.sh ..."
rm -f CFVERSION
{ misc/determine-version.sh .CFVERSION > CFVERSION && cp CFVERSION libntech/CFVERSION ; } \
    || echo "$0: Unable to auto-detect CFEngine version, continuing"

echo "$0: Running autoreconf ..."
autoreconf -Wno-portability --force --install -I m4  ||  exit

cd -  >/dev/null              # back to original directory

if [ -z "$NO_CONFIGURE" ]
then
    echo "$0: Running configure ..."
    "$srcdir"/configure --enable-maintainer-mode "$@"
fi
