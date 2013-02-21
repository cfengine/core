
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

#include "client_code.h"

#include "communication.h"
#include "net.h"
#include "sysinfo.h"
#include "dir.h"
#include "dir_priv.h"
#include "client_protocol.h"
#include "crypto.h"
#include "cfstream.h"
#include "files_hashes.h"
#include "files_copy.h"
#include "transaction.h"
#include "logging.h"
#include "rlist.h"
#include "policy.h"
#include "item_lib.h"

typedef struct
{
    char *server;
    AgentConnection *conn;
    int busy;
} ServerItem;

#define CFENGINE_SERVICE "cfengine"

/* seconds */
#define RECVTIMEOUT 30

#define CF_COULD_NOT_CONNECT -2

Rlist *SERVERLIST = NULL;

static void NewClientCache(Stat *data, Promise *pp);
static void CacheServerConnection(AgentConnection *conn, const char *server);
static void MarkServerOffline(const char *server);
static AgentConnection *GetIdleConnectionToServer(const char *server);
static bool ServerOffline(const char *server);
static void FlushFileStream(int sd, int toget);
static int CacheStat(const char *file, struct stat *statbuf, const char *stattype, Attributes attr, Promise *pp);

#if !defined(__MINGW32__)
static int TryConnect(AgentConnection *conn, struct timeval *tvp, struct sockaddr *cinp, int cinpSz);
#endif

/*********************************************************************/

static int FSWrite(char *new, int dd, char *buf, int towrite, int *last_write_made_hole, int n_read, Attributes attr,
                   Promise *pp)
{
    int *intp;
    char *cp;

    intp = 0;

    if (pp && (pp->makeholes))
    {
        buf[n_read] = 1;        /* Sentinel to stop loop.  */

        /* Find first non-zero *word*, or the word with the sentinel.  */
        intp = (int *) buf;

        while (*intp++ == 0)
        {
        }

        /* Find the first non-zero *byte*, or the sentinel.  */

        cp = (char *) (intp - 1);

        while (*cp++ == 0)
        {
        }

        /* If we found the sentinel, the whole input block was zero,
           and we can make a hole.  */

        if (cp > buf + n_read)
        {
            /* Make a hole.  */

            if (lseek(dd, (off_t) n_read, SEEK_CUR) < 0L)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "lseek", "lseek in EmbeddedWrite, dest=%s\n", new);
                return false;
            }

            *last_write_made_hole = 1;
        }
        else
        {
            /* Clear to indicate that a normal write is needed. */
            intp = 0;
        }
    }

    if (intp == 0)
    {
        if (FullWrite(dd, buf, towrite) < 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "write", "Local disk write(%.256s) failed\n", new);
            pp->conn->error = true;
            return false;
        }

        *last_write_made_hole = 0;
    }

    return true;
}

void DetermineCfenginePort()
{
    struct servent *server;

    errno = 0;
    if ((server = getservbyname(CFENGINE_SERVICE, "tcp")) == NULL)
    {
        if (errno == 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "No registered cfengine service, using default");
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "getservbyname", "Unable to query services database, using default");
        }
        snprintf(STR_CFENGINEPORT, 15, "5308");
        SHORT_CFENGINEPORT = htons((unsigned short) 5308);
    }
    else
    {
        snprintf(STR_CFENGINEPORT, 15, "%u", ntohs(server->s_port));
        SHORT_CFENGINEPORT = server->s_port;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Setting cfengine default port to %u = %s\n", ntohs(SHORT_CFENGINEPORT), STR_CFENGINEPORT);
}

/*********************************************************************/

AgentConnection *NewServerConnection(Attributes attr, Promise *pp)
{
    AgentConnection *conn;
    Rlist *rp;

// First one in goal has to open the connection, or mark it failed or private (thread)

    // We never close a non-background connection until end
    // mark serial connections as such

    for (rp = attr.copy.servers; rp != NULL; rp = rp->next)
    {
        if (ServerOffline(rp->item))
        {
            continue;
        }

        pp->this_server = rp->item;

        if (attr.transaction.background)
        {
            if (RlistLen(SERVERLIST) < CFA_MAXTHREADS)
            {
                conn = ServerConnection(rp->item, attr, pp);
                return conn;
            }
        }
        else
        {
            if ((conn = GetIdleConnectionToServer(rp->item)))
            {
                return conn;
            }

            /* This is first usage, need to open */

            conn = ServerConnection(rp->item, attr, pp);

            if (conn == NULL)
            {
                cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, "Unable to establish connection with %s\n", RlistScalarValue(rp));
                MarkServerOffline(rp->item);
            }
            else
            {
                CacheServerConnection(conn, rp->item);
                return conn;
            }
        }
    }

    pp->this_server = NULL;
    return NULL;
}

/*****************************************************************************/

AgentConnection *ServerConnection(char *server, Attributes attr, Promise *pp)
{
    AgentConnection *conn;

#if !defined(__MINGW32__)
    signal(SIGPIPE, SIG_IGN);
#endif /* !__MINGW32__ */

#if !defined(__MINGW32__)
    static sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
#endif

    conn = NewAgentConn();

    if (strcmp(server, "localhost") == 0)
    {
        conn->authenticated = true;
        return conn;
    }

    conn->authenticated = false;
    conn->encryption_type = CfEnterpriseOptions();

/* username of the client - say root from Windows */

#ifdef __MINGW32__
    snprintf(conn->username, CF_SMALLBUF, "root");
#else
    GetCurrentUserName(conn->username, CF_SMALLBUF);
#endif /* !__MINGW32__ */

    if (conn->sd == SOCKET_INVALID)
    {
        CfDebug("Opening server connection to %s\n", server);

        if (!ServerConnect(conn, server, attr, pp))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! No server is responding on this port");

            if (conn->sd != SOCKET_INVALID)
            {
                DisconnectServer(conn);
            }

            return NULL;
        }

        if (conn->sd == SOCKET_INVALID)
        {
            return NULL;
        }

        CfDebug("Remote IP set to %s\n", conn->remoteip);

        if (!IdentifyAgent(conn->sd, conn->localip, conn->family))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Id-authentication for %s failed\n", VFQNAME);
            errno = EPERM;
            DisconnectServer(conn);
            return NULL;
        }

        if (!AuthenticateAgent(conn, attr, pp))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Authentication dialogue with %s failed\n", server);
            errno = EPERM;
            DisconnectServer(conn);
            return NULL;
        }

        conn->authenticated = true;
        return conn;
    }
    else
    {
        CfDebug("Server connection to %s already open on %d\n", server, conn->sd);
    }

    return conn;
}

/*********************************************************************/

void DisconnectServer(AgentConnection *conn)
{
    CfDebug("Closing current server connection\n");

    if (conn)
    {
        if (conn->sd != SOCKET_INVALID)
        {
            cf_closesocket(conn->sd);
            conn->sd = SOCKET_INVALID;
        }
        DeleteAgentConn(conn);
    }
}

/*********************************************************************/

int cf_remote_stat(char *file, struct stat *buf, char *stattype, Attributes attr, Promise *pp)
/* If a link, this reads readlink and sends it back in the same
   package. It then caches the value for each copy command */
{
    char sendbuffer[CF_BUFSIZE];
    char recvbuffer[CF_BUFSIZE];
    char in[CF_BUFSIZE], out[CF_BUFSIZE];
    AgentConnection *conn = pp->conn;
    int ret, tosend, cipherlen;
    time_t tloc;

    CfDebug("cf_remotestat(%s,%s)\n", file, stattype);
    memset(recvbuffer, 0, CF_BUFSIZE);

    if (strlen(file) > CF_BUFSIZE - 30)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Filename too long");
        return -1;
    }

    ret = CacheStat(file, buf, stattype, attr, pp);

    if (ret != 0)
    {
        return ret;
    }

    if ((tloc = time((time_t *) NULL)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Couldn't read system clock\n");
    }

    sendbuffer[0] = '\0';

    if (attr.copy.encrypt)
    {
        if (conn->session_key == NULL)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, " !! Cannot do encrypted copy without keys (use cf-key)");
            return -1;
        }

        snprintf(in, CF_BUFSIZE - 1, "SYNCH %jd STAT %s", (intmax_t) tloc, file);
        cipherlen = EncryptString(conn->encryption_type, in, out, conn->session_key, strlen(in) + 1);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "SSYNCH %d", cipherlen);
        memcpy(sendbuffer + CF_PROTO_OFFSET, out, cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "SYNCH %jd STAT %s", (intmax_t) tloc, file);
        tosend = strlen(sendbuffer);
    }

    if (SendTransaction(conn->sd, sendbuffer, tosend, CF_DONE) == -1)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "send", pp, attr, "Transmission failed/refused talking to %.255s:%.255s in stat",
             pp->this_server, file);
        return -1;
    }

    if (ReceiveTransaction(conn->sd, recvbuffer, NULL) == -1)
    {
        return -1;
    }

    if (strstr(recvbuffer, "unsynchronized"))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Clocks differ too much to do copy by date (security) %s", recvbuffer + 4);
        return -1;
    }

    if (BadProtoReply(recvbuffer))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Server returned error: %s\n", recvbuffer + 4);
        errno = EPERM;
        return -1;
    }

    if (OKProtoReply(recvbuffer))
    {
        Stat cfst;

        // use intmax_t here to provide enough space for large values coming over the protocol
        intmax_t d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12 = 0, d13 = 0;
        ret = sscanf(recvbuffer, "OK: "
               "%1" PRIdMAX     // 01 cfst.cf_type
               " %5" PRIdMAX    // 02 cfst.cf_mode
               " %14" PRIdMAX   // 03 cfst.cf_lmode
               " %14" PRIdMAX   // 04 cfst.cf_uid
               " %14" PRIdMAX   // 05 cfst.cf_gid
               " %18" PRIdMAX   // 06 cfst.cf_size
               " %14" PRIdMAX   // 07 cfst.cf_atime
               " %14" PRIdMAX   // 08 cfst.cf_mtime
               " %14" PRIdMAX   // 09 cfst.cf_ctime
               " %1" PRIdMAX    // 10 cfst.cf_makeholes
               " %14" PRIdMAX   // 11 cfst.cf_ino
               " %14" PRIdMAX   // 12 cfst.cf_nlink
               " %18" PRIdMAX,  // 13 cfst.cf_dev
               &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9, &d10, &d11, &d12, &d13);

        if (ret < 13)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Cannot read SYNCH reply from %s: only %d/13 items parsed", conn->remoteip, ret );
            return -1;
        }

        cfst.cf_type = (FileType) d1;
        cfst.cf_mode = (mode_t) d2;
        cfst.cf_lmode = (mode_t) d3;
        cfst.cf_uid = (uid_t) d4;
        cfst.cf_gid = (gid_t) d5;
        cfst.cf_size = (off_t) d6;
        cfst.cf_atime = (time_t) d7;
        cfst.cf_mtime = (time_t) d8;
        cfst.cf_ctime = (time_t) d9;
        cfst.cf_makeholes = (char) d10;
        pp->makeholes = (char) d10;
        cfst.cf_ino = d11;
        cfst.cf_nlink = d12;
        cfst.cf_dev = (dev_t)d13;

        /* Use %?d here to avoid memory overflow attacks */

        CfDebug("Mode = %u, %u\n", cfst.cf_mode, cfst.cf_lmode);

        CfDebug
            ("OK: type=%d\n mode=%" PRIoMAX "\n lmode=%" PRIoMAX "\n uid=%" PRIuMAX "\n gid=%" PRIuMAX "\n size=%ld\n atime=%" PRIdMAX "\n mtime=%" PRIdMAX " ino=%d nlnk=%d, dev=%" PRIdMAX "\n",
             cfst.cf_type, (uintmax_t)cfst.cf_mode, (uintmax_t)cfst.cf_lmode, (uintmax_t)cfst.cf_uid, (uintmax_t)cfst.cf_gid, (long) cfst.cf_size,
             (intmax_t) cfst.cf_atime, (intmax_t) cfst.cf_mtime, cfst.cf_ino, cfst.cf_nlink, (intmax_t) cfst.cf_dev);

        memset(recvbuffer, 0, CF_BUFSIZE);

        if (ReceiveTransaction(conn->sd, recvbuffer, NULL) == -1)
        {
            return -1;
        }

        CfDebug("Linkbuffer: %s\n", recvbuffer);

        if (strlen(recvbuffer) > 3)
        {
            cfst.cf_readlink = xstrdup(recvbuffer + 3);
        }
        else
        {
            cfst.cf_readlink = NULL;
        }

        switch (cfst.cf_type)
        {
        case FILE_TYPE_REGULAR:
            cfst.cf_mode |= (mode_t) S_IFREG;
            break;
        case FILE_TYPE_DIR:
            cfst.cf_mode |= (mode_t) S_IFDIR;
            break;
        case FILE_TYPE_CHAR:
            cfst.cf_mode |= (mode_t) S_IFCHR;
            break;
        case FILE_TYPE_FIFO:
            cfst.cf_mode |= (mode_t) S_IFIFO;
            break;
        case FILE_TYPE_SOCK:
            cfst.cf_mode |= (mode_t) S_IFSOCK;
            break;
        case FILE_TYPE_BLOCK:
            cfst.cf_mode |= (mode_t) S_IFBLK;
            break;
        case FILE_TYPE_LINK:
            cfst.cf_mode |= (mode_t) S_IFLNK;
            break;
        }

        cfst.cf_filename = xstrdup(file);
        cfst.cf_server = xstrdup(pp->this_server);

        if ((cfst.cf_filename == NULL) || (cfst.cf_server == NULL))
        {
            FatalError("Memory allocation in cf_rstat");
        }

        cfst.cf_failed = false;

        if (cfst.cf_lmode != 0)
        {
            cfst.cf_lmode |= (mode_t) S_IFLNK;
        }

        NewClientCache(&cfst, pp);

        if ((cfst.cf_lmode != 0) && (strcmp(stattype, "link") == 0))
        {
            buf->st_mode = cfst.cf_lmode;
        }
        else
        {
            buf->st_mode = cfst.cf_mode;
        }

        buf->st_uid = cfst.cf_uid;
        buf->st_gid = cfst.cf_gid;
        buf->st_size = cfst.cf_size;
        buf->st_mtime = cfst.cf_mtime;
        buf->st_ctime = cfst.cf_ctime;
        buf->st_atime = cfst.cf_atime;
        buf->st_ino = cfst.cf_ino;
        buf->st_dev = cfst.cf_dev;
        buf->st_nlink = cfst.cf_nlink;

        return 0;
    }

    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Transmission refused or failed statting %s\nGot: %s\n", file, recvbuffer);
    errno = EPERM;
    return -1;
}

/*********************************************************************/

Dir *OpenDirRemote(const char *dirname, Attributes attr, Promise *pp)
{
    AgentConnection *conn = pp->conn;
    char sendbuffer[CF_BUFSIZE];
    char recvbuffer[CF_BUFSIZE];
    char in[CF_BUFSIZE];
    char out[CF_BUFSIZE];
    int n, cipherlen = 0, tosend;
    Dir *cfdirh;
    char *sp;
    Item *files = NULL;

    CfDebug("CfOpenDir(%s:%s)\n", pp->this_server, dirname);

    if (strlen(dirname) > CF_BUFSIZE - 20)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Directory name too long");
        return NULL;
    }

    cfdirh = xcalloc(1, sizeof(Dir));

    if (attr.copy.encrypt)
    {
        if (conn->session_key == NULL)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, " !! Cannot do encrypted copy without keys (use cf-key)");
            return NULL;
            free(cfdirh);
        }

        snprintf(in, CF_BUFSIZE, "OPENDIR %s", dirname);
        cipherlen = EncryptString(conn->encryption_type, in, out, conn->session_key, strlen(in) + 1);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "SOPENDIR %d", cipherlen);
        memcpy(sendbuffer + CF_PROTO_OFFSET, out, cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "OPENDIR %s", dirname);
        tosend = strlen(sendbuffer);
    }

    if (SendTransaction(conn->sd, sendbuffer, tosend, CF_DONE) == -1)
    {
        free((char *) cfdirh);
        return NULL;
    }

    while (true)
    {
        if ((n = ReceiveTransaction(conn->sd, recvbuffer, NULL)) == -1)
        {
            free((char *) cfdirh);
            return NULL;
        }

        if (n == 0)
        {
            break;
        }

        if (attr.copy.encrypt)
        {
            memcpy(in, recvbuffer, n);
            DecryptString(conn->encryption_type, in, recvbuffer, conn->session_key, n);
        }

        if (FailedProtoReply(recvbuffer))
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "Network access to %s:%s denied\n", pp->this_server, dirname);
            free((char *) cfdirh);
            return NULL;
        }

        if (BadProtoReply(recvbuffer))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "%s\n", recvbuffer + 4);
            free((char *) cfdirh);
            return NULL;
        }

        for (sp = recvbuffer; *sp != '\0'; sp++)
        {
            Item *ip;

            if (strncmp(sp, CFD_TERMINATOR, strlen(CFD_TERMINATOR)) == 0)       /* End transmission */
            {
                cfdirh->listpos = cfdirh->list;
                return cfdirh;
            }

            ip = xcalloc(1, sizeof(Item));
            ip->name = (char *) AllocateDirentForFilename(sp);

            if (files == NULL)  /* First element */
            {
                cfdirh->list = ip;
                files = ip;
            }
            else
            {
                files->next = ip;
                files = ip;
            }

            while (*sp != '\0')
            {
                sp++;
            }
        }
    }

    cfdirh->listpos = cfdirh->list;
    return cfdirh;
}

/*********************************************************************/

static void NewClientCache(Stat *data, Promise *pp)
{
    Stat *sp;

    CfDebug("NewClientCache\n");

    sp = xmalloc(sizeof(Stat));

    memcpy(sp, data, sizeof(Stat));

    sp->next = pp->cache;
    pp->cache = sp;
}

/*********************************************************************/

void DeleteClientCache(Attributes attr, Promise *pp)
{
    Stat *sp, *sps;

    CfDebug("DeleteClientCache\n");

    sp = pp->cache;

    while (sp != NULL)
    {
        sps = sp;
        sp = sp->next;
        free((char *) sps);
    }

    pp->cache = NULL;
}

/*********************************************************************/

int CompareHashNet(char *file1, char *file2, Attributes attr, Promise *pp)
{
    static unsigned char d[EVP_MAX_MD_SIZE + 1];
    char *sp, sendbuffer[CF_BUFSIZE], recvbuffer[CF_BUFSIZE], in[CF_BUFSIZE], out[CF_BUFSIZE];
    int i, tosend, cipherlen;
    AgentConnection *conn = pp->conn;

    HashFile(file2, d, CF_DEFAULT_DIGEST);
    CfDebug("Send digest of %s to server, %s\n", file2, HashPrint(CF_DEFAULT_DIGEST, d));

    memset(recvbuffer, 0, CF_BUFSIZE);

    if (attr.copy.encrypt)
    {
        snprintf(in, CF_BUFSIZE, "MD5 %s", file1);

        sp = in + strlen(in) + CF_SMALL_OFFSET;

        for (i = 0; i < CF_DEFAULT_DIGEST_LEN; i++)
        {
            *sp++ = d[i];
        }

        cipherlen =
            EncryptString(conn->encryption_type, in, out, conn->session_key,
                          strlen(in) + CF_SMALL_OFFSET + CF_DEFAULT_DIGEST_LEN);
        snprintf(sendbuffer, CF_BUFSIZE, "SMD5 %d", cipherlen);
        memcpy(sendbuffer + CF_PROTO_OFFSET, out, cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "MD5 %s", file1);
        sp = sendbuffer + strlen(sendbuffer) + CF_SMALL_OFFSET;

        for (i = 0; i < CF_DEFAULT_DIGEST_LEN; i++)
        {
            *sp++ = d[i];
        }

        tosend = strlen(sendbuffer) + CF_SMALL_OFFSET + CF_DEFAULT_DIGEST_LEN;
    }

    if (SendTransaction(conn->sd, sendbuffer, tosend, CF_DONE) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "send", pp, attr, "Failed send");
        return false;
    }

    if (ReceiveTransaction(conn->sd, recvbuffer, NULL) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "recv", pp, attr, "Failed send");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "No answer from host, assuming checksum ok to avoid remote copy for now...\n");
        return false;
    }

    if (strcmp(CFD_TRUE, recvbuffer) == 0)
    {
        CfDebug("Hash mismatch: (reply - %s)\n", recvbuffer);
        return true;            /* mismatch */
    }
    else
    {
        CfDebug("Hash matched ok: (reply - %s)\n", recvbuffer);
        return false;
    }

/* Not reached */
}

/*********************************************************************/

int CopyRegularFileNet(char *source, char *new, off_t size, Attributes attr, Promise *pp)
{
    int dd, buf_size, n_read = 0, toget, towrite;
    int last_write_made_hole = 0, done = false, tosend, value;
    char *buf, workbuf[CF_BUFSIZE], cfchangedstr[265];

    off_t n_read_total = 0;
    EVP_CIPHER_CTX ctx;
    AgentConnection *conn = pp->conn;

    snprintf(cfchangedstr, 255, "%s%s", CF_CHANGEDSTR1, CF_CHANGEDSTR2);

    if ((strlen(new) > CF_BUFSIZE - 20))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Filename too long");
        return false;
    }

    unlink(new);                /* To avoid link attacks */

    if ((dd = open(new, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, 0600)) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "open", pp, attr,
             " !! NetCopy to destination %s:%s security - failed attempt to exploit a race? (Not copied)\n",
             pp->this_server, new);
        unlink(new);
        return false;
    }

    workbuf[0] = '\0';

    buf_size = 2048;

/* Send proposition C0 */

    snprintf(workbuf, CF_BUFSIZE, "GET %d %s", buf_size, source);
    tosend = strlen(workbuf);

    if (SendTransaction(conn->sd, workbuf, tosend, CF_DONE) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Couldn't send data");
        close(dd);
        return false;
    }

    buf = xmalloc(CF_BUFSIZE + sizeof(int));    /* Note CF_BUFSIZE not buf_size !! */
    n_read_total = 0;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Copying remote file %s:%s, expecting %jd bytes",
          pp->this_server, source, (intmax_t)size);

    while (!done)
    {
        if ((size - n_read_total) >= buf_size)
        {
            toget = towrite = buf_size;
        }
        else if (size != 0)
        {
            towrite = (size - n_read_total);
            toget = towrite;
        }
        else
        {
            toget = towrite = 0;
        }

        /* Stage C1 - receive */

        if ((n_read = RecvSocketStream(conn->sd, buf, toget, 0)) == -1)
        {
            /* This may happen on race conditions,
             * where the file has shrunk since we asked for its size in SYNCH ... STAT source */

            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Error in client-server stream (has %s:%s shrunk?)", pp->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        /* If the first thing we get is an error message, break. */

        if ((n_read_total == 0) && (strncmp(buf, CF_FAILEDSTR, strlen(CF_FAILEDSTR)) == 0))
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "Network access to %s:%s denied\n", pp->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        if (strncmp(buf, cfchangedstr, strlen(cfchangedstr)) == 0)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "Source %s:%s changed while copying\n", pp->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        value = -1;

        /* Check for mismatch between encryption here and on server - can lead to misunderstanding */

        sscanf(buf, "t %d", &value);

        if ((value > 0) && (strncmp(buf + CF_INBAND_OFFSET, "BAD: ", 5) == 0))
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "Network access to cleartext %s:%s denied\n", pp->this_server,
                 source);
            close(dd);
            free(buf);
            return false;
        }

        if (!FSWrite(new, dd, buf, towrite, &last_write_made_hole, n_read, attr, pp))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, " !! Local disk write failed copying %s:%s to %s\n", pp->this_server,
                 source, new);
            free(buf);
            unlink(new);
            close(dd);
            FlushFileStream(conn->sd, size - n_read_total);
            EVP_CIPHER_CTX_cleanup(&ctx);
            return false;
        }

        n_read_total += towrite;        /* n_read; */

        if (n_read_total >= size)        /* Handle EOF without closing socket */
        {
            done = true;
        }
    }

    /* If the file ends with a `hole', something needs to be written at
       the end.  Otherwise the kernel would truncate the file at the end
       of the last write operation. Write a null character and truncate
       it again.  */

    if (last_write_made_hole)
    {
        if ((FullWrite(dd, "", 1) < 0) || (ftruncate(dd, n_read_total) < 0))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "FullWrite or ftruncate error in CopyReg, source %s\n", source);
            free(buf);
            unlink(new);
            close(dd);
            FlushFileStream(conn->sd, size - n_read_total);
            return false;
        }
    }

    CfDebug("End of CopyNetReg\n");
    close(dd);
    free(buf);
    return true;
}

/*********************************************************************/

int EncryptCopyRegularFileNet(char *source, char *new, off_t size, Attributes attr, Promise *pp)
{
    int dd, blocksize = 2048, n_read = 0, towrite, plainlen, more = true, finlen, cnt = 0;
    int last_write_made_hole = 0, tosend, cipherlen = 0;
    char *buf, in[CF_BUFSIZE], out[CF_BUFSIZE], workbuf[CF_BUFSIZE], cfchangedstr[265];
    unsigned char iv[32] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
    long n_read_total = 0;
    EVP_CIPHER_CTX ctx;
    AgentConnection *conn = pp->conn;

    snprintf(cfchangedstr, 255, "%s%s", CF_CHANGEDSTR1, CF_CHANGEDSTR2);

    if ((strlen(new) > CF_BUFSIZE - 20))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Filename too long");
        return false;
    }

    unlink(new);                /* To avoid link attacks */

    if ((dd = open(new, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, 0600)) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "open", pp, attr,
             " !! NetCopy to destination %s:%s security - failed attempt to exploit a race? (Not copied)\n",
             pp->this_server, new);
        unlink(new);
        return false;
    }

    if (size == 0)
    {
        // No sense in copying an empty file
        close(dd);
        return true;
    }

    workbuf[0] = '\0';
    EVP_CIPHER_CTX_init(&ctx);

    snprintf(in, CF_BUFSIZE - CF_PROTO_OFFSET, "GET dummykey %s", source);
    cipherlen = EncryptString(conn->encryption_type, in, out, conn->session_key, strlen(in) + 1);
    snprintf(workbuf, CF_BUFSIZE, "SGET %4d %4d", cipherlen, blocksize);
    memcpy(workbuf + CF_PROTO_OFFSET, out, cipherlen);
    tosend = cipherlen + CF_PROTO_OFFSET;

/* Send proposition C0 - query */

    if (SendTransaction(conn->sd, workbuf, tosend, CF_DONE) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Couldn't send data");
        close(dd);
        return false;
    }

    buf = xmalloc(CF_BUFSIZE + sizeof(int));

    n_read_total = 0;

    while (more)
    {
        if ((cipherlen = ReceiveTransaction(conn->sd, buf, &more)) == -1)
        {
            free(buf);
            return false;
        }

        cnt++;

        /* If the first thing we get is an error message, break. */

        if ((n_read_total == 0) && (strncmp(buf + CF_INBAND_OFFSET, CF_FAILEDSTR, strlen(CF_FAILEDSTR)) == 0))
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "Network access to %s:%s denied\n", pp->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        if (strncmp(buf + CF_INBAND_OFFSET, cfchangedstr, strlen(cfchangedstr)) == 0)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "Source %s:%s changed while copying\n", pp->this_server, source);
            close(dd);
            free(buf);
            return false;
        }

        EVP_DecryptInit_ex(&ctx, CfengineCipher(CfEnterpriseOptions()), NULL, conn->session_key, iv);

        if (!EVP_DecryptUpdate(&ctx, workbuf, &plainlen, buf, cipherlen))
        {
            CfDebug("Decryption failed\n");
            close(dd);
            free(buf);
            return false;
        }

        if (!EVP_DecryptFinal_ex(&ctx, workbuf + plainlen, &finlen))
        {
            CfDebug("Final decrypt failed\n");
            close(dd);
            free(buf);
            return false;
        }

        towrite = n_read = plainlen + finlen;

        n_read_total += n_read;

        if (!FSWrite(new, dd, workbuf, towrite, &last_write_made_hole, n_read, attr, pp))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, " !! Local disk write failed copying %s:%s to %s\n", pp->this_server,
                 source, new);
            free(buf);
            unlink(new);
            close(dd);
            EVP_CIPHER_CTX_cleanup(&ctx);
            return false;
        }
    }

    /* If the file ends with a `hole', something needs to be written at
       the end.  Otherwise the kernel would truncate the file at the end
       of the last write operation. Write a null character and truncate
       it again.  */

    if (last_write_made_hole)
    {
        if ((FullWrite(dd, "", 1) < 0) || (ftruncate(dd, n_read_total) < 0))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "FullWrite or ftruncate error in CopyReg, source %s\n", source);
            free(buf);
            unlink(new);
            close(dd);
            EVP_CIPHER_CTX_cleanup(&ctx);
            return false;
        }
    }

    close(dd);
    free(buf);
    EVP_CIPHER_CTX_cleanup(&ctx);
    return true;
}

/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

int ServerConnect(AgentConnection *conn, char *host, Attributes attr, Promise *pp)
{
    short shortport;
    char strport[CF_MAXVARSIZE] = { 0 };
    struct sockaddr_in cin = { 0 };
    struct timeval tv = { 0 };

    if (attr.copy.portnumber == (short) CF_NOINT)
    {
        shortport = SHORT_CFENGINEPORT;
        strncpy(strport, STR_CFENGINEPORT, CF_MAXVARSIZE);
    }
    else
    {
        shortport = htons(attr.copy.portnumber);
        snprintf(strport, CF_MAXVARSIZE, "%u", (int) attr.copy.portnumber);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Set cfengine port number to %s = %u\n", strport, (int) ntohs(shortport));

    if ((attr.copy.timeout == (short) CF_NOINT) || (attr.copy.timeout <= 0))
    {
        tv.tv_sec = CONNTIMEOUT;
    }
    else
    {
        tv.tv_sec = attr.copy.timeout;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Set connection timeout to %jd\n", (intmax_t) tv.tv_sec);

    tv.tv_usec = 0;

#if defined(HAVE_GETADDRINFO)

    if (!attr.copy.force_ipv4)
    {
        struct addrinfo query = { 0 }, *response, *ap;
        struct addrinfo query2 = { 0 }, *response2, *ap2;
        int err, connected = false;

        memset(&query, 0, sizeof(query));
        query.ai_family = AF_UNSPEC;
        query.ai_socktype = SOCK_STREAM;

        if ((err = getaddrinfo(host, strport, &query, &response)) != 0)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, " !! Unable to find host or service: (%s/%s) %s", host, strport,
                 gai_strerror(err));
            return false;
        }

        for (ap = response; ap != NULL; ap = ap->ai_next)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Connect to %s = %s on port %s\n", host, sockaddr_ntop(ap->ai_addr), strport);

            if ((conn->sd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol)) == SOCKET_INVALID)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "socket", " !! Couldn't open a socket");
                continue;
            }

            if (BINDINTERFACE[0] != '\0')
            {
                memset(&query2, 0, sizeof(query2));
                query2.ai_family = AF_UNSPEC;
                query2.ai_socktype = SOCK_STREAM;

                if ((err = getaddrinfo(BINDINTERFACE, NULL, &query2, &response2)) != 0)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, " !! Unable to lookup hostname or cfengine service: %s",
                         gai_strerror(err));
                    cf_closesocket(conn->sd);
                    conn->sd = SOCKET_INVALID;
                    return false;
                }

                for (ap2 = response2; ap2 != NULL; ap2 = ap2->ai_next)
                {
                    if (bind(conn->sd, ap2->ai_addr, ap2->ai_addrlen) == 0)
                    {
                        freeaddrinfo(response2);
                        response2 = NULL;
                        break;
                    }
                }

                if (response2)
                {
                    freeaddrinfo(response2);
                }
            }

            if (TryConnect(conn, &tv, ap->ai_addr, ap->ai_addrlen))
            {
                connected = true;
                break;
            }

        }

        if (connected)
        {
            conn->family = ap->ai_family;
            snprintf(conn->remoteip, CF_MAX_IP_LEN - 1, "%s", sockaddr_ntop(ap->ai_addr));
        }
        else
        {
            if (conn->sd != SOCKET_INVALID)
            {
                cf_closesocket(conn->sd);
                conn->sd = SOCKET_INVALID;
            }
        }

        if (response != NULL)
        {
            freeaddrinfo(response);
        }

        if (!connected)
        {
            if (pp)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "connect", pp, attr, " !! Unable to connect to server %s", host);
            }

            return false;
        }

        return true;
    }

    else
#endif /* ---------------------- only have ipv4 --------------------------------- */

    {
        struct hostent *hp;

        memset(&cin, 0, sizeof(cin));

        if ((hp = gethostbyname(host)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "gethostbyname", " !! Unable to look up IP address of %s", host);
            return false;
        }

        cin.sin_port = shortport;
        cin.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
        cin.sin_family = AF_INET;

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Connect to %s = %s, port = (%u=%s)\n", host, inet_ntoa(cin.sin_addr),
              (int) ntohs(shortport), strport);

        if ((conn->sd = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_INVALID)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "socket", pp, attr, "Couldn't open a socket");
            return false;
        }

        if (BINDINTERFACE[0] != '\0')
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Cannot bind interface with this OS.\n");
            /* Could fix this - any point? */
        }

        conn->family = AF_INET;
        snprintf(conn->remoteip, CF_MAX_IP_LEN - 1, "%s", inet_ntoa(cin.sin_addr));

        return TryConnect(conn, &tv, (struct sockaddr *) &cin, sizeof(cin));
    }
}

/*********************************************************************/

static bool ServerOffline(const char *server)
{
    Rlist *rp;
    ServerItem *svp;
    char ipname[CF_MAXVARSIZE];

    ThreadLock(cft_getaddr);
    strncpy(ipname, Hostname2IPString(server), CF_MAXVARSIZE - 1);
    ThreadUnlock(cft_getaddr);

    for (rp = SERVERLIST; rp != NULL; rp = rp->next)
    {
        svp = (ServerItem *) rp->item;

        if (svp == NULL)
        {
            continue;
        }

        if ((strcmp(ipname, svp->server) == 0) && (svp->conn == NULL))
        {
            return true;
        }
    }

    return false;
}

/*********************************************************************/

/*
 * We need to destroy connection as it has got an fatal (or non-fatal) error
 */
void DestroyServerConnection(AgentConnection *conn)
{
    Rlist *entry = RlistKeyIn(SERVERLIST, conn->remoteip);

    DisconnectServer(conn);

    if (entry != NULL)
    {
        entry->item = NULL;     /* Has been freed by DisconnectServer */
        RlistDestroyEntry(&SERVERLIST, entry);
    }
}

/*********************************************************************/

static AgentConnection *GetIdleConnectionToServer(const char *server)
{
    Rlist *rp;
    ServerItem *svp;
    char ipname[CF_MAXVARSIZE];

    ThreadLock(cft_getaddr);
    strncpy(ipname, Hostname2IPString(server), CF_MAXVARSIZE - 1);
    ThreadUnlock(cft_getaddr);

    for (rp = SERVERLIST; rp != NULL; rp = rp->next)
    {
        svp = (ServerItem *) rp->item;

        if (svp == NULL)
        {
            continue;
        }

        if (svp->busy)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Existing connection to %s seems to be active...\n", ipname);
            return NULL;
        }

        if ((strcmp(ipname, svp->server) == 0) && (svp->conn) && (svp->conn->sd > 0))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Connection to %s is already open and ready...\n", ipname);
            svp->busy = true;
            return svp->conn;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "No existing connection to %s is established...\n", ipname);
    return NULL;
}

/*********************************************************************/

void ServerNotBusy(AgentConnection *conn)
{
    Rlist *rp;
    ServerItem *svp;

    for (rp = SERVERLIST; rp != NULL; rp = rp->next)
    {
        svp = (ServerItem *) rp->item;

        if (svp->conn == conn)
        {
            svp->busy = false;
            break;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Existing connection just became free...\n");
}

/*********************************************************************/

static void MarkServerOffline(const char *server)
/* Unable to contact the server so don't waste time trying for
   other connections, mark it offline */
{
    Rlist *rp;
    AgentConnection *conn = NULL;
    ServerItem *svp;
    char ipname[CF_MAXVARSIZE];

    ThreadLock(cft_getaddr);
    strncpy(ipname, Hostname2IPString(server), CF_MAXVARSIZE - 1);
    ThreadUnlock(cft_getaddr);

    for (rp = SERVERLIST; rp != NULL; rp = rp->next)
    {
        svp = (ServerItem *) rp->item;

        if (svp == NULL)
        {
            continue;
        }

        conn = svp->conn;

        if (strcmp(ipname, conn->localip) == 0)
        {
            conn->sd = CF_COULD_NOT_CONNECT;
            return;
        }
    }

    ThreadLock(cft_getaddr);

/* If no existing connection, get one .. */

    rp = RlistPrepend(&SERVERLIST, "nothing", RVAL_TYPE_SCALAR);

    svp = xmalloc(sizeof(ServerItem));

    svp->server = xstrdup(ipname);

    free(rp->item);
    rp->item = svp;

    svp->conn = NewAgentConn();

    svp->busy = false;

    ThreadUnlock(cft_getaddr);
}

/*********************************************************************/

static void CacheServerConnection(AgentConnection *conn, const char *server)
/* First time we open a connection, so store it */
{
    Rlist *rp;
    ServerItem *svp;
    char ipname[CF_MAXVARSIZE];

    if (!ThreadLock(cft_getaddr))
    {
        exit(1);
    }

    strlcpy(ipname, Hostname2IPString(server), CF_MAXVARSIZE);

    rp = RlistPrepend(&SERVERLIST, "nothing", RVAL_TYPE_SCALAR);
    free(rp->item);
    svp = xmalloc(sizeof(ServerItem));
    rp->item = svp;
    svp->server = xstrdup(ipname);
    svp->conn = conn;
    svp->busy = true;

    ThreadUnlock(cft_getaddr);
}

/*********************************************************************/

static int CacheStat(const char *file, struct stat *statbuf, const char *stattype, Attributes attr, Promise *pp)
{
    Stat *sp;

    CfDebug("CacheStat(%s)\n", file);

    for (sp = pp->cache; sp != NULL; sp = sp->next)
    {
        if ((strcmp(pp->this_server, sp->cf_server) == 0) && (strcmp(file, sp->cf_filename) == 0))
        {
            if (sp->cf_failed)  /* cached failure from cfopendir */
            {
                errno = EPERM;
                CfDebug("Cached failure to stat\n");
                return -1;
            }

            if ((strcmp(stattype, "link") == 0) && (sp->cf_lmode != 0))
            {
                statbuf->st_mode = sp->cf_lmode;
            }
            else
            {
                statbuf->st_mode = sp->cf_mode;
            }

            statbuf->st_uid = sp->cf_uid;
            statbuf->st_gid = sp->cf_gid;
            statbuf->st_size = sp->cf_size;
            statbuf->st_atime = sp->cf_atime;
            statbuf->st_mtime = sp->cf_mtime;
            statbuf->st_ctime = sp->cf_ctime;
            statbuf->st_ino = sp->cf_ino;
            statbuf->st_nlink = sp->cf_nlink;

            CfDebug("Found in cache\n");
            return true;
        }
    }

    CfDebug("Did not find in cache\n");
    return false;
}

/*********************************************************************/

static void FlushFileStream(int sd, int toget)
{
    int i;
    char buffer[2];

    CfOut(OUTPUT_LEVEL_INFORM, "", "Flushing rest of file...%d bytes\n", toget);

    for (i = 0; i < toget; i++)
    {
        recv(sd, buffer, 1, 0); /* flush to end of current file */
    }
}

/*********************************************************************/

void ConnectionsInit(void)
{
    SERVERLIST = NULL;
}

/*********************************************************************/

void ConnectionsCleanup(void)
{
    Rlist *rp;
    ServerItem *svp;

    for (rp = SERVERLIST; rp != NULL; rp = rp->next)
    {
        svp = (ServerItem *) rp->item;

        if (svp == NULL)
        {
            continue;
        }

        DisconnectServer(svp->conn);

        if (svp->server)
        {
            free(svp->server);
        }

        rp->item = NULL;
    }

    RlistDestroy(SERVERLIST);
    SERVERLIST = NULL;
}

/*********************************************************************/

#if !defined(__MINGW32__)
static int TryConnect(AgentConnection *conn, struct timeval *tvp, struct sockaddr *cinp, int cinpSz)
/** 
 * Tries a nonblocking connect and then restores blocking if
 * successful. Returns true on success, false otherwise.
 * NB! Do not use recv() timeout - see note below.
 **/
{
    int res;
    long arg;
    struct sockaddr_in emptyCin = { 0 };

    if (!cinp)
    {
        cinp = (struct sockaddr *) &emptyCin;
        cinpSz = sizeof(emptyCin);
    }

    /* set non-blocking socket */
    arg = fcntl(conn->sd, F_GETFL, NULL);

    if (fcntl(conn->sd, F_SETFL, arg | O_NONBLOCK) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not set socket to non-blocking mode");
    }

    res = connect(conn->sd, cinp, (socklen_t) cinpSz);

    if (res < 0)
    {
        if (errno == EINPROGRESS)
        {
            fd_set myset;
            int valopt;
            socklen_t lon = sizeof(int);

            FD_ZERO(&myset);

#if defined(__hpux) && defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
// HP-UX GCC type-pun warning on FD_SET() macro:
// While the "fd_set" type is defined in /usr/include/sys/_fd_macros.h as a
// struct of an array of "long" values in accordance with the XPG4 standard's
// requirements, the macros for the FD operations "pretend it is an array of
// int32_t's so the binary layout is the same for both Narrow and Wide
// processes," as described in _fd_macros.h. In the FD_SET, FD_CLR, and
// FD_ISSET macros at line 101, the result is cast to an "__fd_mask *" type,
// which is defined as int32_t at _fd_macros.h:82.
//
// This conflict between the "long fds_bits[]" array in the XPG4-compliant
// fd_set structure, and the cast to an int32_t - not long - pointer in the
// macros, causes a type-pun warning if -Wstrict-aliasing is enabled.
// The warning is merely a side effect of HP-UX working as designed,
// so it can be ignored.
#endif
            FD_SET(conn->sd, &myset);
#if defined(__hpux) && defined(__GNUC__)
#pragma GCC diagnostic warning "-Wstrict-aliasing"
#endif

            /* now wait for connect, but no more than tvp.sec */
            res = select(conn->sd + 1, NULL, &myset, NULL, tvp);
            if (getsockopt(conn->sd, SOL_SOCKET, SO_ERROR, (void *) (&valopt), &lon) != 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "getsockopt", "!! Could not check connection status");
                return false;
            }

            if (valopt || (res <= 0))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "connect", " !! Error connecting to server (timeout)");
                return false;
            }
        }
        else
        {
            CfOut(OUTPUT_LEVEL_INFORM, "connect", " !! Error connecting to server");
            return false;
        }
    }

    /* connection suceeded; return to blocking mode */

    if (fcntl(conn->sd, F_SETFL, arg) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not set socket to blocking mode");
    }

    if (SetReceiveTimeout(conn->sd, tvp) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "setsockopt", "!! Could not set socket timeout");
    }

    return true;
}

#endif /* !defined(__MINGW32__) */
