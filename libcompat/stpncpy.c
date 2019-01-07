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
#include <assert.h>
#include <stdlib.h>

#if !HAVE_DECL_STPNCPY
char *stpncpy(char *dst, const char *src, size_t len);
#endif

// Copy at most len bytes, including NUL terminating byte
// if src is too long, don't add terminating NUL byte
// if src is shorter than len, fill remainder with NUL bytes
// return pointer to terminator in dst
// if no terminator, return dst + len (address after last byte written)
//
// This is not the fastest way to implement stpncpy, and it is only
// used where it is missing (mingw/windows)
char *stpncpy(char *dst, const char *src, size_t len)
{
    assert(dst != NULL);
    assert(src != NULL);
    assert(dst != src);

    for (int i = 0; i < len; ++i)
    {
        const char copy_byte = src[i];
        dst[i] = copy_byte;
        if (copy_byte == '\0')
        {
            // Zero fill and return:
            for (int j = i+1; j < len; ++j)
            {
                dst[j] = '\0';
            }
            return dst + i;
        }
    }
    return dst + len;
}
