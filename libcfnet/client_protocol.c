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

#include <client_protocol.h>

#include <openssl/bn.h>                                    /* BN_* */
#include <openssl/err.h>                                   /* ERR_get_error */
#include <libcrypto-compat.h>

#include <communication.h>
#include <net.h>

/* libutils */
#include <logging.h>                                            /* Log */

/* TODO remove all includes from libpromises. */
extern char VIPADDRESS[CF_MAX_IP_LEN];
extern char VDOMAIN[];
extern char VFQNAME[];
#include <unix.h>                       /* GetCurrentUsername */
#include <lastseen.h>                   /* LastSaw */
#include <crypto.h>                     /* PublicKeyFile */
#include <files_hashes.h>               /* HashString,HashesMatch,HashPubKey*/
#include <known_dirs.h>
#include <hash.h>
#include <connection_info.h>
#include <tls_generic.h>                                  /* TLSErrorString */


/*********************************************************************/

static bool SKIPIDENTIFY = false; /* GLOBAL_P */

/*********************************************************************/

void SetSkipIdentify(bool enabled)
{
    SKIPIDENTIFY = enabled;
}

/*********************************************************************/

int IdentifyAgent(ConnectionInfo *conn_info)
{
    char uname[CF_BUFSIZE], sendbuff[CF_BUFSIZE];
    char dnsname[CF_MAXVARSIZE], localip[CF_MAX_IP_LEN];
    int ret;

    if ((!SKIPIDENTIFY) && (strcmp(VDOMAIN, CF_START_DOMAIN) == 0))
    {
        Log(LOG_LEVEL_ERR, "Undefined domain name");
        return false;
    }

    if (!SKIPIDENTIFY)
    {
        /* First we need to find out the IP address and DNS name of the socket
           we are sending from. This is not necessarily the same as VFQNAME if
           the machine has a different uname from its IP name (!) This can
           happen on poorly set up machines or on hosts with multiple
           interfaces, with different names on each interface ... */
        struct sockaddr_storage myaddr = {0};
        socklen_t myaddr_len = sizeof(myaddr);

        if (getsockname(conn_info->sd, (struct sockaddr *) &myaddr, &myaddr_len) == -1)
        {
            Log(LOG_LEVEL_ERR, "Couldn't get socket address. (getsockname: %s)", GetErrorStr());
            return false;
        }

        /* No lookup, just convert the bound address to string. */
        ret = getnameinfo((struct sockaddr *) &myaddr, myaddr_len,
                          localip, sizeof(localip),
                          NULL, 0, NI_NUMERICHOST);
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR,
                "During agent identification. (getnameinfo: %s)",
                  gai_strerror(ret));
            return false;
        }

        /* dnsname: Reverse lookup of the bound IP address. */
        ret = getnameinfo((struct sockaddr *) &myaddr, myaddr_len,
                          dnsname, sizeof(dnsname), NULL, 0, 0);
        if (ret != 0)
        {
            /* getnameinfo doesn't fail on resolution failure, it just prints
             * the IP, so here something else is wrong. */
            Log(LOG_LEVEL_ERR,
                "During agent identification for '%s'. (getnameinfo: %s)",
                  localip, gai_strerror(ret));
            return false;
        }

        /* Append a hostname if getnameinfo() does not return FQDN. Might only
         * happen if the host has no domainname, in which case VDOMAIN might
         * also be empty! In addition, missing PTR will give numerical result,
         * so use it either it's IPv4 or IPv6. Finally don't append the
         * VDOMAIN if "localhost" is resolved name, it means we're connecting
         * via loopback. */
        if ((strlen(VDOMAIN) > 0) &&
            !IsIPV6Address(dnsname) && (strchr(dnsname, '.') == NULL) &&
            strcmp(dnsname, "localhost") != 0)
        {
            strcat(dnsname, ".");
            strlcat(dnsname, VDOMAIN, sizeof(dnsname));
        }

        /* Seems to be a bug in some resolvers that adds garbage, when it just
         * returns the input. */
        if (strncmp(dnsname, localip, strlen(localip)) == 0
            && dnsname[strlen(localip)] != '\0')
        {
            dnsname[strlen(localip)] = '\0';
            Log(LOG_LEVEL_WARNING,
                "getnameinfo() seems to append garbage to unresolvable IPs, bug mitigated by CFEngine but please report your platform!");
        }
    }
    else
    {
        assert(sizeof(localip) >= sizeof(VIPADDRESS));
        strcpy(localip, VIPADDRESS);

        Log(LOG_LEVEL_VERBOSE,
            "skipidentify was promised, so we are trusting and simply announcing the identity as '%s' for this host",
            strlen(VFQNAME) > 0 ? VFQNAME : "skipident");
        if (strlen(VFQNAME) > 0)
        {
            strcpy(dnsname, VFQNAME);
        }
        else
        {
            strcpy(dnsname, "skipident");
        }
    }

/* client always identifies as root on windows */
#ifdef __MINGW32__
    snprintf(uname, sizeof(uname), "%s", "root");
#else
    GetCurrentUserName(uname, sizeof(uname));
#endif

    snprintf(sendbuff, sizeof(sendbuff), "CAUTH %s %s %s %d",
             localip, dnsname, uname, 0);

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

    conn->session_key = xmalloc(BN_num_bytes(bp));
    BN_bn2bin(bp, conn->session_key);

    BN_clear_free(bp);
    return true;
}

int AuthenticateAgent(AgentConnection *conn, bool trust_key)
{
    char sendbuffer[CF_EXPANDSIZE], in[CF_BUFSIZE], *out, *decrypted_cchall;
    BIGNUM *nonce_challenge, *bn = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    int encrypted_len, nonce_len = 0, len, session_size;
    bool need_to_implicitly_trust_server;
    char enterprise_field = 'c';

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_ERR, "No public/private key pair is loaded,"
            " please create one using cf-key");
        return false;
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
    RSA *server_pubkey = HavePublicKeyByIP(conn->username, conn->remoteip);
    if (server_pubkey)
    {
        need_to_implicitly_trust_server = false;
        encrypted_len = RSA_size(server_pubkey);
    }
    else
    {
        need_to_implicitly_trust_server = true;
        encrypted_len = nonce_len;
    }

// Server pubkey is what we want to has as a unique ID

    snprintf(sendbuffer, sizeof(sendbuffer), "SAUTH %c %d %d %c",
             need_to_implicitly_trust_server ? 'n': 'y',
             encrypted_len, nonce_len, enterprise_field);

    out = xmalloc(encrypted_len);

    if (server_pubkey != NULL)
    {
        if (RSA_public_encrypt(nonce_len, in, out, server_pubkey, RSA_PKCS1_PADDING) <= 0)
        {
            Log(LOG_LEVEL_ERR,
                "Public encryption failed. (RSA_public_encrypt: %s)",
            CryptoLastErrorString());
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

    const BIGNUM *pubkey_n, *pubkey_e;
    RSA_get0_key(PUBKEY, &pubkey_n, &pubkey_e, NULL);

    memset(sendbuffer, 0, CF_EXPANDSIZE);
    len = BN_bn2mpi(pubkey_n, sendbuffer);
    SendTransaction(conn->conn_info, sendbuffer, len, CF_DONE);        /* No need to encrypt the public key ... */

/* proposition C3 */
    memset(sendbuffer, 0, CF_EXPANDSIZE);
    len = BN_bn2mpi(pubkey_e, sendbuffer);
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
        Log(LOG_LEVEL_ERR, "Bad protocol reply: %s", in);
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

    /* Check if challenge reply was correct */
    if ((HashesMatch(digest, in, CF_DEFAULT_DIGEST)) ||
        (HashesMatch(digest, in, HASH_METHOD_MD5)))  // Legacy
    {
        if (need_to_implicitly_trust_server == false)
        {
            /* The IP was found in lastseen. */
            Log(LOG_LEVEL_VERBOSE,
                ".....................[.h.a.i.l.].................................");
            Log(LOG_LEVEL_VERBOSE,
                "Strong authentication of server '%s' connection confirmed",
                conn->this_server);
        }
        else                                /* IP was not found in lastseen */
        {
            if (trust_key)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Trusting server identity, promise to accept key from '%s' = '%s'",
                    conn->this_server, conn->remoteip);
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "Not authorized to trust public key of server '%s' (trustkey = false)",
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

    if (encrypted_len == -1)
    {
        Log(LOG_LEVEL_ERR, "Protocol transaction sent illegal cipher length");
        RSA_free(server_pubkey);
        return false;
    }

    decrypted_cchall = xmalloc(encrypted_len);

    if (RSA_private_decrypt(encrypted_len, in, decrypted_cchall, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
    {
        Log(LOG_LEVEL_ERR,
            "Private decrypt failed, abandoning. (RSA_private_decrypt: %s)",
            CryptoLastErrorString());
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
        if ((len = ReceiveTransaction(conn->conn_info, in, NULL)) == -1)
        {
            Log(LOG_LEVEL_ERR, "Protocol error in RSA authentation from IP '%s'", conn->this_server);
            return false;
        }

        BIGNUM *newkey_n, *newkey_e;
        if ((newkey_n = BN_mpi2bn(in, len, NULL)) == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Private key decrypt failed. (BN_mpi2bn: %s)",
                CryptoLastErrorString());
            RSA_free(newkey);
            return false;
        }

        /* proposition S5 - conditional */

        if ((len = ReceiveTransaction(conn->conn_info, in, NULL)) == -1)
        {
            Log(LOG_LEVEL_INFO, "Protocol error in RSA authentation from IP '%s'",
                 conn->this_server);
            BN_clear_free(newkey_n);
            RSA_free(newkey);
            return false;
        }

        if ((newkey_e = BN_mpi2bn(in, len, NULL)) == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Public key decrypt failed. (BN_mpi2bn: %s)",
                CryptoLastErrorString());
            BN_clear_free(newkey_n);
            RSA_free(newkey);
            return false;
        }

        if (RSA_set0_key(newkey, newkey_n, newkey_e, NULL) != 1)
        {
            Log(LOG_LEVEL_ERR, "Failed to set RSA key: %s",
                TLSErrorString(ERR_get_error()));
            BN_clear_free(newkey_e);
            BN_clear_free(newkey_n);
            RSA_free(newkey);
            return false;
        }

        server_pubkey = RSAPublicKey_dup(newkey);
        RSA_free(newkey);
    }
    assert(server_pubkey != NULL);

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
        Log(LOG_LEVEL_ERR,
            "Public encryption failed. (RSA_public_encrypt: %s)",
            CryptoLastErrorString());
        free(out);
        RSA_free(server_pubkey);
        return false;
    }

    SendTransaction(conn->conn_info, out, encrypted_len, CF_DONE);

    Key *key = KeyNew(server_pubkey, CF_DEFAULT_DIGEST);
    conn->conn_info->remote_key = key;

    Log(LOG_LEVEL_VERBOSE, "Public key identity of host '%s' is: %s",
        conn->remoteip, KeyPrintableHash(conn->conn_info->remote_key));

    SavePublicKey(conn->username, KeyPrintableHash(conn->conn_info->remote_key), server_pubkey);

    unsigned int length = 0;
    LastSaw(conn->remoteip, KeyBinaryHash(conn->conn_info->remote_key, &length), LAST_SEEN_ROLE_CONNECT);

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
