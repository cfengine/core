#include <platform.h>
#include <alloc.h>
#include <logging.h>
#include <known_dirs.h>
#include <file_lib.h>

#include <openssl/evp.h>   /* OpenSSL_add_all_* */
#include <openssl/rand.h>  /* RAND_* */
#include <openssl/err.h>   /* ERR_* */

#include <crypto_init.h>

static bool crypto_initialized = false; /* GLOBAL_X */

/*
  Edition-time constant (MD5 for community, something else for Enterprise)

  Used as a default hash everywhere (not only in network protocol)
*/
HashMethod CF_DEFAULT_DIGEST; /* GLOBAL_C, initialized later */
int CF_DEFAULT_DIGEST_LEN;    /* GLOBAL_C, initialized later */

#if OPENSSL_VERSION_NUMBER < 0x10100000
/* The deprecated is the easy way to setup threads for OpenSSL. */
#ifdef OPENSSL_NO_DEPRECATED
void CRYPTO_set_id_callback(unsigned long (*func)(void));
#endif
#endif

/*********************************************************************
 * Functions for threadsafe OpenSSL usage                            *
 * Only pthread support - we don't create threads with any other API *
 *********************************************************************/

static pthread_mutex_t *cf_openssl_locks = NULL;

#ifndef __MINGW32__
#if OPENSSL_VERSION_NUMBER < 0x10100000
unsigned long ThreadId_callback(void)
{
    return (unsigned long) pthread_self();
}
#endif
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000

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

#endif // Callback only for openssl < 1.1.0

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
    // This is a no-op macro for OpenSSL >= 1.1.0
    // The callback function is not used (or defined) then
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

static void RandomSeed(time_t start_time, const char *host)
{
    /* 1. Seed the weak C PRNGs. */

    /* Mix various stuff. */
    pid_t pid = getpid();
    size_t host_len = strlen(host) > 0 ? strlen(host) : 1;
    time_t now = time(NULL);

    srand((unsigned) pid      * start_time ^
          (unsigned) host_len * now);
    srand48((long)  pid      * start_time ^
            (long)  host_len * now);

    /* 2. Seed the strong OpenSSL PRNG. */

#ifndef __MINGW32__
    RAND_poll();                                        /* windows may hang */
#else
    RAND_screen();                       /* noop unless openssl is very old */
#endif

    if (RAND_status() != 1)
    {
        /* randseed file is written on deinitialization of crypto system */
        char randfile[PATH_MAX];
        snprintf(randfile, PATH_MAX, "%s%crandseed",
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

void CryptoInitialize(time_t start_time, const char *host)
{
    if (!crypto_initialized)
    {
        SetupOpenSSLThreadLocks();
        OpenSSL_add_all_algorithms();
        OpenSSL_add_all_digests();
        ERR_load_crypto_strings();

        RandomSeed(start_time, host);

        crypto_initialized = true;
    }
}

void CryptoDeInitialize()
{
    if (crypto_initialized)
    {
        char randfile[PATH_MAX];
        snprintf(randfile, PATH_MAX, "%s%crandseed",
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

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, GenericAgentSetDefaultDigest, HashMethod *, digest, int *, digest_len)
{
    *digest = HASH_METHOD_MD5;
    *digest_len = CF_MD5_LEN;
}

ENTERPRISE_FUNC_1ARG_DEFINE_STUB(const EVP_CIPHER *, CfengineCipher, ARG_UNUSED char, type)
{
    return EVP_bf_cbc();
}

