/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#ifndef UNICODE_H
#define UNICODE_H

#include <bool.h>

#include <stdint.h>

/**
 * Dumb conversion from 8-bit strings to 16-bit.
 *
 * Does not take locales or any special characters into account.
 * @param dst The destination string.
 * @param src The source string.
 * @param size The size of dst, in wchars.
 */
void ConvertFromCharToWChar(int16_t *dst, const char *src, size_t size);

/**
 * Dumb conversion from 16-bit strings to 8-bit.
 *
 * Does not take locales or any special characters into account. Since
 * it's possible to lose information this way, this function returns a
 * value indicating whether the conversion was "clean" or whether information
 * was lost.
 * @param dst The destination string.
 * @param src The source string.
 * @param size The size of dst, in bytes.
 * @return Returns true if conversion was successful. Returns false if the
 *         16-bit string could not be converted cleanly to 8-bit. Note that dst
 *         will always contain a valid string.
 */
bool ConvertFromWCharToChar(char *dst, const int16_t *src, size_t size);

#endif // !UNICODE_H
