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


static const int CF_NOSIZE = -1;


#include <server_common.h>

#include <item_lib.h>                                 /* ItemList2CSV_bound */
#include <string_lib.h>                              /* ToLower,StrCatDelim */
#include <regex.h>                                    /* StringMatchFull */
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
#include <openssl/err.h>                                   /* ERR_get_error */
#include <tls_generic.h>              /* TLSSend */
#include <rlist.h>
#include <cf-serverd-enterprise-stubs.h>
#include <connection_info.h>
#include <misc_lib.h>                              /* UnexpectedError */
#include <cf-windows-functions.h>                  /* NovaWin_UserNameToSid */
#include <mutex.h>                                 /* ThreadLock */
#include <stat_cache.h>                            /* struct Stat */
#include "server_access.h"


/* NOTE: Always Log(LOG_LEVEL_INFO) before calling RefuseAccess(), so that
 * some clue is printed in the cf-serverd logs. */
void RefuseAccess(ServerConnectionState *conn, char *errmesg)
{
    SendTransaction(conn->conn_info, CF_FAILEDSTR, 0, CF_DONE);

    /* TODO remove logging, it's done elsewhere. */
    Log(LOG_LEVEL_VERBOSE, "REFUSAL to user='%s' of request: %s",
        NULL_OR_EMPTY(conn->username) ? "?" : conn->username,
        errmesg);
}

bool IsUserNameValid(const char *username)
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
    if (IsItemIn(SERVER_ACCESS.allowuserlist, user))
    {
        Log(LOG_LEVEL_DEBUG, "User %s granted connection privileges", user);
        return true;
    }

    Log(LOG_LEVEL_DEBUG, "User %s is not allowed on this server", user);
    return false;
}

Item *ListPersistentClasses()
{
    Log(LOG_LEVEL_VERBOSE, "Scanning for all persistent classes");

    CF_DB *dbp;
    CF_DBC *dbcp;

    if (!OpenDB(&dbp, dbid_state))
    {
        char *db_path = DBIdToPath(dbid_state);
        Log(LOG_LEVEL_ERR, "Unable to open persistent classes database '%s'", db_path);
        free(db_path);
        return NULL;
    }

    if (!NewDBCursor(dbp, &dbcp))
    {
        char *db_path = DBIdToPath(dbid_state);
        Log(LOG_LEVEL_ERR, "Unable to get cursor for persistent classes database '%s'", db_path);
        free(db_path);
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
int MatchClasses(const EvalContext *ctx, ServerConnectionState *conn)
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

        if (strncmp(recvbuffer, CFD_TERMINATOR, strlen(CFD_TERMINATOR)) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "Got CFD_TERMINATOR");
            if (count == 1)
            {
                /* This is the common case, that cf-runagent had no
                   "-s class1,class2" argument. */
                Log(LOG_LEVEL_DEBUG, "No classes were sent, assuming no restrictions...");
                return true;
            }

            break;
        }

        Log(LOG_LEVEL_DEBUG, "Got class buffer: %s", recvbuffer);

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

/* TODO deprecate this function, only a simple SendTransaction(CFD_TERMINATOR)
 * should be enough, without even error printing (it's already done in
 * SendTransaction()). */
void Terminate(ConnectionInfo *connection)
{
    /* We send a trailing NULL in this transaction packet. TODO WHY? */
    if (SendTransaction(connection, CFD_TERMINATOR,
                        strlen(CFD_TERMINATOR) + 1, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Unable to reply with terminator. (send: %s)",
            GetErrorStr());
    }
}

static int TransferRights(const ServerConnectionState *conn,
                          const char *filename, const struct stat *sb)
{
    Log(LOG_LEVEL_DEBUG, "Checking ownership of file: %s", filename);

    /* Don't do any check if connected user claims to be "root" or if
     * "maproot" in access_rules contains the connecting IP address. */
    if ((conn->uid == 0) || (conn->maproot))
    {
        Log(LOG_LEVEL_DEBUG, "Access granted because %s",
            (conn->uid == 0) ? "remote user is root"
                             : "of maproot");
        return true;                                      /* access granted */
    }

#ifdef __MINGW32__

    SECURITY_DESCRIPTOR *secDesc;
    SID *ownerSid;

    if (GetNamedSecurityInfo(
            filename, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
            (PSID *) &ownerSid, NULL, NULL, NULL, (void **) &secDesc)
        != ERROR_SUCCESS)
    {
        Log(LOG_LEVEL_ERR,
            "Could not retrieve owner of file '%s' "
            "(GetNamedSecurityInfo: %s)",
            filename, GetErrorStr());
        return false;
    }

    LocalFree(secDesc);

    if (!IsValidSid(conn->sid) ||
        !EqualSid(ownerSid, conn->sid))
    {
        /* If "maproot" we've already granted access. */
        assert(!conn->maproot);

        Log(LOG_LEVEL_INFO,
            "Remote user '%s' is not the owner of the file, access denied, "
            "consider maproot", conn->username);

        return false;
    }

    Log(LOG_LEVEL_DEBUG,
        "User '%s' is the owner of the file, access granted",
        conn->username);

#else                                         /* UNIX systems - common path */

    if (sb->st_uid != conn->uid)                   /* does not own the file */
    {
        if (!(sb->st_mode & S_IROTH))            /* file not world readable */
        {
            Log(LOG_LEVEL_INFO,
                "Remote user '%s' is not owner of the file, access denied, "
                "consider maproot or making file world-readable",
                conn->username);
            return false;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG,
                "Remote user '%s' is not the owner of the file, "
                "but file is world readable, access granted",
                conn->username);                 /* access granted */
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG,
            "User '%s' is the owner of the file, access granted",
            conn->username);                     /* access granted */
    }

    /* ADMIT ACCESS, to summarise the following condition is now true: */

    /* Remote user is root, where "user" is just a string in the protocol, he
     * might claim whatever he wants but will be able to login only if the
     * user-key.pub key is found, */
    assert((conn->uid == 0) ||
    /* OR remote IP has maproot in the file's access_rules, */
           (conn->maproot == true) ||
    /* OR file is owned by the same username the user claimed - useless or
     * even dangerous outside NIS, KERBEROS or LDAP authenticated domains,  */
           (sb->st_uid == conn->uid) ||
    /* OR file is readable by everyone */
           (sb->st_mode & S_IROTH));

#endif

    return true;
}

static void AbortTransfer(ConnectionInfo *connection, char *filename)
{
    Log(LOG_LEVEL_VERBOSE, "Aborting transfer of file due to source changes");

    char sendbuffer[CF_BUFSIZE];
    snprintf(sendbuffer, CF_BUFSIZE, "%s%s: %s",
             CF_CHANGEDSTR1, CF_CHANGEDSTR2, filename);

    if (SendTransaction(connection, sendbuffer, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)",
            GetErrorStr());
    }
}

static void FailedTransfer(ConnectionInfo *connection)
{
    Log(LOG_LEVEL_VERBOSE, "Transfer failure");

    char sendbuffer[CF_BUFSIZE];

    snprintf(sendbuffer, CF_BUFSIZE, "%s", CF_FAILEDSTR);

    if (SendTransaction(connection, sendbuffer, 0, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)",
            GetErrorStr());
    }
}

void CfGetFile(ServerFileGetState *args)
{
    int fd;
    off_t n_read, total = 0, sendlen = 0, count = 0;
    char sendbuffer[CF_BUFSIZE + 256], filename[CF_BUFSIZE];
    struct stat sb;
    int blocksize = 2048;

    ConnectionInfo *conn_info = args->conn->conn_info;

    TranslatePath(filename, args->replyfile);

    stat(filename, &sb);

    Log(LOG_LEVEL_DEBUG, "CfGetFile('%s'), size = %jd",
        filename, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(args->conn, filename, &sb))
    {
        Log(LOG_LEVEL_INFO, "REFUSE access to file: %s", filename);
        RefuseAccess(args->conn, args->replyfile);
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
    char *key, enctype;
    struct stat sb;
    ConnectionInfo *conn_info = args->conn->conn_info;

    key = args->conn->session_key;
    enctype = args->conn->encryption_type;

    TranslatePath(filename, args->replyfile);

    stat(filename, &sb);

    Log(LOG_LEVEL_DEBUG, "CfEncryptGetFile('%s'), size = %jd",
        filename, (intmax_t) sb.st_size);

/* Now check to see if we have remote permission */

    if (!TransferRights(args->conn, filename, &sb))
    {
        Log(LOG_LEVEL_INFO, "REFUSE access to file: %s", filename);
        RefuseAccess(args->conn, args->replyfile);
        FailedTransfer(conn_info);
        return;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to allocate cipher: %s",
            TLSErrorString(ERR_get_error()));
        return;
    }

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
                EVP_EncryptInit_ex(ctx, CfengineCipher(enctype), NULL, key, iv);

                if (!EVP_EncryptUpdate(ctx, out, &cipherlen, sendbuffer, n_read))
                {
                    FailedTransfer(conn_info);
                    EVP_CIPHER_CTX_free(ctx);
                    close(fd);
                    return;
                }

                if (!EVP_EncryptFinal_ex(ctx, out + cipherlen, &finlen))
                {
                    FailedTransfer(conn_info);
                    EVP_CIPHER_CTX_free(ctx);
                    close(fd);
                    return;
                }
            }

            if (total >= savedlen)
            {
                if (SendTransaction(conn_info, out, cipherlen + finlen, CF_DONE) == -1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Send failed in GetFile. (send: %s)", GetErrorStr());
                    EVP_CIPHER_CTX_free(ctx);
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
                    EVP_CIPHER_CTX_free(ctx);
                    return;
                }
            }
        }
    }

    EVP_CIPHER_CTX_free(ctx);
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

    if (islink && (stat(filename, &statlinkbuf) != -1))       /* linktype=copy used by agent */
    {
        Log(LOG_LEVEL_DEBUG, "Getting size of link deref '%s'", linkbuf);
        statbuf.st_size = statlinkbuf.st_size;
        statbuf.st_mode = statlinkbuf.st_mode;
        statbuf.st_uid = statlinkbuf.st_uid;
        statbuf.st_gid = statlinkbuf.st_gid;
        statbuf.st_mtime = statlinkbuf.st_mtime;
        statbuf.st_ctime = statlinkbuf.st_ctime;
    }

#endif /* !__MINGW32__ */

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

    /* Is file sparse? */
    if (statbuf.st_size > ST_NBYTES(statbuf))
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
        cipherlen = EncryptString(out, sizeof(out),
                                  sendbuffer, strlen(sendbuffer) + 1,
                                  conn->encryption_type, conn->session_key);
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
    char sendbuffer[CF_BUFSIZE - CF_INBAND_OFFSET];

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
        int cipherlen = EncryptString(out, sizeof(out),
                                      sendbuffer, strlen(sendbuffer) + 1,
                                      conn->encryption_type, conn->session_key);
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

    offset = 0;
    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        /* Always leave MAXLINKSIZE bytes for CFD_TERMINATOR. Why??? */
        if (strlen(dirp->d_name) + 1 + offset >= CF_BUFSIZE - CF_MAXLINKSIZE)
        {
            /* Double '\0' indicates end of packet. */
            sendbuffer[offset] = '\0';
            SendTransaction(conn->conn_info, sendbuffer, offset + 1, CF_MORE);

            offset = 0;                                       /* new packet */
        }

        /* TODO fix copying names greater than 256. */
        strlcpy(sendbuffer + offset, dirp->d_name, CF_MAXLINKSIZE);
        offset += strlen(dirp->d_name) + 1;                  /* +1 for '\0' */
    }

    strcpy(sendbuffer + offset, CFD_TERMINATOR);
    offset += strlen(CFD_TERMINATOR) + 1;                    /* +1 for '\0' */
    /* Double '\0' indicates end of packet. */
    sendbuffer[offset] = '\0';
    SendTransaction(conn->conn_info, sendbuffer, offset + 1, CF_DONE);

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
        cipherlen = EncryptString(out, sizeof(out),
                                  sendbuffer, strlen(sendbuffer) + 1,
                                  conn->encryption_type, conn->session_key);
        SendTransaction(conn->conn_info, out, cipherlen, CF_DONE);
        return -1;
    }

    if ((dirh = DirOpen(dirname)) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Couldn't open dir %s", dirname);
        snprintf(sendbuffer, CF_BUFSIZE, "BAD: cfengine, couldn't open dir %s", dirname);
        cipherlen = EncryptString(out, sizeof(out),
                                  sendbuffer, strlen(sendbuffer) + 1,
                                  conn->encryption_type, conn->session_key);
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
            cipherlen = EncryptString(out, sizeof(out),
                                      sendbuffer, offset + 1,
                                      conn->encryption_type, conn->session_key);
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
        EncryptString(out, sizeof(out),
                      sendbuffer, offset + 2 + strlen(CFD_TERMINATOR),
                      conn->encryption_type, conn->session_key);
    SendTransaction(conn->conn_info, out, cipherlen, CF_DONE);
    DirClose(dirh);
    return 0;
}


/********************* MISC UTILITY FUNCTIONS *************************/


/**
 * Search and replace occurrences of #find1, #find2, #find3, with
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
        struct stat statbuf;
        if ((lstat(reqpath, &statbuf) == 0) && S_ISLNK(statbuf.st_mode))
        {
            Log(LOG_LEVEL_VERBOSE, "Requested file is a dead symbolic link (filename: %s)", reqpath);
            strlcpy(dst, reqpath, CF_BUFSIZE);
        }
        else
        {
            Log(LOG_LEVEL_INFO,
                "Failed to canonicalise filename '%s' (realpath: %s)",
                reqpath, GetErrorStr());
            return (size_t) -1;
        }
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


/**
 * Set conn->uid (and conn->sid on Windows).
 */
void SetConnIdentity(ServerConnectionState *conn, const char *username)
{
    size_t username_len = strlen(username);

    conn->uid = CF_UNKNOWN_OWNER;
    conn->username[0] = '\0';

    if (username_len < sizeof(conn->username))
    {
        memcpy(conn->username, username, username_len + 1);
    }

    bool is_root = strcmp(conn->username, "root") == 0;
    if (is_root)
    {
        /* If the remote user identifies himself as root, even on Windows
         * cf-serverd must grant access to all files. uid==0 is checked later
         * in TranferRights() for that. */
        conn->uid = 0;
    }

#ifdef __MINGW32__            /* NT uses security identifier instead of uid */

    if (!NovaWin_UserNameToSid(conn->username, (SID *) conn->sid,
                               CF_MAXSIDSIZE, !is_root))
    {
        memset(conn->sid, 0, CF_MAXSIDSIZE);  /* is invalid sid - discarded */
    }

#else                                                 /* UNIX - common path */

    if (conn->uid == CF_UNKNOWN_OWNER)      /* skip looking up UID for root */
    {
        static pthread_mutex_t pwnam_mtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
        struct passwd *pw = NULL;

        if (ThreadLock(&pwnam_mtx))
        {
            /* TODO Redmine#7643: looking up the UID is expensive and should
             * not be needed, since today's agent machine VS hub most probably
             * do not share the accounts. */
            pw = getpwnam(conn->username);
            if (pw != NULL)
            {
                conn->uid = pw->pw_uid;
            }
            ThreadUnlock(&pwnam_mtx);
        }
    }

#endif
}


static bool CharsetAcceptable(const char *s, size_t s_len)
{
    const char *ACCEPT =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_:";
    size_t acceptable_chars = strspn(s, ACCEPT);
    if (s_len == 0)
    {
        s_len = strlen(s);
    }

    if (acceptable_chars < s_len)
    {
        Log(LOG_LEVEL_INFO,
            "llegal character in column %zu of: %s",
            acceptable_chars, s);
        return false;
    }

    return true;
}


/**
 * @param #args_start is a comma separated list of words, which may be
 *                    prefixed with spaces and suffixed with spaces and other
 *                    words. Example: " asd,fgh,jk blah". In this example the
 *                    list has 3 words, and "blah" is not one of them.
 *
 * Both #args_start and #args_len are in-out parameters.
 * At the end of execution #args_start returns the real start of the list, and
 * #args_len the real length.
 */
static bool AuthorizeDelimitedArgs(const ServerConnectionState *conn,
                                   struct acl *acl,
                                   char **args_start, size_t *args_len)
{
    char *s;
    size_t s_len, skip;

    assert(args_start != NULL);
    assert(args_len != NULL);

    /* Give the name s and s_len purely for ease of use. */
    s_len = *args_len;
    s     = *args_start;
    /* Skip spaces in the beginning of argument list. */
    skip  = strspn(s, " \t");
    s    += skip;

    if (s_len == 0)                        /* if end was not given, find it */
    {
        s_len = strcspn(s, " \t");
    }
    else                                                /* if end was given */
    {
        s_len = (skip <= s_len) ? (s_len - skip) : 0;
    }

    /* Admit, unless any token fails to be authorised. */
    bool admit = true;
    if (s_len > 0)
    {
        const char tmp_c = s[s_len];
        s[s_len] = '\0';

        /* Iterate over comma-separated list. */

        char *token = &s[0];
        while (token < &s[s_len] && admit)
        {
            char *token_end = strchrnul(token, ',');

            const char tmp_sep = *token_end;
            *token_end = '\0';

            if (!CharsetAcceptable(token, 0) ||
                !acl_CheckRegex(acl, token,
                                conn->ipaddr, conn->revdns,
                                KeyPrintableHash(conn->conn_info->remote_key),
                                conn->username))
            {
                Log(LOG_LEVEL_INFO, "Access denied to: %s", token);
                admit = false;                              /* EARLY RETURN */
            }

            *token_end = tmp_sep;
            token      = token_end + 1;
        }

        s[s_len] = tmp_c;
    }

    *args_start = s;
    *args_len   = s_len;
    return admit;
}


/**
 * @return #true if the connection should remain open for next requests, or
 *         #false if the server should actively close it - for example when
 *         protocol errors have occurred.
 */
bool DoExec2(const EvalContext *ctx,
             ServerConnectionState *conn,
             char *exec_args,
             char *sendbuf, size_t sendbuf_size)
{
    /* STEP 0: Verify cfruncommand was successfully configured. */
    if (NULL_OR_EMPTY(CFRUNCOMMAND))
    {
        Log(LOG_LEVEL_INFO, "EXEC denied due to empty cfruncommand");
        RefuseAccess(conn, "EXEC");
        return false;
    }

    /* STEP 1: Resolve and check permissions of CFRUNCOMMAND's arg0. IT is
     *         done now and not at configuration time, as the file stat may
     *         have changed since then. */
    {
        char arg0[PATH_MAX];
        if (CommandArg0_bound(arg0, CFRUNCOMMAND, sizeof(arg0)) == (size_t) -1 ||
            PreprocessRequestPath(arg0, sizeof(arg0))           == (size_t) -1)
        {
            Log(LOG_LEVEL_INFO, "EXEC failed, invalid cfruncommand arg0");
            RefuseAccess(conn, "EXEC");
            return false;
        }

        /* Check body server access_rules, whether arg0 is authorized. */

        /* TODO EXEC should not just use paths_acl access control, but
         * specific "exec_path" ACL. Then different command execution could be
         * allowed per host, and the host could even set argv[0] in his EXEC
         * request, rather than only the arguments. */

        if (acl_CheckPath(paths_acl, arg0,
                          conn->ipaddr, conn->revdns,
                          KeyPrintableHash(conn->conn_info->remote_key))
            == false)
        {
            Log(LOG_LEVEL_INFO, "EXEC denied due to ACL for file: %s", arg0);
            RefuseAccess(conn, "EXEC");
            return false;
        }
    }

    /* STEP 2: Check body server control "allowusers" */
    if (!AllowedUser(conn->username))
    {
        Log(LOG_LEVEL_INFO, "EXEC denied due to not allowed user: %s",
            conn->username);
        RefuseAccess(conn, "EXEC");
        return false;
    }

    /* STEP 3: This matches cf-runagent -s class1,class2 against classes
     *         set during cf-serverd's policy evaluation. */

    if (!MatchClasses(ctx, conn))
    {
        snprintf(sendbuf, sendbuf_size,
                 "EXEC denied due to failed class match (check cf-serverd verbose output)");
        Log(LOG_LEVEL_INFO, "%s", sendbuf);
        SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE);
        return true;
    }


    /* STEP 4: Parse and authorise the EXEC arguments, which will be used as
     *         arguments to CFRUNCOMMAND. Currently we only accept
     *         [ -D classlist ] and [ -b bundlesequence ] arguments. */

    char   cmdbuf[CF_BUFSIZE] = "";
    size_t cmdbuf_len         = 0;

    assert(sizeof(CFRUNCOMMAND) <= sizeof(cmdbuf));

    StrCat(cmdbuf, sizeof(cmdbuf), &cmdbuf_len, CFRUNCOMMAND, 0);

    exec_args += strspn(exec_args,  " \t");                  /* skip spaces */
    while (exec_args[0] != '\0')
    {
        if (strncmp(exec_args, "-D", 2) == 0)
        {
            exec_args += 2;

            char *classlist = exec_args;
            size_t classlist_len = 0;
            bool allow = AuthorizeDelimitedArgs(conn, roles_acl,
                                                &classlist, &classlist_len);
            if (!allow)
            {
                snprintf(sendbuf, sendbuf_size,
                         "EXEC denied role activation (check cf-serverd verbose output)");
                Log(LOG_LEVEL_INFO, "%s", sendbuf);
                SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE);
                return true;
            }

            if (classlist_len > 0)
            {
                /* Append "-D classlist" to cfruncommand. */
                StrCat(cmdbuf, sizeof(cmdbuf), &cmdbuf_len,
                       " -D ", 0);
                StrCat(cmdbuf, sizeof(cmdbuf), &cmdbuf_len,
                       classlist, classlist_len);
            }

            exec_args = classlist + classlist_len;
        }
        else if (strncmp(exec_args, "-b", 2) == 0)
        {
            exec_args += 2;

            char *bundlesequence = exec_args;
            size_t bundlesequence_len = 0;

            bool allow = AuthorizeDelimitedArgs(conn, bundles_acl,
                                                &bundlesequence,
                                                &bundlesequence_len);
            if (!allow)
            {
                snprintf(sendbuf, sendbuf_size,
                         "EXEC denied bundle activation (check cf-serverd verbose output)");
                Log(LOG_LEVEL_INFO, "%s", sendbuf);
                SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE);
                return true;
            }

            if (bundlesequence_len > 0)
            {
                /* Append "--bundlesequence bundlesequence" to cfruncommand. */
                StrCat(cmdbuf, sizeof(cmdbuf), &cmdbuf_len,
                       " --bundlesequence ", 0);
                StrCat(cmdbuf, sizeof(cmdbuf), &cmdbuf_len,
                       bundlesequence, bundlesequence_len);
            }

            exec_args = bundlesequence + bundlesequence_len;
        }
        else                                        /* disallowed parameter */
        {
            snprintf(sendbuf, sendbuf_size,
                     "EXEC denied: invalid arguments: %s",
                     exec_args);
            Log(LOG_LEVEL_INFO, "%s", sendbuf);
            SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE);
            return true;
        }

        exec_args += strspn(exec_args,  " \t");              /* skip spaces */
    }

    if (cmdbuf_len >= sizeof(cmdbuf))
    {
        snprintf(sendbuf, sendbuf_size,
                 "EXEC denied: too long (%zu B) command: %s",
                 cmdbuf_len, cmdbuf);
        Log(LOG_LEVEL_INFO, "%s", sendbuf);
        SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE);
        return false;
    }

    /* STEP 5: RUN CFRUNCOMMAND. */

    snprintf(sendbuf, sendbuf_size,
             "cf-serverd executing cfruncommand: %s\n",
             cmdbuf);
    SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE);
    Log(LOG_LEVEL_INFO, "%s", sendbuf);

    FILE *pp = cf_popen(cmdbuf, "r", true);
    if (pp == NULL)
    {
        snprintf(sendbuf, sendbuf_size,
                 "Unable to run '%s' (pipe: %s)",
                 cmdbuf, GetErrorStr());
        Log(LOG_LEVEL_INFO, "%s", sendbuf);
        SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE);
        return false;
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);
    while (true)
    {
        ssize_t res = CfReadLine(&line, &line_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                /* Error reading, discard all unconsumed input before
                 * aborting - linux-specific! */
                fflush(pp);
            }
            break;
        }

        /* NOTICE: we can't SendTransaction() overlong strings, and we need to
         * prepend and append to the string. */
        size_t line_len = strlen(line);
        if (line_len >= sendbuf_size - 5)
        {
            line[sendbuf_size - 5] = '\0';
        }

        /* Prefixing output with "> " and postfixing with '\n' is new
         * behaviour as of 3.7.0. Prefixing happens to avoid zero-length
         * transaction packet. */
        /* Old cf-runagent versions do not append a newline, so we must do
         * it here. New ones do though, so TODO deprecate. */
        xsnprintf(sendbuf, sendbuf_size, "> %s\n", line);
        if (SendTransaction(conn->conn_info, sendbuf, 0, CF_DONE) == -1)
        {
            Log(LOG_LEVEL_INFO,
                "Sending failed, aborting EXEC (send: %s)",
                GetErrorStr());
            break;
        }
    }
    free(line);
    cf_pclose(pp);

    return true;
}
