/*

   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "generic_agent.h"

#include "dbm_api.h"
#include "lastseen.h"
#include "dir.h"
#include "reporting.h"
#include "scope.h"
#include "files_copy.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "keyring.h"
#include "logging.h"
#include "communication.h"
#include "env_context.h"
#include "crypto.h"

#ifdef HAVE_NOVA
#include "license.h"
#endif

#include "cf-key-functions.h"

RSA* LoadPublicKey(const char* filename)
{
    unsigned long err;
    FILE* fp;
    RSA* key;
    static char *passphrase = "Cfengine passphrase";

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Cannot open file '%s'.\n", filename);
        return NULL;
    };

    if ((key = PEM_read_RSAPublicKey(fp, NULL, NULL, passphrase)) == NULL)
    {
        err = ERR_get_error();
        CfOut(OUTPUT_LEVEL_ERROR, "PEM_read_RSAPublicKey", "Error reading public key = %s\n", ERR_reason_error_string(err));
        fclose(fp);
        return NULL;
    };

    fclose(fp);

    if (BN_num_bits(key->e) < 2 || !BN_is_odd(key->e))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "BN_num_bits", "ERROR: RSA Exponent in key %s too small or not odd\n", filename);
        return NULL;
    };

    return key;
}

/** Return a string with the printed digest of the given key file,
    or NULL if an error occurred. */
char* GetPubkeyDigest(const char* pubkey)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    RSA* key = NULL;
    char* buffer = xmalloc(EVP_MAX_MD_SIZE * 4);

    key = LoadPublicKey(pubkey);
    if (NULL == key)
    {
        return NULL;
    }

    HashPubKey(key, digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(CF_DEFAULT_DIGEST, digest, buffer);
    return buffer;
}

/*****************************************************************************/

/** Print digest of the specified public key file.
    Return 0 on success and 1 on error. */
int PrintDigest(const char* pubkey)
{
    char *digeststr = GetPubkeyDigest(pubkey);

    if (NULL == digeststr)
    {
        return 1; /* ERROR exitcode */
    }

    fprintf(stdout, "%s\n", digeststr);
    free(digeststr);
    return 0; /* OK exitcode */
}

int TrustKey(const char* pubkey)
{
    char *digeststr = GetPubkeyDigest(pubkey);
    char outfilename[CF_BUFSIZE];
    bool ok;

    if (NULL == digeststr)
        return 1; /* ERROR exitcode */

    snprintf(outfilename, CF_BUFSIZE, "%s/ppkeys/root-%s.pub", CFWORKDIR, digeststr);
    free(digeststr);

    ok = CopyRegularFileDisk(pubkey, outfilename);

    return (ok? 0 : 1);
}

bool ShowHost(const char *hostkey, const char *address, bool incoming,
                     const KeyHostSeen *quality, void *ctx)
{
    int *count = ctx;
    char timebuf[26];

    char hostname[CF_BUFSIZE];
    strlcpy(hostname, IPString2Hostname(address), CF_BUFSIZE);

    (*count)++;
    printf("%-10.10s %-17.17s %-25.25s %-26.26s %-s\n", incoming ? "Incoming" : "Outgoing",
           address, hostname, cf_strtimestamp_local(quality->lastseen, timebuf), hostkey);

    return true;
}

void ShowLastSeenHosts()
{
    int count = 0;

    printf("%-10.10s %-17.17s %-25.25s %-26.26s %-s\n", "Direction", "IP", "Name", "Last connection", "Key");

    if (!ScanLastSeenQuality(ShowHost, &count))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to show lastseen database");
        return;
    }

    printf("Total Entries: %d\n", count);
}


int RemoveKeys(const char *host)
{
    char ip[CF_BUFSIZE];
    char digest[CF_BUFSIZE];

    strcpy(ip, Hostname2IPString(host));
    Address2Hostkey(ip, digest);

    RemoveHostFromLastSeen(digest);

    int removed_by_ip = RemovePublicKey(ip);
    int removed_by_digest = RemovePublicKey(digest);

    if ((removed_by_ip == -1) || (removed_by_digest == -1))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to remove keys for the host %s", host);
        return 255;
    }
    else if (removed_by_ip + removed_by_digest == 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "No keys for host %s were found", host);
        return 1;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Removed %d key(s) for host %s",
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

    if (cfstat(public_key_file, &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_CMDOUT, "", "A key file already exists at %s\n", public_key_file);
        return;
    }

    if (cfstat(private_key_file, &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_CMDOUT, "", "A key file already exists at %s\n", private_key_file);
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
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to generate key: %s\n", ERR_reason_error_string(err));
        return;
    }

    if (DEBUG)
    {
        RSA_print_fp(stdout, pair, 0);
    }

    fd = open(private_key_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "open", "Open %s failed: %s.", private_key_file, strerror(errno));
        return;
    }

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fdopen", "Couldn't open private key %s.", private_key_file);
        close(fd);
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Writing private key to %s\n", private_key_file);

    if (!PEM_write_RSAPrivateKey(fp, pair, cipher, passphrase, strlen(passphrase), NULL, NULL))
    {
        err = ERR_get_error();
        CfOut(OUTPUT_LEVEL_ERROR, "", "Couldn't write private key: %s\n", ERR_reason_error_string(err));
        return;
    }

    fclose(fp);

    fd = open(public_key_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "open", "Unable to open public key %s.", public_key_file);
        return;
    }

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fdopen", "Open %s failed.", public_key_file);
        close(fd);
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Writing public key to %s\n", public_key_file);

    if (!PEM_write_RSAPublicKey(fp, pair))
    {
        err = ERR_get_error();
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to write public key: %s\n", ERR_reason_error_string(err));
        return;
    }

    fclose(fp);

    snprintf(vbuff, CF_BUFSIZE, "%s/randseed", CFWORKDIR);
    RAND_write_file(vbuff);
    cf_chmod(vbuff, 0644);
}


#ifndef HAVE_NOVA
bool LicenseInstall(ARG_UNUSED char *path_source)
{
    CfOut(OUTPUT_LEVEL_ERROR, "", "!! License installation only applies to CFEngine Enterprise");

    return false;
}
#endif  /* HAVE_NOVA */

