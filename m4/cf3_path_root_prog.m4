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
# CF3_PATH_ROOT_PROG(variable, program, value-if-not-found, path = $PATH)
# --------------------------------------
#
# This function has almost the same semantics as the AC_PATH_PROG
# function. The difference is that this will detect tools that are
# runnable by root, but not by the current user. These tools are
# typically used not by the build, but by CFEngine itself, after
# it is installed.
#
AC_DEFUN([CF3_PATH_ROOT_PROG],
[
  found=0
  AS_IF([test "x$4" = "x"], [
    path=$PATH
  ], [
    path=$4
  ])
  AS_ECHO_N(["checking for $2... "])
  for i in $(echo $path | sed -e 's/:/ /g'); do
    AS_IF([test -e $i/$2 && ls -ld $i/$2 | grep ['^[^ ][^ ][^ ][xs][^ ][^ ][^ ][^ ][^ ][^ ]'] > /dev/null], [
      $1=$i/$2
      found=1
      break
    ])
  done

  AS_IF([test "$found" = "1"], [
    AS_ECHO(["$][$1"])
  ], [
    AS_ECHO([no])
    $1=$3
  ])
])
