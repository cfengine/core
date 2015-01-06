/* Part of publib.

   Copyright (c) 1994-2006 Lars Wirzenius.  All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*
 * strrstr.c -- find last occurrence of string in another string
 *
 * Part of publib.  See man page for more information.
 * "@(#)publib-strutil:$Id: strrstr.c,v 1.1.1.1 1994/02/03 17:25:29 liw Exp $"
 */

#include <assert.h>
#include <string.h>

#if !HAVE_DECL_STRRSTR
char *strrstr(const char *haystack, const char *needle);
#endif

char *strrstr(const char *str, const char *pat) {
	size_t len, patlen;
	const char *p;

	assert(str != NULL);
	assert(pat != NULL);

	len = strlen(str);
	patlen = strlen(pat);

	if (patlen > len)
		return NULL;
	for (p = str + (len - patlen); p > str; --p)
		if (*p == *pat && strncmp(p, pat, patlen) == 0)
			return (char *) p;
	return NULL;
}

