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

#include <platform.h>

#include <openssl/evp.h>                                       /* EVP_* */
#include <openssl/bn.h>                                        /* BN_bn2bin */
#include <libcrypto-compat.h>

#include <alloc.h>
#include <logging.h>
#include <hash.h>
#include <misc_lib.h>


static const char *const CF_DIGEST_TYPES[10] =
{
    "md5",
    "sha224",
    "sha256",
    "sha384",
    "sha512",
    "sha1",
    "sha",
    "best",
    "crypt",
    NULL
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
    CF_NO_HASH
};

struct Hash {
    unsigned char digest[EVP_MAX_MD_SIZE];
    char printable[EVP_MAX_MD_SIZE * 4];
    HashMethod method;
    HashSize size;
};

/*
 * These methods are not exported through the public API.
 * These are internal methods used by the constructors of a
 * Hash object. Do not export them since they have very little
 * meaning outside of the constructors.
 */
Hash *HashBasicInit(HashMethod method)
{
    Hash *hash = xcalloc (1, sizeof(Hash));
    hash->size = CF_DIGEST_SIZES[method];
    hash->method = method;
    return hash;
}

void HashCalculatePrintableRepresentation(Hash *hash)
{
    switch (hash->method)
    {
        case HASH_METHOD_MD5:
            strcpy(hash->printable, "MD5=");
            break;
        case HASH_METHOD_SHA224:
        case HASH_METHOD_SHA256:
        case HASH_METHOD_SHA384:
        case HASH_METHOD_SHA512:
        case HASH_METHOD_SHA:
        case HASH_METHOD_SHA1:
            strcpy(hash->printable, "SHA=");
            break;
        default:
            strcpy(hash->printable, "UNK=");
            break;
    }

    unsigned int i;
    for (i = 0; i < hash->size; i++)
    {
        snprintf(hash->printable + 4 + 2 * i,
                 sizeof(hash->printable) - (4 + 2 * i), "%02x",
                 hash->digest[i]);
    }
    hash->printable[4 + 2 * hash->size] = '\0';
}

/*
 * Constructors
 * All constructors call two common methods: HashBasicInit(...) and HashCalculatePrintableRepresentation(...).
 * Each constructor reads the data to create the Hash from different sources so after the basic
 * initialization and up to the point where the hash is computed, each follows its own path.
 */
Hash *HashNew(const char *data, const unsigned int length, HashMethod method)
{
    if (!data || (length == 0))
    {
        return NULL;
    }
    if (method >= HASH_METHOD_NONE)
    {
        return NULL;
    }
    /*
     * OpenSSL documentation marked EVP_DigestInit and EVP_DigestFinal functions as deprecated and
     * recommends moving to EVP_DigestInit_ex and EVP_DigestFinal_ex.
     */
    const EVP_MD *md = NULL;
    int md_len = 0;
    md = EVP_get_digestbyname(CF_DIGEST_TYPES[method]);
    if (md == NULL)
    {
        Log(LOG_LEVEL_INFO, "Digest type %s not supported by OpenSSL library", CF_DIGEST_TYPES[method]);
        return NULL;
    }

    EVP_MD_CTX *const context = EVP_MD_CTX_create();
    if (context == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not allocate openssl hash context");
        return NULL;
    }
    Hash *hash = HashBasicInit(method);
    EVP_DigestInit_ex(context, md, NULL);
    EVP_DigestUpdate(context, data, (size_t) length);
    EVP_DigestFinal_ex(context, hash->digest, &md_len);
    EVP_MD_CTX_destroy(context);
    /* Update the printable representation */
    HashCalculatePrintableRepresentation(hash);
    /* Return the hash */
    return hash;
}

Hash *HashNewFromDescriptor(const int descriptor, HashMethod method)
{
    if (descriptor < 0)
    {
        return NULL;
    }
    if (method >= HASH_METHOD_NONE)
    {
        return NULL;
    }
    char buffer[1024];
    int read_count = 0;
    EVP_MD_CTX *context = NULL;
    const EVP_MD *md = NULL;
    int md_len = 0;
    md = EVP_get_digestbyname(CF_DIGEST_TYPES[method]);
    if (md == NULL)
    {
        Log(LOG_LEVEL_INFO, "Digest type %s not supported by OpenSSL library", CF_DIGEST_TYPES[method]);
        return NULL;
    }
    Hash *hash = HashBasicInit(method);
    context = EVP_MD_CTX_create();
    if (context == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not allocate openssl hash context");
        return NULL;
    }
    EVP_DigestInit_ex(context, md, NULL);
    do
    {
        read_count = read(descriptor, buffer, 1024);
        EVP_DigestUpdate(context, buffer, (size_t) read_count);
    } while (read_count > 0);
    EVP_DigestFinal_ex(context, hash->digest, &md_len);
    EVP_MD_CTX_destroy(context);
    /* Update the printable representation */
    HashCalculatePrintableRepresentation(hash);
    /* Return the hash */
    return hash;
}

Hash *HashNewFromKey(const RSA *rsa, HashMethod method)
{
    if (rsa == NULL)
    {
        return NULL;
    }
    if (method >= HASH_METHOD_NONE)
    {
        return NULL;
    }

    const BIGNUM *n, *e;
    RSA_get0_key(rsa, &n, &e, NULL);

    size_t n_len = (n == NULL) ? 0 : (size_t) BN_num_bytes(n);
    size_t e_len = (e == NULL) ? 0 : (size_t) BN_num_bytes(e);
    size_t buf_len = MAX(n_len, e_len);

    if (buf_len <= 0)
    {
        // Should never happen
        Log(LOG_LEVEL_ERR, "Invalid RSA key, internal OpenSSL related error");
        return NULL;
    }

    const EVP_MD *md = EVP_get_digestbyname(CF_DIGEST_TYPES[method]);
    if (md == NULL)
    {
        Log(LOG_LEVEL_INFO, "Digest type %s not supported by OpenSSL library", CF_DIGEST_TYPES[method]);
        return NULL;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (context == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to allocate openssl hashing context");
        return NULL;
    }

    if (EVP_DigestInit_ex(context, md, NULL) != 1)
    {
        EVP_MD_CTX_free(context);
        return NULL;
    }

    unsigned char buffer[buf_len];
    int md_len, actlen;

    actlen = BN_bn2bin(n, buffer);
    CF_ASSERT(actlen <= buf_len, "Buffer overflow n, %d > %zu!",
              actlen, buf_len);
    EVP_DigestUpdate(context, buffer, actlen);

    actlen = BN_bn2bin(e, buffer);
    CF_ASSERT(actlen <= buf_len, "Buffer overflow e, %d > %zu!",
              actlen, buf_len);
    EVP_DigestUpdate(context, buffer, actlen);

    Hash *hash = HashBasicInit(method);
    EVP_DigestFinal_ex(context, hash->digest, &md_len);

    EVP_MD_CTX_free(context);

    /* Update the printable representation */
    HashCalculatePrintableRepresentation(hash);

    return hash;
}

void HashDestroy(Hash **hash)
{
    if (!hash || !*hash)
    {
        return;
    }
    free (*hash);
    *hash = NULL;
}

int HashCopy(Hash *origin, Hash **destination)
{
    if (!origin || !destination)
    {
        return -1;
    }
    *destination = xmalloc(sizeof(Hash));
    memcpy((*destination)->digest, origin->digest, origin->size);
    strlcpy((*destination)->printable, origin->printable, (EVP_MAX_MD_SIZE * 4));
    (*destination)->method = origin->method;
    (*destination)->size = origin->size;
    return 0;
}

int HashEqual(const Hash *a, const Hash *b)
{
    if (!a && !b)
    {
        return true;
    }
    if (!a && b)
    {
        return false;
    }
    if (a && !b)
    {
        return false;
    }
    if (a->method != b->method)
    {
        return false;
    }
    int i = 0;
    for (i = 0; i < a->size; ++i)
    {
        if (a->digest[i] != b->digest[i])
        {
            return false;
        }
    }
    return true;
}

const unsigned char *HashData(const Hash *hash, unsigned int *length)
{
    if (!hash || !length)
    {
        return NULL;
    }
    *length = hash->size;
    return hash->digest;
}

const char *HashPrintable(const Hash *hash)
{
    return hash ? hash->printable : NULL;
}

HashMethod HashType(const Hash *hash)
{
    return hash ? hash->method : HASH_METHOD_NONE;
}

HashSize HashLength(const Hash *hash)
{
    return hash ? hash->size : CF_NO_HASH;
}

/* Class methods */
HashMethod HashIdFromName(const char *hash_name)
{
    int i;
    for (i = 0; CF_DIGEST_TYPES[i] != NULL; i++)
    {
        if (hash_name && (strcmp(hash_name, CF_DIGEST_TYPES[i]) == 0))
        {
            return (HashMethod) i;
        }
    }

    return HASH_METHOD_NONE;
}

const char *HashNameFromId(HashMethod hash_id)
{
    return (hash_id >= HASH_METHOD_NONE) ? NULL : CF_DIGEST_TYPES[hash_id];
}

const EVP_MD *HashDigestFromId(HashMethod type)
{
    const char *const name = HashNameFromId(type);
    if (name == NULL)
    {
        return NULL;
    }
    return EVP_get_digestbyname(name);
}


HashSize HashSizeFromId(HashMethod hash_id)
{
    return (hash_id >= HASH_METHOD_NONE) ? CF_NO_HASH : CF_DIGEST_SIZES[hash_id];
}
