/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_FILES_HASHES_H
#define CFENGINE_FILES_HASHES_H

#include <cf3.defs.h>


/* Enough room for "SHA=asdfasdfasdf". */
#define CF_HOSTKEY_STRING_SIZE (4 + 2 * EVP_MAX_MD_SIZE + 1)


void HashFile(const char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);
void HashString(const char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);
int HashesMatch(const unsigned char digest1[EVP_MAX_MD_SIZE + 1],
                const unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                HashMethod type);
char *HashPrintSafe(char *dst, size_t dst_size, const unsigned char *digest,
                    HashMethod type, bool use_prefix);
char *SkipHashType(char *hash);
void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);


#endif
