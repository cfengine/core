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


static const int CF_NOSIZE = -1;


#include <server_common.h>

#include <item_lib.h>                                 /* ItemList2CSV_bound */
#include <string_lib.h>                               /* ToLower */
#include <crypto.h>                                   /* EncryptString */
#include <files_names.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <file_lib.h>
#include <eval_context.h>
#include <dir.h>
#include <conversion.h>
#include <matching.h>                        /* IsRegexItemIn */
#include <pipes.h>
#include <classic.h>                  /* SendSocketStream */
#include <net.h>                      /* SendTransaction,ReceiveTransaction */
#include <tls_generic.h>              /* TLSSend */
#include <rlist.h>
#include <cf-serverd-enterprise-stubs.h>
#include <connection_info.h>
#include <misc_lib.h>                                    /* UnexpectedError */


void RefuseAccess(ServerConnectionState *conn, char *errmesg)
{
    char *username, *ipaddr;
    char *def = "?";

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

    char buf[CF_BUFSIZE] = "";
    snprintf(buf, sizeof(buf), "%s", CF_FAILEDSTR);
    SendTransaction(conn->conn_info, buf, 0, CF_DONE);

    Log(LOG_LEVEL_VERBOSE, "REFUSAL to (user=%s,ip=%s) of request: %s",
        username, ipaddr, errmesg);
}

bool IsUserNameValid(char *username)
{
    /* Add whatever characters are considered invalid in username */
    const char *invalid_username_characters = "\\/";

    if (strpbrk(username, invalid_username_characters) == NULL)
    {
        return true;
    }

    return false;
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

Item *ListPersistentClasses()
{
    Log(LOG_LEVEL_VERBOSE, "Scanning for all persistent classes");

    CF_DB *dbp;
    CF_DBC *dbcp;
    if (!OpenDB(&dbp, dbid_state))
    {
        Log(LOG_LEVEL_ERR, "Unable to open state database");
        return NULL;
    }
    if (!NewDBCursor(dbp, &dbcp))
    {
        Log(LOG_LEVEL_ERR, "Unable to scan state database");
        CloseDB(dbp);
        return NULL;
    }

    const PersistentClassInfo *value;
    int ksize, vsize;
    char *key;
    size_t count = 0;
    time_t now = time(NULL);
    Item *persistent_classes = NULL;
    while (NextDB(dbcp, &key, &ksize, (void **)&value, &vsize))
    {
        if (now > value->expires)
        {
            Log(LOG_LEVEL_DEBUG,
                "Persistent class %s expired, removing from database", key);
            DBCursorDeleteEntry(dbcp);
        }
        else
        {
            count++;
            PrependItem(&persistent_classes, key, NULL);
        }
    }

    DeleteDBCursor(dbcp);
    CloseDB(dbp);

    if (LogGetGlobalLevel() >= LOG_LEVEL_VERBOSE)
    {
        char logbuf[CF_BUFSIZE];
        ItemList2CSV_bound(persistent_classes, logbuf, sizeof(logbuf), ' ');
        Log(LOG_LEVEL_VERBOSE,
            "Found %zu valid persistent classes in state database: %s",
            count, logbuf);
    }

    return persistent_classes;
}


static void ReplyNothing(ServerConnectionState *conn)
{
    char buffer[CF_BUFSIZE];

    snprintf(buffer, CF_BUFSIZE, "Hello %s (%s), nothing relevant to do here...\n\n", conn->hostname, conn->ipaddr);

    if (SendTransaction(conn->conn_info, buffer, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to send transaction. (send: %s)", GetErrorStr());
    }
}

/* Used only in EXEC protocol command, to check if any of the received classes
 * is defined in the server. */
int MatchClasses(EvalContext *ctx, ServerConnectionState *conn)
{
    char recvbuffer[CF_BUFSIZE];
    Item *classlist = NULL, *ip;
    int count = 0;

    while (true && (count < 10))        /* arbitrary check to avoid infinite loop, DoS attack */
    {
        count++;

        if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
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

            if (IsDefinedClass(ctx, ip->name))
            {
                Log(LOG_LEVEL_DEBUG, "Class '%s' matched, accepting...", ip->name);
                DeleteItemList(classlist);
                return true;
            }

            {
                /* What the heck are we doing here? */
                /* Hmmm so we iterate over all classes to see if the regex
                 * received (ip->name) matches (StringMatchFull) to any local
                 * class (expr)... SLOW! Change the spec! Don't accept
                 * regexes! How many will be affected if a specific class has
                 * to be set to run command, instead of matching a pattern?
                 * It's safer anyway... */
                ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
                Class *cls = NULL;
                while ((cls = ClassTableIteratorNext(iter)))
                {
                    char *expr = ClassRefToString(cls->ns, cls->name);
                    /* FIXME: review this strcmp. Moved out from StringMatch */
                    bool match = (strcmp(ip->name, expr) == 0 ||
                                  StringMatchFull(ip->name, expr));
                    free(expr);
                    if (match)
                    {
                        Log(LOG_LEVEL_DEBUG, "Class matched regular expression '%s', accepting...", ip->name);
                        DeleteItemList(classlist);
                        return true;
                    }
                }
                ClassTableIteratorDestroy(iter);
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
            if (StringMatchFull(ap->path, RlistScalarValue(rp)))
            {
                /* We have a pattern covering this class - so are we allowed to activate it? */
                if ((IsMatchItemIn(ap->accesslist, conn->ipaddr)) ||
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
    char ebuff[CF_EXPANDSIZE], *sp = NULL;
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
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Examining command string '%s'", args);

    for (sp = args; *sp != '\0'; sp++)  /* Blank out -K -f */
    {
        if ((*sp == ';') || (*sp == '&') || (*sp == '|'))
        {
            char sendbuffer[CF_BUFSIZE];
            snprintf(sendbuffer, CF_BUFSIZE, "You are not authorized to activate these classes/roles on host %s\n", VFQNAME);
            SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
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
                SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
                return;
            }
        }
    }

    snprintf(ebuff, CF_BUFSIZE, "%s -Dcfruncommand --inform", CFRUNCOMMAND);

    if (strlen(ebuff) + strlen(args) + 6 > CF_BUFSIZE)
    {
        char sendbuffer[CF_BUFSIZE];
        snprintf(sendbuffer, CF_BUFSIZE, "Command line too long with args: %s\n", ebuff);
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
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
            SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        }
    }

    Log(LOG_LEVEL_INFO, "Executing command %s", ebuff);

    if ((pp = cf_popen_sh(ebuff, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe to command '%s'. (pipe: %s)", ebuff, GetErrorStr());
        char sendbuffer[CF_BUFSIZE];
        snprintf(sendbuffer, CF_BUFSIZE, "Unable to run %s\n", ebuff);
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        return;
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    for (;;)
    {
        ssize_t res = CfReadLine(&line, &line_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                fflush(pp); /* FIXME: is it necessary? */
            }
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
            if (SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE) == -1)
            {
                Log(LOG_LEVEL_ERR, "Sending failed, aborting. (send: %s)", GetErrorStr());
                break;
            }
        }
    }

    free(line);
    cf_pclose(pp);
}

/* TODO don't pass "args" just the things we actually check: sid, username, maproot, uid */
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

    /* Return true if one of the following is true: */

    /* Remote user is root, where "user" is just a string in the protocol, he
     * might claim whatever he wants but will be able to login only if the
     * user-key.pub key is found, */
    assert((args->connect->uid == 0) ||
    /* OR remote IP has maproot in the file's access_rules, */
           (args->connect->maproot == true) ||
    /* OR file is owned by the same username the user claimed - useless or
     * even dangerous outside NIS, KERBEROS or LDAP authenticated domains,  */
           (sb->st_uid == uid) ||
    /* file is readable by everyone */
           (sb->st_mode & S_IROTH));

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

    ConnectionInfo *conn_info = (args->connect)->conn_info;

    TranslatePath(filename, args->replyfile);

    stat(filename, &sb);

    Log(LOG_LEVEL_DEBUG, "CfGetFile('%s'), size = %jd",
        filename, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(filename, args, &sb))
    {
        RefuseAccess(args->connect, args->replyfile);
        snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
        if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_CLASSIC)
        {
            SendSocketStream(ConnectionInfoSocket(conn_info), sendbuffer, args->buf_size);
        }
        else if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_TLS)
        {
            TLSSend(ConnectionInfoSSL(conn_info), sendbuffer, args->buf_size);
        }
        return;
    }

/* File transfer */

    if ((fd = safe_open(filename, O_RDONLY)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Open error of file '%s'. (open: %s)",
            filename, GetErrorStr());
        snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);
        if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_CLASSIC)
        {
            SendSocketStream(ConnectionInfoSocket(conn_info), sendbuffer, args->buf_size);
        }
        else if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_TLS)
        {
            TLSSend(ConnectionInfoSSL(conn_info), sendbuffer, args->buf_size);
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

                    if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_CLASSIC)
                    {
                        if (SendSocketStream(ConnectionInfoSocket(conn_info), sendbuffer, blocksize) == -1)
                        {
                            Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                        }
                    }
                    else if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_TLS)
                    {
                        if (TLSSend(ConnectionInfoSSL(conn_info), sendbuffer, blocksize) == -1)
                        {
                            Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                        }
                    }

                    Log(LOG_LEVEL_DEBUG,
                        "Aborting transfer after %jd: file is changing rapidly at source.",
                        (intmax_t) total);
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

            if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_CLASSIC)
            {
                if (SendSocketStream(ConnectionInfoSocket(conn_info), sendbuffer, sendlen) == -1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                    break;
                }
            }
            else if (ConnectionInfoProtocolVersion(conn_info) == CF_PROTOCOL_TLS)
            {
                if (TLSSend(ConnectionInfoSSL(conn_info), sendbuffer, sendlen) == -1)
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
    int fd, n_read, cipherlen = 0, finlen = 0;
    off_t total = 0, count = 0;
    char sendbuffer[CF_BUFSIZE + 256], out[CF_BUFSIZE], filename[CF_BUFSIZE];
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    int blocksize = CF_BUFSIZE - 4 * CF_INBAND_OFFSET;
    EVP_CIPHER_CTX ctx;
    char *key, enctype;
    struct stat sb;
    ConnectionInfo *conn_info = args->connect->conn_info;

    key = (args->connect)->session_key;
    enctype = (args->connect)->encryption_type;

    TranslatePath(filename, args->replyfile);

    stat(filename, &sb);

    Log(LOG_LEVEL_DEBUG, "CfEncryptGetFile('%s'), size = %jd",
        filename, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(filename, args, &sb))
    {
        RefuseAccess(args->connect, args->replyfile);
        FailedTransfer(conn_info);
    }

    EVP_CIPHER_CTX_init(&ctx);

    if ((fd = safe_open(filename, O_RDONLY)) == -1)
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
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: Filename suspiciously long [%s]", filename);
        Log(LOG_LEVEL_ERR, "%s", sendbuffer);
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

    if (lstat(filename, &statbuf) == -1)
    {
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: unable to stat file %s", filename);
        Log(LOG_LEVEL_VERBOSE, "%s. (lstat: %s)", sendbuffer, GetErrorStr());
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
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
            strcpy(sendbuffer, "BAD: unable to read link");
            Log(LOG_LEVEL_ERR, "%s. (readlink: %s)", sendbuffer, GetErrorStr());
            SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
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
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
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

    Log(LOG_LEVEL_DEBUG, "OK: type = %d, mode = %jo, lmode = %jo, "
        "uid = %ju, gid = %ju, size = %jd, atime=%jd, mtime = %jd",
        cfst.cf_type, (uintmax_t) cfst.cf_mode, (uintmax_t) cfst.cf_lmode,
        (uintmax_t) cfst.cf_uid, (uintmax_t) cfst.cf_gid, (intmax_t) cfst.cf_size,
        (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime);

    snprintf(sendbuffer, CF_BUFSIZE,
             "OK: %d %ju %ju %ju %ju %jd %jd %jd %jd %d %d %d %jd",
             cfst.cf_type, (uintmax_t) cfst.cf_mode, (uintmax_t) cfst.cf_lmode,
             (uintmax_t) cfst.cf_uid, (uintmax_t) cfst.cf_gid,   (intmax_t) cfst.cf_size,
             (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime, (intmax_t) cfst.cf_ctime,
             cfst.cf_makeholes, cfst.cf_ino, cfst.cf_nlink, (intmax_t) cfst.cf_dev);

    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);

    memset(sendbuffer, 0, CF_BUFSIZE);

    if (cfst.cf_readlink != NULL)
    {
        strcpy(sendbuffer, "OK:");
        strcat(sendbuffer, cfst.cf_readlink);
    }
    else
    {
        strcpy(sendbuffer, "OK:");
    }

    SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    return 0;
}

bool CompareLocalHash(const char *filename, const char digest[EVP_MAX_MD_SIZE + 1],
                      char sendbuffer[CF_BUFSIZE])
{
    char translated_filename[CF_BUFSIZE] = { 0 };
    TranslatePath(translated_filename, filename);

    unsigned char file_digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    /* TODO connection might timeout if this takes long! */
    HashFile(translated_filename, file_digest, CF_DEFAULT_DIGEST);

    if (HashesMatch(digest, file_digest, CF_DEFAULT_DIGEST))
    {
        assert(strlen(CFD_FALSE) < CF_BUFSIZE);
        strcpy(sendbuffer, CFD_FALSE);
        Log(LOG_LEVEL_DEBUG, "Hashes matched ok");
        return true;
    }
    else
    {
        assert(strlen(CFD_TRUE) < CF_BUFSIZE);
        strcpy(sendbuffer, CFD_TRUE);
        Log(LOG_LEVEL_DEBUG, "Hashes didn't match");
        return false;
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
        SendTransaction(conn->conn_info, out, cipherlen, CF_DONE);
    }
    else
    {
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    }
}

int GetServerQuery(ServerConnectionState *conn, char *recvbuffer, int encrypt)
{
    char query[CF_BUFSIZE];

    query[0] = '\0';
    sscanf(recvbuffer, "QUERY %255[^\n]", query);

    if (strlen(query) == 0)
    {
        return false;
    }

    return ReturnQueryData(conn, query, encrypt);
}

void ReplyServerContext(ServerConnectionState *conn, int encrypted, Item *classes)
{
    char sendbuffer[CF_BUFSIZE];
    size_t ret = ItemList2CSV_bound(classes,
                                    sendbuffer, sizeof(sendbuffer), ',');
    if (ret >= sizeof(sendbuffer))
    {
        Log(LOG_LEVEL_ERR, "Overflow: classes don't fit in send buffer");
    }

    DeleteItemList(classes);

    if (encrypted)
    {
        char out[CF_BUFSIZE];
        int cipherlen = EncryptString(conn->encryption_type, sendbuffer, out,
                                      conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(conn->conn_info, out, cipherlen, CF_DONE);
    }
    else
    {
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
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
        strcpy(sendbuffer, "BAD: request to access a non-absolute filename");
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

    if ((dirh = DirOpen(dirname)) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Couldn't open directory '%s' (DirOpen:%s)",
            dirname, GetErrorStr());
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: cfengine, couldn't open dir %s", dirname);
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
        return -1;
    }

/* Pack names for transmission */

    memset(sendbuffer, 0, CF_BUFSIZE);

    offset = 0;

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (strlen(dirp->d_name) + 1 + offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
        {
            SendTransaction(conn->conn_info, sendbuffer, offset + 1, CF_MORE);
            offset = 0;
            memset(sendbuffer, 0, CF_BUFSIZE);
        }

        strlcpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
        offset += strlen(dirp->d_name) + 1;     /* + zero byte separator */
    }

    strcpy(sendbuffer + offset, CFD_TERMINATOR);
    SendTransaction(conn->conn_info, sendbuffer, offset + 2 + strlen(CFD_TERMINATOR), CF_DONE);
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
        strcpy(sendbuffer, "BAD: request to access a non-absolute filename");
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(conn->conn_info, out, cipherlen, CF_DONE);
        return -1;
    }

    if ((dirh = DirOpen(dirname)) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Couldn't open dir %s", dirname);
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: cfengine, couldn't open dir %s", dirname);
        cipherlen = EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, strlen(sendbuffer) + 1);
        SendTransaction(conn->conn_info, out, cipherlen, CF_DONE);
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
            SendTransaction(conn->conn_info, out, cipherlen, CF_MORE);
            offset = 0;
            memset(sendbuffer, 0, CF_BUFSIZE);
            memset(out, 0, CF_BUFSIZE);
        }

        strlcpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
        /* + zero byte separator */
        offset += strlen(dirp->d_name) + 1;
    }

    strcpy(sendbuffer + offset, CFD_TERMINATOR);

    cipherlen =
        EncryptString(conn->encryption_type, sendbuffer, out, conn->session_key, offset + 2 + strlen(CFD_TERMINATOR));
    SendTransaction(conn->conn_info, out, cipherlen, CF_DONE);
    DirClose(dirh);
    return 0;
}


/********************* MISC UTILITY FUNCTIONS *************************/


/**
 * Replace all occurences of #find with #replace.
 *
 * @return the length of #buf or (size_t) -1 in case of overflow, or 0
 *         if no replace took place.
 */
static size_t StringReplace(char *buf, size_t buf_size,
                            const char *find, const char *replace)
{
    assert(find[0] != '\0');

    char *p = strstr(buf, find);
    if (p == NULL)
    {
        return 0;
    }

    size_t find_len = strlen(find);
    size_t replace_len = strlen(replace);
    size_t buf_len = strlen(buf);
    size_t buf_idx = 0;
    char tmp[buf_size];
    size_t tmp_len = 0;

    /* Do all replacements we find. */
    do
    {
        size_t buf_newidx = p - buf;
        size_t prefix_len = buf_newidx - buf_idx;

        if (tmp_len + prefix_len + replace_len >= buf_size)
        {
            return (size_t) -1;
        }

        memcpy(&tmp[tmp_len], &buf[buf_idx], prefix_len);
        tmp_len += prefix_len;

        memcpy(&tmp[tmp_len], replace, replace_len);
        tmp_len += replace_len;

        buf_idx = buf_newidx + find_len;
        p = strstr(&buf[buf_idx], find);
    }
    while (p != NULL);

    /* Copy leftover plus terminating '\0'. */
    size_t leftover_len = buf_len - buf_idx;
    if (tmp_len + leftover_len >= buf_size)
    {
        return (size_t) -1;
    }
    memcpy(&tmp[tmp_len], &buf[buf_idx], leftover_len + 1);
    tmp_len += leftover_len;

    /* And finally copy to source, we are supposed to modify it in place. */
    memcpy(buf, tmp, tmp_len + 1);

    return tmp_len;
}

/**
 * Search and replace occurences of #find1, #find2, #find3, with
 * #repl1, #repl2, #repl3 respectively.
 *
 *   "$(connection.ip)" from "191.168.0.1"
 *   "$(connection.hostname)" from "blah.cfengine.com",
 *   "$(connection.key)" from "SHA=asdfghjkl"
 *
 * @return the output length of #buf, (size_t) -1 if overflow would occur,
 *         or 0 if no replacement happened and #buf was not touched.
 *
 * @TODO change the function to more generic interface accepting arbitrary
 *       find/replace pairs.
 */
size_t ReplaceSpecialVariables(char *buf, size_t buf_size,
                               const char *find1, const char *repl1,
                               const char *find2, const char *repl2,
                               const char *find3, const char *repl3)
{
    size_t ret = 0;

    if ((find1 != NULL) && (find1[0] != '\0') &&
        (repl1 != NULL) && (repl1[0] != '\0'))
    {
        size_t ret2 = StringReplace(buf, buf_size, find1, repl1);
        ret = MAX(ret, ret2);           /* size_t is unsigned, thus -1 wins */
    }
    if ((ret != (size_t) -1) &&
        (find2 != NULL) && (find2[0] != '\0') &&
        (repl2 != NULL) && (repl2[0] != '\0'))
    {
        size_t ret2 = StringReplace(buf, buf_size, find2, repl2);
        ret = MAX(ret, ret2);
    }
    if ((ret != (size_t) -1) &&
        (find3 != NULL) && (find3[0] != '\0') &&
        (repl3 != NULL) && (repl3[0] != '\0'))
    {
        size_t ret2 = StringReplace(buf, buf_size, find3, repl3);
        ret = MAX(ret, ret2);
    }

    /* Zero is returned only if all of the above were zero. */
    return ret;
}


/**
 * Remove trailing FILE_SEPARATOR, unless we're referring to root dir: '/' or 'a:\'
 */
bool PathRemoveTrailingSlash(char *s, size_t s_len)
{
    char *first_separator = strchr(s, FILE_SEPARATOR);

    if (first_separator != NULL &&
         s[s_len-1] == FILE_SEPARATOR &&
        &s[s_len-1] != first_separator)
    {
        s[s_len-1] = '\0';
        return true;
    }

    return false;
}

/**
 * Append a trailing FILE_SEPARATOR if it's not there.
 */
bool PathAppendTrailingSlash(char *s, size_t s_len)
{
    if (s_len > 0 && s[s_len-1] != FILE_SEPARATOR)
    {
        s[s_len] = FILE_SEPARATOR;
        s[s_len+1] = '\0';
        return true;
    }

    return false;
}

/* We use this instead of IsAbsoluteFileName() which also checks for
 * quotes. There is no meaning in receiving quoted strings over the
 * network. */
static bool PathIsAbsolute(const char *s)
{
    bool result = false;

#if defined(__MINGW32__)
    if (isalpha(s[0]) && (s[1] == ':') && (s[2] == FILE_SEPARATOR))
    {
        result = true;                                          /* A:\ */
    }
    else                                                        /* \\ */
    {
        result = (s[0] == FILE_SEPARATOR && s[1] == FILE_SEPARATOR);
    }
#else
    if (s[0] == FILE_SEPARATOR)                                 /* / */
    {
        result = true;
    }
#endif

    return result;
}

/**
 * If #path is relative, expand the first part accorting to #shortcuts, doing
 * any replacements of special variables "$(connection.*)" on the way, with
 * the provided #ipaddr, #hostname, #key.
 *
 * @return the length of the new string or 0 if no replace took place. -1 in
 * case of overflow.
 */
size_t ShortcutsExpand(char *path, size_t path_size,
                       const StringMap *shortcuts,
                       const char *ipaddr, const char *hostname,
                       const char *key)
{
    char dst[path_size];
    size_t path_len = strlen(path);

    if (path_len == 0)
    {
        UnexpectedError("ShortcutsExpand: 0 length string!");
        return (size_t) -1;
    }

    if (!PathIsAbsolute(path))
    {
        char *separ = strchr(path, FILE_SEPARATOR);
        size_t first_part_len;
        if (separ != NULL)
        {
            first_part_len = separ - path;
            assert(first_part_len < path_len);
        }
        else
        {
            first_part_len = path_len;
        }
        size_t second_part_len = path_len - first_part_len;

        /* '\0'-terminate first_part, do StringMapGet(), undo '\0'-term */
        char separ_char = path[first_part_len];
        path[first_part_len] = '\0';
        char *replacement = StringMapGet(shortcuts, path);
        path[first_part_len] = separ_char;

        /* Either the first_part ends with separator, or its all the string */
        assert(separ_char == FILE_SEPARATOR ||
               separ_char == '\0');

        if (replacement != NULL)                 /* we found a shortcut */
        {
            size_t replacement_len = strlen(replacement);
            if (replacement_len + 1 > path_size)
            {
                goto err_too_long;
            }

            /* Replacement path for shortcut was found, but it may contain
             * special variables such as $(connection.ip), that we also need
             * to expand. */
            /* TODO if StrAnyStr(replacement, "$(connection.ip)", "$(connection.hostname)", "$(connection.key)") */
            char replacement_expanded[path_size];
            memcpy(replacement_expanded, replacement, replacement_len + 1);

            size_t ret =
                ReplaceSpecialVariables(replacement_expanded, sizeof(replacement_expanded),
                                        "$(connection.ip)", ipaddr,
                                        "$(connection.hostname)", hostname,
                                        "$(connection.key)", key);

            size_t replacement_expanded_len;
            /* (ret == -1) is checked later. */
            if (ret == 0)                        /* No expansion took place */
            {
                replacement_expanded_len = replacement_len;
            }
            else
            {
                replacement_expanded_len = ret;
            }

            size_t dst_len = replacement_expanded_len + second_part_len;
            if (ret == (size_t) -1 || dst_len + 1 > path_size)
            {
                goto err_too_long;
            }

            /* Assemble final result. */
            memcpy(dst, replacement_expanded, replacement_expanded_len);
            /* Second part may be empty, then this only copies '\0'. */
            memcpy(&dst[replacement_expanded_len], &path[first_part_len],
                   second_part_len + 1);

            Log(LOG_LEVEL_DEBUG,
                "ShortcutsExpand: Path '%s' became: %s",
                path, dst);

            /* Copy back to path. */
            memcpy(path, dst, dst_len + 1);
            return dst_len;
        }
    }

    /* No expansion took place, either because path was absolute, or because
     * no shortcut was found. */
    return 0;

  err_too_long:
    Log(LOG_LEVEL_INFO, "Path too long after shortcut expansion!");
    return (size_t) -1;
}

/**
 * Canonicalize a path, ensure it is absolute, and resolve all symlinks.
 * In detail:
 *
 * 1. MinGW: Translate to windows-compatible: slashes to FILE_SEPARATOR
 *           and uppercase to lowercase.
 * 2. Ensure the path is absolute.
 * 3. Resolve symlinks, resolve '.' and '..' and remove double '/'
 *    WARNING this will currently fail if file does not exist,
 *    returning -1 and setting errno==ENOENT!
 *
 * @note trailing slash is left as is if it's there.
 * @note #reqpath is written in place (if success was returned). It is always
 *       an absolute path.
 * @note #reqpath is invalid to be of zero length.
 * @note #reqpath_size must be at least PATH_MAX.
 *
 * @return the length of #reqpath after preprocessing. In case of error
 *         return (size_t) -1.
 */
size_t PreprocessRequestPath(char *reqpath, size_t reqpath_size)
{
    errno = 0;             /* on return, errno might be set from realpath() */
    char dst[reqpath_size];
    size_t reqpath_len = strlen(reqpath);

    if (reqpath_len == 0)
    {
        UnexpectedError("PreprocessRequestPath: 0 length string!");
        return (size_t) -1;
    }

    /* Translate all slashes to backslashes on Windows so that all the rest
     * of work is done using FILE_SEPARATOR. THIS HAS TO BE FIRST. */
    #if defined(__MINGW32__)
    {
        char *p = reqpath;
        while ((p = strchr(p, '/')) != NULL)
        {
            *p = FILE_SEPARATOR;
        }
        /* Also convert everything to lowercase. */
        ToLowerStrInplace(reqpath);
    }
    #endif

    if (!PathIsAbsolute(reqpath))
    {
        Log(LOG_LEVEL_INFO, "Relative paths are not allowed: %s", reqpath);
        return (size_t) -1;
    }

    /* TODO replace realpath with Solaris' resolvepath(), in all
     * platforms. That one does not check for existence, just resolves
     * symlinks and canonicalises. Ideally we would want the following:
     *
     * PathResolve(dst, src, dst_size, basedir);
     *
     * - It prepends basedir if path relative (could be the shortcut)
     * - It compresses double '/', '..', '.'
     * - It follows each component of the path replacing symlinks
     * - errno = ENOENT if path component does not exist, but keeps
     *   compressing path anyway.
     * - Leaves trailing slash as it was passed to it.
     *   OR appends it depending on last component ISDIR.
     */

    assert(sizeof(dst) >= PATH_MAX);               /* needed for realpath() */
    char *p = realpath(reqpath, dst);
    if (p == NULL)
    {
        /* TODO If path does not exist try to canonicalise only directory. INSECURE?*/
        /* if (errno == ENOENT) */
        /* { */

        /* } */

        Log(LOG_LEVEL_INFO,
            "Failed to canonicalise filename '%s' (realpath: %s)",
            reqpath, GetErrorStr());
        return (size_t) -1;
    }

    size_t dst_len = strlen(dst);

    /* Some realpath()s remove trailing '/' even for dirs! Put it back if
     * original request had it. */
    if (reqpath[reqpath_len - 1] == FILE_SEPARATOR &&
        dst[dst_len - 1]         != FILE_SEPARATOR)
    {
        if (dst_len + 2 > sizeof(dst))
        {
            Log(LOG_LEVEL_INFO, "Error, path too long: %s", reqpath);
            return (size_t) -1;
        }

        PathAppendTrailingSlash(dst, dst_len);
        dst_len++;
    }

    memcpy(reqpath, dst, dst_len + 1);
    reqpath_len = dst_len;

    return reqpath_len;
}

