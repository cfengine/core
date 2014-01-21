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

    CfState q;
    int ksize, vsize;
    char *key;
    void *value;
    size_t count = 0;
    time_t now = time(NULL);
    Item *persistent_classes = NULL;
    while (NextDB(dbcp, &key, &ksize, &value, &vsize))
    {
        memcpy(&q, value, sizeof(CfState));

        if (now > q.expires)
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

            if (IsDefinedClass(ctx, ip->name, NULL))
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

    snprintf(ebuff, CF_BUFSIZE, "%s --inform", CFRUNCOMMAND);

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

    /* Remote user is root, really useless, "user" is just a string in the
     * protocol he might claim whatever he wants, *key* is what matters */
    assert((args->connect->uid == 0) ||
    /* remote IP has maproot in our access_rules, useless because of previous */
           (args->connect->maproot == true) ||
    /* file is owned by the same username the user claimed - useless as well */
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

    Log(LOG_LEVEL_DEBUG, "CfGetFile('%s'), size = %" PRIdMAX, filename, (intmax_t) sb.st_size);

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

    Log(LOG_LEVEL_DEBUG, "CfEncryptGetFile('%s'), size = %" PRIdMAX, filename, (intmax_t) sb.st_size);

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

    Log(LOG_LEVEL_DEBUG, "OK: type = %d, mode = %" PRIoMAX ", lmode = %" PRIoMAX ", uid = %" PRIuMAX ", gid = %" PRIuMAX ", size = %" PRIdMAX ", atime=%" PRIdMAX ", mtime = %" PRIdMAX,
            cfst.cf_type, (uintmax_t)cfst.cf_mode, (uintmax_t)cfst.cf_lmode, (intmax_t)cfst.cf_uid, (intmax_t)cfst.cf_gid, (intmax_t) cfst.cf_size,
            (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime);

    snprintf(sendbuffer, CF_BUFSIZE, "OK: %d %ju %ju %ju %ju %jd %jd %jd %jd %d %d %d %jd",
             cfst.cf_type, (uintmax_t)cfst.cf_mode, (uintmax_t)cfst.cf_lmode,
             (uintmax_t)cfst.cf_uid, (uintmax_t)cfst.cf_gid, (intmax_t)cfst.cf_size,
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
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
    }
    else
    {
        sprintf(sendbuffer, "%s", CFD_TRUE);
        Log(LOG_LEVEL_DEBUG, "Hashes didn't match");
        SendTransaction(conn->conn_info, sendbuffer, 0, CF_DONE);
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

        strncpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
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

        strncpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
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

