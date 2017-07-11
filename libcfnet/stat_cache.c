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
#include <stat_cache.h>

#include <cfnet.h>                            /* AgentConnection */
#include <net.h>                              /* {Send,Receive}Transaction */
#include <client_protocol.h>                  /* BadProtoReply,OKProtoReply */
#include <alloc.h>                            /* xmemdup */
#include <logging.h>                          /* Log */
#include <crypto.h>                           /* EncryptString */

static void NewStatCache(Stat *data, AgentConnection *conn)
{
    Stat *sp = xmemdup(data, sizeof(Stat));
    sp->next = conn->cache;
    conn->cache = sp;
}

/**
 * @brief Find remote stat information for #file in cache and
 *        return it in #statbuf.
 * @return 0 if found, 1 if not found, -1 in case of error.
 */
static int StatFromCache(AgentConnection *conn, const char *file,
                         struct stat *statbuf, const char *stattype)
{
    for (Stat *sp = conn->cache; sp != NULL; sp = sp->next)
    {
        /* TODO differentiate ports etc in this stat cache! */

        if (strcmp(conn->this_server, sp->cf_server) == 0 &&
            strcmp(file, sp->cf_filename) == 0)
        {
            if (sp->cf_failed)  /* cached failure from cfopendir */
            {
                errno = EPERM;
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
            statbuf->st_dev = sp->cf_dev;
            statbuf->st_nlink = sp->cf_nlink;

            return 0;
        }
    }

    return 1;                                                  /* not found */
}

/**
 * @param #stattype should be either "link" or "file". If a link, this reads
 *                  readlink and sends it back in the same packet. It then
 *                  caches the value for each copy command.
 *
 */
int cf_remote_stat(AgentConnection *conn, bool encrypt, const char *file,
                   struct stat *statbuf, const char *stattype)
{
    assert(strcmp(stattype, "file") == 0 ||
           strcmp(stattype, "link") == 0);

    /* We encrypt only for CLASSIC protocol. The TLS protocol is always over
     * encrypted layer, so it does not support encrypted (S*) commands. */
    encrypt = encrypt && conn->conn_info->protocol == CF_PROTOCOL_CLASSIC;

    if (strlen(file) > CF_BUFSIZE - 30)
    {
        Log(LOG_LEVEL_ERR, "Filename too long");
        return -1;
    }

    int ret = StatFromCache(conn, file, statbuf, stattype);
    if (ret == 0 || ret == -1)                            /* found or error */
    {
        return ret;
    }

    /* Not found in cache */

    char recvbuffer[CF_BUFSIZE];
    memset(recvbuffer, 0, CF_BUFSIZE);

    time_t tloc = time(NULL);
    if (tloc == (time_t) -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't read system clock (time: %s)",
            GetErrorStr());
        tloc = 0;
    }

    char sendbuffer[CF_BUFSIZE];
    int tosend;
    sendbuffer[0] = '\0';

    if (encrypt)
    {
        if (conn->session_key == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Cannot do encrypted copy without keys (use cf-key)");
            return -1;
        }

        char in[CF_BUFSIZE], out[CF_BUFSIZE];

        snprintf(in, CF_BUFSIZE - 1, "SYNCH %jd STAT %s",
                 (intmax_t) tloc, file);
        int cipherlen = EncryptString(conn->encryption_type, in, out,
                                      conn->session_key, strlen(in) + 1);
        snprintf(sendbuffer, CF_BUFSIZE - 1, "SSYNCH %d", cipherlen);
        memcpy(sendbuffer + CF_PROTO_OFFSET, out, cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    }
    else
    {
        snprintf(sendbuffer, CF_BUFSIZE, "SYNCH %jd STAT %s",
                 (intmax_t) tloc, file);
        tosend = strlen(sendbuffer);
    }

    if (SendTransaction(conn->conn_info, sendbuffer, tosend, CF_DONE) == -1)
    {
        Log(LOG_LEVEL_INFO,
            "Transmission failed/refused talking to %.255s:%.255s. (stat: %s)",
            conn->this_server, file, GetErrorStr());
        return -1;
    }

    if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
    {
        /* TODO mark connection in the cache as closed. */
        return -1;
    }

    if (strstr(recvbuffer, "unsynchronized"))
    {
        Log(LOG_LEVEL_ERR,
            "Clocks differ too much to do copy by date (security), server reported: %s",
            recvbuffer + strlen("BAD: "));
        return -1;
    }

    if (BadProtoReply(recvbuffer))
    {
        Log(LOG_LEVEL_VERBOSE, "Server returned error: %s",
            recvbuffer + strlen("BAD: "));
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
            Log(LOG_LEVEL_ERR, "Cannot read SYNCH reply from '%s', only %d/13 items parsed", conn->remoteip, ret );
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
        cfst.cf_ino = d11;
        cfst.cf_nlink = d12;
        cfst.cf_dev = (dev_t)d13;

        /* Use %?d here to avoid memory overflow attacks */

        memset(recvbuffer, 0, CF_BUFSIZE);

        if (ReceiveTransaction(conn->conn_info, recvbuffer, NULL) == -1)
        {
            /* TODO mark connection in the cache as closed. */
            return -1;
        }

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
        case FILE_TYPE_CHAR_:
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
        cfst.cf_server = xstrdup(conn->this_server);
        cfst.cf_failed = false;

        if (cfst.cf_lmode != 0)
        {
            cfst.cf_lmode |= (mode_t) S_IFLNK;
        }

        NewStatCache(&cfst, conn);

        if ((cfst.cf_lmode != 0) && (strcmp(stattype, "link") == 0))
        {
            statbuf->st_mode = cfst.cf_lmode;
        }
        else
        {
            statbuf->st_mode = cfst.cf_mode;
        }

        statbuf->st_uid = cfst.cf_uid;
        statbuf->st_gid = cfst.cf_gid;
        statbuf->st_size = cfst.cf_size;
        statbuf->st_mtime = cfst.cf_mtime;
        statbuf->st_ctime = cfst.cf_ctime;
        statbuf->st_atime = cfst.cf_atime;
        statbuf->st_ino = cfst.cf_ino;
        statbuf->st_dev = cfst.cf_dev;
        statbuf->st_nlink = cfst.cf_nlink;

        return 0;
    }

    Log(LOG_LEVEL_ERR, "Transmission refused or failed statting '%s', got '%s'", file, recvbuffer);
    errno = EPERM;
    return -1;
}

/*********************************************************************/

/* TODO only a server_name is not enough for stat'ing of files... */
const Stat *StatCacheLookup(const AgentConnection *conn, const char *file_name,
                            const char *server_name)
{
    for (const Stat *sp = conn->cache; sp != NULL; sp = sp->next)
    {
        if (strcmp(server_name, sp->cf_server) == 0 &&
            strcmp(file_name, sp->cf_filename) == 0)
        {
            return sp;
        }
    }

    return NULL;
}
