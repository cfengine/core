static const int CF_NOSIZE = -1;


#include "server_common.h"

#include "item_lib.h"
#include "crypto.h"                                        /* EncryptString */
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "env_context.h"
#include "dir.h"
#include "conversion.h"
#include "matching.h"                        /* IsRegexItemIn,FullTextMatch */
#include "pipes.h"
#include "classic.h"                  /* SendSocketStream */
#include "net.h"                      /* SendTransaction,ReceiveTransaction */
#include "tls_generic.h"              /* TLSSend */
#include "rlist.h"
#include "cf-serverd-enterprise-stubs.h"

#ifdef HAVE_NOVA
# include "cf.nova.h"
#endif


void RefuseAccess(ServerConnectionState *conn, int size, char *errmesg)
{
    char *hostname, *username, *ipaddr;
    char *def = "?";

    if (strlen(conn->hostname) == 0)
    {
        hostname = def;
    }
    else
    {
        hostname = conn->hostname;
    }

    if (strlen(conn->username) == 0)
    {
        username = def;
    }
    else
    {
        username = conn->username;
    }

    if (strlen(conn->ipaddr) == 0)
    {
        ipaddr = def;
    }
    else
    {
        ipaddr = conn->ipaddr;
    }

    char sendbuffer[CF_BUFSIZE];
    snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
    SendTransaction(&conn->conn_info, sendbuffer, size, CF_DONE);

    Log(LOG_LEVEL_INFO, "From (host=%s,user=%s,ip=%s)", hostname, username, ipaddr);

    if (strlen(errmesg) > 0)
    {
        if (SV.logconns)
        {
            Log(LOG_LEVEL_INFO, "REFUSAL of request from connecting host: (%s)", errmesg);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "REFUSAL of request from connecting host: (%s)", errmesg);
        }
    }
}

int AllowedUser(char *user)
{
    if (IsItemIn(SV.allowuserlist, user))
    {
        Log(LOG_LEVEL_VERBOSE, "User %s granted connection privileges", user);
        return true;
    }

    Log(LOG_LEVEL_VERBOSE, "User %s is not allowed on this server", user);
    return false;
}

/* 'resolved' argument needs to be at least CF_BUFSIZE long */
bool ResolveFilename(const char *req_path, char *res_path)
{
    char req_dir[CF_BUFSIZE];
    char req_filename[CF_BUFSIZE];

/*
 * Eliminate symlinks from path, but do not resolve the file itself if it is a
 * symlink.
 */

    strlcpy(req_dir, req_path, CF_BUFSIZE);
    ChopLastNode(req_dir);

    strlcpy(req_filename, ReadLastNode(req_path), CF_BUFSIZE);

#if defined HAVE_REALPATH && !defined _WIN32
    if (realpath(req_dir, res_path) == NULL)
    {
        return false;
    }
#else
    memset(res_path, 0, CF_BUFSIZE);
    CompressPath(res_path, req_dir);
#endif

    AddSlash(res_path);
    strlcat(res_path, req_filename, CF_BUFSIZE);

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

int AccessControl(EvalContext *ctx, const char *req_path, ServerConnectionState *conn, int encrypt)
{
    Auth *ap;
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
        Log(LOG_LEVEL_VERBOSE, "Couldn't resolve filename '%s' from host '%s'. (lstat: %s)",
            translated_req_path, conn->hostname, GetErrorStr());
    }

    if (lstat(transrequest, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Couldn't stat filename '%s' requested by host '%s'. (lstat: %s)",
            transrequest, conn->hostname, GetErrorStr());
        return false;
    }

    Log(LOG_LEVEL_DEBUG, "AccessControl, match (%s,%s) encrypt request = %d", transrequest, conn->hostname, encrypt);

    if (SV.admit == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "cf-serverd access list is empty, no files are visible");
        return false;
    }

    conn->maproot = false;

    for (ap = SV.admit; ap != NULL; ap = ap->next)
    {
        int res = false;

        Log(LOG_LEVEL_DEBUG, "Examining rule in access list (%s,%s)", transrequest, ap->path);

        strncpy(transpath, ap->path, CF_BUFSIZE - 1);
        MapName(transpath);

        if ((strlen(transrequest) > strlen(transpath)) && (strncmp(transpath, transrequest, strlen(transpath)) == 0)
            && (transrequest[strlen(transpath)] == FILE_SEPARATOR))
        {
            res = true;         /* Substring means must be a / to link, else just a substring og filename */
        }

        /* Exact match means single file to admit */

        if (strcmp(transpath, transrequest) == 0)
        {
            res = true;
        }

        if (strcmp(transpath, "/") == 0)
        {
            res = true;
        }

        if (res)
        {
            Log(LOG_LEVEL_VERBOSE, "Found a matching rule in access list (%s in %s)", transrequest, transpath);

            if (stat(transpath, &statbuf) == -1)
            {
                Log(LOG_LEVEL_INFO,
                      "Warning cannot stat file object %s in admit/grant, or access list refers to dangling link\n",
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

                if ((IsMatchItemIn(ap->maproot, MapAddress(conn->ipaddr))) || (IsRegexItemIn(ctx, ap->maproot, conn->hostname)))
                {
                    conn->maproot = true;
                    Log(LOG_LEVEL_VERBOSE, "Mapping root privileges to access non-root files");
                }

                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
                    || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
                {
                    access = true;
                    Log(LOG_LEVEL_DEBUG, "Access privileges - match found");
                }
            }
            break;
        }
    }

    if (strncmp(transpath, transrequest, strlen(transpath)) == 0)
    {
        for (ap = SV.deny; ap != NULL; ap = ap->next)
        {
            if (IsRegexItemIn(ctx, ap->accesslist, conn->hostname))
            {
                access = false;
                Log(LOG_LEVEL_VERBOSE, "Host %s explicitly denied access to %s", conn->hostname, transrequest);
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
        Log(LOG_LEVEL_VERBOSE, "Host %s denied access to %s", conn->hostname, req_path);
    }

    if (!conn->rsa_auth)
    {
        Log(LOG_LEVEL_VERBOSE, "Cannot map root access without RSA authentication");
        conn->maproot = false;  /* only public files accessible */
    }

    return access;
}

int LiteralAccessControl(EvalContext *ctx, char *in, ServerConnectionState *conn, int encrypt)
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
        int res = false;

        Log(LOG_LEVEL_VERBOSE, "Examining rule in access list (%s,%s)?", name, ap->path);

        if (strcmp(ap->path, name) == 0)
        {
            res = true;         /* Exact match means single file to admit */
        }

        if (res)
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

                if ((IsMatchItemIn(ap->maproot, MapAddress(conn->ipaddr))) || (IsRegexItemIn(ctx, ap->maproot, conn->hostname)))
                {
                    conn->maproot = true;
                    Log(LOG_LEVEL_VERBOSE, "Mapping root privileges");
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "No root privileges granted");
                }

                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
                    || (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)))
                {
                    access = true;
                    Log(LOG_LEVEL_DEBUG, "Access privileges - match found\n");
                }
            }
        }
    }

    for (ap = SV.vardeny; ap != NULL; ap = ap->next)
    {
        if (strcmp(ap->path, name) == 0)
        {
            if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
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

    if (!conn->rsa_auth)
    {
        Log(LOG_LEVEL_VERBOSE, "Cannot map root access without RSA authentication");
        conn->maproot = false;  /* only public files accessible */
    }

    return access;
}

Item *ContextAccessControl(EvalContext *ctx, char *in, ServerConnectionState *conn, int encrypt)
{
    Auth *ap;
    int access = false;
    char client_regex[CF_BUFSIZE];
    CF_DB *dbp;
    CF_DBC *dbcp;
    int ksize, vsize;
    char *key;
    void *value;
    time_t now = time(NULL);
    CfState q;
    Item *ip, *matches = NULL, *candidates = NULL;

    sscanf(in, "CONTEXT %255[^\n]", client_regex);

    if (!OpenDB(&dbp, dbid_state))
    {
        return NULL;
    }

    if (!NewDBCursor(dbp, &dbcp))
    {
        Log(LOG_LEVEL_INFO, "Unable to scan persistence cache");
        CloseDB(dbp);
        return NULL;
    }

    while (NextDB(dbcp, &key, &ksize, &value, &vsize))
    {
        memcpy((void *) &q, value, sizeof(CfState));

        if (now > q.expires)
        {
            Log(LOG_LEVEL_VERBOSE, " Persistent class %s expired", key);
            DBCursorDeleteEntry(dbcp);
        }
        else
        {
            if (FullTextMatch(client_regex, key))
            {
                Log(LOG_LEVEL_VERBOSE, " - Found key %s...", key);
                AppendItem(&candidates, key, NULL);
            }
        }
    }

    DeleteDBCursor(dbcp);
    CloseDB(dbp);

    for (ip = candidates; ip != NULL; ip = ip->next)
    {
        for (ap = SV.varadmit; ap != NULL; ap = ap->next)
        {
            int res = false;

            if (FullTextMatch(ap->path, ip->name))
            {
                res = true;
            }

            if (res)
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

                    if ((IsMatchItemIn(ap->maproot, MapAddress(conn->ipaddr)))
                        || (IsRegexItemIn(ctx, ap->maproot, conn->hostname)))
                    {
                        conn->maproot = true;
                        Log(LOG_LEVEL_VERBOSE, "Mapping root privileges");
                    }
                    else
                    {
                        Log(LOG_LEVEL_VERBOSE, "No root privileges granted");
                    }

                    if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
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
                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr)))
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

    DeleteItemList(candidates);
    return matches;
}


static void ReplyNothing(ServerConnectionState *conn)
{
    char buffer[CF_BUFSIZE];

    snprintf(buffer, CF_BUFSIZE, "Hello %s (%s), nothing relevant to do here...\n\n", conn->hostname, conn->ipaddr);

    if (SendTransaction(&conn->conn_info, buffer, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to send transaction. (send: %s)", GetErrorStr());
    }
}

int MatchClasses(EvalContext *ctx, ServerConnectionState *conn)
{
    char recvbuffer[CF_BUFSIZE];
    Item *classlist = NULL, *ip;
    int count = 0;

    while (true && (count < 10))        /* arbitrary check to avoid infinite loop, DoS attack */
    {
        count++;

        if (ReceiveTransaction(&conn->conn_info, recvbuffer, NULL) == -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Unable to read data from network. (ReceiveTransaction: %s)", GetErrorStr());
            return false;
        }

        Log(LOG_LEVEL_DEBUG, "Got class buffer '%s'", recvbuffer);

        if (strncmp(recvbuffer, CFD_TERMINATOR, strlen(CFD_TERMINATOR)) == 0)
        {
            if (count == 1)
            {
                Log(LOG_LEVEL_DEBUG, "No classes were sent, assuming no restrictions...");
                return true;
            }

            break;
        }

        classlist = SplitStringAsItemList(recvbuffer, ' ');

        for (ip = classlist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "Checking whether class %s can be identified as me...", ip->name);

            if (IsDefinedClass(ctx, ip->name, NULL))
            {
                Log(LOG_LEVEL_DEBUG, "Class '%s' matched, accepting...", ip->name);
                DeleteItemList(classlist);
                return true;
            }

            if (EvalContextHeapMatchCountSoft(ctx, ip->name) > 0)
            {
                Log(LOG_LEVEL_DEBUG, "Class matched regular expression '%s', accepting...", ip->name);
                DeleteItemList(classlist);
                return true;
            }

            if (EvalContextHeapMatchCountHard(ctx, ip->name))
            {
                Log(LOG_LEVEL_DEBUG, "Class matched regular expression '%s', accepting...", ip->name);
                DeleteItemList(classlist);
                return true;
            }

            if (strncmp(ip->name, CFD_TERMINATOR, strlen(CFD_TERMINATOR)) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "No classes matched, rejecting....");
                ReplyNothing(conn);
                DeleteItemList(classlist);
                return false;
            }
        }
    }

    ReplyNothing(conn);
    Log(LOG_LEVEL_VERBOSE, "No classes matched, rejecting....");
    DeleteItemList(classlist);
    return false;
}

void Terminate(ConnectionInfo *connection)
{
    char buffer[CF_BUFSIZE];

    memset(buffer, 0, CF_BUFSIZE);

    strcpy(buffer, CFD_TERMINATOR);

    if (SendTransaction(connection, buffer, strlen(buffer) + 1, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Unable to reply with terminator. (send: %s)", GetErrorStr());
    }
}


static int AuthorizeRoles(EvalContext *ctx, ServerConnectionState *conn, char *args)
{
    char *sp;
    Auth *ap;
    char userid1[CF_MAXVARSIZE], userid2[CF_MAXVARSIZE];
    Rlist *rp, *defines = NULL;
    int permitted = false;

    snprintf(userid1, CF_MAXVARSIZE, "%s@%s", conn->username, conn->hostname);
    snprintf(userid2, CF_MAXVARSIZE, "%s@%s", conn->username, conn->ipaddr);

    Log(LOG_LEVEL_VERBOSE, "Checking authorized roles in %s", args);

    if (strncmp(args, "--define", strlen("--define")) == 0)
    {
        sp = args + strlen("--define");
    }
    else
    {
        sp = args + strlen("-D");
    }

    while (*sp == ' ')
    {
        sp++;
    }

    defines = RlistFromSplitRegex(sp, "[,:;]", 99, false);

/* For each user-defined class attempt, check RBAC */

    for (rp = defines; rp != NULL; rp = rp->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Verifying %s", RlistScalarValue(rp));

        for (ap = SV.roles; ap != NULL; ap = ap->next)
        {
            if (FullTextMatch(ap->path, rp->item))
            {
                /* We have a pattern covering this class - so are we allowed to activate it? */
                if ((IsMatchItemIn(ap->accesslist, MapAddress(conn->ipaddr))) ||
                    (IsRegexItemIn(ctx, ap->accesslist, conn->hostname)) ||
                    (IsRegexItemIn(ctx, ap->accesslist, userid1)) ||
                    (IsRegexItemIn(ctx, ap->accesslist, userid2)) ||
                    (IsRegexItemIn(ctx, ap->accesslist, conn->username)))
                {
                    Log(LOG_LEVEL_VERBOSE, "Attempt to define role/class %s is permitted", RlistScalarValue(rp));
                    permitted = true;
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Attempt to define role/class %s is denied", RlistScalarValue(rp));
                    RlistDestroy(defines);
                    return false;
                }
            }
        }

    }

    if (permitted)
    {
        Log(LOG_LEVEL_VERBOSE, "Role activation allowed");
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Role activation disallowed - abort execution");
    }

    RlistDestroy(defines);
    return permitted;
}

static int OptionFound(char *args, char *pos, char *word)
/*
 * Returns true if the current position 'pos' in buffer
 * 'args' corresponds to the word 'word'.  Words are
 * separated by spaces.
 */
{
    size_t len;

    if (pos < args)
    {
        return false;
    }

/* Single options do not have to have spaces between */

    if ((strlen(word) == 2) && (strncmp(pos, word, 2) == 0))
    {
        return true;
    }

    len = strlen(word);

    if (strncmp(pos, word, len) != 0)
    {
        return false;
    }

    if (pos == args)
    {
        return true;
    }
    else if ((*(pos - 1) == ' ') && ((pos[len] == ' ') || (pos[len] == '\0')))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void DoExec(EvalContext *ctx, ServerConnectionState *conn, char *args)
{
    char ebuff[CF_EXPANDSIZE], line[CF_BUFSIZE], *sp;
    int print = false, i;
    FILE *pp;

    if ((CFSTARTTIME = time((time_t *) NULL)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't read system clock. (time: %s)", GetErrorStr());
    }

    if (strlen(CFRUNCOMMAND) == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "cf-serverd exec request: no cfruncommand defined");
        char sendbuffer[CF_BUFSIZE];
        strlcpy(sendbuffer, "Exec request: no cfruncommand defined\n", CF_BUFSIZE);
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Examining command string '%s'", args);

    for (sp = args; *sp != '\0'; sp++)  /* Blank out -K -f */
    {
        if ((*sp == ';') || (*sp == '&') || (*sp == '|'))
        {
            char sendbuffer[CF_BUFSIZE];
            snprintf(sendbuffer, CF_BUFSIZE, "You are not authorized to activate these classes/roles on host %s\n", VFQNAME);
            SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
            return;
        }

        if ((OptionFound(args, sp, "-K")) || (OptionFound(args, sp, "-f")))
        {
            *sp = ' ';
            *(sp + 1) = ' ';
        }
        else if (OptionFound(args, sp, "--no-lock"))
        {
            for (i = 0; i < strlen("--no-lock"); i++)
            {
                *(sp + i) = ' ';
            }
        }
        else if (OptionFound(args, sp, "--file"))
        {
            for (i = 0; i < strlen("--file"); i++)
            {
                *(sp + i) = ' ';
            }
        }
        else if ((OptionFound(args, sp, "--define")) || (OptionFound(args, sp, "-D")))
        {
            Log(LOG_LEVEL_VERBOSE, "Attempt to activate a predefined role..");

            if (!AuthorizeRoles(ctx, conn, sp))
            {
                char sendbuffer[CF_BUFSIZE];
                snprintf(sendbuffer, CF_BUFSIZE, "You are not authorized to activate these classes/roles on host %s\n", VFQNAME);
                SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
                return;
            }
        }
    }

    snprintf(ebuff, CF_BUFSIZE, "%s --inform", CFRUNCOMMAND);

    if (strlen(ebuff) + strlen(args) + 6 > CF_BUFSIZE)
    {
        char sendbuffer[CF_BUFSIZE];
        snprintf(sendbuffer, CF_BUFSIZE, "Command line too long with args: %s\n", ebuff);
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return;
    }
    else
    {
        if ((args != NULL) && (strlen(args) > 0))
        {
            char sendbuffer[CF_BUFSIZE];
            strcat(ebuff, " ");
            strncat(ebuff, args, CF_BUFSIZE - strlen(ebuff));
            snprintf(sendbuffer, CF_BUFSIZE, "cf-serverd Executing %s\n", ebuff);
            SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        }
    }

    Log(LOG_LEVEL_INFO, "Executing command %s", ebuff);

    if ((pp = cf_popen_sh(ebuff, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe to command '%s'. (pipe: %s)", ebuff, GetErrorStr());
        char sendbuffer[CF_BUFSIZE];
        snprintf(sendbuffer, CF_BUFSIZE, "Unable to run %s\n", ebuff);
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return;
    }

    for (;;)
    {
        ssize_t res = CfReadLine(line, CF_BUFSIZE, pp);

        if (res == 0)
        {
            break;
        }

        if (res == -1)
        {
            fflush(pp); /* FIXME: is it necessary? */
            break;
        }

        print = false;

        for (sp = line; *sp != '\0'; sp++)
        {
            if (!isspace((int) *sp))
            {
                print = true;
                break;
            }
        }

        if (print)
        {
            char sendbuffer[CF_BUFSIZE];
            snprintf(sendbuffer, CF_BUFSIZE, "%s\n", line);
            if (SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE) == -1)
            {
                Log(LOG_LEVEL_ERR, "Sending failed, aborting. (send: %s)", GetErrorStr());
                break;
            }
        }
    }

    cf_pclose(pp);
}

static int TransferRights(char *filename, ServerFileGetState *args, struct stat *sb)
{
#ifdef __MINGW32__
    SECURITY_DESCRIPTOR *secDesc;
    SID *ownerSid;

    if (GetNamedSecurityInfo
        (filename, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, (PSID *) & ownerSid, NULL, NULL, NULL,
         (void **)&secDesc) == ERROR_SUCCESS)
    {
        if (IsValidSid((args->connect)->sid) && EqualSid(ownerSid, (args->connect)->sid))
        {
            Log(LOG_LEVEL_DEBUG, "Caller '%s' is the owner of the file", (args->connect)->username);
        }
        else
        {
            // If the process doesn't own the file, we can access if we are
            // root AND granted root map

            LocalFree(secDesc);

            if (args->connect->maproot)
            {
                Log(LOG_LEVEL_VERBOSE, "Caller '%s' not owner of '%s', but mapping privilege",
                      (args->connect)->username, filename);
                return true;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Remote user denied right to file '%s' (consider maproot?)", filename);
                return false;
            }
        }

        LocalFree(secDesc);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not retreive existing owner of '%s'. (GetNamedSecurityInfo)", filename);
        return false;
    }

#else

    uid_t uid = (args->connect)->uid;

    if ((uid != 0) && (!args->connect->maproot))    /* should remote root be local root */
    {
        if (sb->st_uid == uid)
        {
            Log(LOG_LEVEL_DEBUG, "Caller '%s' is the owner of the file", (args->connect)->username);
        }
        else
        {
            if (sb->st_mode & S_IROTH)
            {
                Log(LOG_LEVEL_DEBUG, "Caller %s not owner of the file but permission granted", (args->connect)->username);
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Caller '%s' is not the owner of the file", (args->connect)->username);
                Log(LOG_LEVEL_VERBOSE, "Remote user denied right to file '%s' (consider maproot?)", filename);
                return false;
            }
        }
    }
#endif

    return true;
}

static void AbortTransfer(ConnectionInfo *connection, char *filename)
{
    Log(LOG_LEVEL_VERBOSE, "Aborting transfer of file due to source changes");

    char sendbuffer[CF_BUFSIZE];
    snprintf(sendbuffer, CF_BUFSIZE, "%s%s: %s", CF_CHANGEDSTR1, CF_CHANGEDSTR2, filename);

    if (SendTransaction(connection, sendbuffer, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
    }
}

static void FailedTransfer(ConnectionInfo *connection)
{
    Log(LOG_LEVEL_VERBOSE, "Transfer failure");

    char sendbuffer[CF_BUFSIZE];

    snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);

    if (SendTransaction(connection, sendbuffer, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
    }
}

void CfGetFile(ServerFileGetState *args)
{
    int fd;
    off_t n_read, total = 0, sendlen = 0, count = 0;
    char sendbuffer[CF_BUFSIZE + 256], filename[CF_BUFSIZE];
    struct stat sb;
    int blocksize = 2048;

    ConnectionInfo *conn_info = &(args->connect)->conn_info;

    TranslatePath(filename, args->replyfile);

    stat(filename, &sb);

    Log(LOG_LEVEL_DEBUG, "CfGetFile('%s'), size = %" PRIdMAX, filename, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(filename, args, &sb))
    {
        RefuseAccess(args->connect, args->buf_size, "");
        snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
        if (conn_info->type == CF_PROTOCOL_CLASSIC)
        {
            SendSocketStream(conn_info->sd, sendbuffer, args->buf_size);
        }
        else if (conn_info->type == CF_PROTOCOL_TLS)
        {
            TLSSend(conn_info->ssl, sendbuffer, args->buf_size);
        }
        return;
    }

/* File transfer */

    if ((fd = open(filename, O_RDONLY)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Open error of file '%s'. (open: %s)",
            filename, GetErrorStr());
        snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
        if (conn_info->type == CF_PROTOCOL_CLASSIC)
        {
            SendSocketStream(conn_info->sd, sendbuffer, args->buf_size);
        }
        else if (conn_info->type == CF_PROTOCOL_TLS)
        {
            TLSSend(conn_info->ssl, sendbuffer, args->buf_size);
        }
    }
    else
    {
        int div = 3;

        if (sb.st_size > 10485760L) /* File larger than 10 MB, checks every 64kB */
        {
            div = 32;
        }

        while (true)
        {
            memset(sendbuffer, 0, CF_BUFSIZE);

            Log(LOG_LEVEL_DEBUG, "Now reading from disk...");

            if ((n_read = read(fd, sendbuffer, blocksize)) == -1)
            {
                Log(LOG_LEVEL_ERR, "Read failed in GetFile. (read: %s)", GetErrorStr());
                break;
            }

            if (n_read == 0)
            {
                break;
            }
            else
            {
                off_t savedlen = sb.st_size;

                /* check the file is not changing at source */

                if (count++ % div == 0)   /* Don't do this too often */
                {
                    if (stat(filename, &sb))
                    {
                        Log(LOG_LEVEL_ERR, "Cannot stat file '%s'. (stat: %s)",
                            filename, GetErrorStr());
                        break;
                    }
                }

                if (sb.st_size != savedlen)
                {
                    snprintf(sendbuffer, CF_BUFSIZE, "%s%s: %s", CF_CHANGEDSTR1, CF_CHANGEDSTR2, filename);

                    if (conn_info->type == CF_PROTOCOL_CLASSIC)
                    {
                        if (SendSocketStream(conn_info->sd, sendbuffer, blocksize) == -1)
                        {
                            Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                        }
                    }
                    else if (conn_info->type == CF_PROTOCOL_TLS)
                    {
                        if (TLSSend(conn_info->ssl, sendbuffer, blocksize) == -1)
                        {
                            Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                        }
                    }

                    Log(LOG_LEVEL_DEBUG, "Aborting transfer after %" PRIdMAX ": file is changing rapidly at source.", (intmax_t)total);
                    break;
                }

                if ((savedlen - total) / blocksize > 0)
                {
                    sendlen = blocksize;
                }
                else if (savedlen != 0)
                {
                    sendlen = (savedlen - total);
                }
            }

            total += n_read;

            if (conn_info->type == CF_PROTOCOL_CLASSIC)
            {
                if (SendSocketStream(conn_info->sd, sendbuffer, sendlen) == -1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                    break;
                }
            }
            else if (conn_info->type == CF_PROTOCOL_TLS)
            {
                if (TLSSend(conn_info->ssl, sendbuffer, sendlen) == -1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                    break;
                }
            }
        }

        close(fd);
    }
}

void CfEncryptGetFile(ServerFileGetState *args)
/* Because the stream doesn't end for each file, we need to know the
   exact number of bytes transmitted, which might change during
   encryption, hence we need to handle this with transactions */
{
    int fd, n_read, cipherlen, finlen;
    off_t total = 0, count = 0;
    char sendbuffer[CF_BUFSIZE + 256], out[CF_BUFSIZE], filename[CF_BUFSIZE];
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    int blocksize = CF_BUFSIZE - 4 * CF_INBAND_OFFSET;
    EVP_CIPHER_CTX ctx;
    char *key, enctype;
    struct stat sb;
    ConnectionInfo *conn_info = &args->connect->conn_info;

    key = (args->connect)->session_key;
    enctype = (args->connect)->encryption_type;

    TranslatePath(filename, args->replyfile);

    stat(filename, &sb);

    Log(LOG_LEVEL_DEBUG, "CfEncryptGetFile('%s'), size = %" PRIdMAX, filename, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(filename, args, &sb))
    {
        RefuseAccess(args->connect, args->buf_size, "");
        FailedTransfer(conn_info);
    }

    EVP_CIPHER_CTX_init(&ctx);

    if ((fd = open(filename, O_RDONLY)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Open error of file '%s'. (open: %s)", filename, GetErrorStr());
        FailedTransfer(conn_info);
    }
    else
    {
        int div = 3;

        if (sb.st_size > 10485760L) /* File larger than 10 MB, checks every 64kB */
        {
            div = 32;
        }

        while (true)
        {
            memset(sendbuffer, 0, CF_BUFSIZE);

            if ((n_read = read(fd, sendbuffer, blocksize)) == -1)
            {
                Log(LOG_LEVEL_ERR, "Read failed in EncryptGetFile. (read: %s)", GetErrorStr());
                break;
            }

            off_t savedlen = sb.st_size;

            if (count++ % div == 0)       /* Don't do this too often */
            {
                Log(LOG_LEVEL_DEBUG, "Restatting '%s' - size %d", filename, n_read);
                if (stat(filename, &sb))
                {
                    Log(LOG_LEVEL_ERR, "Cannot stat file '%s' (stat: %s)",
                            filename, GetErrorStr());
                    break;
                }
            }

            if (sb.st_size != savedlen)
            {
                AbortTransfer(conn_info, filename);
                break;
            }

            total += n_read;

            if (n_read > 0)
            {
                EVP_EncryptInit_ex(&ctx, CfengineCipher(enctype), NULL, key, iv);

                if (!EVP_EncryptUpdate(&ctx, out, &cipherlen, sendbuffer, n_read))
                {
                    FailedTransfer(conn_info);
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    close(fd);
                    return;
                }

                if (!EVP_EncryptFinal_ex(&ctx, out + cipherlen, &finlen))
                {
                    FailedTransfer(conn_info);
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    close(fd);
                    return;
                }
            }

            if (total >= savedlen)
            {
                if (SendTransaction(conn_info, out, cipherlen + finlen, CF_DONE) == -1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    close(fd);
                    return;
                }
                break;
            }
            else
            {
                if (SendTransaction(conn_info, out, cipherlen + finlen, CF_MORE) == -1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                    close(fd);
                    EVP_CIPHER_CTX_cleanup(&ctx);
                    return;
                }
            }
        }
    }

    EVP_CIPHER_CTX_cleanup(&ctx);
    close(fd);
}

int StatFile(ServerConnectionState *conn, char *sendbuffer, char *ofilename)
/* Because we do not know the size or structure of remote datatypes,*/
/* the simplest way to transfer the data is to convert them into */
/* plain text and interpret them on the other side. */
{
    Stat cfst;
    struct stat statbuf, statlinkbuf;
    char linkbuf[CF_BUFSIZE], filename[CF_BUFSIZE];
    int islink = false;

    TranslatePath(filename, ofilename);

    memset(&cfst, 0, sizeof(Stat));

    if (strlen(ReadLastNode(filename)) > CF_MAXLINKSIZE)
    {
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: Filename suspiciously long [%s]\n", filename);
        Log(LOG_LEVEL_ERR, "%s", sendbuffer);
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

    if (lstat(filename, &statbuf) == -1)
    {
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: unable to stat file %s", filename);
        Log(LOG_LEVEL_VERBOSE, "%s. (lstat: %s)", sendbuffer, GetErrorStr());
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

    cfst.cf_readlink = NULL;
    cfst.cf_lmode = 0;
    cfst.cf_nlink = CF_NOSIZE;

    memset(linkbuf, 0, CF_BUFSIZE);

#ifndef __MINGW32__                   // windows doesn't support symbolic links
    if (S_ISLNK(statbuf.st_mode))
    {
        islink = true;
        cfst.cf_type = FILE_TYPE_LINK; /* pointless - overwritten */
        cfst.cf_lmode = statbuf.st_mode & 07777;
        cfst.cf_nlink = statbuf.st_nlink;

        if (readlink(filename, linkbuf, CF_BUFSIZE - 1) == -1)
        {
            sprintf(sendbuffer, "BAD: unable to read link\n");
            Log(LOG_LEVEL_ERR, "%s. (readlink: %s)", sendbuffer, GetErrorStr());
            SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
            return -1;
        }

        Log(LOG_LEVEL_DEBUG, "readlink '%s'", linkbuf);

        cfst.cf_readlink = linkbuf;
    }
#endif /* !__MINGW32__ */

    if ((!islink) && (stat(filename, &statbuf) == -1))
    {
        Log(LOG_LEVEL_VERBOSE, "BAD: unable to stat file '%s'. (stat: %s)",
            filename, GetErrorStr());
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

    Log(LOG_LEVEL_DEBUG, "Getting size of link deref '%s'", linkbuf);

    if (islink && (stat(filename, &statlinkbuf) != -1))       /* linktype=copy used by agent */
    {
        statbuf.st_size = statlinkbuf.st_size;
        statbuf.st_mode = statlinkbuf.st_mode;
        statbuf.st_uid = statlinkbuf.st_uid;
        statbuf.st_gid = statlinkbuf.st_gid;
        statbuf.st_mtime = statlinkbuf.st_mtime;
        statbuf.st_ctime = statlinkbuf.st_ctime;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_DIR;
    }

    if (S_ISREG(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_REGULAR;
    }

    if (S_ISSOCK(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_SOCK;
    }

    if (S_ISCHR(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_CHAR_;
    }

    if (S_ISBLK(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_BLOCK;
    }

    if (S_ISFIFO(statbuf.st_mode))
    {
        cfst.cf_type = FILE_TYPE_FIFO;
    }

    cfst.cf_mode = statbuf.st_mode & 07777;
    cfst.cf_uid = statbuf.st_uid & 0xFFFFFFFF;
    cfst.cf_gid = statbuf.st_gid & 0xFFFFFFFF;
    cfst.cf_size = statbuf.st_size;
    cfst.cf_atime = statbuf.st_atime;
    cfst.cf_mtime = statbuf.st_mtime;
    cfst.cf_ctime = statbuf.st_ctime;
    cfst.cf_ino = statbuf.st_ino;
    cfst.cf_dev = statbuf.st_dev;
    cfst.cf_readlink = linkbuf;

    if (cfst.cf_nlink == CF_NOSIZE)
    {
        cfst.cf_nlink = statbuf.st_nlink;
    }

#if !defined(__MINGW32__)
    if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
#else
# ifdef HAVE_ST_BLOCKS
    if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
# else
    if (statbuf.st_size > ST_NBLOCKS(statbuf) * DEV_BSIZE)
# endif
#endif
    {
        cfst.cf_makeholes = 1;  /* must have a hole to get checksum right */
    }
    else
    {
        cfst.cf_makeholes = 0;
    }

    memset(sendbuffer, 0, CF_BUFSIZE);

    /* send as plain text */

    Log(LOG_LEVEL_DEBUG, "OK: type = %d, mode = %" PRIoMAX ", lmode = %" PRIoMAX ", uid = %" PRIuMAX ", gid = %" PRIuMAX ", size = %" PRIdMAX ", atime=%" PRIdMAX ", mtime = %" PRIdMAX,
            cfst.cf_type, (uintmax_t)cfst.cf_mode, (uintmax_t)cfst.cf_lmode, (intmax_t)cfst.cf_uid, (intmax_t)cfst.cf_gid, (intmax_t) cfst.cf_size,
            (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime);

    snprintf(sendbuffer, CF_BUFSIZE, "OK: %d %ju %ju %ju %ju %jd %jd %jd %jd %d %d %d %jd",
             cfst.cf_type, (uintmax_t)cfst.cf_mode, (uintmax_t)cfst.cf_lmode,
             (uintmax_t)cfst.cf_uid, (uintmax_t)cfst.cf_gid, (intmax_t)cfst.cf_size,
             (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime, (intmax_t) cfst.cf_ctime,
             cfst.cf_makeholes, cfst.cf_ino, cfst.cf_nlink, (intmax_t) cfst.cf_dev);

    SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);

    memset(sendbuffer, 0, CF_BUFSIZE);

    if (cfst.cf_readlink != NULL)
    {
        strcpy(sendbuffer, "OK:");
        strcat(sendbuffer, cfst.cf_readlink);
    }
    else
    {
        sprintf(sendbuffer, "OK:");
    }

    SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
    return 0;
}

void CompareLocalHash(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer)
{
    unsigned char digest1[EVP_MAX_MD_SIZE + 1], digest2[EVP_MAX_MD_SIZE + 1];
    char filename[CF_BUFSIZE], rfilename[CF_BUFSIZE];
    char *sp;
    int i;

/* TODO - when safe change this proto string to sha2 */

    sscanf(recvbuffer, "MD5 %[^\n]", rfilename);

    sp = recvbuffer + strlen(recvbuffer) + CF_SMALL_OFFSET;

    for (i = 0; i < CF_DEFAULT_DIGEST_LEN; i++)
    {
        digest1[i] = *sp++;
    }

    memset(sendbuffer, 0, CF_BUFSIZE);

    TranslatePath(filename, rfilename);

    HashFile(filename, digest2, CF_DEFAULT_DIGEST);

    if ((HashesMatch(digest1, digest2, CF_DEFAULT_DIGEST)) || (HashesMatch(digest1, digest2, HASH_METHOD_MD5)))
    {
        sprintf(sendbuffer, "%s", CFD_FALSE);
        Log(LOG_LEVEL_DEBUG, "Hashes matched ok");
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
    }
    else
    {
        sprintf(sendbuffer, "%s", CFD_TRUE);
        Log(LOG_LEVEL_DEBUG, "Hashes didn't match");
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
    }
}

void GetServerLiteral(EvalContext *ctx, ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted)
{
    char handle[CF_BUFSIZE], out[CF_BUFSIZE];
    int cipherlen;

    sscanf(recvbuffer, "VAR %255[^\n]", handle);

    if (ReturnLiteralData(ctx, handle, out))
    {
        memset(sendbuffer, 0, CF_BUFSIZE);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "%s", out);
    }
    else
    {
        memset(sendbuffer, 0, CF_BUFSIZE);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "BAD: Not found");
    }

    if (encrypted)
    {
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(&conn->conn_info, out, cipherlen, CF_DONE);
    }
    else
    {
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
    }
}

int GetServerQuery(ServerConnectionState *conn, char *recvbuffer)
{
    char query[CF_BUFSIZE];

    query[0] = '\0';
    sscanf(recvbuffer, "QUERY %255[^\n]", query);

    if (strlen(query) == 0)
    {
        return false;
    }

    return ReturnQueryData(conn, query);
}

void ReplyServerContext(ServerConnectionState *conn, int encrypted, Item *classes)
{
    char out[CF_BUFSIZE];
    int cipherlen;
    Item *ip;

    char sendbuffer[CF_BUFSIZE];
    memset(sendbuffer, 0, CF_BUFSIZE);

    for (ip = classes; ip != NULL; ip = ip->next)
    {
        if (strlen(sendbuffer) + strlen(ip->name) < CF_BUFSIZE - 3)
        {
            strcat(sendbuffer, ip->name);
            strcat(sendbuffer, ",");
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Overflow in context grab");
            break;
        }
    }

    DeleteItemList(classes);

    if (encrypted)
    {
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(&conn->conn_info, out, cipherlen, CF_DONE);
    }
    else
    {
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
    }
}

int CfOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *oldDirname)
{
    Dir *dirh;
    const struct dirent *dirp;
    int offset;
    char dirname[CF_BUFSIZE];

    TranslatePath(dirname, oldDirname);

    if (!IsAbsoluteFileName(dirname))
    {
        sprintf(sendbuffer, "BAD: request to access a non-absolute filename\n");
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

    if ((dirh = DirOpen(dirname)) == NULL)
    {
        Log(LOG_LEVEL_DEBUG, "Couldn't open dir '%s'", dirname);
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: cfengine, couldn't open dir %s\n", dirname);
        SendTransaction(&conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

/* Pack names for transmission */

    memset(sendbuffer, 0, CF_BUFSIZE);

    offset = 0;

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (strlen(dirp->d_name) + 1 + offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
        {
            SendTransaction(&conn->conn_info, sendbuffer, offset + 1, CF_MORE);
            offset = 0;
            memset(sendbuffer, 0, CF_BUFSIZE);
        }

        strncpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
        offset += strlen(dirp->d_name) + 1;     /* + zero byte separator */
    }

    strcpy(sendbuffer + offset, CFD_TERMINATOR);
    SendTransaction(&conn->conn_info, sendbuffer, offset + 2 + strlen(CFD_TERMINATOR), CF_DONE);
    DirClose(dirh);
    return 0;
}

/**************************************************************/

int CfSecOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *dirname)
{
    Dir *dirh;
    const struct dirent *dirp;
    int offset, cipherlen;
    char out[CF_BUFSIZE];

    if (!IsAbsoluteFileName(dirname))
    {
        sprintf(sendbuffer, "BAD: request to access a non-absolute filename\n");
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(&conn->conn_info, out, cipherlen, CF_DONE);
        return -1;
    }

    if ((dirh = DirOpen(dirname)) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Couldn't open dir %s", dirname);
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: cfengine, couldn't open dir %s\n", dirname);
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(&conn->conn_info, out, cipherlen, CF_DONE);
        return -1;
    }

/* Pack names for transmission */

    memset(sendbuffer, 0, CF_BUFSIZE);

    offset = 0;

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (strlen(dirp->d_name) + 1 + offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
        {
            cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, offset + 1);
            SendTransaction(&conn->conn_info, out, cipherlen, CF_MORE);
            offset = 0;
            memset(sendbuffer, 0, CF_BUFSIZE);
            memset(out, 0, CF_BUFSIZE);
        }

        strncpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
        /* + zero byte separator */
        offset += strlen(dirp->d_name) + 1;
    }

    strcpy(sendbuffer + offset, CFD_TERMINATOR);

    cipherlen =
        EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, offset + 2 + strlen(CFD_TERMINATOR));
    SendTransaction(&conn->conn_info, out, cipherlen, CF_DONE);
    DirClose(dirh);
    return 0;
}

int cfscanf(char *in, int len1, int len2, char *out1, char *out2, char *out3)
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

