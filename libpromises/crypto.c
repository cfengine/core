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

#include <crypto.h>

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
LogLevel CryptoGetMissingKeyLogLevel();

/* TODO move crypto.[ch] to libutils. Will need to remove all manipulation of
 * lastseen db. */

static bool crypto_initialized = false; /* GLOBAL_X */

void CryptoInitialize()
{
    if (!crypto_initialized)
    {
        SetupOpenSSLThreadLocks();
        OpenSSL_add_all_algorithms();
        OpenSSL_add_all_digests();
        ERR_load_crypto_strings();

        RandomSeed();

        long seed = 0;
        RAND_bytes((unsigned char *)&seed, sizeof(seed));
        srand48(seed);

        crypto_initialized = true;
    }
}

void CryptoDeInitialize()
{
    if (crypto_initialized)
    {
        EVP_cleanup();
        CleanupOpenSSLThreadLocks();
        // TODO: Is there an ERR_unload_crypto_strings() ?
        // TODO: Are there OpenSSL_clear_all_{digests,algorithms} ?
        crypto_initialized = false;
    }
}

static void RandomSeed(void)
{
    char vbuff[CF_BUFSIZE];

/* Use the system database as the entropy source for random numbers */
    Log(LOG_LEVEL_DEBUG, "RandomSeed() work directory is '%s'", CFWORKDIR);

    snprintf(vbuff, CF_BUFSIZE, "%s%crandseed", CFWORKDIR, FILE_SEPARATOR);

    Log(LOG_LEVEL_VERBOSE, "Looking for a source of entropy in '%s'", vbuff);

    if (!RAND_load_file(vbuff, -1))
    {
        Log(LOG_LEVEL_VERBOSE, "Could not read sufficient randomness from '%s'", vbuff);
    }

    /* Submit some random data to random pool */
    RAND_seed(&CFSTARTTIME, sizeof(time_t));
    RAND_seed(VFQNAME, strlen(VFQNAME));
    time_t now = time(NULL);
    RAND_seed(&now, sizeof(time_t));
    char uninitbuffer[100];
    RAND_seed(uninitbuffer, sizeof(uninitbuffer));
}

static const char *const priv_passphrase = "Cfengine passphrase";

/**
 * @return true the error is not so severe that we must stop
 */
bool LoadSecretKeys(void)
{
    {
        char *privkeyfile = PrivateKeyFile(GetWorkDir());
        FILE *fp = fopen(privkeyfile, "r");
        if (!fp)
        {
            Log(CryptoGetMissingKeyLogLevel(),
                "Couldn't find a private key at '%s', use cf-key to get one. (fopen: %s)",
                privkeyfile, GetErrorStr());
            free(privkeyfile);
            return false;
        }

        if ((PRIVKEY = PEM_read_RSAPrivateKey(fp, (RSA **) NULL, NULL,
                                              (void *)priv_passphrase)) == NULL)
        {
            unsigned long err = ERR_get_error();
            Log(LOG_LEVEL_ERR,
                "Error reading private key. (PEM_read_RSAPrivateKey: %s)",
                ERR_reason_error_string(err));
            PRIVKEY = NULL;
            fclose(fp);
            return false;
        }

        fclose(fp);
        Log(LOG_LEVEL_VERBOSE, "Loaded private key at '%s'", privkeyfile);
        free(privkeyfile);
    }

    {
        char *pubkeyfile = PublicKeyFile(GetWorkDir());
        FILE *fp = fopen(pubkeyfile, "r");
        if (!fp)
        {
            Log(CryptoGetMissingKeyLogLevel(),
                "Couldn't find a public key at '%s', use cf-key to get one (fopen: %s)",
                pubkeyfile, GetErrorStr());
            free(pubkeyfile);
            return false;
        }

        PUBKEY = PEM_read_RSAPublicKey(fp, NULL, NULL, (void *)priv_passphrase);
        if (NULL == PUBKEY)
        {
            unsigned long err = ERR_get_error();
            Log(LOG_LEVEL_ERR,
                "Error reading public key at '%s'. (PEM_read_RSAPublicKey: %s)",
                pubkeyfile, ERR_reason_error_string(err));
            fclose(fp);
            free(pubkeyfile);
            return false;
        }

        Log(LOG_LEVEL_VERBOSE, "Loaded public key '%s'", pubkeyfile);
        free(pubkeyfile);
        fclose(fp);
    }

    if (NULL != PUBKEY
        && ((BN_num_bits(PUBKEY->e) < 2) || (!BN_is_odd(PUBKEY->e))))
    {
        Log(LOG_LEVEL_ERR, "The public key RSA exponent is too small or not odd");
        return false;
    }

    return true;
}

void PolicyHubUpdateKeys(const char *policy_server)
{
    if (GetAmPolicyHub(CFWORKDIR)
        && NULL != PUBKEY)
    {
        unsigned char digest[EVP_MAX_MD_SIZE + 1];

        char dst_public_key_filename[CF_BUFSIZE] = "";
        {
            char buffer[EVP_MAX_MD_SIZE * 4];
            HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
            snprintf(dst_public_key_filename, CF_MAXVARSIZE,
                     "%s/ppkeys/%s-%s.pub",
                     CFWORKDIR,
                     "root",
                     HashPrintSafe(CF_DEFAULT_DIGEST, true, digest, buffer));
            MapName(dst_public_key_filename);
        }

        struct stat sb;
        if ((stat(dst_public_key_filename, &sb) == -1))
        {
            char src_public_key_filename[CF_BUFSIZE] = "";
            snprintf(src_public_key_filename, CF_MAXVARSIZE, "%s/ppkeys/localhost.pub", CFWORKDIR);
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
 * @brief Search for a key just like HavePublicKey(), but get the
 *        hash value from lastseen db.
 * @return NULL if the key was not found in any form.
 */
RSA *HavePublicKeyByIP(const char *username, const char *ipaddress)
{
    char hash[CF_MAXVARSIZE];

    bool found = Address2Hostkey(ipaddress, hash);
    if (found)
    {
        return HavePublicKey(username, ipaddress, hash);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Key for host '%s' not found in lastseen db",
            ipaddress);
        return HavePublicKey(username, ipaddress, "");
    }
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
    unsigned long err;
    FILE *fp;
    RSA *newkey = NULL;

    snprintf(keyname, CF_MAXVARSIZE, "%s-%s", username, digest);

    snprintf(newname, CF_BUFSIZE, "%s/ppkeys/%s.pub", CFWORKDIR, keyname);
    MapName(newname);

    if (stat(newname, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Did not find new key format '%s'", newname);
        snprintf(oldname, CF_BUFSIZE, "%s/ppkeys/%s-%s.pub", CFWORKDIR, username, ipaddress);
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
        Log(CryptoGetMissingKeyLogLevel(), "Couldn't find a public key '%s'. (fopen: %s)", newname, GetErrorStr());
        return NULL;
    }

    if ((newkey = PEM_read_RSAPublicKey(fp, NULL, NULL,
                                        (void *)pub_passphrase)) == NULL)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Error reading public key. (PEM_read_RSAPublicKey: %s)", ERR_reason_error_string(err));
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    if ((BN_num_bits(newkey->e) < 2) || (!BN_is_odd(newkey->e)))
    {
        Log(LOG_LEVEL_ERR, "RSA Exponent too small or not odd");
        RSA_free(newkey);
        return NULL;
    }

    return newkey;
}

/*********************************************************************/

void SavePublicKey(const char *user, const char *digest, const RSA *key)
{
    char keyname[CF_MAXVARSIZE], filename[CF_BUFSIZE];
    struct stat statbuf;
    FILE *fp;
    int err, ret;

    ret = snprintf(keyname, sizeof(keyname), "%s-%s", user, digest);
    if (ret >= sizeof(keyname))
    {
        Log(LOG_LEVEL_ERR, "USERNAME-KEY (%s-%s) string too long!",
            user, digest);
        return;
    }

    ret = snprintf(filename, sizeof(filename), "%s/ppkeys/%s.pub",
                   CFWORKDIR, keyname);
    if (ret >= sizeof(filename))
    {
        Log(LOG_LEVEL_ERR, "Filename too long!");
        return;
    }

    MapName(filename);
    if (stat(filename, &statbuf) != -1)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Saving public key to file '%s'", filename);

    if ((fp = fopen(filename, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to write a public key '%s'. (fopen: %s)", filename, GetErrorStr());
        return;
    }

    if (!PEM_write_RSAPublicKey(fp, key))
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Error saving public key to '%s'. (PEM_write_RSAPublicKey: %s)", filename, ERR_reason_error_string(err));
    }

    fclose(fp);
}

int EncryptString(char type, const char *in, char *out, unsigned char *key, int plainlen)
{
    int cipherlen = 0, tmplen;
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    EVP_CIPHER_CTX ctx;

    if (key == NULL)
        ProgrammingError("EncryptString: session key == NULL");

    EVP_CIPHER_CTX_init(&ctx);
    EVP_EncryptInit_ex(&ctx, CfengineCipher(type), NULL, key, iv);

    if (!EVP_EncryptUpdate(&ctx, out, &cipherlen, in, plainlen))
    {
        EVP_CIPHER_CTX_cleanup(&ctx);
        return -1;
    }

    if (!EVP_EncryptFinal_ex(&ctx, out + cipherlen, &tmplen))
    {
        EVP_CIPHER_CTX_cleanup(&ctx);
        return -1;
    }

    cipherlen += tmplen;
    EVP_CIPHER_CTX_cleanup(&ctx);
    return cipherlen;
}

/*********************************************************************/

int DecryptString(char type, const char *in, char *out, unsigned char *key, int cipherlen)
{
    int plainlen = 0, tmplen;
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    EVP_CIPHER_CTX ctx;

    if (key == NULL)
        ProgrammingError("DecryptString: session key == NULL");

    EVP_CIPHER_CTX_init(&ctx);
    EVP_DecryptInit_ex(&ctx, CfengineCipher(type), NULL, key, iv);

    if (!EVP_DecryptUpdate(&ctx, out, &plainlen, in, cipherlen))
    {
        Log(LOG_LEVEL_ERR, "Failed to decrypt string");
        EVP_CIPHER_CTX_cleanup(&ctx);
        return -1;
    }

    if (!EVP_DecryptFinal_ex(&ctx, out + plainlen, &tmplen))
    {
        unsigned long err = ERR_get_error();

        Log(LOG_LEVEL_ERR, "Failed to decrypt at final of cipher length %d. (EVP_DecryptFinal_ex: %s)", cipherlen, ERR_error_string(err, NULL));
        EVP_CIPHER_CTX_cleanup(&ctx);
        return -1;
    }

    plainlen += tmplen;

    EVP_CIPHER_CTX_cleanup(&ctx);

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
        snprintf(hexStr, sizeof(hexStr), "%2.2x", (int) *sp);
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

LogLevel CryptoGetMissingKeyLogLevel(void)
{
    if (getuid() == 0 &&
        NULL == getenv("FAKEROOTKEY") &&
        NULL == getenv("CFENGINE_TEST_OVERRIDE_WORKDIR"))
    {
        return LOG_LEVEL_ERR;
    }
    else
    {
        return LOG_LEVEL_VERBOSE;
    }
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
