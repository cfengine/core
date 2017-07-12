/*
   Copyright 2017 Northern.tech AS

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
#include <platform.h>

#include <openssl/bn.h>                                         /* BN_* */

#include <cf3.defs.h>
#include <item_lib.h>                 /* IsMatchItemIn */
#include <matching.h>                 /* IsRegexItemIn */
#include <net.h>                      /* ReceiveTransaction,SendTransaction */
#include <signals.h>
#include <string_lib.h>                               /* ToLowerStrInplace */
#include <regex.h>                                    /* StringMatchFull */
#include <lastseen.h>                                 /* LastSaw1 */
#include <files_hashes.h>                             /* HashString */
#include <crypto.h>                                   /* HavePublicKey */
#include <cf-serverd-enterprise-stubs.h>              /* ReceiveCollectCall */

#include "server.h"                                /* ServerConnectionState */
#include "server_common.h"                         /* ListPersistentClasses */


/* Functionality needed exclusively for the classic protocol. */


//*******************************************************************
// COMMANDS
//*******************************************************************

typedef enum
{
    PROTOCOL_COMMAND_EXEC,
    PROTOCOL_COMMAND_AUTH,
    PROTOCOL_COMMAND_GET,
    PROTOCOL_COMMAND_OPENDIR,
    PROTOCOL_COMMAND_SYNC,
    PROTOCOL_COMMAND_CONTEXTS,
    PROTOCOL_COMMAND_MD5,
    PROTOCOL_COMMAND_MD5_SECURE,
    PROTOCOL_COMMAND_AUTH_PLAIN,
    PROTOCOL_COMMAND_AUTH_SECURE,
    PROTOCOL_COMMAND_SYNC_SECURE,
    PROTOCOL_COMMAND_GET_SECURE,
    PROTOCOL_COMMAND_VERSION,
    PROTOCOL_COMMAND_OPENDIR_SECURE,
    PROTOCOL_COMMAND_VAR,
    PROTOCOL_COMMAND_VAR_SECURE,
    PROTOCOL_COMMAND_CONTEXT,
    PROTOCOL_COMMAND_CONTEXT_SECURE,
    PROTOCOL_COMMAND_QUERY_SECURE,
    PROTOCOL_COMMAND_CALL_ME_BACK,
    PROTOCOL_COMMAND_BAD
} ProtocolCommandClassic;

static const char *PROTOCOL_CLASSIC[] =
{
    "EXEC",
    "AUTH",                     /* old protocol */
    "GET",
    "OPENDIR",
    "SYNCH",
    "CLASSES",
    "MD5",
    "SMD5",
    "CAUTH",
    "SAUTH",
    "SSYNCH",
    "SGET",
    "VERSION",
    "SOPENDIR",
    "VAR",
    "SVAR",
    "CONTEXT",
    "SCONTEXT",
    "SQUERY",
    "SCALLBACK",
    NULL
};

static ProtocolCommandClassic GetCommandClassic(char *str)
{
    int i;
    for (i = 0; PROTOCOL_CLASSIC[i] != NULL; i++)
    {
        int cmdlen = strlen(PROTOCOL_CLASSIC[i]);
        if ((strncmp(str, PROTOCOL_CLASSIC[i], cmdlen) == 0) &&
            (str[cmdlen] == ' ' || str[cmdlen] == '\0'))
        {
            return i;
        }
    }
    assert (i == PROTOCOL_COMMAND_BAD);
    return i;
}


/* 'resolved' argument needs to be at least CF_BUFSIZE long */
static bool ResolveFilename(const char *req_path, char *res_path)
{

#if !defined _WIN32
    if (realpath(req_path, res_path) == NULL)
    {
        return false;
    }
#else
    memset(res_path, 0, CF_BUFSIZE);
    CompressPath(res_path, CF_BUFSIZE, req_path);
#endif

    /* Adjust for forward slashes */
    MapName(res_path);

/* NT has case-insensitive path names */
#ifdef __MINGW32__
    int i;

    for (i = 0; i < strlen(res_path); i++)
    {
        res_path[i] = ToLower(res_path[i]);
    }
#endif /* __MINGW32__ */

    return true;
}

static bool PathMatch(const char *stem, const char *request)
{
    const size_t stemlen = strlen(stem);
    if (strcmp(stem, FILE_SEPARATOR_STR) == 0)
    {
        /* Matches everything: */
        return true;
    }

    if (strcmp(stem, request) == 0)
    {
        /* An exact match is a match: */
        return true;
    }

    /* Otherwise, match only if stem names a parent directory of request: */
    return (strlen(request) > stemlen &&
            request[stemlen] == FILE_SEPARATOR &&
            strncmp(stem, request, stemlen) == 0);
}

static int AccessControl(EvalContext *ctx, const char *req_path, ServerConnectionState *conn, int encrypt)
{
    int access = false;
    char transrequest[CF_BUFSIZE];
    struct stat statbuf;
    char translated_req_path[CF_BUFSIZE];
    char transpath[CF_BUFSIZE];

/*
 * /var/cfengine -> $workdir translation.
 */
    TranslatePath(translated_req_path, req_path);

    if (ResolveFilename(translated_req_path, transrequest))
    {
        Log(LOG_LEVEL_VERBOSE, "Filename %s is resolved to %s", translated_req_path, transrequest);
    }
    else if ((lstat(translated_req_path, &statbuf) == -1) && !S_ISLNK(statbuf.st_mode))
    {
        Log(LOG_LEVEL_INFO, "Couldn't resolve (realpath: %s) filename: %s",
            GetErrorStr(), translated_req_path);
        return false;                /* can't continue without transrequest */
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Requested file is a dead symbolic link (filename: %s)", translated_req_path);
        strlcpy(transrequest, translated_req_path, CF_BUFSIZE);
    }

    if (lstat(transrequest, &statbuf) == -1)
    {
        Log(LOG_LEVEL_INFO, "Couldn't stat (lstat: %s) filename: %s",
            GetErrorStr(), transrequest);
        return false;
    }

    Log(LOG_LEVEL_DEBUG, "AccessControl, match (%s,%s) encrypt request = %d", transrequest, conn->hostname, encrypt);

    if (SV.admit == NULL)
    {
        Log(LOG_LEVEL_INFO, "cf-serverd access list is empty, no files are visible");
        return false;
    }

    conn->maproot = false;

    for (Auth *ap = SV.admit; ap != NULL; ap = ap->next)
    {
        Log(LOG_LEVEL_DEBUG, "Examining rule in access list (%s,%s)", transrequest, ap->path);

        /* TODO MapName when constructing this list. */
        strlcpy(transpath, ap->path, CF_BUFSIZE);
        MapName(transpath);

        if (PathMatch(transpath, transrequest))
        {
            Log(LOG_LEVEL_VERBOSE, "Found a matching rule in access list (%s in %s)", transrequest, transpath);

            if (stat(transpath, &statbuf) == -1)
            {
                Log(LOG_LEVEL_INFO,
                    "Warning cannot stat file object %s in admit/grant, or access list refers to dangling link",
                    transpath);
                continue;
            }

            if (!encrypt && ap->encrypt)
            {
                Log(LOG_LEVEL_ERR, "File %s requires encrypt connection...will not serve", transpath);
                access = false;
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Checking whether to map root privileges..");

                if (IsMatchItemIn(ap->maproot, conn->ipaddr) ||
                    IsRegexItemIn(ctx, ap->maproot, conn->hostname))
                {
                    conn->maproot = true;
                    Log(LOG_LEVEL_VERBOSE, "Mapping root privileges to access non-root files");
                }

                if (IsMatchItemIn(ap->accesslist, conn->ipaddr) ||
                    IsRegexItemIn(ctx, ap->accesslist, conn->hostname))
                {
                    access = true;
                    Log(LOG_LEVEL_DEBUG, "Access granted to host: %s", conn->ipaddr);
                }
            }
            break;
        }
    }

    for (Auth *dp = SV.deny; dp != NULL; dp = dp->next)
    {
        strlcpy(transpath, dp->path, CF_BUFSIZE);
        MapName(transpath);

        if (PathMatch(transpath, transrequest))
        {
            if ((IsMatchItemIn(dp->accesslist, conn->ipaddr)) ||
                (IsRegexItemIn(ctx, dp->accesslist, conn->hostname)))
            {
                access = false;
                Log(LOG_LEVEL_INFO,
                    "Host '%s' in deny list, explicitly denying access to '%s' in '%s'",
                    conn->ipaddr, transrequest, transpath);
                break;
            }
        }
    }

    if (access)
    {
        Log(LOG_LEVEL_VERBOSE, "Host %s granted access to %s", conn->hostname, req_path);

        if (encrypt && LOGENCRYPT)
        {
            /* Log files that were marked as requiring encryption */
            Log(LOG_LEVEL_INFO, "Host %s granted access to %s", conn->hostname, req_path);
        }
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Host %s denied access to %s", conn->hostname, req_path);
    }

    return access;
}

/* Checks the "varadmit" legacy ACL. */
static int LiteralAccessControl(EvalContext *ctx, char *in, ServerConnectionState *conn, int encrypt)
{
    Auth *ap;
    int access = false;
    char name[CF_BUFSIZE];

    name[0] = '\0';

    if (strncmp(in, "VAR", 3) == 0)
    {
        sscanf(in, "VAR %255[^\n]", name);
    }
    else if (strncmp(in, "CALL_ME_BACK", strlen("CALL_ME_BACK")) == 0)
    {
        sscanf(in, "CALL_ME_BACK %255[^\n]", name);
    }
    else
    {
        sscanf(in, "QUERY %128s", name);
    }

    conn->maproot = false;

    for (ap = SV.varadmit; ap != NULL; ap = ap->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Examining rule in access list (%s,%s)?", name, ap->path);

        if (strcmp(ap->path, name) == 0)                     /* exact match */
        {
            Log(LOG_LEVEL_VERBOSE, "Found a matching rule in access list (%s in %s)", name, ap->path);

            if ((!ap->literal) && (!ap->variable))
            {
                Log(LOG_LEVEL_ERR,
                    "Variable/query '%s' requires a literal server item...cannot set variable directly by path",
                      ap->path);
                access = false;
                break;
            }

            if (!encrypt && ap->encrypt)
            {
                Log(LOG_LEVEL_ERR, "Variable %s requires encrypt connection...will not serve", name);
                access = false;
                break;
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Checking whether to map root privileges");

                if ((IsMatchItemIn(ap->maproot, conn->ipaddr)) ||
                    (IsRegexItemIn(ctx, ap->maproot, conn->hostname)))
                {
                    conn->maproot = true;
                    Log(LOG_LEVEL_VERBOSE, "Mapping root privileges");
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "No root privileges granted");
                }

                if ((IsMatchItemIn(ap->accesslist, conn->ipaddr))
                    || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
                {
                    access = true;
                    Log(LOG_LEVEL_DEBUG, "Access privileges - match found");
                }
            }
        }
    }

    for (ap = SV.vardeny; ap != NULL; ap = ap->next)
    {
        if (strcmp(ap->path, name) == 0)
        {
            if ((IsMatchItemIn(ap->accesslist, conn->ipaddr))
                || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
            {
                access = false;
                Log(LOG_LEVEL_VERBOSE, "Host %s explicitly denied access to %s", conn->hostname, name);
                break;
            }
        }
    }

    if (access)
    {
        Log(LOG_LEVEL_VERBOSE, "Host %s granted access to literal '%s'", conn->hostname, name);

        if (encrypt && LOGENCRYPT)
        {
            /* Log files that were marked as requiring encryption */
            Log(LOG_LEVEL_INFO, "Host %s granted access to literal '%s'", conn->hostname, name);
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Host %s denied access to literal '%s'", conn->hostname, name);
    }

    return access;
}

/* Checks the "varadmit" legacy ACL. */
static Item *ContextAccessControl(EvalContext *ctx, char *in, ServerConnectionState *conn, int encrypt)
{
    Auth *ap;
    int access = false;
    char client_regex[CF_BUFSIZE];
    Item *ip, *matches = NULL;

    int ret = sscanf(in, "CONTEXT %255[^\n]", client_regex);
    Item *persistent_classes = ListPersistentClasses();
    if (ret != 1 || persistent_classes == NULL)
    {
        return NULL;
    }

    for (ip = persistent_classes; ip != NULL; ip = ip->next)
    {
        /* Does the class match the regex that the agent requested? */
        if (StringMatchFull(client_regex, ip->name))
        {
            for (ap = SV.varadmit; ap != NULL; ap = ap->next)
            {
                /* Does the class match any of the regex in ACLs? */
                if (StringMatchFull(ap->path, ip->name))
                {
                    Log(LOG_LEVEL_VERBOSE,
                        "Found a matching rule in access list (%s in %s)",
                        ip->name, ap->path);

                    if (!ap->classpattern)
                    {
                        Log(LOG_LEVEL_ERR,
                            "Context %s requires a literal server item... "
                            "cannot set variable directly by path",
                            ap->path);
                        access = false;
                        continue;
                    }

                    if (!encrypt && ap->encrypt)
                    {
                        Log(LOG_LEVEL_ERR,
                            "Context %s requires encrypt connection... "
                            "will not serve",
                            ip->name);
                        access = false;
                        break;
                    }
                    else
                    {
                        Log(LOG_LEVEL_DEBUG,
                            "Checking whether to map root privileges");

                        if ((IsMatchItemIn(ap->maproot, conn->ipaddr))
                            || (IsRegexItemIn(ctx, ap->maproot, conn->hostname)))
                        {
                            conn->maproot = true;
                            Log(LOG_LEVEL_VERBOSE,
                                "Mapping root privileges");
                        }
                        else
                        {
                            Log(LOG_LEVEL_VERBOSE,
                                "No root privileges granted");
                        }

                        if ((IsMatchItemIn(ap->accesslist, conn->ipaddr))
                            || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
                        {
                            access = true;
                            Log(LOG_LEVEL_DEBUG,
                                "Access privileges - match found");
                        }
                    }
                }
            }

            for (ap = SV.vardeny; ap != NULL; ap = ap->next)
            {
                if (strcmp(ap->path, ip->name) == 0)
                {
                    if ((IsMatchItemIn(ap->accesslist, conn->ipaddr))
                        || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
                    {
                        access = false;
                        Log(LOG_LEVEL_VERBOSE,
                            "Host %s explicitly denied access to context %s",
                            conn->hostname, ip->name);
                        break;
                    }
                }
            }

            if (access)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Host %s granted access to context '%s'",
                    conn->hostname, ip->name);
                AppendItem(&matches, ip->name, NULL);

                if (encrypt && LOGENCRYPT)
                {
                    /* Log files that were marked as requiring encryption */
                    Log(LOG_LEVEL_INFO,
                        "Host %s granted access to context '%s'",
                        conn->hostname, ip->name);
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Host %s denied access to context '%s'",
                    conn->hostname, ip->name);
            }
        }
    }

    DeleteItemList(persistent_classes);
    return matches;
}

static int cfscanf(char *in, int len1, int len2, char *out1, char *out2, char *out3)
{
    int len3 = 0;
    char *sp;

    sp = in;
    memcpy(out1, sp, len1);
    out1[len1] = '\0';

    sp += len1 + 1;
    memcpy(out2, sp, len2);

    sp += len2 + 1;
    len3 = strlen(sp);
    memcpy(out3, sp, len3);
    out3[len3] = '\0';

    return (len1 + len2 + len3 + 2);
}

static void SetConnectionData(ServerConnectionState *conn, char *buf)
{
    char ipstring[CF_MAXVARSIZE], fqname[CF_MAXVARSIZE], username[CF_MAXVARSIZE];

    Log(LOG_LEVEL_DEBUG, "Connecting host identifies itself as '%s'", buf);

    memset(ipstring, 0, CF_MAXVARSIZE);
    memset(fqname, 0, CF_MAXVARSIZE);
    memset(username, 0, CF_MAXVARSIZE);

    sscanf(buf, "%255s %255s %255s", ipstring, fqname, username);

    /* The "ipstring" that the client sends is currently *ignored* as
     * conn->ipaddr is always set from the connecting socket address. */

    Log(LOG_LEVEL_DEBUG, "(ipstring=[%s],fqname=[%s],username=[%s],socket=[%s])",
            ipstring, fqname, username, conn->ipaddr);

    ToLowerStrInplace(fqname);

    strlcpy(conn->hostname, fqname, CF_MAXVARSIZE);

    SetConnIdentity(conn, username);
}

static int CheckStoreKey(ServerConnectionState *conn, RSA *key)
{
    RSA *savedkey;

    const char *udigest = KeyPrintableHash(ConnectionInfoKey(conn->conn_info));
    assert(udigest != NULL);

    if ((savedkey = HavePublicKey(conn->username, conn->ipaddr, udigest)))
    {
        Log(LOG_LEVEL_VERBOSE,
            "A public key was already known from %s/%s - no trust required",
            conn->hostname, conn->ipaddr);

        if ((BN_cmp(savedkey->e, key->e) == 0) && (BN_cmp(savedkey->n, key->n) == 0))
        {
            Log(LOG_LEVEL_VERBOSE,
                "The public key identity was confirmed as %s@%s",
                conn->username, conn->hostname);
            SendTransaction(conn->conn_info, "OK: key accepted", 0, CF_DONE);
            RSA_free(savedkey);
            return true;
        }
    }

    /* Finally, if we're still here then the key is new (not in ppkeys
     * directory): Allow access only if host is listed in "trustkeysfrom" body
     * server control option. */

    if ((SV.trustkeylist != NULL) &&
        (IsMatchItemIn(SV.trustkeylist, conn->ipaddr)))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Host %s/%s was found in the list of hosts to trust",
            conn->hostname, conn->ipaddr);
        SendTransaction(conn->conn_info,
                        "OK: unknown key was accepted on trust", 0, CF_DONE);
        SavePublicKey(conn->username, udigest, key);
        return true;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,
            "No previous key found, and unable to accept this one on trust");
        SendTransaction(conn->conn_info,
                        "BAD: key could not be accepted on trust",
                        0, CF_DONE);
        return false;
    }
}

static int AuthenticationDialogue(ServerConnectionState *conn, char *recvbuffer, int recvlen)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1] = { 0 };

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_ERR, "No public/private key pair is loaded,"
            " please create one using cf-key");
        return false;
    }

    int PRIVKEY_size = RSA_size(PRIVKEY);
    int digestLen;
    HashMethod digestType;

    if (FIPS_MODE)
    {
        digestType = CF_DEFAULT_DIGEST;
        digestLen = CF_DEFAULT_DIGEST_LEN;
    }
    else
    {
        digestType = HASH_METHOD_MD5;
        digestLen = CF_MD5_LEN;
    }

/* parameters received in SAUTH command */
char iscrypt, enterprise_field;

/* proposition C1 - SAUTH command */
{
    char sauth[10] = { 0 };
    unsigned int crypt_len;          /* received encrypted challenge length */
    unsigned int challenge_len;        /* challenge length after decryption */

    int nparam = sscanf(recvbuffer, "%9s %c %u %u %c",
                        sauth, &iscrypt, &crypt_len,
                        &challenge_len, &enterprise_field);

    if (nparam >= 1 && strcmp(sauth, "SAUTH") != 0)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "was expecting SAUTH command but got '%s'",
            sauth);
        return false;
    }

    if (nparam != 5 && nparam != 4)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "peer sent only %d arguments to SAUTH command",
            nparam - 1);
        return false;
    }

    /* CFEngine 2 had no enterprise/community differentiation. */
    if (nparam == 4)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Peer sent only 4 parameters, "
            "assuming it is a legacy community client");
        enterprise_field = 'c';
    }

    if ((challenge_len == 0) || (crypt_len == 0))
    {
        Log(LOG_LEVEL_ERR,
            "Authentication failure: received unexpected challenge length "
            "(%u that decrypts to %u bytes)",
            challenge_len, crypt_len);
        return false;
    }

    if (crypt_len > CF_NONCELEN * 2)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "received encrypted challenge is too long "
            "(%d bytes)", crypt_len);
        return false;
    }

    if (challenge_len > CF_NONCELEN * 2)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "received challenge is too long (%u bytes)",
            challenge_len);
        return false;
    }

    Log(LOG_LEVEL_DEBUG,
        "Challenge encryption = %c, challenge_len = %u, crypt_len = %u",
        iscrypt, challenge_len, crypt_len);

    char *challenge;
    char decrypted_challenge[PRIVKEY_size];

    if (iscrypt == 'y')                         /* challenge came encrypted */
    {
        if (recvlen < CF_RSA_PROTO_OFFSET + crypt_len)
        {
            Log(LOG_LEVEL_ERR, "Authentication failure: peer sent only %d "
                "bytes as encrypted challenge but claims to have sent %u bytes",
                recvlen - CF_RSA_PROTO_OFFSET, crypt_len);
        }

        int ret = RSA_private_decrypt(crypt_len, recvbuffer + CF_RSA_PROTO_OFFSET,
                                      decrypted_challenge, PRIVKEY, RSA_PKCS1_PADDING);
        if (ret < 0)
        {
            Log(LOG_LEVEL_ERR, "Authentication failure: "
                "private decrypt of received challenge failed (%s)",
                CryptoLastErrorString());
            Log(LOG_LEVEL_ERR,
                "Probably the client has wrong public key for this server");
            return false;
        }
        if (ret != challenge_len)
        {
            Log(LOG_LEVEL_ERR, "Authentication failure: "
                "private decrypt of received challenge (%u bytes) "
                "resulted in %d bytes instead of promised %u bytes",
                crypt_len, ret, challenge_len);
            return false;
        }

        challenge = decrypted_challenge;
    }
    else                                      /* challenge came unencrypted */
    {
        if (challenge_len != crypt_len)
        {
            Log(LOG_LEVEL_ERR,
                "Authentication failure: peer sent illegal challenge "
                "(challenge_len %u != crypt_len %u)",
                challenge_len, crypt_len);
            return false;
        }

        if (recvlen < CF_RSA_PROTO_OFFSET + challenge_len)
        {
            Log(LOG_LEVEL_ERR,
                "Authentication failure: peer sent only %d "
                "bytes as challenge but claims to have sent %u bytes",
                recvlen - CF_RSA_PROTO_OFFSET, challenge_len);
            return false;
        }

        challenge = &recvbuffer[CF_RSA_PROTO_OFFSET];
    }

/* Client's ID is now established by key or trusted, reply with digest */
    HashString(challenge, challenge_len, digest, digestType);
}

/* proposition C2 - Receive client's public key modulus */
RSA *newkey = RSA_new();
{

    int len_n = ReceiveTransaction(conn->conn_info, recvbuffer, NULL);
    if (len_n == -1)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "error while receiving public key modulus");
        RSA_free(newkey);
        return false;
    }

    if ((newkey->n = BN_mpi2bn(recvbuffer, len_n, NULL)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "private decrypt of received public key modulus failed "
            "(%s)", CryptoLastErrorString());
        RSA_free(newkey);
        return false;
    }
}

/* proposition C3 - Receive client's public key exponent. */
{
    int len_e = ReceiveTransaction(conn->conn_info, recvbuffer, NULL);
    if (len_e == -1)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "error while receiving public key exponent");
        RSA_free(newkey);
        return false;
    }

    if ((newkey->e = BN_mpi2bn(recvbuffer, len_e, NULL)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "private decrypt of received public key exponent failed "
            "(%s)", CryptoLastErrorString());
        RSA_free(newkey);
        return false;
    }
}

/* Compute and store hash of the client's public key. */
{
    Key *key = KeyNew(newkey, CF_DEFAULT_DIGEST);
    conn->conn_info->remote_key = key;

    Log(LOG_LEVEL_VERBOSE, "Peer's identity is: %s",
        KeyPrintableHash(key));

    LastSaw1(conn->ipaddr, KeyPrintableHash(key),
             LAST_SEEN_ROLE_ACCEPT);

    /* Do we want to trust the received key? */
    if (!CheckStoreKey(conn, newkey))   /* conceals proposition S1 */
    {
        return false;
    }
}

/* proposition S2 - reply with digest of challenge. */
{
    Log(LOG_LEVEL_DEBUG, "Sending challenge response");
    SendTransaction(conn->conn_info, digest, digestLen, CF_DONE);
}

/* proposition S3 - send counter-challenge */
{
    BIGNUM *counter_challenge_BN = BN_new();
    if (counter_challenge_BN == NULL)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "cannot allocate BIGNUM structure for counter-challenge");
        return false;
    }

    BN_rand(counter_challenge_BN, CF_NONCELEN, 0, 0);

    char counter_challenge[CF_BUFSIZE];
    int counter_challenge_len = BN_bn2mpi(counter_challenge_BN, counter_challenge);
    BN_free(counter_challenge_BN);

    /* Compute counter-challenge digest. */
    HashString(counter_challenge, counter_challenge_len, digest, digestType);

    /* Encryption buffer is always RSA_size(key) and buffer needs 11 bytes,
     * see RSA_public_encrypt manual. */
    int encrypted_len = RSA_size(newkey);
    char encrypted_counter_challenge[encrypted_len];
    assert(counter_challenge_len < encrypted_len - 11);

    int ret = RSA_public_encrypt(counter_challenge_len, counter_challenge,
                                 encrypted_counter_challenge, newkey,
                                 RSA_PKCS1_PADDING);
    if (ret != encrypted_len)
    {
        if (ret == -1)
        {
            Log(LOG_LEVEL_ERR, "Authentication failure: "
                "public encryption of counter-challenge failed "
                "(%s)", CryptoLastErrorString());
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Authentication failure: "
                "public encryption of counter-challenge failed "
                "(result length %d but should be %d)",
                ret, encrypted_len);
        }
        return false;
    }

    Log(LOG_LEVEL_DEBUG, "Sending counter-challenge");
    SendTransaction(conn->conn_info, encrypted_counter_challenge,
                    encrypted_len, CF_DONE);
}

/* proposition S4, S5 - If the client doesn't have our public key, send it. */
{
    if (iscrypt != 'y')
    {
        Log(LOG_LEVEL_DEBUG, "Sending server's public key");

        char bignum_buf[CF_BUFSIZE] = { 0 };

        /* proposition S4  - conditional */
        int len_n = BN_bn2mpi(PUBKEY->n, bignum_buf);
        SendTransaction(conn->conn_info, bignum_buf, len_n, CF_DONE);

        /* proposition S5  - conditional */
        int len_e = BN_bn2mpi(PUBKEY->e, bignum_buf);
        SendTransaction(conn->conn_info, bignum_buf, len_e, CF_DONE);
    }
}

/* proposition C4 - Receive counter-challenge response. */
{
    char recv_buf[CF_BUFSIZE] = { 0 };

    int recv_len = ReceiveTransaction(conn->conn_info, recv_buf, NULL);
    if (recv_len < digestLen)
    {
        if (recv_len == -1)
        {
            Log(LOG_LEVEL_ERR, "Authentication failure: "
                "error receiving counter-challenge response; "
                "maybe the client does not trust our key?");
        }
        else                                      /* 0 < recv_len < expected_len */
        {
            Log(LOG_LEVEL_ERR, "Authentication failure: "
                "error receiving counter-challenge response, "
                "only got %d out of %d bytes",
                recv_len, digestLen);
        }
        return false;
    }

    if (HashesMatch(digest, recv_buf, digestType))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Authentication of client %s/%s achieved",
            conn->hostname, conn->ipaddr);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "counter-challenge response was incorrect");
        return false;
    }
}

/* proposition C5 - Receive session key */
{
    Log(LOG_LEVEL_DEBUG, "Receiving session key from client...");

    char session_key[CF_BUFSIZE] = { 0 };
    int  session_key_size = CfSessionKeySize(enterprise_field);
    int keylen = ReceiveTransaction(conn->conn_info, session_key, NULL);

    Log(LOG_LEVEL_DEBUG,
        "Received encrypted session key of %d bytes, "
        "should decrypt to %d bytes",
        keylen, session_key_size);

    if (keylen == -1)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "error receiving session key");
        return false;
    }

    if (keylen > CF_BUFSIZE / 2)
    {
        Log(LOG_LEVEL_ERR, "Authentication failure: "
            "session key received is too long (%d bytes)",
            keylen);
        return false;
    }

    conn->session_key = xmalloc(session_key_size);
    conn->encryption_type = enterprise_field;

    if (keylen == CF_BLOWFISHSIZE)      /* Support the old non-ecnrypted for upgrade */
    {
        memcpy(conn->session_key, session_key, session_key_size);
    }
    else
    {
        char decrypted_session_key[PRIVKEY_size];
        int ret = RSA_private_decrypt(keylen, session_key,
                                      decrypted_session_key, PRIVKEY,
                                      RSA_PKCS1_PADDING);
        if (ret != session_key_size)
        {
            if (ret < 0)
            {
                Log(LOG_LEVEL_ERR, "Authentication failure: "
                    "private decrypt of session key failed "
                    "(%s)", CryptoLastErrorString());
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Authentication failure: "
                    "session key decrypts to invalid size, "
                    "expected %d but got %d bytes",
                    session_key_size, ret);
            }
            return false;
        }

        memcpy(conn->session_key, decrypted_session_key, session_key_size);
    }
}

return true;
}



int BusyWithClassicConnection(EvalContext *ctx, ServerConnectionState *conn)
{
    time_t tloc, trem = 0;
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT], check[CF_BUFSIZE];
    char sendbuffer[CF_BUFSIZE] = { 0 };
    char filename[CF_BUFSIZE], buffer[CF_BUFSIZE], out[CF_BUFSIZE];
    long time_no_see = 0;
    unsigned int len = 0;
    int drift, plainlen, received, encrypted = 0;
    size_t zret;
    ServerFileGetState get_args;
    Item *classes;

    memset(recvbuffer, 0, CF_BUFSIZE + CF_BUFEXT);
    memset(&get_args, 0, sizeof(get_args));

    received = ReceiveTransaction(conn->conn_info, recvbuffer, NULL);
    if (received == -1)
    {
        return false;
    }

    if (strlen(recvbuffer) == 0)
    {
        Log(LOG_LEVEL_WARNING, "Got NULL transmission, skipping!");
        return true;
    }

    /* Don't process request if we're signalled to exit. */
    if (IsPendingTermination())
    {
        Log(LOG_LEVEL_VERBOSE, "Server must exit, closing connection");
        return false;
    }

    ProtocolCommandClassic command = GetCommandClassic(recvbuffer);

    switch (command)
    {
    /* Plain text authentication; this MUST be the first command client
       using classic protocol is sending. */
    case PROTOCOL_COMMAND_AUTH_PLAIN:
        SetConnectionData(conn, (char *) (recvbuffer + strlen("CAUTH ")));

        if (conn->username == NULL || !IsUserNameValid(conn->username))
        {
            Log(LOG_LEVEL_INFO, "Client is sending wrong username: %s",
                conn->username);
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        /* This is used only for forcing correct state of state machine while
           connecting and authenticating user using classic protocol. */
        conn->user_data_set = true;

        return true;

    /* This MUST be exactly second command client using classic protocol is
       sending.  This is where key agreement takes place. */
    case PROTOCOL_COMMAND_AUTH_SECURE:
        /* First command was omitted by client; this is protocol violation. */
        if (!conn->user_data_set)
        {
            Log(LOG_LEVEL_INFO,
                "Client is not verified; rejecting connection");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        conn->rsa_auth = AuthenticationDialogue(conn, recvbuffer, received);
        if (!conn->rsa_auth)
        {
            Log(LOG_LEVEL_INFO, "Auth dialogue error");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        return true;

    default:
        break;
    }

    /* At this point we should have both user_data_set and rsa_auth set to
       perform any operation.  We can check only for second one as without
       first it won't be set up. */
    if (!conn->rsa_auth)
    {
        Log(LOG_LEVEL_INFO,
            "REFUSAL due to no RSA authentication (command: %d)",
            command);
        RefuseAccess(conn, recvbuffer);
        return false;
    }

    /* We have to have key at this point. */
    assert(conn->session_key);

    /* At this point we can safely do next switch and make sure user is
     * authenticated. */
    switch (command)
    {
    case PROTOCOL_COMMAND_EXEC:
    {
        const size_t EXEC_len = strlen(PROTOCOL_CLASSIC[PROTOCOL_COMMAND_EXEC]);
        /* Assert recvbuffer starts with EXEC. */
        assert(strncmp(PROTOCOL_CLASSIC[PROTOCOL_COMMAND_EXEC],
                       recvbuffer, EXEC_len) == 0);

        char *args = &recvbuffer[EXEC_len];
        args += strspn(args, " \t");                       /* bypass spaces */

        Log(LOG_LEVEL_VERBOSE, "%14s %7s %s",
            "Received:", "EXEC", args);

        bool b = DoExec2(ctx, conn, args,
                         sendbuffer, sizeof(sendbuffer));

        /* In the end we might keep the connection open (return true) to be
         * ready for next requests, but we must always send the TERMINATOR
         * string so that the client can close the connection at will. */
        Terminate(conn->conn_info);

        return b;
    }
    case PROTOCOL_COMMAND_VERSION:
        snprintf(sendbuffer, sizeof(sendbuffer), "OK: %s", Version());
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        return conn->user_data_set;

    case PROTOCOL_COMMAND_GET:
        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "GET %d %[^\n]", &(get_args.buf_size), filename);

        if ((get_args.buf_size < 0) || (get_args.buf_size > CF_BUFSIZE))
        {
            Log(LOG_LEVEL_INFO, "GET buffer out of bounds");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        zret = ShortcutsExpand(filename, sizeof(filename),
            SV.path_shortcuts,
            conn->ipaddr, conn->hostname,
            KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

        if (zret == (size_t) -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Expanding filename (%s) made it too long (>= %zu)", filename, sizeof(filename));
            return false;
        }

        if (!AccessControl(ctx, filename, conn, false))
        {
            Log(LOG_LEVEL_INFO, "Access denied to get object");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        memset(sendbuffer, 0, sizeof(sendbuffer));

        if (get_args.buf_size >= CF_BUFSIZE)
        {
            get_args.buf_size = 2048;
        }

        get_args.conn = conn;
        get_args.encrypt = false;
        get_args.replybuff = sendbuffer;
        get_args.replyfile = filename;

        CfGetFile(&get_args);

        return true;

    case PROTOCOL_COMMAND_GET_SECURE:
        memset(buffer, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SGET %u %d", &len, &(get_args.buf_size));

        if (received != len + CF_PROTO_OFFSET)
        {
            Log(LOG_LEVEL_INFO, "Protocol error SGET");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        plainlen = DecryptString(buffer, sizeof(buffer),
                                 recvbuffer + CF_PROTO_OFFSET, len,
                                 conn->encryption_type, conn->session_key);

        cfscanf(buffer, strlen("GET"), strlen("dummykey"), check, sendbuffer, filename);

        if (strcmp(check, "GET") != 0)
        {
            Log(LOG_LEVEL_INFO, "SGET/GET problem");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        if ((get_args.buf_size < 0) || (get_args.buf_size > 8192))
        {
            Log(LOG_LEVEL_INFO, "SGET bounding error");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        if (get_args.buf_size >= CF_BUFSIZE)
        {
            get_args.buf_size = 2048;
        }

        zret = ShortcutsExpand(filename, sizeof(filename),
            SV.path_shortcuts,
            conn->ipaddr, conn->hostname,
            KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

        if (zret == (size_t) -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Expanding filename (%s) made it too long (>= %zu)", filename, sizeof(filename));
            return false;
        }

        Log(LOG_LEVEL_DEBUG, "Confirm decryption, and thus validity of caller");
        Log(LOG_LEVEL_DEBUG, "SGET '%s' with blocksize %d", filename, get_args.buf_size);

        if (!AccessControl(ctx, filename, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Access control error");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        memset(sendbuffer, 0, sizeof(sendbuffer));

        get_args.conn = conn;
        get_args.encrypt = true;
        get_args.replybuff = sendbuffer;
        get_args.replyfile = filename;

        CfEncryptGetFile(&get_args);
        return true;

    case PROTOCOL_COMMAND_OPENDIR_SECURE:
        memset(buffer, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SOPENDIR %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Protocol error OPENDIR: %d", len);
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(recvbuffer, sizeof(recvbuffer),
                                 out, len,
                                 conn->encryption_type, conn->session_key);

        if (strncmp(recvbuffer, "OPENDIR", 7) != 0)
        {
            Log(LOG_LEVEL_INFO, "Opendir failed to decrypt");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "OPENDIR %[^\n]", filename);

        zret = ShortcutsExpand(filename, sizeof(filename),
            SV.path_shortcuts,
            conn->ipaddr, conn->hostname,
            KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

        if (zret == (size_t) -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Expanding filename (%s) made it too long (>= %zu)", filename, sizeof(filename));
            return false;
        }

        if (!AccessControl(ctx, filename, conn, true))        /* opendir don't care about privacy */
        {
            Log(LOG_LEVEL_INFO, "Access error");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        CfSecOpenDirectory(conn, sendbuffer, filename);
        return true;

    case PROTOCOL_COMMAND_OPENDIR:
        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "OPENDIR %[^\n]", filename);

        zret = ShortcutsExpand(filename, sizeof(filename),
            SV.path_shortcuts,
            conn->ipaddr, conn->hostname,
            KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

        if (zret == (size_t) -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Expanding filename (%s) made it too long (>= %zu)", filename, sizeof(filename));
            return false;
        }

        if (!AccessControl(ctx, filename, conn, false))        /* opendir don't care about privacy */
        {
            Log(LOG_LEVEL_INFO, "DIR access error");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        CfOpenDirectory(conn, sendbuffer, filename);
        return true;

    case PROTOCOL_COMMAND_SYNC_SECURE:
        memset(buffer, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SSYNCH %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Protocol error SSYNCH: %d", len);
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(recvbuffer, sizeof(recvbuffer),
                                 out, len,
                                 conn->encryption_type, conn->session_key);

        if (plainlen < 0)
        {
            DebugBinOut((char *) conn->session_key, 32, "Session key");
            Log(LOG_LEVEL_ERR, "Bad decrypt (%d)", len);
        }

        if (strncmp(recvbuffer, "SYNCH", 5) != 0)
        {
            Log(LOG_LEVEL_INFO, "No synch");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_SYNC:
        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "SYNCH %ld STAT %[^\n]", &time_no_see, filename);

        trem = (time_t) time_no_see;

        if (filename[0] == '\0')
        {
            break;
        }

        if ((tloc = time((time_t *) NULL)) == -1)
        {
            Log(LOG_LEVEL_INFO, "Couldn't read system clock. (time: %s)", GetErrorStr());
            SendTransaction(conn->conn_info, "BAD: clocks out of synch", 0, CF_DONE);
            return true;
        }

        drift = (int) (tloc - trem);

        zret = ShortcutsExpand(filename, sizeof(filename),
            SV.path_shortcuts,
            conn->ipaddr, conn->hostname,
            KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

        if (zret == (size_t) -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Expanding filename (%s) made it too long (>= %zu)", filename, sizeof(filename));
            return false;
        }

        if (!AccessControl(ctx, filename, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Access control in sync");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        if (DENYBADCLOCKS && (drift * drift > CLOCK_DRIFT * CLOCK_DRIFT))
        {
            snprintf(sendbuffer, sizeof(sendbuffer),
                     "BAD: Clocks are too far unsynchronized %ld/%ld",
                     (long) tloc, (long) trem);
            SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Clocks were off by %ld", (long) tloc - (long) trem);
            StatFile(conn, sendbuffer, filename);
        }

        return true;

    case PROTOCOL_COMMAND_MD5_SECURE:
        sscanf(recvbuffer, "SMD5 %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decryption error");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(recvbuffer, sizeof(recvbuffer),
                                 out, len,
                                 conn->encryption_type, conn->session_key);

        if (strncmp(recvbuffer, "MD5", 3) != 0)
        {
            Log(LOG_LEVEL_INFO, "MD5 protocol error");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        encrypted = true;
        /* roll through, no break */

    case PROTOCOL_COMMAND_MD5:

        memset(filename, 0, sizeof(filename));
        sscanf(recvbuffer, "MD5 %[^\n]", filename);

        zret = ShortcutsExpand(filename, sizeof(filename),
            SV.path_shortcuts,
            conn->ipaddr, conn->hostname,
            KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

        if (zret == (size_t) -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Expanding filename (%s) made it too long (>= %zu)", filename, sizeof(filename));
            return false;
        }

        if (!AccessControl(ctx, filename, conn, encrypted))
        {
            Log(LOG_LEVEL_INFO, "Access denied to get object");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        assert(CF_DEFAULT_DIGEST_LEN <= EVP_MAX_MD_SIZE);
        unsigned char digest[EVP_MAX_MD_SIZE + 1];

        assert(CF_BUFSIZE + CF_SMALL_OFFSET + CF_DEFAULT_DIGEST_LEN
               <= sizeof(recvbuffer));
        memcpy(digest, recvbuffer + strlen(recvbuffer) + CF_SMALL_OFFSET,
               CF_DEFAULT_DIGEST_LEN);

        CompareLocalHash(filename, digest, sendbuffer);
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);

        return true;

    case PROTOCOL_COMMAND_VAR_SECURE:
        sscanf(recvbuffer, "SVAR %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error SVAR");
            RefuseAccess(conn, "decrypt error SVAR");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(recvbuffer, sizeof(recvbuffer),
                                 out, len,
                                 conn->encryption_type, conn->session_key);
        encrypted = true;

        if (strncmp(recvbuffer, "VAR", 3) != 0)
        {
            Log(LOG_LEVEL_INFO, "VAR protocol defect");
            RefuseAccess(conn, "decryption failure");
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_VAR:
        if (!LiteralAccessControl(ctx, recvbuffer, conn, encrypted))
        {
            Log(LOG_LEVEL_INFO, "Literal access failure");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        GetServerLiteral(ctx, conn, sendbuffer, recvbuffer, encrypted);
        return true;

    case PROTOCOL_COMMAND_CONTEXT_SECURE:
        sscanf(recvbuffer, "SCONTEXT %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error SCONTEXT, len,received = %d,%d", len, received);
            RefuseAccess(conn, "decrypt error SCONTEXT");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(recvbuffer, sizeof(recvbuffer),
                                 out, len,
                                 conn->encryption_type, conn->session_key);
        encrypted = true;

        if (strncmp(recvbuffer, "CONTEXT", 7) != 0)
        {
            Log(LOG_LEVEL_INFO, "CONTEXT protocol defect...");
            RefuseAccess(conn, "Decryption failed?");
            return false;
        }

        /* roll through, no break */

    case PROTOCOL_COMMAND_CONTEXT:
        if ((classes = ContextAccessControl(ctx, recvbuffer, conn, encrypted)) == NULL)
        {
            Log(LOG_LEVEL_INFO, "Context access failure on %s", recvbuffer);
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        ReplyServerContext(conn, encrypted, classes);
        return true;

    case PROTOCOL_COMMAND_QUERY_SECURE:
        sscanf(recvbuffer, "SQUERY %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error SQUERY");
            RefuseAccess(conn, "decrypt error SQUERY");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(recvbuffer, sizeof(recvbuffer),
                                 out, len,
                                 conn->encryption_type, conn->session_key);

        if (strncmp(recvbuffer, "QUERY", 5) != 0)
        {
            Log(LOG_LEVEL_INFO, "QUERY protocol defect");
            RefuseAccess(conn, "decryption failure");
            return false;
        }

        if (!LiteralAccessControl(ctx, recvbuffer, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Query access failure");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        if (GetServerQuery(conn, recvbuffer, true))       /* always encrypt */
        {
            return true;
        }

        break;

    case PROTOCOL_COMMAND_CALL_ME_BACK:
        sscanf(recvbuffer, "SCALLBACK %u", &len);

        if ((len >= sizeof(out)) || (received != (len + CF_PROTO_OFFSET)))
        {
            Log(LOG_LEVEL_INFO, "Decrypt error CALL_ME_BACK");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(recvbuffer, sizeof(recvbuffer),
                                 out, len,
                                 conn->encryption_type, conn->session_key);

        if (strncmp(recvbuffer, "CALL_ME_BACK collect_calls", strlen("CALL_ME_BACK collect_calls")) != 0)
        {
            Log(LOG_LEVEL_INFO, "CALL_ME_BACK protocol defect");
            return false;
        }

        if (!LiteralAccessControl(ctx, recvbuffer, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Query access failure");
            return false;
        }

        ReceiveCollectCall(conn);
        /* On success that returned true; otherwise, it did all
         * relevant Log()ging.  Either way, we're no longer busy with
         * it and our caller can close the connection: */
        return false;

    case PROTOCOL_COMMAND_AUTH_PLAIN:
    case PROTOCOL_COMMAND_AUTH_SECURE:
    case PROTOCOL_COMMAND_AUTH:
    case PROTOCOL_COMMAND_CONTEXTS:
    case PROTOCOL_COMMAND_BAD:
        Log(LOG_LEVEL_WARNING, "Unexpected protocol command");
    }

    strcpy(sendbuffer, "BAD: Request denied");
    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO, "Closing connection due to request: %s", recvbuffer);
    return false;
}
