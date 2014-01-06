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
#include <assert.h>

#include "generic_agent.h"

#include "dbm_api.h"
#include "lastseen.h"
#include "dir.h"
#include "scope.h"
#include "files_copy.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "keyring.h"
#include "communication.h"
#include "env_context.h"
#include "crypto.h"
#include "file_lib.h"
#ifndef __MINGW32__
# include "sysinfo.h"
#endif

#include "cf-key-functions.h"

#ifdef HAVE_NOVA
#include "license.h"
#endif

#ifdef HAVE_NOVA
static bool LicensePublicKeyPath(char path_public_key[MAX_FILENAME], char *path_license);
#endif

RSA* LoadPublicKey(const char* filename)
{
    unsigned long err;
    FILE* fp;
    RSA* key;
    static char *passphrase = "Cfengine passphrase";

    fp = safe_fopen(filename, "r");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot open file '%s'. (fopen: %s)", filename, GetErrorStr());
        return NULL;
    };

    if ((key = PEM_read_RSAPublicKey(fp, NULL, NULL, passphrase)) == NULL)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Error reading public key. (PEM_read_RSAPublicKey: %s)",
            ERR_reason_error_string(err));
        fclose(fp);
        return NULL;
    };

    fclose(fp);

    if (BN_num_bits(key->e) < 2 || !BN_is_odd(key->e))
    {
        Log(LOG_LEVEL_ERR, "RSA Exponent in key '%s' too small or not odd. (BN_num_bits: %s)",
            filename, GetErrorStr());
        return NULL;
    };

    return key;
}

/** Return a string with the printed digest of the given key file,
    or NULL if an error occurred. */
char* LoadPubkeyDigest(const char* filename)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    RSA* key = NULL;
    char* buffer = xmalloc(EVP_MAX_MD_SIZE * 4);

    key = LoadPublicKey(filename);
    if (NULL == key)
    {
        return NULL;
    }

    HashPubKey(key, digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(CF_DEFAULT_DIGEST, digest, buffer);
    return buffer;
}

/** Return a string with the printed digest of the given key file. */
char* GetPubkeyDigest(RSA* pubkey)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    char* buffer = xmalloc(EVP_MAX_MD_SIZE * 4);

    assert(NULL != pubkey);

    HashPubKey(pubkey, digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(CF_DEFAULT_DIGEST, digest, buffer);
    return buffer;
}

/*****************************************************************************/

/** Print digest of the specified public key file.
    Return 0 on success and 1 on error. */
int PrintDigest(const char* pubkey)
{
    char *digeststr = LoadPubkeyDigest(pubkey);

    if (NULL == digeststr)
    {
        return 1; /* ERROR exitcode */
    }

    fprintf(stdout, "%s\n", digeststr);
    free(digeststr);
    return 0; /* OK exitcode */
}

/** Split a "key" argument of the form "user@address:filename" into
 * components (public) key file name, IP address, and (remote) user
 * name.  Pointers to the corresponding segments of the @c keyarg
 * string will be written into the three output arguments @c filename,
 * @c ipaddr, and @c username. (Hence, the three output string have
 * the same lifetime/scope as the @c keyarg string.)
 *
 * The only required component is the file name.  If IP address is
 * missing, @c NULL is written into the @c ipaddr pointer.  If the
 * username is missing, @c username will point to the constant string
 * @c "root".
 *
 * NOTE: the @c keyarg argument is modified by this function!
 */
void ParseKeyArg(char* keyarg, char** filename, char** ipaddr, char** username)
{
    char *s;

    /* set defaults */
    *ipaddr = NULL;
    *username = "root";

    /* use rightmost colon so we can cope with IPv6 addresses */
    s = strrchr(keyarg, ':');
    if (NULL == s)
    {
        /* no colon, entire argument is a filename */
        *filename = keyarg;
        return;
    }

    *s = '\0'; /* split string */
    *filename = s+1; /* `filename` starts at 1st character after ':' */

    s = strchr(keyarg, '@');
    if (NULL == s)
    {
        /* no username given, use default */
        *ipaddr = keyarg;
        return;
    }

    *s = '\0';
    *ipaddr = s+1;
    *username = keyarg;

    /* special case: if we got user@:/path/to/file
       then reset `ipaddr` to NULL instead of empty string */
    if ('\0' == **ipaddr)
    {
        *ipaddr = NULL;
    }

    return;
}

/** Trust the given key.  If @c ipaddress is not @c NULL, then also
 * update the "last seen" database.  The IP address is required for
 * trusting a server key (on the client); it is -currently- optional
 * for trusting a client key (on the server). */
int TrustKey(const char* filename, const char* ipaddress, const char* username)
{
    RSA* key;
    char *digest;

    key = LoadPublicKey(filename);
    if (NULL == key)
    {
        return 1; /* ERROR exitcode */
    }

    digest = GetPubkeyDigest(key);
    if (NULL == digest)
    {
        return 1; /* ERROR exitcode */
    }

    if (NULL != ipaddress)
    {
        LastSaw(ipaddress, digest, LAST_SEEN_ROLE_CONNECT);
    }
    SavePublicKey(username, digest, key);

    free(digest);
    return 0; /* OK exitcode */
}

bool ShowHost(const char *hostkey, const char *address, bool incoming,
                     const KeyHostSeen *quality, void *ctx)
{
    int *count = ctx;
    char timebuf[26];

    char hostname[MAXHOSTNAMELEN];
    int ret = IPString2Hostname(hostname, address, sizeof(hostname));

    (*count)++;
    printf("%-10.10s %-17.17s %-25.25s %-26.26s %-s\n",
           incoming ? "Incoming" : "Outgoing",
           address, (ret != -1) ? hostname : "-",
           cf_strtimestamp_local(quality->lastseen, timebuf), hostkey);

    return true;
}

void ShowLastSeenHosts()
{
    int count = 0;

    printf("%-10.10s %-17.17s %-25.25s %-26.26s %-s\n", "Direction", "IP", "Name", "Last connection", "Key");

    if (!ScanLastSeenQuality(ShowHost, &count))
    {
        Log(LOG_LEVEL_ERR, "Unable to show lastseen database");
        return;
    }

    printf("Total Entries: %d\n", count);
}


int RemoveKeys(const char *host)
{
    char digest[CF_BUFSIZE];
    char ipaddr[CF_MAX_IP_LEN];

    if (Hostname2IPString(ipaddr, host, sizeof(ipaddr)) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "ERROR, could not resolve '%s', not removing", host);
        return 255;
    }

    Address2Hostkey(ipaddr, digest);
    RemoveHostFromLastSeen(digest);

    int removed_by_ip = RemovePublicKey(ipaddr);
    int removed_by_digest = RemovePublicKey(digest);

    if ((removed_by_ip == -1) || (removed_by_digest == -1))
    {
        Log(LOG_LEVEL_ERR, "Unable to remove keys for the host '%s'", host);
        return 255;
    }
    else if (removed_by_ip + removed_by_digest == 0)
    {
        Log(LOG_LEVEL_ERR, "No keys for host '%s' were found", host);
        return 1;
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Removed %d key(s) for host '%s'",
              removed_by_ip + removed_by_digest, host);
        return 0;
    }
}


void KeepKeyPromises(const char *public_key_file, const char *private_key_file)
{
    unsigned long err;
#ifdef OPENSSL_NO_DEPRECATED
    RSA *pair = RSA_new();
    BIGNUM *rsa_bignum = BN_new();
#else
    RSA *pair;
#endif
    FILE *fp;
    struct stat statbuf;
    int fd;
    static char *passphrase = "Cfengine passphrase";
    const EVP_CIPHER *cipher;
    char vbuff[CF_BUFSIZE];

    cipher = EVP_des_ede3_cbc();

    if (stat(public_key_file, &statbuf) != -1)
    {
        printf("A key file already exists at %s\n", public_key_file);
        return;
    }

    if (stat(private_key_file, &statbuf) != -1)
    {
        printf("A key file already exists at %s\n", private_key_file);
        return;
    }

    printf("Making a key pair for cfengine, please wait, this could take a minute...\n");

#ifdef OPENSSL_NO_DEPRECATED
    BN_set_word(rsa_bignum, 35);

    if (!RSA_generate_key_ex(pair, 2048, rsa_bignum, NULL))
#else
    pair = RSA_generate_key(2048, 35, NULL, NULL);

    if (pair == NULL)
#endif
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Unable to generate key '%s'", ERR_reason_error_string(err));
        return;
    }

    fd = safe_open(private_key_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        Log(LOG_LEVEL_ERR, "Open '%s' failed. (open: %s)", private_key_file, GetErrorStr());
        return;
    }

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open private key '%s'. (fdopen: %s)", private_key_file, GetErrorStr());
        close(fd);
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Writing private key to '%s'", private_key_file);

    if (!PEM_write_RSAPrivateKey(fp, pair, cipher, passphrase, strlen(passphrase), NULL, NULL))
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Couldn't write private key. (PEM_write_RSAPrivateKey: %s)", ERR_reason_error_string(err));
        return;
    }

    fclose(fp);

    fd = safe_open(public_key_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        Log(LOG_LEVEL_ERR, "Unable to open public key '%s'. (open: %s)",
            public_key_file, GetErrorStr());
        return;
    }

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Open '%s' failed. (fdopen: %s)", public_key_file, GetErrorStr());
        close(fd);
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Writing public key to file '%s'", public_key_file);

    if (!PEM_write_RSAPublicKey(fp, pair))
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Unable to write public key. (PEM_write_RSAPublicKey: %s)", ERR_reason_error_string(err));
        return;
    }

    fclose(fp);

    snprintf(vbuff, CF_BUFSIZE, "%s/randseed", CFWORKDIR);
    RAND_write_file(vbuff);
    chmod(vbuff, 0644);
}


#ifndef HAVE_NOVA
bool LicenseInstall(ARG_UNUSED char *path_source)
{
    Log(LOG_LEVEL_ERR, "License installation only applies to CFEngine Enterprise");

    return false;
}

#else  /* HAVE_NOVA */
bool LicenseInstall(char *path_source)
{
    struct stat sb;

    if(stat(path_source, &sb) == -1)
    {
        Log(LOG_LEVEL_ERR, "Can not stat input license file '%s'. (stat: %s)", path_source, GetErrorStr());
        return false;
    }

    char path_destination[MAX_FILENAME];
    snprintf(path_destination, sizeof(path_destination), "%s/inputs/license.dat", CFWORKDIR);
    MapName(path_destination);

    if(stat(path_destination, &sb) == 0)
    {
        Log(LOG_LEVEL_ERR, "A license file is already installed in '%s' -- please move it out of the way and try again", path_destination);
        return false;
    }

    char path_public_key[MAX_FILENAME];

    if(!LicensePublicKeyPath(path_public_key, path_source))
    {
        Log(LOG_LEVEL_ERR, "Could not find path to public key -- license parse error?");
    }

    if(stat(path_public_key, &sb) != 0)
    {
        Log(LOG_LEVEL_ERR, "The licensed public key is not installed -- please copy it to '%s' and try again", path_public_key);
        return false;
    }


    bool success = CopyRegularFileDisk(path_source, path_destination);

    if(success)
    {
        Log(LOG_LEVEL_INFO, "Installed license at '%s'", path_destination);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed copying license from '%s' to '%s'", path_source, path_destination);
    }

    return success;
}

static bool LicensePublicKeyPath(char path_public_key[MAX_FILENAME], char *path_license)
{
    EnterpriseLicense license;

    if(!LicenseFileParse(&license, path_license))
    {
        return false;
    }

    snprintf(path_public_key, MAX_FILENAME, "%s/ppkeys/root-SHA=%s.pub", CFWORKDIR, license.public_key_digest);
    MapName(path_public_key);

    return true;
}
#endif  /* HAVE_NOVA */
