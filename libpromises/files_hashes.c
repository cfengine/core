/*
   Copyright 2018 Northern.tech AS

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

#include <files_hashes.h>

#include <openssl/err.h>                                        /* ERR_* */
#include <openssl/bn.h>                                         /* BN_* */
#include <openssl/evp.h>                                        /* EVP_* */
#include <libcrypto-compat.h>

#include <dbm_api.h>
#include <files_interfaces.h>
#include <client_code.h>
#include <files_lib.h>
#include <rlist.h>
#include <policy.h>
#include <string_lib.h>                                 /* StringBytesToHex */
#include <misc_lib.h>                                   /* UnexpectedError */

static void HashFile_Stream(
    FILE *const file,
    unsigned char digest[EVP_MAX_MD_SIZE + 1],
    const HashMethod type)
{
    assert(file != NULL);
    const EVP_MD *const md = HashDigestFromId(type);
    if (md == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Could not determine function for file hashing (type=%d)",
            (int) type);
        return;
    }

    EVP_MD_CTX *const context = EVP_MD_CTX_new();
    if (context == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to allocate openssl hashing context");
        return;
    }

    if (EVP_DigestInit(context, md) == 1)
    {
        unsigned char buffer[1024];
        size_t len;
        while ((len = fread(buffer, 1, 1024, file)))
        {
            EVP_DigestUpdate(context, buffer, len);
        }

        unsigned int digest_length;
        EVP_DigestFinal(context, digest, &digest_length);
    }

    EVP_MD_CTX_free(context);
}

/**
 * @param text_mode whether to read the file in text mode or not (binary mode)
 * @note Reading/writing file in text mode on Windows changes Unix newlines
 *       into Windows newlines.
 */
void HashFile(
    const char *const filename,
    unsigned char digest[EVP_MAX_MD_SIZE + 1],
    HashMethod type,
    bool text_mode)
{
    assert(filename != NULL);
    assert(digest != NULL);

    memset(digest, 0, EVP_MAX_MD_SIZE + 1);

    FILE *file = NULL;
    if (text_mode)
    {
        file = safe_fopen(filename, "rt");
    }
    else
    {
        file = safe_fopen(filename, "rb");
    }
    if (file == NULL)
    {
        Log(LOG_LEVEL_INFO,
            "Cannot open file for hashing '%s'. (fopen: %s)",
            filename,
            GetErrorStr());
        return;
    }

    HashFile_Stream(file, digest, type);
    fclose(file);
}

/*******************************************************************/

void HashString(
    const char *const buffer,
    const int len,
    unsigned char digest[EVP_MAX_MD_SIZE + 1],
    HashMethod type)
{
    assert(buffer != NULL);
    assert(digest != NULL);

    memset(digest, 0, EVP_MAX_MD_SIZE + 1);

    if (type == HASH_METHOD_CRYPT)
    {
        Log(LOG_LEVEL_ERR,
            "The crypt support is not presently implemented, please use another algorithm instead");
        return;
    }

    const EVP_MD *const md = HashDigestFromId(type);
    if (md == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Could not determine function for file hashing (type=%d)",
            (int) type);
        return;
    }

    EVP_MD_CTX *const context = EVP_MD_CTX_new();
    if (context == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to allocate openssl hashing context");
        return;
    }

    if (EVP_DigestInit(context, md) == 1)
    {
        EVP_DigestUpdate(context, buffer, len);

        unsigned int digest_length;
        EVP_DigestFinal(context, digest, &digest_length);
    }
    else
    {
        Log(LOG_LEVEL_ERR,
            "Failed to initialize digest for hashing: '%s'",
            buffer);
    }

    EVP_MD_CTX_free(context);
}

/*******************************************************************/

void HashPubKey(
    const RSA *const key,
    unsigned char digest[EVP_MAX_MD_SIZE + 1],
    const HashMethod type)
{
    assert(key != NULL);

    memset(digest, 0, EVP_MAX_MD_SIZE + 1);

    if (type == HASH_METHOD_CRYPT)
    {
        Log(LOG_LEVEL_ERR,
            "The crypt support is not presently implemented, please use sha256 instead");
        return;
    }

    const EVP_MD *const md = HashDigestFromId(type);
    if (md == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Could not determine function for file hashing (type=%d)",
            (int) type);
        return;
    }

    EVP_MD_CTX *const context = EVP_MD_CTX_new();
    if (context == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to allocate openssl hashing context");
        return;
    }


    if (EVP_DigestInit(context, md) == 1)
    {
        const BIGNUM *n, *e;
        RSA_get0_key(key, &n, &e, NULL);

        const size_t n_len = (n == NULL) ? 0 : (size_t) BN_num_bytes(n);
        const size_t e_len = (e == NULL) ? 0 : (size_t) BN_num_bytes(e);
        const size_t buf_len = MAX(n_len, e_len);

        unsigned char buffer[buf_len];
        int actlen;
        actlen = BN_bn2bin(n, buffer);
        CF_ASSERT(actlen <= buf_len, "Buffer overflow n, %d > %zu!",
                  actlen, buf_len);
        EVP_DigestUpdate(context, buffer, actlen);

        actlen = BN_bn2bin(e, buffer);
        CF_ASSERT(actlen <= buf_len, "Buffer overflow e, %d > %zu!",
                  actlen, buf_len);
        EVP_DigestUpdate(context, buffer, actlen);

        unsigned int digest_length;
        EVP_DigestFinal(context, digest, &digest_length);
    }

    EVP_MD_CTX_free(context);
}

/*******************************************************************/

int HashesMatch(const unsigned char digest1[EVP_MAX_MD_SIZE + 1],
                const unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                HashMethod type)
{
    const HashSize size = HashSizeFromId(type);
    if (size <= 0) // HashSize is an enum (so int)
    {
        return false;
    }

    for (int i = 0; i < size; i++)
    {
        if (digest1[i] != digest2[i])
        {
            return false;
        }
    }

    return true;
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

#if 0 /* TODO return proper exit status and check it in the callers */
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

    if(STARTSWITH(hash, "MD5=") || STARTSWITH(hash, "SHA="))
    {
        str = hash + 4;
    }

    return str;
}
