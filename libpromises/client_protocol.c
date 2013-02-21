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

#include "cf3.defs.h"
#include "client_protocol.h"

#include "communication.h"
#include "net.h"
#include "sysinfo.h"
#include "promises.h"
#include "lastseen.h"
#include "crypto.h"
#include "cfstream.h"
#include "files_hashes.h"
#include "logging.h"
#include "policy.h"

static void SetSessionKey(AgentConnection *conn);

/*********************************************************************/

static int SKIPIDENTIFY;

/*********************************************************************/

void SetSkipIdentify(bool enabled)
{
    SKIPIDENTIFY = enabled;
}

/*********************************************************************/

int IdentifyAgent(int sd, char *localip, int family)
{
    char uname[CF_BUFSIZE], sendbuff[CF_BUFSIZE], dnsname[CF_BUFSIZE];
    socklen_t len;

#if defined(HAVE_GETADDRINFO)
    int err;
    char myaddr[256] = {0};           /* Compilation trick for systems that don't know ipv6 */
#else    
    struct sockaddr_in myaddr = {0};
    struct in_addr *iaddr;
    struct hostent *hp;
#endif

    memset(sendbuff, 0, CF_BUFSIZE);
    memset(dnsname, 0, CF_BUFSIZE);

    if ((!SKIPIDENTIFY) && (strcmp(VDOMAIN, CF_START_DOMAIN) == 0))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Undefined domain name");
        return false;
    }

    if (!SKIPIDENTIFY)
    {
/* First we need to find out the IP address and DNS name of the socket
   we are sending from. This is not necessarily the same as VFQNAME if
   the machine has a different uname from its IP name (!) This can
   happen on poorly set up machines or on hosts with multiple
   interfaces, with different names on each interface ... */

        switch (family)
        {
        case AF_INET:
            len = sizeof(struct sockaddr_in);
            break;
#if defined(HAVE_GETADDRINFO)
        case AF_INET6:
            len = sizeof(struct sockaddr_in6);
            break;
#endif
        default:
            CfOut(OUTPUT_LEVEL_ERROR, "", "Software error in IdentifyForVerification, family = %d", family);
            return false;
        }

        if (getsockname(sd, (struct sockaddr *) &myaddr, &len) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "getsockname", "Couldn't get socket address\n");
            return false;
        }

        snprintf(localip, CF_MAX_IP_LEN - 1, "%s", sockaddr_ntop((struct sockaddr *) &myaddr));

        CfDebug("Identifying this agent as %s i.e. %s, with signature %d, family %d\n", localip, VFQNAME, 0, family);

#if defined(HAVE_GETADDRINFO)

        if ((err = getnameinfo((struct sockaddr *) &myaddr, len, dnsname, CF_MAXVARSIZE, NULL, 0, 0)) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Couldn't look up address v6 for %s: %s\n", dnsname, gai_strerror(err));
            return false;
        }

#else

        iaddr = &(myaddr.sin_addr);

        hp = gethostbyaddr((void *) iaddr, sizeof(myaddr.sin_addr), family);

        if ((hp == NULL) || (hp->h_name == NULL))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "gethostbyaddr", "Couldn't lookup IP address\n");
            return false;
        }

        strncpy(dnsname, hp->h_name, CF_MAXVARSIZE);

        if ((strstr(hp->h_name, ".") == 0) && (strlen(VDOMAIN) > 0))
        {
            strcat(dnsname, ".");
            strcat(dnsname, VDOMAIN);
        }
#endif
    }
    else
    {
        strcpy(localip, VIPADDRESS);

        if (strlen(VFQNAME) > 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  "skipidentify was promised, so we are trusting and simply announcing the identity as (%s) for this host\n",
                  VFQNAME);
            strcat(dnsname, VFQNAME);
        }
        else
        {
            strcat(dnsname, "skipident");
        }
    }

/* client always identifies as root on windows */
#ifdef __MINGW32__
    snprintf(uname, sizeof(uname), "%s", "root");
#else
    GetCurrentUserName(uname, sizeof(uname));
#endif

/* Some resolvers will not return FQNAME and missing PTR will give numerical result */

    if ((strlen(VDOMAIN) > 0) && (!IsIPV6Address(dnsname)) && (!strchr(dnsname, '.')))
    {
        CfDebug("Appending domain %s to %s\n", VDOMAIN, dnsname);
        strcat(dnsname, ".");
        strncat(dnsname, VDOMAIN, CF_MAXVARSIZE / 2);
    }

    if (strncmp(dnsname, localip, strlen(localip)) == 0)
    {
        /* Seems to be a bug in some resolvers that adds garbage, when it just returns the input */
        strcpy(dnsname, localip);
    }

    if (strlen(dnsname) == 0)
    {
        strcpy(dnsname, localip);
    }

    snprintf(sendbuff, CF_BUFSIZE - 1, "CAUTH %s %s %s %d", localip, dnsname, uname, 0);

    if (SendTransaction(sd, sendbuff, 0, CF_DONE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! IdentifyAgent: Could not send auth response");
        return false;
    }

    CfDebug("SENT:::%s\n", sendbuff);

    return true;
}

/*********************************************************************/

int AuthenticateAgent(AgentConnection *conn, Attributes attr, Promise *pp)
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
        CfOut(OUTPUT_LEVEL_ERROR, "", "No public/private key pair found at %s\n", CFPUBKEYFILE);
        return false;
    }

    enterprise_field = CfEnterpriseOptions();
    session_size = CfSessionKeySize(enterprise_field);

/* Generate a random challenge to authenticate the server */

    nonce_challenge = BN_new();
    if (nonce_challenge == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Cannot allocate BIGNUM structure for server challenge\n");
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

    snprintf(sendbuffer, sizeof(sendbuffer), "SAUTH %c %d %d %c", implicitly_trust_server ? 'n': 'y', encrypted_len,
             nonce_len, enterprise_field);

    out = xmalloc(encrypted_len);

    if (server_pubkey != NULL)
    {
        if (RSA_public_encrypt(nonce_len, in, out, server_pubkey, RSA_PKCS1_PADDING) <= 0)
        {
            err = ERR_get_error();
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "Public encryption failed = %s\n", ERR_reason_error_string(err));
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

    SendTransaction(conn->sd, sendbuffer, CF_RSA_PROTO_OFFSET + encrypted_len, CF_DONE);

    BN_free(bn);
    BN_free(nonce_challenge);
    free(out);

    if (DEBUG)
    {
        RSA_print_fp(stdout, PUBKEY, 0);
    }

/*Send the public key - we don't know if server has it */
/* proposition C2 */

    memset(sendbuffer, 0, CF_EXPANDSIZE);
    len = BN_bn2mpi(PUBKEY->n, sendbuffer);
    SendTransaction(conn->sd, sendbuffer, len, CF_DONE);        /* No need to encrypt the public key ... */

/* proposition C3 */
    memset(sendbuffer, 0, CF_EXPANDSIZE);
    len = BN_bn2mpi(PUBKEY->e, sendbuffer);
    SendTransaction(conn->sd, sendbuffer, len, CF_DONE);

/* check reply about public key - server can break connection here */

/* proposition S1 */
    memset(in, 0, CF_BUFSIZE);

    if (ReceiveTransaction(conn->sd, in, NULL) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "recv", pp, attr, "Protocol transaction broken off (1)");
        RSA_free(server_pubkey);
        return false;
    }

    if (BadProtoReply(in))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "%s", in);
        RSA_free(server_pubkey);
        return false;
    }

/* Get challenge response - should be CF_DEFAULT_DIGEST of challenge */

/* proposition S2 */
    memset(in, 0, CF_BUFSIZE);

    if (ReceiveTransaction(conn->sd, in, NULL) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "recv", pp, attr, "Protocol transaction broken off (2)");
        RSA_free(server_pubkey);
        return false;
    }

    if ((HashesMatch(digest, in, CF_DEFAULT_DIGEST)) || (HashesMatch(digest, in, HASH_METHOD_MD5)))  // Legacy
    {
        if (implicitly_trust_server == false)        /* challenge reply was correct */
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", ".....................[.h.a.i.l.].................................\n");
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Strong authentication of server=%s connection confirmed\n", pp->this_server);
        }
        else
        {
            if (attr.copy.trustkey)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Trusting server identity, promise to accept key from %s=%s", pp->this_server,
                      conn->remoteip);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Not authorized to trust the server=%s's public key (trustkey=false)\n",
                      pp->this_server);
                PromiseRef(OUTPUT_LEVEL_VERBOSE, pp);
                RSA_free(server_pubkey);
                return false;
            }
        }
    }
    else
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Challenge response from server %s/%s was incorrect!", pp->this_server,
             conn->remoteip);
        RSA_free(server_pubkey);
        return false;
    }

/* Receive counter challenge from server */

    CfDebug("Receive counter challenge from server\n");

/* proposition S3 */
    memset(in, 0, CF_BUFSIZE);
    encrypted_len = ReceiveTransaction(conn->sd, in, NULL);

    if (encrypted_len <= 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Protocol transaction sent illegal cipher length");
        RSA_free(server_pubkey);
        return false;
    }

    decrypted_cchall = xmalloc(encrypted_len);

    if (RSA_private_decrypt(encrypted_len, in, decrypted_cchall, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
    {
        err = ERR_get_error();
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Private decrypt failed = %s, abandoning\n",
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

    CfDebug("Replying to counter challenge with hash\n");

    if (FIPS_MODE)
    {
        SendTransaction(conn->sd, digest, CF_DEFAULT_DIGEST_LEN, CF_DONE);
    }
    else
    {
        SendTransaction(conn->sd, digest, CF_MD5_LEN, CF_DONE);
    }

    free(decrypted_cchall);

/* If we don't have the server's public key, it will be sent */

    if (server_pubkey == NULL)
    {
        RSA *newkey = RSA_new();

        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Collecting public key from server!\n");

        /* proposition S4 - conditional */
        if ((len = ReceiveTransaction(conn->sd, in, NULL)) <= 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Protocol error in RSA authentation from IP %s\n", pp->this_server);
            return false;
        }

        if ((newkey->n = BN_mpi2bn(in, len, NULL)) == NULL)
        {
            err = ERR_get_error();
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Private key decrypt failed = %s\n", ERR_reason_error_string(err));
            RSA_free(newkey);
            return false;
        }

        /* proposition S5 - conditional */

        if ((len = ReceiveTransaction(conn->sd, in, NULL)) <= 0)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "Protocol error in RSA authentation from IP %s\n",
                 pp->this_server);
            RSA_free(newkey);
            return false;
        }

        if ((newkey->e = BN_mpi2bn(in, len, NULL)) == NULL)
        {
            err = ERR_get_error();
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Public key decrypt failed = %s\n", ERR_reason_error_string(err));
            RSA_free(newkey);
            return false;
        }

        server_pubkey = RSAPublicKey_dup(newkey);
        RSA_free(newkey);
    }

/* proposition C5 */

    SetSessionKey(conn);

    if (conn->session_key == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "A random session key could not be established");
        RSA_free(server_pubkey);
        return false;
    }

    encrypted_len = RSA_size(server_pubkey);

    CfDebug("Encrypt %d bytes of session key into %d RSA bytes\n", session_size, encrypted_len);

    out = xmalloc(encrypted_len);

    if (RSA_public_encrypt(session_size, conn->session_key, out, server_pubkey, RSA_PKCS1_PADDING) <= 0)
    {
        err = ERR_get_error();
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Public encryption failed = %s\n", ERR_reason_error_string(err));
        free(out);
        RSA_free(server_pubkey);
        return false;
    }

    SendTransaction(conn->sd, out, encrypted_len, CF_DONE);

    if (server_pubkey != NULL)
    {
        HashPubKey(server_pubkey, conn->digest, CF_DEFAULT_DIGEST);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Public key identity of host \"%s\" is \"%s\"", conn->remoteip,
              HashPrint(CF_DEFAULT_DIGEST, conn->digest));
        SavePublicKey(conn->username, conn->remoteip, HashPrint(CF_DEFAULT_DIGEST, conn->digest), server_pubkey);       // FIXME: username is local
        LastSaw(conn->remoteip, conn->digest, LAST_SEEN_ROLE_CONNECT);
    }

    free(out);
    RSA_free(server_pubkey);

    return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void SetSessionKey(AgentConnection *conn)
{
    BIGNUM *bp;
    int session_size = CfSessionKeySize(conn->encryption_type);

    bp = BN_new();

    if (bp == NULL)
    {
        FatalError("Could not allocate session key");
    }

// session_size is in bytes
    if (!BN_rand(bp, session_size * 8, -1, 0))
    {
        FatalError("Can't generate cryptographic key");
    }

    conn->session_key = (unsigned char *) bp->d;
}

/*********************************************************************/

int BadProtoReply(char *buf)
{
    CfDebug("Protoreply: (%s)\n", buf);
    return (strncmp(buf, "BAD:", 4) == 0);
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
