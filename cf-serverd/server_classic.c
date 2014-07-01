#include <platform.h>

#include <cf3.defs.h>
#include <item_lib.h>                 /* IsMatchItemIn */
#include <matching.h>                 /* IsRegexItemIn */
#include <net.h>                      /* ReceiveTransaction,SendTransaction */
#include <signals.h>
#include <string_lib.h>                               /* ToLowerStrInplace */
#include <lastseen.h>                                 /* LastSaw1 */
#include <files_hashes.h>                             /* HashString */
#include <crypto.h>                                   /* HavePublicKey */
#include <cf-serverd-enterprise-stubs.h>              /* ReceiveCollectCall */
#include <cf-windows-functions.h>

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
    CompressPath(res_path, req_path);
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
    else
    {
        Log(LOG_LEVEL_INFO, "Couldn't resolve (realpath: %s) filename: %s",
            GetErrorStr(), translated_req_path);
        return false;                /* can't continue without transrequest */
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
        int res = false;

        Log(LOG_LEVEL_DEBUG, "Examining rule in access list (%s,%s)", transrequest, ap->path);

        /* TODO MapName when constructing this list. */
        strlcpy(transpath, ap->path, CF_BUFSIZE);
        MapName(transpath);

        /* If everything is allowed */
        if ((strcmp(transpath, FILE_SEPARATOR_STR) == 0)
            ||
            /* or if transpath is a parent directory of transrequest */
            (strlen(transrequest) > strlen(transpath)
            && strncmp(transpath, transrequest, strlen(transpath)) == 0
            && transrequest[strlen(transpath)] == FILE_SEPARATOR)
            ||
            /* or if it's an exact match */
            (strcmp(transpath, transrequest) == 0))
        {
            res = true;
        }

        /* Exact match means single file to admit */
        if (strcmp(transpath, transrequest) == 0)
        {
            res = true;
        }

        if (res)
        {
            Log(LOG_LEVEL_VERBOSE, "Found a matching rule in access list (%s in %s)", transrequest, transpath);

            if (stat(transpath, &statbuf) == -1)
            {
                Log(LOG_LEVEL_INFO,
                      "Warning cannot stat file object %s in admit/grant, or access list refers to dangling link",
                      transpath);
                continue;
            }

            if ((!encrypt) && (ap->encrypt == true))
            {
                Log(LOG_LEVEL_ERR, "File %s requires encrypt connection...will not serve", transpath);
                access = false;
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Checking whether to map root privileges..");

                if ((IsMatchItemIn(ap->maproot, conn->ipaddr)) ||
                    (IsRegexItemIn(ctx, ap->maproot, conn->hostname)))
                {
                    conn->maproot = true;
                    Log(LOG_LEVEL_VERBOSE, "Mapping root privileges to access non-root files");
                }

                if ((IsMatchItemIn(ap->accesslist, conn->ipaddr))
                    || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
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

        /* If everything is denied */
        if ((strcmp(transpath, FILE_SEPARATOR_STR) == 0)
            ||
            /* or if transpath is a parent directory of transrequest */
            (strlen(transrequest) > strlen(transpath) &&
             strncmp(transpath, transrequest, strlen(transpath)) == 0 &&
             transrequest[strlen(transpath)] == FILE_SEPARATOR)
            ||
            /* or if it's an exact match */
            (strcmp(transpath, transrequest) == 0))
        {
            if ((IsMatchItemIn(dp->accesslist, conn->ipaddr)) ||
                (IsRegexItemIn(ctx, dp->accesslist, conn->hostname)))
            {
                access = false;
                Log(LOG_LEVEL_INFO, "Host '%s' in deny list, explicitly denying access to '%s'",
                    conn->ipaddr, transrequest);
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

            if ((!encrypt) && (ap->encrypt == true))
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
                    Log(LOG_LEVEL_VERBOSE, "Found a matching rule in access list (%s in %s)", ip->name, ap->path);

                    if (ap->classpattern == false)
                    {
                        Log(LOG_LEVEL_ERR,
                            "Context %s requires a literal server item...cannot set variable directly by path",
                            ap->path);
                        access = false;
                        continue;
                    }

                    if ((!encrypt) && (ap->encrypt == true))
                    {
                        Log(LOG_LEVEL_ERR, "Context %s requires encrypt connection...will not serve", ip->name);
                        access = false;
                        break;
                    }
                    else
                    {
                        Log(LOG_LEVEL_DEBUG, "Checking whether to map root privileges");

                        if ((IsMatchItemIn(ap->maproot, conn->ipaddr))
                            || (IsRegexItemIn(ctx, ap->maproot, conn->hostname)))
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
                if (strcmp(ap->path, ip->name) == 0)
                {
                    if ((IsMatchItemIn(ap->accesslist, conn->ipaddr))
                        || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
                    {
                        access = false;
                        Log(LOG_LEVEL_VERBOSE, "Host %s explicitly denied access to context %s", conn->hostname, ip->name);
                        break;
                    }
                }
            }

            if (access)
            {
                Log(LOG_LEVEL_VERBOSE, "Host %s granted access to context '%s'", conn->hostname, ip->name);
                AppendItem(&matches, ip->name, NULL);

                if (encrypt && LOGENCRYPT)
                {
                    /* Log files that were marked as requiring encryption */
                    Log(LOG_LEVEL_INFO, "Host %s granted access to context '%s'", conn->hostname, ip->name);
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Host %s denied access to context '%s'", conn->hostname, ip->name);
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

    Log(LOG_LEVEL_DEBUG, "(ipstring=[%s],fqname=[%s],username=[%s],socket=[%s])",
            ipstring, fqname, username, conn->ipaddr);

    ToLowerStrInplace(fqname);

    strlcpy(conn->hostname, fqname, CF_MAXVARSIZE);
    strlcpy(conn->username, username, CF_MAXVARSIZE);

#ifdef __MINGW32__                   /* NT uses security identifier instead of uid */
    if (!NovaWin_UserNameToSid(username, (SID *) conn->sid,
                               CF_MAXSIDSIZE, false))
    {
        memset(conn->sid, 0, CF_MAXSIDSIZE);    /* is invalid sid - discarded */
    }

#else  /* !__MINGW32__ */
    struct passwd *pw;
    if ((pw = getpwnam(username)) == NULL)      /* Keep this inside mutex */
    {
        conn->uid = -2;
    }
    else
    {
        conn->uid = pw->pw_uid;
    }
#endif  /* !__MINGW32__ */
}

static int CheckStoreKey(ServerConnectionState *conn, RSA *key)
{
    RSA *savedkey;

    const char *udigest = KeyPrintableHash(ConnectionInfoKey(conn->conn_info));
    assert(udigest != NULL);

    if ((savedkey = HavePublicKey(conn->username, conn->ipaddr, udigest)))
    {
        Log(LOG_LEVEL_VERBOSE, "A public key was already known from %s/%s - no trust required", conn->hostname,
              conn->ipaddr);

        if ((BN_cmp(savedkey->e, key->e) == 0) && (BN_cmp(savedkey->n, key->n) == 0))
        {
            Log(LOG_LEVEL_VERBOSE, "The public key identity was confirmed as %s@%s", conn->username, conn->hostname);
            SendTransaction(conn->conn_info, "OK: key accepted", 0, CF_DONE);
            RSA_free(savedkey);
            return true;
        }
    }

    /* Finally, if we're still here then the key is new (not in ppkeys
     * directory): Allow access only if host is listed in "trustkeysfrom" body
     * server control option. */

    if ((SV.trustkeylist != NULL) && (IsMatchItemIn(SV.trustkeylist, conn->ipaddr)))
    {
        Log(LOG_LEVEL_VERBOSE, "Host %s/%s was found in the list of hosts to trust", conn->hostname, conn->ipaddr);
        SendTransaction(conn->conn_info, "OK: unknown key was accepted on trust", 0, CF_DONE);
        SavePublicKey(conn->username, udigest, key);
        return true;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "No previous key found, and unable to accept this one on trust");
        SendTransaction(conn->conn_info, "BAD: key could not be accepted on trust", 0, CF_DONE);
        return false;
    }
}

static int AuthenticationDialogue(ServerConnectionState *conn, char *recvbuffer, int recvlen)
{
    char in[CF_BUFSIZE], *out, *decrypted_nonce;
    BIGNUM *counter_challenge = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    unsigned int crypt_len, nonce_len = 0, encrypted_len = 0;
    char sauth[10], iscrypt = 'n', enterprise_field = 'c';
    int len_n = 0, len_e = 0, keylen, session_size;
    RSA *newkey;
    int digestLen = 0;
    HashMethod digestType;

    if ((PRIVKEY == NULL) || (PUBKEY == NULL))
    {
        Log(LOG_LEVEL_ERR, "No public/private key pair exists, create one with cf-key");
        return false;
    }

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

/* proposition C1 */
/* Opening string is a challenge from the client (some agent) */

    sauth[0] = '\0';

    sscanf(recvbuffer, "%s %c %u %u %c", sauth, &iscrypt, &crypt_len, &nonce_len, &enterprise_field);

    if ((crypt_len == 0) || (nonce_len == 0) || (strlen(sauth) == 0))
    {
        Log(LOG_LEVEL_INFO, "Protocol format error in authentation from IP %s", conn->hostname);
        return false;
    }

    if (nonce_len > CF_NONCELEN * 2)
    {
        Log(LOG_LEVEL_INFO, "Protocol deviant authentication nonce from %s", conn->hostname);
        return false;
    }

    if (crypt_len > 2 * CF_NONCELEN)
    {
        Log(LOG_LEVEL_INFO, "Protocol abuse in unlikely cipher from %s", conn->hostname);
        return false;
    }

/* Check there is no attempt to read past the end of the received input */

    if (recvbuffer + CF_RSA_PROTO_OFFSET + nonce_len > recvbuffer + recvlen)
    {
        Log(LOG_LEVEL_INFO, "Protocol consistency error in authentication from %s", conn->hostname);
        return false;
    }

    if ((strcmp(sauth, "SAUTH") != 0) || (nonce_len == 0) || (crypt_len == 0))
    {
        Log(LOG_LEVEL_INFO, "Protocol error in RSA authentication from IP '%s'", conn->hostname);
        return false;
    }

    Log(LOG_LEVEL_DEBUG, "Challenge encryption = %c, nonce = %d, buf = %d", iscrypt, nonce_len, crypt_len);


    decrypted_nonce = xmalloc(crypt_len);

    if (iscrypt == 'y')
    {
        if (RSA_private_decrypt(crypt_len, recvbuffer + CF_RSA_PROTO_OFFSET,
                                decrypted_nonce, PRIVKEY, RSA_PKCS1_PADDING)
            <= 0)
        {
            Log(LOG_LEVEL_ERR,
                "Private decrypt failed = '%s'. Probably the client has the wrong public key for this server",
                CryptoLastErrorString());
            free(decrypted_nonce);
            return false;
        }
    }
    else
    {
        if (nonce_len > crypt_len)
        {
            Log(LOG_LEVEL_ERR, "Illegal challenge");
            free(decrypted_nonce);
            return false;
        }

        memcpy(decrypted_nonce, recvbuffer + CF_RSA_PROTO_OFFSET, nonce_len);
    }

/* Client's ID is now established by key or trusted, reply with digest */

    HashString(decrypted_nonce, nonce_len, digest, digestType);

    free(decrypted_nonce);

/* Get the public key from the client */
    newkey = RSA_new();

/* proposition C2 */
    if ((len_n = ReceiveTransaction(conn->conn_info, recvbuffer, NULL)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 1 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len_n == 0)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 2 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->n = BN_mpi2bn(recvbuffer, len_n, NULL)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Private decrypt failed = %s",
            CryptoLastErrorString());
        RSA_free(newkey);
        return false;
    }

/* proposition C3 */

    if ((len_e = ReceiveTransaction(conn->conn_info, recvbuffer, NULL)) == -1)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 3 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len_e == 0)
    {
        Log(LOG_LEVEL_INFO, "Protocol error 4 in RSA authentation from IP %s", conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->e = BN_mpi2bn(recvbuffer, len_e, NULL)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Private decrypt failed = %s",
            CryptoLastErrorString());
        RSA_free(newkey);
        return false;
    }

    /* Compute and store hash of the client's public key. */
    Key *key = KeyNew(newkey, CF_DEFAULT_DIGEST);
    ConnectionInfoSetKey(conn->conn_info, key);
    Log(LOG_LEVEL_VERBOSE, "Public key identity of host '%s' is '%s'",
        conn->ipaddr, KeyPrintableHash(ConnectionInfoKey(conn->conn_info)));

    LastSaw1(conn->ipaddr, KeyPrintableHash(ConnectionInfoKey(conn->conn_info)),
             LAST_SEEN_ROLE_ACCEPT);

    if (!CheckStoreKey(conn, newkey))   /* conceals proposition S1 */
    {
        return false;
    }

/* Reply with digest of original challenge */

/* proposition S2 */

    SendTransaction(conn->conn_info, digest, digestLen, CF_DONE);

/* Send counter challenge to be sure this is a live session */

    counter_challenge = BN_new();
    if (counter_challenge == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot allocate BIGNUM structure for counter challenge");
        return false;
    }

    BN_rand(counter_challenge, CF_NONCELEN, 0, 0);
    nonce_len = BN_bn2mpi(counter_challenge, in);

// hash the challenge from the client

    HashString(in, nonce_len, digest, digestType);

    encrypted_len = RSA_size(newkey);   /* encryption buffer is always the same size as n */

    out = xmalloc(encrypted_len + 1);

    if (RSA_public_encrypt(nonce_len, in, out, newkey, RSA_PKCS1_PADDING) <= 0)
    {
        Log(LOG_LEVEL_ERR, "Public encryption failed = %s",
            CryptoLastErrorString());
        free(out);
        return false;
    }

/* proposition S3 */
    SendTransaction(conn->conn_info, out, encrypted_len, CF_DONE);

/* if the client doesn't have our public key, send it */

    if (iscrypt != 'y')
    {
        /* proposition S4  - conditional */
        memset(in, 0, CF_BUFSIZE);
        len_n = BN_bn2mpi(PUBKEY->n, in);
        SendTransaction(conn->conn_info, in, len_n, CF_DONE);

        /* proposition S5  - conditional */
        memset(in, 0, CF_BUFSIZE);
        len_e = BN_bn2mpi(PUBKEY->e, in);
        SendTransaction(conn->conn_info, in, len_e, CF_DONE);
    }

/* Receive reply to counter_challenge */

/* proposition C4 */
    memset(in, 0, CF_BUFSIZE);

    if (ReceiveTransaction(conn->conn_info, in, NULL) == -1)
    {
        BN_free(counter_challenge);
        free(out);
        return false;
    }

    if (HashesMatch(digest, in, digestType))    /* replay / piggy in the middle attack ? */
    {
        Log(LOG_LEVEL_VERBOSE, "Authentication of client %s/%s achieved", conn->hostname, conn->ipaddr);
    }
    else
    {
        BN_free(counter_challenge);
        free(out);
        Log(LOG_LEVEL_INFO, "Challenge response from client %s was incorrect - ID false?", conn->ipaddr);
        return false;
    }

/* Receive random session key,... */

/* proposition C5 */

    memset(in, 0, CF_BUFSIZE);

    if ((keylen = ReceiveTransaction(conn->conn_info, in, NULL)) == -1)
    {
        BN_free(counter_challenge);
        free(out);
        return false;
    }

    if (keylen > CF_BUFSIZE / 2)
    {
        BN_free(counter_challenge);
        free(out);
        Log(LOG_LEVEL_INFO, "Session key length received from %s is too long", conn->ipaddr);
        return false;
    }

    session_size = CfSessionKeySize(enterprise_field);
    conn->session_key = xmalloc(session_size);
    conn->encryption_type = enterprise_field;

    Log(LOG_LEVEL_VERBOSE, "Receiving session key from client (size=%d)...", keylen);

    Log(LOG_LEVEL_DEBUG, "keylen = %d, session_size = %d", keylen, session_size);

    if (keylen == CF_BLOWFISHSIZE)      /* Support the old non-ecnrypted for upgrade */
    {
        memcpy(conn->session_key, in, session_size);
    }
    else
    {
        /* New protocol encrypted */

        if (RSA_private_decrypt(keylen, in, out, PRIVKEY, RSA_PKCS1_PADDING) <= 0)
        {
            Log(LOG_LEVEL_ERR, "Private decrypt failed = %s",
                CryptoLastErrorString());
            BN_free(counter_challenge);
            free(out);
            return false;
        }

        memcpy(conn->session_key, out, session_size);
    }

    BN_free(counter_challenge);
    free(out);
    return true;
}



int BusyWithClassicConnection(EvalContext *ctx, ServerConnectionState *conn)
{
    time_t tloc, trem = 0;
    char recvbuffer[CF_BUFSIZE + CF_BUFEXT], check[CF_BUFSIZE];
    char sendbuffer[CF_BUFSIZE] = { 0 };
    char filename[CF_BUFSIZE], buffer[CF_BUFSIZE], args[CF_BUFSIZE], out[CF_BUFSIZE];
    long time_no_see = 0;
    unsigned int len = 0;
    int drift, plainlen, received, encrypted = 0;
    ServerFileGetState get_args;
    Item *classes;

    memset(recvbuffer, 0, CF_BUFSIZE + CF_BUFEXT);
    memset(&get_args, 0, sizeof(get_args));

    received = ReceiveTransaction(conn->conn_info, recvbuffer, NULL);
    if (received == -1 || received == 0)
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
        return false;
    }

    ProtocolCommandClassic command = GetCommandClassic(recvbuffer);

    switch (command)
    {
    /* Plain text authentication; this MUST be the first command client
       using classic protocol is sending. */
    case PROTOCOL_COMMAND_AUTH_PLAIN:
        SetConnectionData(conn, (char *) (recvbuffer + strlen("CAUTH ")));

        if (conn->username == NULL || IsUserNameValid(conn->username) == false)
        {
            Log(LOG_LEVEL_INFO, "Client is sending wrong username: '%s'", conn->username);
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        /* This is used only for forcing correct state of state machine while
           connecting and authenticating user using classic protocol. */
        conn->user_data_set = true;

        return true;

    /* This MUST be exactly second command client using classic protocol is sending.
       This is where key agreement takes place. */
    case PROTOCOL_COMMAND_AUTH_SECURE:
        /* First command was ommited by client; this is protocol violation. */
        if (!conn->user_data_set)
        {
            Log(LOG_LEVEL_INFO, "Client is not verified; rejecting connection");
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

    /* At this point we should have both user_data_set and rsa_auth set to perform any operation.
       We can check only for second one as without first it won't be set up. */
    if (!conn->rsa_auth)
    {
        Log(LOG_LEVEL_INFO, "Server refusal due to no RSA authentication [command: %d]", command);
        RefuseAccess(conn, recvbuffer);
        return false;
    }

    /* We have to have key at this point. */
    assert(conn->session_key);

    /* At this point we can safely do next switch and make sure user is authenticated. */
    switch (command)
    {
    case PROTOCOL_COMMAND_EXEC:
        memset(args, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "EXEC %255[^\n]", args);

        if (!AllowedUser(conn->username))
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to non-allowed user");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        if (!AccessControl(ctx, CommandArg0(CFRUNCOMMAND), conn, false))
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to denied access to requested object");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        if (!MatchClasses(ctx, conn))
        {
            Log(LOG_LEVEL_INFO, "Server refusal due to failed class/context match");
            Terminate(conn->conn_info);
            return false;
        }

        DoExec(ctx, conn, args);
        Terminate(conn->conn_info);
        return false;

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

        get_args.connect = conn;
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
            Log(LOG_LEVEL_VERBOSE, "Protocol error SGET");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        plainlen = DecryptString(conn->encryption_type, recvbuffer + CF_PROTO_OFFSET, buffer, conn->session_key, len);

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

        Log(LOG_LEVEL_DEBUG, "Confirm decryption, and thus validity of caller");
        Log(LOG_LEVEL_DEBUG, "SGET '%s' with blocksize %d", filename, get_args.buf_size);

        if (!AccessControl(ctx, filename, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Access control error");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        memset(sendbuffer, 0, sizeof(sendbuffer));

        get_args.connect = conn;
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
            Log(LOG_LEVEL_VERBOSE, "Protocol error OPENDIR: %d", len);
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "OPENDIR", 7) != 0)
        {
            Log(LOG_LEVEL_INFO, "Opendir failed to decrypt");
            RefuseAccess(conn, recvbuffer);
            return true;
        }

        memset(filename, 0, CF_BUFSIZE);
        sscanf(recvbuffer, "OPENDIR %[^\n]", filename);

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

        if (!AccessControl(ctx, filename, conn, true))        /* opendir don't care about privacy */
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
            Log(LOG_LEVEL_VERBOSE, "Protocol error SSYNCH: %d", len);
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);

        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

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

        if ((time_no_see == 0) || (filename[0] == '\0'))
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
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

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
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);
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
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);
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
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

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
            RefuseAccess(conn, "decrypt error CALL_ME_BACK");
            return true;
        }

        memcpy(out, recvbuffer + CF_PROTO_OFFSET, len);
        plainlen = DecryptString(conn->encryption_type, out, recvbuffer, conn->session_key, len);

        if (strncmp(recvbuffer, "CALL_ME_BACK collect_calls", strlen("CALL_ME_BACK collect_calls")) != 0)
        {
            Log(LOG_LEVEL_INFO, "CALL_ME_BACK protocol defect");
            RefuseAccess(conn, "decryption failure");
            return false;
        }

        if (!LiteralAccessControl(ctx, recvbuffer, conn, true))
        {
            Log(LOG_LEVEL_INFO, "Query access failure");
            RefuseAccess(conn, recvbuffer);
            return false;
        }

        if (ReceiveCollectCall(conn))
        {
            return true;
        }

    case PROTOCOL_COMMAND_AUTH_PLAIN:
    case PROTOCOL_COMMAND_AUTH_SECURE:
    case PROTOCOL_COMMAND_AUTH:
    case PROTOCOL_COMMAND_CONTEXTS:
    case PROTOCOL_COMMAND_BAD:
        Log(LOG_LEVEL_WARNING, "Unexpected protocol command");
    }

    strcpy(sendbuffer, "BAD: Request denied");
    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    Log(LOG_LEVEL_INFO, "Closing connection, due to request: '%s'", recvbuffer);
    return false;
}

