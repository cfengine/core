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

#include "crypto.h"

#include "cf3.defs.h"
#include "lastseen.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "hashes.h"
#include "logging.h"
#include "pipes.h"
#include "mutex.h"
#include "sysinfo.h"
#include "bootstrap.h"

static void RandomSeed(void);

static char *CFPUBKEYFILE;
static char *CFPRIVKEYFILE;

/**********************************************************************/

void CryptoInitialize()
{
    static bool crypto_initialized = false;

    if (!crypto_initialized)
    {
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

/*********************************************************************/

/**
 * @return true the error is not so severe that we must stop
 */
bool LoadSecretKeys(const char *policy_server)
{
    static char *passphrase = "Cfengine passphrase";

    {
        FILE *fp = fopen(PrivateKeyFile(GetWorkDir()), "r");
        if (!fp)
        {
            Log(LOG_LEVEL_INFO, "Couldn't find a private key at '%s', use cf-key to get one. (fopen: %s)", PrivateKeyFile(GetWorkDir()), GetErrorStr());
            return true;
        }

        if ((PRIVKEY = PEM_read_RSAPrivateKey(fp, (RSA **) NULL, NULL, passphrase)) == NULL)
        {
            unsigned long err = ERR_get_error();
            Log(LOG_LEVEL_ERR, "Error reading private key. (PEM_read_RSAPrivateKey: %s)", ERR_reason_error_string(err));
            PRIVKEY = NULL;
            fclose(fp);
            return true;
        }

        fclose(fp);
        Log(LOG_LEVEL_VERBOSE, "Loaded private key at '%s'", PrivateKeyFile(GetWorkDir()));
    }

    {
        FILE *fp = fopen(PublicKeyFile(GetWorkDir()), "r");
        if (!fp)
        {
            Log(LOG_LEVEL_ERR, "Couldn't find a public key at '%s', use cf-key to get one (fopen: %s)", PublicKeyFile(GetWorkDir()), GetErrorStr());
            return true;
        }

        if ((PUBKEY = PEM_read_RSAPublicKey(fp, NULL, NULL, passphrase)) == NULL)
        {
            unsigned long err = ERR_get_error();
            Log(LOG_LEVEL_ERR, "Error reading public key at '%s'. (PEM_read_RSAPublicKey: %s)", PublicKeyFile(GetWorkDir()), ERR_reason_error_string(err));
            PUBKEY = NULL;
            fclose(fp);
            return true;
        }

        Log(LOG_LEVEL_VERBOSE, "Loaded public key '%s'", PublicKeyFile(GetWorkDir()));
        fclose(fp);
    }

    if ((BN_num_bits(PUBKEY->e) < 2) || (!BN_is_odd(PUBKEY->e)))
    {
        Log(LOG_LEVEL_ERR, "The public key RSA exponent is too small or not odd");
        return false;
    }

    if (GetAmPolicyHub(CFWORKDIR))
    {
        unsigned char digest[EVP_MAX_MD_SIZE + 1];

        char dst_public_key_filename[CF_BUFSIZE] = "";
        {
            char buffer[EVP_MAX_MD_SIZE * 4];
            HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
            snprintf(dst_public_key_filename, CF_MAXVARSIZE, "%s/ppkeys/%s-%s.pub", CFWORKDIR, "root", HashPrintSafe(CF_DEFAULT_DIGEST, digest, buffer));
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

    return true;
}

/*********************************************************************/

RSA *HavePublicKeyByIP(const char *username, const char *ipaddress)
{
    char hash[CF_MAXVARSIZE];

    Address2Hostkey(ipaddress, hash);

    return HavePublicKey(username, ipaddress, hash);
}

/*********************************************************************/

RSA *HavePublicKey(const char *username, const char *ipaddress, const char *digest)
{
    char keyname[CF_MAXVARSIZE], newname[CF_BUFSIZE], oldname[CF_BUFSIZE];
    struct stat statbuf;
    static char *passphrase = "public";
    unsigned long err;
    FILE *fp;
    RSA *newkey = NULL;

    snprintf(keyname, CF_MAXVARSIZE, "%s-%s", username, digest);

    snprintf(newname, CF_BUFSIZE, "%s/ppkeys/%s.pub", CFWORKDIR, keyname);
    MapName(newname);

    if (stat(newname, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Did not find new key format %s", newname);
        snprintf(oldname, CF_BUFSIZE, "%s/ppkeys/%s-%s.pub", CFWORKDIR, username, ipaddress);
        MapName(oldname);

        Log(LOG_LEVEL_VERBOSE, "Trying old style %s", oldname);

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
        else                    // we don't know the digest (e.g. because we are a client and
            // have no lastseen-map and/or root-SHA...pub of the server's key
            // yet) Just using old file format (root-IP.pub) without renaming for now.
        {
            Log(LOG_LEVEL_VERBOSE, "Could not map key file to new format - we have no digest yet (using %s)",
                  oldname);
            snprintf(newname, sizeof(newname), "%s", oldname);
        }
    }

    if ((fp = fopen(newname, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't find a public key '%s'. (fopen: %s)", newname, GetErrorStr());
        return NULL;
    }

    if ((newkey = PEM_read_RSAPublicKey(fp, NULL, NULL, passphrase)) == NULL)
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
    int err;

    snprintf(keyname, CF_MAXVARSIZE, "%s-%s", user, digest);

    snprintf(filename, CF_BUFSIZE, "%s/ppkeys/%s.pub", CFWORKDIR, keyname);
    MapName(filename);

    if (stat(filename, &statbuf) != -1)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Saving public key %s", filename);

    if ((fp = fopen(filename, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to write a public key '%s'. (fopen: %s)", filename, GetErrorStr());
        return;
    }

    ThreadLock(cft_system);

    if (!PEM_write_RSAPublicKey(fp, key))
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Error saving public key to '%s'. (PEM_write_RSAPublicKey: %s)", filename, ERR_reason_error_string(err));
    }

    ThreadUnlock(cft_system);
    fclose(fp);
}

int EncryptString(char type, char *in, char *out, unsigned char *key, int plainlen)
{
    int cipherlen = 0, tmplen;
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    EVP_CIPHER_CTX ctx;

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

int DecryptString(char type, char *in, char *out, unsigned char *key, int cipherlen)
{
    int plainlen = 0, tmplen;
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    EVP_CIPHER_CTX ctx;

    EVP_CIPHER_CTX_init(&ctx);
    EVP_DecryptInit_ex(&ctx, CfengineCipher(type), NULL, key, iv);

    if (!EVP_DecryptUpdate(&ctx, out, &plainlen, in, cipherlen))
    {
        Log(LOG_LEVEL_ERR, "Decrypt FAILED");
        EVP_CIPHER_CTX_cleanup(&ctx);
        return -1;
    }

    if (!EVP_DecryptFinal_ex(&ctx, out + plainlen, &tmplen))
    {
        unsigned long err = ERR_get_error();

        Log(LOG_LEVEL_ERR, "decryption FAILED at final of %d: %s", cipherlen, ERR_error_string(err, NULL));
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
        Log(LOG_LEVEL_DEBUG, "Debug binary print is too large (len=%d)", len);
        return;
    }

    memset(buf, 0, sizeof(buf));

    for (sp = buffer; sp < (unsigned char *) (buffer + len); sp++)
    {
        snprintf(hexStr, sizeof(hexStr), "%2.2x", (int) *sp);
        strcat(buf, hexStr);
    }

    Log(LOG_LEVEL_VERBOSE, "BinaryBuffer(%d bytes => %s) -> [%s]", len, comment, buf);
}

const char *PublicKeyFile(const char *workdir)
{
    if (!CFPUBKEYFILE)
    {
        xasprintf(&CFPUBKEYFILE,
                  "%s" FILE_SEPARATOR_STR "ppkeys" FILE_SEPARATOR_STR "localhost.pub", workdir);
    }
    return CFPUBKEYFILE;
}

const char *PrivateKeyFile(const char *workdir)
{
    if (!CFPRIVKEYFILE)
    {
        xasprintf(&CFPRIVKEYFILE,
                  "%s" FILE_SEPARATOR_STR "ppkeys" FILE_SEPARATOR_STR "localhost.priv", workdir);
    }
    return CFPRIVKEYFILE;
}
