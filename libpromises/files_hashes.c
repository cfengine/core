/*
   Copyright (C) CFEngine AS

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

#include <files_hashes.h>

#include <openssl/bn.h>                                         /* BN_* */
#include <openssl/evp.h>                                        /* EVP_* */

#include <dbm_api.h>
#include <files_interfaces.h>
#include <client_code.h>
#include <files_lib.h>
#include <rlist.h>
#include <policy.h>
#include <string_lib.h>                                 /* StringBytesToHex */
#include <misc_lib.h>                                   /* UnexpectedError */


void HashFile(const char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    FILE *file = safe_fopen(filename, "rb");
    if (file == NULL)
    {
        Log(LOG_LEVEL_INFO,
            "Cannot open file for hashing '%s'. (fopen: %s)",
            filename, GetErrorStr());
    }
    else
    {
        int md_len;
        unsigned char buffer[1024];
        const EVP_MD *const md = EVP_get_digestbyname(HashNameFromId(type));
        EVP_MD_CTX context;
        EVP_DigestInit(&context, md);

        size_t len;
        while (!feof(file) &&
               /* When reading a directory (!) fread() returns 0 while !feof(). */
               0 < (len = fread(buffer, 1, sizeof(buffer), file)))
        {
            EVP_DigestUpdate(&context, buffer, len);
        }
        fclose(file);

        /* Digest length stored in md_len */
        EVP_DigestFinal(&context, digest, &md_len);
        /* TODO: should we return md_len (also for HashString) ? */
    }
}

/*******************************************************************/

void HashString(const char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    if (type == HASH_METHOD_CRYPT)
    {
        Log(LOG_LEVEL_ERR,
            "The crypt support is not presently implemented, please use another algorithm instead");
        memset(digest, 0, EVP_MAX_MD_SIZE + 1);
    }
    else
    {
        const EVP_MD *md = EVP_get_digestbyname(HashNameFromId(type));
        if (md == NULL)
        {
            Log(LOG_LEVEL_INFO,
                "Digest type %s not supported by OpenSSL library",
                HashNameFromId(type));
        }
        EVP_MD_CTX context;
        int md_len;

        EVP_DigestInit(&context, md);
        EVP_DigestUpdate(&context, (unsigned char *) buffer, (size_t) len);
        EVP_DigestFinal(&context, digest, &md_len);
    }
}

/*******************************************************************/

void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    int buf_len = key->n ? BN_num_bytes(key->n) : 0;
    if (key->e)
    {
        int i = BN_num_bytes(key->e);
        if (buf_len < i)
        {
            buf_len = i;
        }
    }

    unsigned char *buffer = xmalloc(buf_len + 10);
    if (type == HASH_METHOD_CRYPT)
    {
        Log(LOG_LEVEL_ERR,
            "The crypt support is not presently implemented, please use sha256 instead");
    }
    else
    {
        const EVP_MD *md = EVP_get_digestbyname(HashNameFromId(type));
        if (md == NULL)
        {
            Log(LOG_LEVEL_INFO,
                "Digest type %s not supported by OpenSSL library",
                HashNameFromId(type));
        }
        EVP_MD_CTX context;
        int md_len, actlen;

        EVP_DigestInit(&context, md);

        actlen = BN_bn2bin(key->n, buffer);
        EVP_DigestUpdate(&context, buffer, actlen);
        actlen = BN_bn2bin(key->e, buffer);
        EVP_DigestUpdate(&context, buffer, actlen);
        EVP_DigestFinal(&context, digest, &md_len);
    }

    free(buffer);
}

/*******************************************************************/

int HashesMatch(const unsigned char digest1[EVP_MAX_MD_SIZE + 1],
                const unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                HashMethod type)
{
    size_t size = HashSizeFromId(type);
    return memcmp(digest1, digest2, size) == 0;
}

/* TODO rewrite this ugliness, currently it's not safe, it truncates! */
/**
 * @WARNING #dst must have enough space to hold the result!
 */
char *HashPrintSafe(char *dst, size_t dst_size, const unsigned char *digest,
                    HashMethod type, bool use_prefix)
{
    const char *prefix;

    if (use_prefix)
    {
        switch (type)
        {
        case HASH_METHOD_MD5:
            prefix = "MD5=";
            break;
        default:
            prefix = "SHA=";
            break;
        }
    }
    else
    {
        prefix = "";
    }

    size_t dst_len = MIN(dst_size - 1, strlen(prefix));
    memcpy(dst, prefix, dst_len);

    size_t digest_len = HashSizeFromId(type);
    assert(dst_size >= strlen(prefix) + digest_len*2 + 1);

#ifndef NDEBUG // Avoids warning.
    size_t ret =
#endif
        StringBytesToHex(&dst[dst_len], dst_size - dst_len,
                         digest, digest_len);
    assert(ret == 2 * digest_len);

#if 0         /* TODO return proper exit status and check it in the callers */
    if (ret < 2 * digest_len)
    {
        return NULL;
    }
#endif

    return dst;
}


char *SkipHashType(char *hash)
{
    char *str = hash;

    if (BEGINSWITH(hash, "MD5=") || BEGINSWITH(hash, "SHA="))
    {
        str = hash + 4;
    }

    return str;
}
