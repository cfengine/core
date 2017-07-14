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

#include <hashes.h>

#include <file_lib.h>

#include <openssl/evp.h>


int FileChecksum(const char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    FILE *file = safe_fopen(filename, "rb");
    if (!file)
    {
        printf("%s can't be opened\n", filename);
        return 0;
    }
    else
    {
        const EVP_MD *md = EVP_get_digestbyname("md5");

        if (!md)
        {
            fclose(file);
            return 0;
        }

        EVP_MD_CTX context;
        EVP_DigestInit(&context, md);

        int len = 0;
        unsigned char buffer[1024];
        while ((len = fread(buffer, 1, 1024, file)))
        {
            EVP_DigestUpdate(&context, buffer, len);
        }

        unsigned int md_len = 0;
        EVP_DigestFinal(&context, digest, &md_len);
        fclose(file);

        return md_len;
    }
}
