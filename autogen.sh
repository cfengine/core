#!/bin/sh
#
#  Copyright 2017 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by CFEngine AS.
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

echo "$0: Running determine-version.py ..."
rm -f CFVERSION
misc/determine-version.py > CFVERSION \
    || echo "$0: Unable to auto-detect CFEngine version, continuing"

echo "$0: Running autoreconf ..."
autoreconf -Wno-portability --force --install -I m4  ||  exit

cd -                # back to original directory

if [ -z "$NO_CONFIGURE" ]
then
    echo "$0: Running configure ..."
    "$srcdir"/configure --enable-maintainer-mode "$@"
fi
