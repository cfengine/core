/*
 * HashMethod (unfortunately) needs to be defined in cf3.defs.h. By putting it
 * separate here we avoid cf3.defs.h pulling the whole OpenSSL includes that
 * hash.h has.
 */


#ifndef CFENGINE_HASH_METHOD_H
#define CFENGINE_HASH_METHOD_H


typedef enum
{
    HASH_METHOD_MD5,
    HASH_METHOD_SHA224,
    HASH_METHOD_SHA256,
    HASH_METHOD_SHA384,
    HASH_METHOD_SHA512,
    HASH_METHOD_SHA1,
    HASH_METHOD_SHA,
    HASH_METHOD_BEST,
    HASH_METHOD_CRYPT,
    HASH_METHOD_NONE
} HashMethod;

typedef enum {
    CF_MD5_LEN = 16,
    CF_SHA224_LEN = 28,
    CF_SHA256_LEN = 32,
    CF_SHA384_LEN = 48,
    CF_SHA512_LEN = 64,
    CF_SHA1_LEN = 20,
    CF_SHA_LEN = 20,
    CF_BEST_LEN = 0,
    CF_CRYPT_LEN = 64,
    CF_NO_HASH = 0
} HashSize;


#endif  /* CFENGINE_HASH_METHOD_H */
