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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "files_hashes.h"

#include "dbm_api.h"
#include "files_interfaces.h"
#include "client_code.h"
#include "files_lib.h"
#include "rlist.h"
#include "policy.h"

static const char *CF_DIGEST_TYPES[10][2] =
{
    {"md5", "m"},
    {"sha224", "c"},
    {"sha256", "C"},
    {"sha384", "h"},
    {"sha512", "H"},
    {"sha1", "S"},
    {"sha", "s"},               /* Should come last, since substring */
    {"best", "b"},
    {"crypt", "o"},
    {NULL, NULL}
};

static const int CF_DIGEST_SIZES[10] =
{
    CF_MD5_LEN,
    CF_SHA224_LEN,
    CF_SHA256_LEN,
    CF_SHA384_LEN,
    CF_SHA512_LEN,
    CF_SHA1_LEN,
    CF_SHA_LEN,
    CF_BEST_LEN,
    CF_CRYPT_LEN,
    0
};

void HashFile(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    FILE *file;
    EVP_MD_CTX context;
    int len, md_len;
    unsigned char buffer[1024];
    const EVP_MD *md = NULL;

    if ((file = fopen(filename, "rb")) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Cannot open file for hashing '%s'. (fopen: %s)", filename, GetErrorStr());
    }
    else
    {
        md = EVP_get_digestbyname(FileHashName(type));

        EVP_DigestInit(&context, md);

        while ((len = fread(buffer, 1, 1024, file)))
        {
            EVP_DigestUpdate(&context, buffer, len);
        }

        EVP_DigestFinal(&context, digest, &md_len);

        /* Digest length stored in md_len */
        fclose(file);
    }
}

/*******************************************************************/

void HashString(const char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    EVP_MD_CTX context;
    const EVP_MD *md = NULL;
    int md_len;

    switch (type)
    {
    case HASH_METHOD_CRYPT:
        Log(LOG_LEVEL_ERR, "The crypt support is not presently implemented, please use another algorithm instead");
        memset(digest, 0, EVP_MAX_MD_SIZE + 1);
        break;

    default:
        md = EVP_get_digestbyname(FileHashName(type));

        if (md == NULL)
        {
            Log(LOG_LEVEL_INFO, "Digest type %s not supported by OpenSSL library", CF_DIGEST_TYPES[type][0]);
        }

        EVP_DigestInit(&context, md);
        EVP_DigestUpdate(&context, (unsigned char *) buffer, (size_t) len);
        EVP_DigestFinal(&context, digest, &md_len);
        break;
    }
}

/*******************************************************************/

void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    EVP_MD_CTX context;
    const EVP_MD *md = NULL;
    int md_len, i, buf_len, actlen;
    unsigned char *buffer;

    if (key->n)
    {
        buf_len = (size_t) BN_num_bytes(key->n);
    }
    else
    {
        buf_len = 0;
    }

    if (key->e)
    {
        if (buf_len < (i = (size_t) BN_num_bytes(key->e)))
        {
            buf_len = i;
        }
    }

    buffer = xmalloc(buf_len + 10);

    switch (type)
    {
    case HASH_METHOD_CRYPT:
        Log(LOG_LEVEL_ERR, "The crypt support is not presently implemented, please use sha256 instead");
        break;

    default:
        md = EVP_get_digestbyname(FileHashName(type));

        if (md == NULL)
        {
            Log(LOG_LEVEL_INFO, "Digest type %s not supported by OpenSSL library", CF_DIGEST_TYPES[type][0]);
        }

        EVP_DigestInit(&context, md);

        actlen = BN_bn2bin(key->n, buffer);
        EVP_DigestUpdate(&context, buffer, actlen);
        actlen = BN_bn2bin(key->e, buffer);
        EVP_DigestUpdate(&context, buffer, actlen);
        EVP_DigestFinal(&context, digest, &md_len);
        break;
    }

    free(buffer);
}

/*******************************************************************/

int HashesMatch(unsigned char digest1[EVP_MAX_MD_SIZE + 1], unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                HashMethod type)
{
    int i, size = EVP_MAX_MD_SIZE;

    size = FileHashSize(type);

    for (i = 0; i < size; i++)
    {
        if (digest1[i] != digest2[i])
        {
            return false;
        }
    }

    return true;
}

char *HashPrintSafe(HashMethod type, unsigned char digest[EVP_MAX_MD_SIZE + 1], char buffer[EVP_MAX_MD_SIZE * 4])
/**
 * Thread safe. Note the buffer size.
 */
{
    unsigned int i;

    switch (type)
    {
    case HASH_METHOD_MD5:
        sprintf(buffer, "MD5=  ");
        break;
    default:
        sprintf(buffer, "SHA=  ");
        break;
    }

    for (i = 0; i < CF_DIGEST_SIZES[type]; i++)
    {
        sprintf((char *) (buffer + 4 + 2 * i), "%02x", digest[i]);
    }

    buffer[4 + 2*CF_DIGEST_SIZES[type]] = '\0';

    return buffer;
}


char *SkipHashType(char *hash)
{
    char *str = hash;

    if(BEGINSWITH(hash, "MD5=") || BEGINSWITH(hash, "SHA="))
    {
        str = hash + 4;
    }

    return str;
}

const char *FileHashName(HashMethod id)
{
    return CF_DIGEST_TYPES[id][0];
}

int FileHashSize(HashMethod id)
{
    return CF_DIGEST_SIZES[id];
}

HashMethod HashMethodFromString(char *typestr)
{
    int i;

    for (i = 0; CF_DIGEST_TYPES[i][0] != NULL; i++)
    {
        if (typestr && (strcmp(typestr, CF_DIGEST_TYPES[i][0]) == 0))
        {
            return (HashMethod) i;
        }
    }

    return HASH_METHOD_NONE;
}
