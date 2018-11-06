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

#include <crypto.h>

#include <openssl/err.h>                                        /* ERR_* */
#include <openssl/rand.h>                                       /* RAND_* */
#include <openssl/bn.h>                                         /* BN_* */
#include <libcrypto-compat.h>

#include <cf3.defs.h>
#include <lastseen.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <hashes.h>
#include <pipes.h>
#include <mutex.h>
#include <known_dirs.h>
#include <bootstrap.h>
#include <misc_lib.h>                   /* UnexpectedError,ProgrammingError */
#include <file_lib.h>

#ifdef DARWIN
// On Mac OSX 10.7 and later, majority of functions in /usr/include/openssl/crypto.h
// are deprecated. No known replacement, so shutting up compiler warnings
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* The deprecated is the easy way to setup threads for OpenSSL. */
#ifdef OPENSSL_NO_DEPRECATED
void CRYPTO_set_id_callback(unsigned long (*func)(void));
#endif

static void RandomSeed(void);
static void SetupOpenSSLThreadLocks(void);
static void CleanupOpenSSLThreadLocks(void);

/* TODO move crypto.[ch] to libutils. Will need to remove all manipulation of
 * lastseen db. */

static bool crypto_initialized = false; /* GLOBAL_X */

const char *CryptoLastErrorString()
{
    const char *errmsg = ERR_reason_error_string(ERR_get_error());
    return (errmsg != NULL) ? errmsg : "no error message";
}

void CryptoInitialize()
{
    if (!crypto_initialized)
    {
        SetupOpenSSLThreadLocks();
        OpenSSL_add_all_algorithms();
        OpenSSL_add_all_digests();
        ERR_load_crypto_strings();

        RandomSeed();

        crypto_initialized = true;
    }
}

void CryptoDeInitialize()
{
    if (crypto_initialized)
    {
        char randfile[CF_BUFSIZE];
        snprintf(randfile, CF_BUFSIZE, "%s%crandseed",
                 GetWorkDir(), FILE_SEPARATOR);

        /* Only write out a seed if the file doesn't exist
         * and we have enough entropy to do so. If RAND_write_File
         * returns a bad value, delete the poor seed.
         */
        if (access(randfile, R_OK) && errno == ENOENT && RAND_write_file(randfile) != 1024)
        {
            Log(LOG_LEVEL_WARNING,
                "Could not write randomness to '%s'", randfile);
            unlink(randfile); /* do not reuse entropy */
        }

        chmod(randfile, 0600);
        EVP_cleanup();
        CleanupOpenSSLThreadLocks();
        ERR_free_strings();
        crypto_initialized = false;
    }
}

static void RandomSeed(void)
{
    /* 1. Seed the weak C PRNGs. */

    /* Mix various stuff. */
    pid_t pid = getpid();
    size_t fqdn_len = strlen(VFQNAME) > 0 ? strlen(VFQNAME) : 1;
    time_t start_time = CFSTARTTIME;
    time_t now = time(NULL);

    srand((unsigned) pid      * start_time ^
          (unsigned) fqdn_len * now);
    srand48((long)  pid      * start_time ^
            (long)  fqdn_len * now);

    /* 2. Seed the strong OpenSSL PRNG. */

#ifndef __MINGW32__
    RAND_poll();                                        /* windows may hang */
#else
    RAND_screen();                       /* noop unless openssl is very old */
#endif

    if (RAND_status() != 1)
    {
        /* randseed file is written on deinitialization of crypto system */
        char randfile[CF_BUFSIZE];
        snprintf(randfile, CF_BUFSIZE, "%s%crandseed",
                 GetWorkDir(), FILE_SEPARATOR);
        Log(LOG_LEVEL_VERBOSE, "Looking for a source of entropy in '%s'",
            randfile);

        if (RAND_load_file(randfile, -1) != 1024)
        {
            Log(LOG_LEVEL_CRIT,
                "Could not read randomness from '%s'", randfile);
            unlink(randfile); /* kill randseed if error reading it */
        }

        /* If we've used the random seed, then delete */
        unlink(randfile);
    }
}

/* PEM functions need the const cast away, but hopefully the default
 * call-back doesn't actually modify its user-data. */
static const char priv_passphrase[] = PRIVKEY_PASSPHRASE;

/**
 * @param[in] priv_key_path path to the private key to use (%NULL to use the default)
 * @param[in] pub_key_path path to the private key to use (%NULL to use the default)
 * @param[out] priv_key a place to store the loaded private key (or %NULL to
 *             use the global PRIVKEY variable)
 * @param[out] pub_key a place to store the loaded public key (or %NULL to
 *             use the global PUBKEY variable)
 * @return true the error is not so severe that we must stop
 */
bool LoadSecretKeys(const char *const priv_key_path,
                    const char *const pub_key_path,
                    RSA **priv_key, RSA **pub_key)
{
    {
        char *privkeyfile = NULL;
        if (priv_key_path == NULL)
        {
            privkeyfile = PrivateKeyFile(GetWorkDir());
        }
        FILE *fp = fopen(privkeyfile != NULL ? privkeyfile : priv_key_path, "r");
        if (!fp)
        {
            /* VERBOSE in case it's a custom, local-only installation. */
            Log(LOG_LEVEL_VERBOSE,
                "Couldn't find a private key at '%s', use cf-key to get one. (fopen: %s)",
                privkeyfile != NULL ? privkeyfile : priv_key_path, GetErrorStr());
            free(privkeyfile);
            return false;
        }

        if (priv_key == NULL)
        {
            /* if no place to store the private key was specified, use the
             * global variable PRIVKEY */
            priv_key = &PRIVKEY;
        }
        if (*priv_key != NULL)
        {
            DESTROY_AND_NULL(RSA_free, *priv_key);
        }
        *priv_key = PEM_read_RSAPrivateKey(fp, NULL, NULL, (void*) priv_passphrase);
        if (*priv_key == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Error reading private key. (PEM_read_RSAPrivateKey: %s)",
                CryptoLastErrorString());
            *priv_key = NULL;
            fclose(fp);
            return false;
        }

        fclose(fp);
        Log(LOG_LEVEL_VERBOSE, "Loaded private key at '%s'", privkeyfile);
        free(privkeyfile);
    }

    {
        char *pubkeyfile = NULL;
        if (pub_key_path == NULL)
        {
            pubkeyfile = PublicKeyFile(GetWorkDir());
        }
        FILE *fp = fopen(pubkeyfile != NULL ? pubkeyfile : pub_key_path, "r");
        if (!fp)
        {
            /* VERBOSE in case it's a custom, local-only installation. */
            Log(LOG_LEVEL_VERBOSE,
                "Couldn't find a public key at '%s', use cf-key to get one (fopen: %s)",
                pubkeyfile != NULL ? pubkeyfile : pub_key_path, GetErrorStr());
            free(pubkeyfile);
            return false;
        }

        if (pub_key == NULL)
        {
            /* if no place to store the public key was specified, use the
             * global variable PUBKEY */
            pub_key = &PUBKEY;
        }
        if (*pub_key != NULL)
        {
            DESTROY_AND_NULL(RSA_free, *pub_key);
        }
        *pub_key = PEM_read_RSAPublicKey(fp, NULL, NULL, (void*) priv_passphrase);
        if (*pub_key == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Error reading public key at '%s'. (PEM_read_RSAPublicKey: %s)",
                pubkeyfile, CryptoLastErrorString());
            fclose(fp);
            free(pubkeyfile);
            return false;
        }

        Log(LOG_LEVEL_VERBOSE, "Loaded public key '%s'", pubkeyfile);
        free(pubkeyfile);
        fclose(fp);
    }

    if (*pub_key != NULL)
    {
        const BIGNUM *n, *e;
        RSA_get0_key(*pub_key, &n, &e, NULL);
        if ((BN_num_bits(e) < 2) || (!BN_is_odd(e)))
        {
            Log(LOG_LEVEL_ERR, "The public key RSA exponent is too small or not odd");
            return false;
        }
    }

    return true;
}

void PolicyHubUpdateKeys(const char *policy_server)
{
    if (GetAmPolicyHub() && PUBKEY != NULL)
    {
        unsigned char digest[EVP_MAX_MD_SIZE + 1];
        const char* const workdir = GetWorkDir();

        char dst_public_key_filename[CF_BUFSIZE] = "";
        {
            char buffer[CF_HOSTKEY_STRING_SIZE];
            HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
            snprintf(dst_public_key_filename, sizeof(dst_public_key_filename),
                     "%s/ppkeys/%s-%s.pub",
                     workdir, "root",
                     HashPrintSafe(buffer, sizeof(buffer), digest,
                                   CF_DEFAULT_DIGEST, true));
            MapName(dst_public_key_filename);
        }

        struct stat sb;
        if ((stat(dst_public_key_filename, &sb) == -1))
        {
            char src_public_key_filename[CF_BUFSIZE] = "";
            snprintf(src_public_key_filename, CF_MAXVARSIZE, "%s/ppkeys/localhost.pub", workdir);
            MapName(src_public_key_filename);

            // copy localhost.pub to root-HASH.pub on policy server
            if (!LinkOrCopy(src_public_key_filename, dst_public_key_filename, false))
            {
                Log(LOG_LEVEL_ERR, "Unable to copy policy server's own public key from '%s' to '%s'", src_public_key_filename, dst_public_key_filename);
            }

            if (policy_server)
            {
                LastSaw(policy_server, digest, LAST_SEEN_ROLE_CONNECT);
            }
        }
    }
}

/*********************************************************************/

/**
 * @brief Search for a key given an IP address, by getting the
 *        key hash value from lastseen db.
 * @return NULL if the key was not found in any form.
 */
RSA *HavePublicKeyByIP(const char *username, const char *ipaddress)
{
    char hash[CF_HOSTKEY_STRING_SIZE];

    /* Get the key hash for that address from lastseen db. */
    bool found = Address2Hostkey(hash, sizeof(hash), ipaddress);

    /* If not found, by passing "" as digest, we effectively look only for
     * the old-style key file, e.g. root-1.2.3.4.pub. */
    return HavePublicKey(username, ipaddress,
                         found ? hash : "");
}

static const char *const pub_passphrase = "public";

/**
 * @brief Search for a key:
 *        1. username-hash.pub
 *        2. username-ip.pub
 * @return NULL if key not found in any form
 */
RSA *HavePublicKey(const char *username, const char *ipaddress, const char *digest)
{
    char keyname[CF_MAXVARSIZE], newname[CF_BUFSIZE], oldname[CF_BUFSIZE];
    struct stat statbuf;
    FILE *fp;
    RSA *newkey = NULL;
    const char* const workdir = GetWorkDir();

    snprintf(keyname, CF_MAXVARSIZE, "%s-%s", username, digest);

    snprintf(newname, CF_BUFSIZE, "%s/ppkeys/%s.pub", workdir, keyname);
    MapName(newname);

    if (stat(newname, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Did not find new key format '%s'", newname);

        snprintf(oldname, CF_BUFSIZE, "%s/ppkeys/%s-%s.pub",
                 workdir, username, ipaddress);
        MapName(oldname);
        Log(LOG_LEVEL_VERBOSE, "Trying old style '%s'", oldname);

        if (stat(oldname, &statbuf) == -1)
        {
            Log(LOG_LEVEL_DEBUG, "Did not have old-style key '%s'", oldname);
            return NULL;
        }

        if (strlen(digest) > 0)
        {
            Log(LOG_LEVEL_INFO, "Renaming old key from '%s' to '%s'", oldname, newname);

            if (rename(oldname, newname) != 0)
            {
                Log(LOG_LEVEL_ERR, "Could not rename from old key format '%s' to new '%s'. (rename: %s)", oldname, newname, GetErrorStr());
            }
        }
        else
        {
            /* We don't know the digest (e.g. because we are a client and have
               no lastseen-map yet), so we're using old file format
               (root-IP.pub). */
            Log(LOG_LEVEL_VERBOSE,
                "We have no digest yet, using old keyfile name: %s",
                oldname);
            snprintf(newname, sizeof(newname), "%s", oldname);
        }
    }

    if ((fp = fopen(newname, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open public key file '%s' (fopen: %s)",
            newname, GetErrorStr());
        return NULL;
    }

    if ((newkey = PEM_read_RSAPublicKey(fp, NULL, NULL,
                                        (void *)pub_passphrase)) == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Error reading public key from '%s' (PEM_read_RSAPublicKey: %s)",
            newname, CryptoLastErrorString());
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    {
        const BIGNUM *n, *e;
        RSA_get0_key(newkey, &n, &e, NULL);
        if ((BN_num_bits(e) < 2) || (!BN_is_odd(e)))
        {
            Log(LOG_LEVEL_ERR, "RSA Exponent too small or not odd for key: %s",
                newname);
            RSA_free(newkey);
            return NULL;
        }
    }

    return newkey;
}

/*********************************************************************/

bool SavePublicKey(const char *user, const char *digest, const RSA *key)
{
    char keyname[CF_MAXVARSIZE], filename[CF_BUFSIZE];
    struct stat statbuf;
    FILE *fp;
    int ret;

    ret = snprintf(keyname, sizeof(keyname), "%s-%s", user, digest);
    if (ret >= sizeof(keyname))
    {
        Log(LOG_LEVEL_ERR, "USERNAME-KEY (%s-%s) string too long!",
            user, digest);
        return false;
    }

    ret = snprintf(filename, sizeof(filename), "%s/ppkeys/%s.pub",
                   GetWorkDir(), keyname);
    if (ret >= sizeof(filename))
    {
        Log(LOG_LEVEL_ERR, "Filename too long!");
        return false;
    }

    MapName(filename);
    if (stat(filename, &statbuf) != -1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Public key file '%s' already exists, not rewriting",
            filename);
        return true;
    }

    Log(LOG_LEVEL_VERBOSE, "Saving public key to file '%s'", filename);

    if ((fp = fopen(filename, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to write a public key '%s'. (fopen: %s)", filename, GetErrorStr());
        return false;
    }

    if (!PEM_write_RSAPublicKey(fp, key))
    {
        Log(LOG_LEVEL_ERR,
            "Error saving public key to '%s'. (PEM_write_RSAPublicKey: %s)",
            filename, CryptoLastErrorString());
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

RSA *LoadPublicKey(const char *filename)
{
    FILE *fp;
    RSA *key;
    const BIGNUM *n, *e;

    fp = safe_fopen(filename, "r");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot open public key file '%s' (fopen: %s)", filename, GetErrorStr());
        return NULL;
    };

    if ((key = PEM_read_RSAPublicKey(fp, NULL, NULL,
                                     (void *)priv_passphrase)) == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Error while reading public key '%s' (PEM_read_RSAPublicKey: %s)",
            filename,
            CryptoLastErrorString());
        fclose(fp);
        return NULL;
    };

    fclose(fp);

    RSA_get0_key(key, &n, &e, NULL);

    if (BN_num_bits(e) < 2 || !BN_is_odd(e))
    {
        Log(LOG_LEVEL_ERR, "Error while reading public key '%s' - RSA Exponent is too small or not odd. (BN_num_bits: %s)",
            filename, GetErrorStr());
        return NULL;
    };

    return key;
}

/** Return a string with the printed digest of the given key file,
    or NULL if an error occurred. */
char *LoadPubkeyDigest(const char *filename)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    RSA *key = NULL;
    char *buffer = xmalloc(CF_HOSTKEY_STRING_SIZE);

    key = LoadPublicKey(filename);
    if (key == NULL)
    {
        return NULL;
    }

    HashPubKey(key, digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(buffer, CF_HOSTKEY_STRING_SIZE,
                  digest, CF_DEFAULT_DIGEST, true);
    RSA_free(key);
    return buffer;
}

/** Return a string with the printed digest of the given key file. */
char *GetPubkeyDigest(RSA *pubkey)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    char *buffer = xmalloc(CF_HOSTKEY_STRING_SIZE);

    HashPubKey(pubkey, digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(buffer, CF_HOSTKEY_STRING_SIZE,
                  digest, CF_DEFAULT_DIGEST, true);
    return buffer;
}


/**
 * Trust the given key.  If #ipaddress is not NULL, then also
 * update the "last seen" database.  The IP address is required for
 * trusting a server key (on the client); it is -currently- optional
 * for trusting a client key (on the server).
 */
bool TrustKey(const char *filename, const char *ipaddress, const char *username)
{
    RSA* key;
    char *digest;

    key = LoadPublicKey(filename);
    if (key == NULL)
    {
        return false;
    }

    digest = GetPubkeyDigest(key);
    if (digest == NULL)
    {
        RSA_free(key);
        return false;
    }

    if (ipaddress != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Adding a CONNECT entry in lastseen db: IP '%s', key '%s'",
            ipaddress, digest);
        LastSaw1(ipaddress, digest, LAST_SEEN_ROLE_CONNECT);
    }

    bool ret = SavePublicKey(username, digest, key);
    RSA_free(key);
    free(digest);

    return ret;
}

int EncryptString(char *out, size_t out_size, const char *in, int plainlen,
                  char type, unsigned char *key)
{
    int cipherlen = 0, tmplen;
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };

    if (key == NULL)
        ProgrammingError("EncryptString: session key == NULL");

    size_t max_ciphertext_size = CipherTextSizeMax(CfengineCipher(type), plainlen);

    if(max_ciphertext_size > out_size)
    {
        ProgrammingError("EncryptString: output buffer too small: max_ciphertext_size (%zd) > out_size (%zd)",
                          max_ciphertext_size, out_size);
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, CfengineCipher(type), NULL, key, iv);

    if (!EVP_EncryptUpdate(ctx, out, &cipherlen, in, plainlen))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (!EVP_EncryptFinal_ex(ctx, out + cipherlen, &tmplen))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    cipherlen += tmplen;

    if(cipherlen > max_ciphertext_size)
    {
        ProgrammingError("EncryptString: too large ciphertext written: cipherlen (%d) > max_ciphertext_size (%zd)",
                          cipherlen, max_ciphertext_size);
    }

    EVP_CIPHER_CTX_free(ctx);
    return cipherlen;
}

size_t CipherBlockSizeBytes(const EVP_CIPHER *cipher)
{
    return EVP_CIPHER_block_size(cipher);
}

size_t CipherTextSizeMax(const EVP_CIPHER* cipher, size_t plaintext_size)
{
    // see man EVP_DecryptUpdate() and EVP_DecryptFinal_ex()
    size_t padding_size = (CipherBlockSizeBytes(cipher) * 2) - 1;

    // check for potential integer overflow, leave some buffer
    if(plaintext_size > SIZE_MAX - padding_size)
    {
        ProgrammingError("CipherTextSizeMax: plaintext_size is too large (%zu)",
                         plaintext_size);
    }

    return plaintext_size + padding_size;
}

size_t PlainTextSizeMax(const EVP_CIPHER* cipher, size_t ciphertext_size)
{
    // see man EVP_DecryptUpdate() and EVP_DecryptFinal_ex()
    size_t padding_size = (CipherBlockSizeBytes(cipher) * 2);

    // check for potential integer overflow, leave some buffer
    if(ciphertext_size > SIZE_MAX - padding_size)
    {
        ProgrammingError("PlainTextSizeMax: ciphertext_size is too large (%zu)",
                         ciphertext_size);
    }

    return ciphertext_size + padding_size;
}

/*********************************************************************/

int DecryptString(char *out, size_t out_size, const char *in, int cipherlen,
                  char type, unsigned char *key)
{
    int plainlen = 0, tmplen;
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };

    if (key == NULL)
        ProgrammingError("DecryptString: session key == NULL");

    size_t max_plaintext_size = PlainTextSizeMax(CfengineCipher(type), cipherlen);

    if(max_plaintext_size > out_size)
    {
        ProgrammingError("DecryptString: output buffer too small: max_plaintext_size (%zd) > out_size (%zd)",
                          max_plaintext_size, out_size);
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, CfengineCipher(type), NULL, key, iv);

    if (!EVP_DecryptUpdate(ctx, out, &plainlen, in, cipherlen))
    {
        Log(LOG_LEVEL_ERR, "Failed to decrypt string");
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (!EVP_DecryptFinal_ex(ctx, out + plainlen, &tmplen))
    {
        unsigned long err = ERR_get_error();

        Log(LOG_LEVEL_ERR, "Failed to decrypt at final of cipher length %d. (EVP_DecryptFinal_ex: %s)", cipherlen, ERR_error_string(err, NULL));
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    plainlen += tmplen;

    if(plainlen > max_plaintext_size)
    {
        ProgrammingError("DecryptString: too large plaintext written: plainlen (%d) > max_plaintext_size (%zd)",
                          plainlen, max_plaintext_size);
    }

    EVP_CIPHER_CTX_free(ctx);
    return plainlen;
}

/*********************************************************************/

void DebugBinOut(char *buffer, int len, char *comment)
{
    unsigned char *sp;
    char buf[CF_BUFSIZE];
    char hexStr[3];             // one byte as hex

    if (len >= (sizeof(buf) / 2))       // hex uses two chars per byte
    {
        Log(LOG_LEVEL_DEBUG, "Debug binary print is too large (len = %d)", len);
        return;
    }

    memset(buf, 0, sizeof(buf));

    for (sp = buffer; sp < (unsigned char *) (buffer + len); sp++)
    {
        xsnprintf(hexStr, sizeof(hexStr), "%2.2x", (int) *sp);
        strcat(buf, hexStr);
    }

    Log(LOG_LEVEL_VERBOSE, "BinaryBuffer, %d bytes, comment '%s', buffer '%s'", len, comment, buf);
}

char *PublicKeyFile(const char *workdir)
{
    char *keyfile;
    xasprintf(&keyfile,
              "%s" FILE_SEPARATOR_STR "ppkeys" FILE_SEPARATOR_STR "localhost.pub", workdir);
    return keyfile;
}

char *PrivateKeyFile(const char *workdir)
{
    char *keyfile;
    xasprintf(&keyfile,
              "%s" FILE_SEPARATOR_STR "ppkeys" FILE_SEPARATOR_STR "localhost.priv", workdir);
    return keyfile;
}


/*********************************************************************
 * Functions for threadsafe OpenSSL usage                            *
 * Only pthread support - we don't create threads with any other API *
 *********************************************************************/

static pthread_mutex_t *cf_openssl_locks = NULL;


#ifndef __MINGW32__
unsigned long ThreadId_callback(void)
{
    return (unsigned long) pthread_self();
}
#endif

static void OpenSSLLock_callback(int mode, int index, char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        int ret = pthread_mutex_lock(&(cf_openssl_locks[index]));
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR,
                "OpenSSL locking failure at %s:%d! (pthread_mutex_lock: %s)",
                file, line, GetErrorStrFromCode(ret));
        }
    }
    else
    {
        int ret = pthread_mutex_unlock(&(cf_openssl_locks[index]));
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR,
                "OpenSSL locking failure at %s:%d! (pthread_mutex_unlock: %s)",
                file, line, GetErrorStrFromCode(ret));
        }
    }
}

static void SetupOpenSSLThreadLocks(void)
{
    const int num_locks = CRYPTO_num_locks();
    assert(cf_openssl_locks == NULL);
    cf_openssl_locks = xmalloc(num_locks * sizeof(*cf_openssl_locks));

    for (int i = 0; i < num_locks; i++)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        int ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to use error-checking mutexes for openssl,"
                " falling back to normal ones (pthread_mutexattr_settype: %s)",
                GetErrorStrFromCode(ret));
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
        }
        ret = pthread_mutex_init(&cf_openssl_locks[i], &attr);
        if (ret != 0)
        {
            Log(LOG_LEVEL_CRIT,
                "Failed to use initialise mutexes for openssl"
                " (pthread_mutex_init: %s)!",
                GetErrorStrFromCode(ret));
        }
        pthread_mutexattr_destroy(&attr);
    }

#ifndef __MINGW32__
    CRYPTO_set_id_callback((unsigned long (*)())ThreadId_callback);
#endif
    CRYPTO_set_locking_callback((void (*)())OpenSSLLock_callback);
}

static void CleanupOpenSSLThreadLocks(void)
{
    const int numLocks = CRYPTO_num_locks();
    CRYPTO_set_locking_callback(NULL);
#ifndef __MINGW32__
    CRYPTO_set_id_callback(NULL);
#endif

    for (int i = 0; i < numLocks; i++)
    {
        pthread_mutex_destroy(&(cf_openssl_locks[i]));
    }

    free(cf_openssl_locks);
    cf_openssl_locks = NULL;
}
