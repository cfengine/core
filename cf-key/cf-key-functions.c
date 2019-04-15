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
#include <cf-key-functions.h>

#include <openssl/bn.h>                                     /* BN_*, BIGNUM */
#include <openssl/rand.h>                                   /* RAND_* */
#include <libcrypto-compat.h>

#include <lastseen.h>
#include <dir.h>
#include <scope.h>
#include <files_copy.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <keyring.h>
#include <communication.h>
#include <eval_context.h>
#include <crypto.h>
#include <file_lib.h>
#include <known_dirs.h>


/*****************************************************************************/

/** Print digest of the specified public key file.
    Return 0 on success and 1 on error. */
int PrintDigest(const char *pubkey)
{
    char *digeststr = LoadPubkeyDigest(pubkey);

    if (digeststr == NULL)
    {
        return 1; /* ERROR exitcode */
    }

    fprintf(stdout, "%s\n", digeststr);
    free(digeststr);
    return 0; /* OK exitcode */
}

/**
 * Split a "key" argument of the form "[[user@]address:]filename" into
 * components (public) key file name, IP address, and (remote) user
 * name.  Pointers to the corresponding segments of the #keyarg
 * string will be written into the three output arguments #filename,
 * #ipaddr, and #username. (Hence, the three output string have
 * the same lifetime/scope as the #keyarg string.)
 *
 * The only required component is the file name.  If IP address is
 * missing, NULL is written into the #ipaddr pointer.  If the
 * username is missing, #username will point to the constant string
 * "root".
 *
 * @NOTE the #keyarg argument is modified by this function!
 */
void ParseKeyArg(char *keyarg, char **filename, char **ipaddr, char **username)
{
    char *s;

    /* set defaults */
    *ipaddr = NULL;
    *username = "root";

    /* use rightmost colon so we can cope with IPv6 addresses */
    s = strrchr(keyarg, ':');
    if (s == NULL)
    {
        /* no colon, entire argument is a filename */
        *filename = keyarg;
        return;
    }

    *s = '\0';              /* split string */
    *filename = s + 1;      /* filename starts at 1st character after ':' */

    s = strchr(keyarg, '@');
    if (s == NULL)
    {
        /* no username given, use default */
        *ipaddr = keyarg;
        return;
    }

    *s = '\0';
    *ipaddr = s + 1;
    *username = keyarg;

    /* special case: if we got user@:/path/to/file
       then reset ipaddr to NULL instead of empty string */
    if (**ipaddr == '\0')
    {
        *ipaddr = NULL;
    }

    return;
}

extern bool cf_key_interrupted;

#define HOST_FMT_TRUNCATE "%-10.10s %-40.40s %-25.25s %-26.26s %-s\n"
#define HOST_FMT_NO_TRUNCATE "%s\t%s\t%s\t%s\t%s\n"

typedef struct _HostPrintState
{
    int count;
    bool truncate;
} HostPrintState;

static bool ShowHost(
    const char *const hostkey,
    const char *const address,
    bool incoming,
    const KeyHostSeen *const quality,
    void *const ctx)
{
    HostPrintState *const state = ctx;
    char hostname[NI_MAXHOST];
    if (LOOKUP_HOSTS)
    {
        int ret = IPString2Hostname(hostname, address, sizeof(hostname));
        if (ret == -1)
        {
            strcpy(hostname, "-");
        }
    }
    else
    {
        strlcpy(hostname, address, sizeof(hostname));
    }
    ++(state->count);

    bool truncate = state->truncate;
    char timebuf[26];
    printf(truncate ? HOST_FMT_TRUNCATE : HOST_FMT_NO_TRUNCATE,
           incoming ? "Incoming" : "Outgoing",
           address, hostname,
           cf_strtimestamp_local(quality->lastseen, timebuf), hostkey);

    return !cf_key_interrupted;
}

void ShowLastSeenHosts(bool truncate)
{
    HostPrintState state = { 0 };
    state.count = 0;
    state.truncate = truncate;

    printf(
        truncate ? HOST_FMT_TRUNCATE : HOST_FMT_NO_TRUNCATE,
        "Direction",
        "IP",
        "Name",
        "Last connection",
        "Key");

    if (!ScanLastSeenQuality(ShowHost, &state))
    {
        Log(LOG_LEVEL_ERR, "Unable to show lastseen database");
        return;
    }

    printf("Total Entries: %d\n", state.count);
}

/**
 * @brief removes all traces of entry 'input' from lastseen and filesystem
 *
 * @param[in] key digest (SHA/MD5 format) or free host name string
 * @param[in] must_be_coherent. false : delete if lastseen is incoherent,
 *                              true :  don't if lastseen is incoherent
 * @retval 0 if entry was deleted, >0 otherwise
 */
int RemoveKeys(const char *input, bool must_be_coherent)
{
    int res = 0;
    char equivalent[CF_BUFSIZE];
    equivalent[0] = '\0';

    res = RemoveKeysFromLastSeen(input, must_be_coherent, equivalent, sizeof(equivalent));
    if (res!=0)
    {
        return res;
    }

    Log(LOG_LEVEL_INFO, "Removed corresponding entries from lastseen database.");

    int removed_input      = RemovePublicKey(input);
    int removed_equivalent = RemovePublicKey(equivalent);

    if ((removed_input == -1) || (removed_equivalent == -1))
    {
        Log(LOG_LEVEL_ERR, "Last seen database: unable to remove keys for the entry '%s'", input);
        return 255;
    }
    else if (removed_input + removed_equivalent == 0)
    {
        Log(LOG_LEVEL_ERR, "No key file(s) for entry '%s' were found on the filesystem", input);
        return 1;
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Removed %d corresponding key file(s) from filesystem.",
              removed_input + removed_equivalent);
        return 0;
    }

    return -1;
}

static bool KeepKeyPromisesRSA(RSA *pair, const char *public_key_file, const char *private_key_file)
{
    int fd = safe_open(private_key_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open private key file '%s' (open: %s)", private_key_file, GetErrorStr());
        return false;
    }

    FILE *fp = fdopen(fd, "w");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Error while writing private key file '%s' (fdopen: %s)", private_key_file, GetErrorStr());
        close(fd);
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Writing private key to '%s'", private_key_file);

    const EVP_CIPHER *cipher = EVP_des_ede3_cbc();
    int res = PEM_write_RSAPrivateKey(fp, pair, cipher, (void *)PRIVKEY_PASSPHRASE,
                                 PRIVKEY_PASSPHRASE_LEN, NULL, NULL);
    fclose(fp);

    if (res == 0)
    {
        Log(LOG_LEVEL_ERR,
            "Couldn't write private key. (PEM_write_RSAPrivateKey: %s)",
            CryptoLastErrorString());
        return false;
    }

    fd = safe_open(public_key_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open public key file '%s' (open: %s)",
            public_key_file, GetErrorStr());
        return false;
    }

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Error while writing public key file '%s' (fdopen: %s)", public_key_file, GetErrorStr());
        close(fd);
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Writing public key to file '%s'", public_key_file);

    if (!PEM_write_RSAPublicKey(fp, pair))
    {
        Log(LOG_LEVEL_ERR,
            "Unable to write public key. (PEM_write_RSAPublicKey: %s)",
            CryptoLastErrorString());
        return false;
    }

    fclose(fp);

    char vbuff[CF_BUFSIZE];
    snprintf(vbuff, CF_BUFSIZE, "%s%crandseed", GetWorkDir(), FILE_SEPARATOR);
    Log(LOG_LEVEL_VERBOSE, "Using '%s' for randseed", vbuff);

    if (RAND_write_file(vbuff) != 1024)
    {
        Log(LOG_LEVEL_ERR, "Unable to write randseed");
        unlink(vbuff); /* randseed isn't safe to use */
        return false;
    }

    if (chmod(vbuff, 0600) != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Unable to set permissions on '%s' (chmod: %s)",
            vbuff, GetErrorStr());
        return false;
    }

    return true;
}

bool KeepKeyPromises(const char *public_key_file, const char *private_key_file, const int key_size)
{
    struct stat statbuf;

    if (stat(public_key_file, &statbuf) != -1)
    {
        Log(LOG_LEVEL_ERR, "A key file already exists at %s", public_key_file);
        return false;
    }

    if (stat(private_key_file, &statbuf) != -1)
    {
        Log(LOG_LEVEL_ERR, "A key file already exists at %s", private_key_file);
        return false;
    }

    Log(LOG_LEVEL_INFO, "Making a key pair for CFEngine, please wait, this could take a minute...");

#ifdef OPENSSL_NO_DEPRECATED
    RSA *pair = RSA_new();
    BIGNUM *rsa_bignum = BN_new();
    if (pair != NULL && rsa_bignum != NULL)
    {
        BN_set_word(rsa_bignum, RSA_F4);
        int res = RSA_generate_key_ex(pair, key_size, rsa_bignum, NULL);
        if (res == 0)
        {
            DESTROY_AND_NULL(RSA_free, pair); // pair = NULL
        }
    }
    else
    {
        DESTROY_AND_NULL(RSA_free, pair); // pair = NULL
    }

    BN_clear_free(rsa_bignum);

#else
    RSA *pair = RSA_generate_key(key_size, 65537, NULL, NULL);

#endif
    if (pair == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to generate cryptographic key (RSA_generate_key: %s)",
            CryptoLastErrorString());
        return false;
    }
    bool ret = KeepKeyPromisesRSA(pair, public_key_file, private_key_file);
    RSA_free(pair);
    return ret;
}


ENTERPRISE_FUNC_1ARG_DEFINE_STUB(bool, LicenseInstall, ARG_UNUSED char *, path_source)
{
    Log(LOG_LEVEL_ERR, "License installation only applies to CFEngine Enterprise");

    return false;
}

int ForceKeyRemoval(const char *hash)
{
/**
    Removal of a key hash is made of two passes :
    Pass #1 (read-only)
      -> fetches the IP addresses directly linked to the hash key
    Pass #2 (made of deletes)
      -> remove all the IP addresses in the previous list
      -> remove the physical key.pub from the filesystem

    WARNING: Please backup your lastseen database before calling this
             function in the case where a 1-to-1 relatioship between
             the IP and a single keyhash does not exist
**/
    CF_DB *dbp;
    CF_DBC *dbcp;
    char *key;
    void *value;
    int ksize, vsize;

    Seq *hostips = SeqNew(100, free);
    if (OpenDB(&dbp, dbid_lastseen))
    {
        if (NewDBCursor(dbp, &dbcp))
        {
            while (NextDB(dbcp, &key, &ksize, &value, &vsize))
            {
                if ((key[0] != 'a') || (value == NULL))
                {
                    continue;
                }
                if (!strncmp(hash, value, strlen(hash)))
                {
                    SeqAppend(hostips, xstrdup(key + 1));
                }
            }
            DeleteDBCursor(dbcp);
        }
        CloseDB(dbp);
    }
    if (OpenDB(&dbp, dbid_lastseen))
    {
        char tmp[CF_BUFSIZE];
        snprintf(tmp, CF_BUFSIZE, "k%s", hash);
        char vtmp[CF_BUFSIZE];
        if (!ReadDB(dbp, tmp, &vtmp, sizeof(vtmp)))
        {
            Log(LOG_LEVEL_ERR, "Failed to read the main hash key entry '%s'. Will continue to purge other entries related to it.", hash);
        }
        else
        {
            SeqAppend(hostips, xstrdup(vtmp + 1));
        }
        snprintf(tmp, CF_BUFSIZE, "k%s", hash);
        DeleteDB(dbp, tmp);
        snprintf(tmp, CF_BUFSIZE, "qi%s", hash);
        DeleteDB(dbp, tmp);
        snprintf(tmp, CF_BUFSIZE, "qo%s", hash);
        DeleteDB(dbp, tmp);
        RemovePublicKey(hash);
        for (int i = 0; i < SeqLength(hostips); ++i)
        {
            const char *myip = SeqAt(hostips, i);
            snprintf(tmp, CF_BUFSIZE, "a%s", myip);
            DeleteDB(dbp, tmp);
        }
        CloseDB(dbp);
    }
    SeqDestroy(hostips);
    return 0;
}

int ForceIpAddressRemoval(const char *ip)
{
/**
    Removal of an ip is made of two passes :
    Pass #1 (read-only)
      -> fetches the key hashes directly linked to the ip address
    Pass #2 (made of deletes)
      -> remove all the key hashes in the previous list
      -> remove the physical key hashes .pub files from the filesystem

    WARNING: Please backup your lastseen database before calling this
             function in the case where a 1-to-1 relatioship between
             the IP and a single keyhash does not exist
**/
    CF_DB *dbp;
    CF_DBC *dbcp;
    char *key;
    void *value;
    int ksize, vsize;

    Seq *hostkeys = SeqNew(100, free);
    if (OpenDB(&dbp, dbid_lastseen))
    {
        if (NewDBCursor(dbp, &dbcp))
        {
            while (NextDB(dbcp, &key, &ksize, &value, &vsize))
            {
                if ((key[0] != 'k') || (value == NULL))
                {
                    continue;
                }
                if (!strncmp(ip, value, strlen(ip)))
                {
                    SeqAppend(hostkeys, xstrdup(key + 1));
                }
            }
            DeleteDBCursor(dbcp);
        }
        CloseDB(dbp);
    }
    if (OpenDB(&dbp, dbid_lastseen))
    {
        char tmp[CF_BUFSIZE];
        snprintf(tmp, CF_BUFSIZE, "a%s", ip);
        char vtmp[CF_BUFSIZE];
        if (!ReadDB(dbp, tmp, &vtmp, sizeof(vtmp)))
        {
            Log(LOG_LEVEL_ERR, "Failed to read the main ip address entry '%s'. Will continue to purge other entries related to it.", ip);
        }
        else
        {
            SeqAppend(hostkeys, xstrdup(vtmp + 1));
        }
        snprintf(tmp, CF_BUFSIZE, "a%s", ip);
        DeleteDB(dbp, tmp);
        for (int i = 0; i < SeqLength(hostkeys); ++i)
        {
            const char *myk = SeqAt(hostkeys, i);
            snprintf(tmp, CF_BUFSIZE, "k%s", myk);
            DeleteDB(dbp, tmp);
            snprintf(tmp, CF_BUFSIZE, "qi%s", myk);
            DeleteDB(dbp, tmp);
            snprintf(tmp, CF_BUFSIZE, "qo%s", myk);
            DeleteDB(dbp, tmp);
            RemovePublicKey(myk);
        }
        CloseDB(dbp);
    }
    SeqDestroy(hostkeys);
    return 0;
}
