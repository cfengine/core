/*
   Copyright 2019 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_PATH_H
#define CFENGINE_PATH_H

#include <platform.h>

// TODO: Move more functions from files_names.c here

/**
 * @brief Returns the filename part of a string/path, similar to basename
 *
 * Differs from basename() by not modifying the input arg and
 * just returning a pointer within the arg (not an internal static buffer).
 * Locates the string after the last `/`. If none are present, assumes it's
 * a relative path, and that the whole string is a filename.
 *
 * @return Pointer to filename within path, NULL if path ends in / (directory)
 */
const char *Path_Basename(const char *path);

char *Path_JoinAlloc(const char *dir, const char *leaf);

#endif
