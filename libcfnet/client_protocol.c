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

#include <client_protocol.h>

#include <communication.h>
#include <net.h>

/* libutils */
#include <logging.h>                                            /* Log */

/* TODO remove all includes from libpromises. */
#include <unix.h>                           /* GetCurrentUsername */
#include <lastseen.h>                          /* LastSaw */
#include <crypto.h>                            /* PublicKeyFile */
#include <files_hashes.h> /* HashString,HashesMatch,HashPubKey,HashPrintSafe */
#include <known_dirs.h>
#include <hash.h>
#include <connection_info.h>

int IdentifyAgent(ConnectionInfo *conn_info)
{
    char uname[CF_BUFSIZE], sendbuff[CF_BUFSIZE];

/* client always identifies as root on windows */
#ifdef __MINGW32__
    strlcpy(uname, sizeof(uname), "root");
#else
    GetCurrentUserName(uname, sizeof(uname));
#endif

    snprintf(sendbuff, sizeof(sendbuff), "CAUTH 127.0.0.1 localhost %s 0", uname);

    if (SendTransaction(conn_info, sendbuff, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR,
              "During identify agent, could not send auth response. (SendTransaction: %s)", GetErrorStr());
        return false;
    }

    return true;
}

/*********************************************************************/

static bool SetSessionKey(AgentConnection *conn)
{
    BIGNUM *bp;
    int session_size = CfSessionKeySize(conn->encryption_type);

    bp = BN_new();

    if (bp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not allocate session key");
        return false;
    }

    // session_size is in bytes
    if (!BN_rand(bp, session_size * 8, -1, 0))
    {
        Log(LOG_LEVEL_ERR, "Can't generate cryptographic key");
        BN_clear_free(bp);
        return false;
    }

    conn->session_key = (unsigned char *) bp->d;
    return true;
}

int AuthenticateAgent(AgentConnection *conn, bool trust_key)
{
    char sendbuffer[CF_EXPANDSIZE], in[CF_BUFSIZE], *out, *decrypted_cchall;
    BIGNUM *nonce_challenge, *bn = NULL;
    unsigned long err;
    unsigned char digest[EVP_MAX_MD_SIZE];
    int encrypted_len, nonce_len = 0, len, session_size;
    bool implicitly_trust_server;
    char enterprise_field = 'c';
    RSA *server_pubkey = NULL;

    if ((PUBKEY == NULL) || (PRIVKEY == NULL))
    {
        /* Try once more to load the keys, maybe the system is converging. */
        LoadSecretKeys();
        if ((PUBKEY == NULL) || (PRIVKEY == NULL))
        {
            char *pubkeyfile = PublicKeyFile(GetWorkDir());
            Log(LOG_LEVEL_ERR, "No public/private key pair found at '%s'", pubkeyfile);
            free(pubkeyfile);
            return false;
        }
    }

    enterprise_field = CfEnterpriseOptions();
    session_size = CfSessionKeySize(enterprise_field);

/* Generate a random challenge to authenticate the server */

    nonce_challenge = BN_new();
    if (nonce_challenge == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot allocate BIGNUM structure for server challenge");
        return false;
    }

    BN_rand(nonce_challenge, CF_NONCELEN, 0, 0);
    nonce_len = BN_bn2mpi(nonce_challenge, in);

    if (FIPS_MODE)
    {
        HashString(in, nonce_len, digest, CF_DEFAULT_DIGEST);
    }
    else
    {
        HashString(in, nonce_len, digest, HASH_METHOD_MD5);
    }

/* We assume that the server bound to the remote socket is the official one i.e. = root's */

    /* Ask the server to send us the public key if we don't have it. */
    if ((server_pubkey = HavePublicKeyByIP(conn->username, conn->remoteip)))
    {
        implicitly_trust_server = false;
        encrypted_len = RSA_size(server_pubkey);
    }
    else
    {
        implicitly_trust_server = true;
        encrypted_len = nonce_len;
    }

// Server pubkey is what we want to has as a unique ID

    snprintf(sendbuffer, sizeof(sendbuffer), "SAUTH %c %d %d %c",
             implicitly_trust_server ? 'n': 'y',
             encrypted_len, nonce_len, enterprise_field);

    out = xmalloc(encrypted_len);

    if (server_pubkey != NULL)
    {
        if (RSA_public_encrypt(nonce_len, in, out, server_pubkey, RSA_PKCS1_PADDING) <= 0)
        {
            err = ERR_get_error();
            Log(LOG_LEVEL_ERR, "Public encryption failed. (RSA_public_encrypt: %s)", ERR_reason_error_string(err));
            free(out);
            RSA_free(server_pubkey);
            return false;
        }

        memcpy(sendbuffer + CF_RSA_PROTO_OFFSET, out, encrypted_len);
    }
    else
    {
        memcpy(sendbuffer + CF_RSA_PROTO_OFFSET, in, nonce_len);
    }

/* proposition C1 - Send challenge / nonce */

    SendTransaction(conn->conn_info, sendbuffer, CF_RSA_PROTO_OFFSET + encrypted_len, CF_DONE);

    BN_free(bn);
    BN_free(nonce_challenge);
    free(out);

/*Send the public key - we don't know if server has it */
/* proposition C2 */

    memset(sendbuffer, 0, CF_EXPANDSIZE);
    len = BN_bn2mpi(PUBKEY->n, sendbuffer);
    SendTransaction(conn->conn_info, sendbuffer, len, CF_DONE);        /* No need to encrypt the public key ... */

/* proposition C3 */
    memset(sendbuffer, 0, CF_EXPANDSIZE);
    len = BN_bn2mpi(PUBKEY->e, sendbuffer);
    SendTransaction(conn->conn_info, sendbuffer, len, CF_DONE);

/* check reply about public key - server can break conn_info here */

/* proposition S1 */
    memset(in, 0, CF_BUFSIZE);

    if (ReceiveTransaction(conn->conn_info, in, NULL) == -1)
    {
        Log(LOG_LEVEL_ERR, "Protocol transaction broken off (1). (ReceiveTransaction: %s)", GetErrorStr());
        RSA_free(server_pubkey);
        return false;
    }

    if (BadProtoReply(in))
    {
        Log(LOG_LEVEL_ERR, "Bad protocol reply '%s'", in);
        RSA_free(server_pubkey);
        return false;
    }

/* Get challenge response - should be CF_DEFAULT_DIGEST of challenge */

/* proposition S2 */
    memset(in, 0, CF_BUFSIZE);

    if (ReceiveTransaction(conn->conn_info, in, NULL) == -1)
    {
        Log(LOG_LEVEL_ERR, "Protocol transaction broken off (2). (ReceiveTransaction: %s)", GetErrorStr());
        RSA_free(server_pubkey);
        return false;
    }

    if ((HashesMatch(digest, in, CF_DEFAULT_DIGEST)) || (HashesMatch(digest, in, HASH_METHOD_MD5)))  // Legacy
    {
        if (implicitly_trust_server == false)        /* challenge reply was correct */
        {
            Log(LOG_LEVEL_VERBOSE, ".....................[.h.a.i.l.].................................");
            Log(LOG_LEVEL_VERBOSE, "Strong authentication of server '%s' connection confirmed", conn->this_server);
        }
        else
        {
            if (trust_key)
            {
                Log(LOG_LEVEL_VERBOSE, "Trusting server identity, promise to accept key from '%s' = '%s'", conn->this_server,
                      conn->remoteip);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Not authorized to trust public key of server '%s' (trustkey = false)",
                      conn->this_server);
                RSA_free(server_pubkey);
                return false;
            }
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Challenge response from server '%s/%s' was incorrect", conn->this_server,
             conn->remoteip);
        RSA_free(server_pubkey);
        return false;
    }

/* Receive counter challenge from server */

/* proposition S3 */
    memset(in, 0, CF_BUFSIZE);
    encrypted_len = ReceiveTransaction(conn->conn_info, in, NULL);

    if (encrypted_len <= 0)
    {
        Log(LOG_LEVEL_ERR, "Protocol transaction sent illegal cipher length");
        RSA_free(server_pubkey);
        return false;
    }

    decrypted_cchall = xmalloc(encrypted_len);

    if (RSA_private_decrypt(encrypted_len, in, decrypted_cchall, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Private decrypt failed, abandoning. (RSA_private_decrypt: %s)",
             ERR_reason_error_string(err));
        RSA_free(server_pubkey);
        return false;
    }

/* proposition C4 */
    if (FIPS_MODE)
    {
        HashString(decrypted_cchall, nonce_len, digest, CF_DEFAULT_DIGEST);
    }
    else
    {
        HashString(decrypted_cchall, nonce_len, digest, HASH_METHOD_MD5);
    }

    if (FIPS_MODE)
    {
        SendTransaction(conn->conn_info, digest, CF_DEFAULT_DIGEST_LEN, CF_DONE);
    }
    else
    {
        SendTransaction(conn->conn_info, digest, CF_MD5_LEN, CF_DONE);
    }

    free(decrypted_cchall);

/* If we don't have the server's public key, it will be sent */

    if (server_pubkey == NULL)
    {
        RSA *newkey = RSA_new();

        Log(LOG_LEVEL_VERBOSE, "Collecting public key from server!");

        /* proposition S4 - conditional */
        if ((len = ReceiveTransaction(conn->conn_info, in, NULL)) <= 0)
        {
            Log(LOG_LEVEL_ERR, "Protocol error in RSA authentation from IP '%s'", conn->this_server);
            return false;
        }

        if ((newkey->n = BN_mpi2bn(in, len, NULL)) == NULL)
        {
            err = ERR_get_error();
            Log(LOG_LEVEL_ERR, "Private key decrypt failed. (BN_mpi2bn: %s)", ERR_reason_error_string(err));
            RSA_free(newkey);
            return false;
        }

        /* proposition S5 - conditional */

        if ((len = ReceiveTransaction(conn->conn_info, in, NULL)) <= 0)
        {
            Log(LOG_LEVEL_INFO, "Protocol error in RSA authentation from IP '%s'",
                 conn->this_server);
            RSA_free(newkey);
            return false;
        }

        if ((newkey->e = BN_mpi2bn(in, len, NULL)) == NULL)
        {
            err = ERR_get_error();
            Log(LOG_LEVEL_ERR, "Public key decrypt failed. (BN_mpi2bn: %s)", ERR_reason_error_string(err));
            RSA_free(newkey);
            return false;
        }

        server_pubkey = RSAPublicKey_dup(newkey);
        Key *key = KeyNew(server_pubkey, CF_DEFAULT_DIGEST);
        ConnectionInfoSetKey(conn->conn_info, key);
        RSA_free(newkey);
    }

/* proposition C5 */

    if (!SetSessionKey(conn))
    {
        Log(LOG_LEVEL_ERR, "Unable to set session key");
        return false;
    }

    if (conn->session_key == NULL)
    {
        Log(LOG_LEVEL_ERR, "A random session key could not be established");
        RSA_free(server_pubkey);
        return false;
    }

    encrypted_len = RSA_size(server_pubkey);

    out = xmalloc(encrypted_len);

    if (RSA_public_encrypt(session_size, conn->session_key, out, server_pubkey, RSA_PKCS1_PADDING) <= 0)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Public encryption failed. (RSA_public_encrypt: %s)", ERR_reason_error_string(err));
        free(out);
        RSA_free(server_pubkey);
        return false;
    }

    SendTransaction(conn->conn_info, out, encrypted_len, CF_DONE);

    if (server_pubkey != NULL)
    {
        char buffer[EVP_MAX_MD_SIZE * 4];
        unsigned int length = 0;
        Log(LOG_LEVEL_VERBOSE, "Public key identity of host '%s' is '%s'", conn->remoteip,
              ConnectionInfoPrintableKeyHash(conn->conn_info));
        SavePublicKey(conn->username, buffer, server_pubkey);       // FIXME: username is local
        LastSaw(conn->remoteip, ConnectionInfoBinaryKeyHash(conn->conn_info, &length), LAST_SEEN_ROLE_CONNECT);
    }

    free(out);

    return true;
}


/*********************************************************************/

int BadProtoReply(char *buf)
{
    return (strncmp(buf, "BAD: ", 5) == 0);
}

/*********************************************************************/

int OKProtoReply(char *buf)
{
    return (strncmp(buf, "OK:", 3) == 0);
}

/*********************************************************************/

int FailedProtoReply(char *buf)
{
    return (strncmp(buf, CF_FAILEDSTR, strlen(CF_FAILEDSTR)) == 0);
}
