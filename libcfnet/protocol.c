/*
  Copyright 2019 Northern.tech AS

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

#include <protocol.h>

#include <client_code.h>
#include <client_protocol.h>
#include <definitions.h>
#include <net.h>
#include <stat_cache.h>
#include <string_lib.h>
#include <tls_generic.h>

Seq *ProtocolOpenDir(AgentConnection *conn, const char *path)
{
    assert(conn != NULL);
    assert(path != NULL);

    char buf[CF_MSGSIZE] = {0};
    int tosend = snprintf(buf, CF_MSGSIZE, "OPENDIR %s", path);
    if (tosend < 0 || tosend >= CF_MSGSIZE)
    {
        return NULL;
    }

    int ret = SendTransaction(conn->conn_info, buf, tosend, CF_DONE);
    if (ret == -1)
    {
        return NULL;
    }

    Seq *seq = SeqNew(0, free);

    int more = 1;
    while (more != 0)
    {
        int len = ReceiveTransaction(conn->conn_info, buf, &more);
        if (len == -1)
        {
            break;
        }

        if (BadProtoReply(buf))
        {
            Log(LOG_LEVEL_ERR, "Protocol error: %s", buf);
            SeqDestroy(seq);
            return NULL;
        }

        /*
         * Iterates over each string in the received transaction and appends
         * it to the Seq list, until it either finds the CFD_TERMINATOR
         * string, or reaches the end of the message.
         */
        for (int i = 0; i < len && buf[i] != '\0'; i += strlen(buf + i) + 1)
        {
            if (StringSafeEqualN(buf + i, CFD_TERMINATOR,
                                 sizeof(CFD_TERMINATOR) - 1))
            {
                more = 0;
                break;
            }

            char *str = xstrdup(buf + i);
            SeqAppend(seq, str);
        }
    }

    return seq;
}

bool ProtocolGet(AgentConnection *conn, const char *remote_path,
                 const char *local_path, const uint32_t file_size, int perms)
{
    assert(conn != NULL);
    assert(remote_path != NULL);
    assert(local_path != NULL);
    assert(file_size != 0);

    perms = (perms == 0) ? CF_PERMS_DEFAULT : perms;

    unlink(local_path);
    FILE *file_ptr = safe_fopen_create_perms(local_path, "wx", perms);
    if (file_ptr == NULL)
    {
        Log(LOG_LEVEL_WARNING, "Failed to open file %s (fopen: %s)",
            local_path, GetErrorStr());
        return false;
    }

    char buf[CF_MSGSIZE] = {0};
    int to_send = snprintf(buf, CF_MSGSIZE, "GET %d %s",
                           CF_MSGSIZE, remote_path);


    int ret = SendTransaction(conn->conn_info, buf, to_send, CF_DONE);
    if (ret == -1)
    {
        Log(LOG_LEVEL_WARNING, "Failed to send request for remote file %s:%s",
            conn->this_server, remote_path);
        unlink(local_path);
        fclose(file_ptr);
        return false;
    }

    char cfchangedstr[sizeof(CF_CHANGEDSTR1 CF_CHANGEDSTR2)];
    snprintf(cfchangedstr, sizeof(cfchangedstr), "%s%s",
             CF_CHANGEDSTR1, CF_CHANGEDSTR2);

    bool success = true;
    uint32_t received_bytes = 0;
    while (received_bytes < file_size)
    {
        int len = TLSRecv(conn->conn_info->ssl, buf, CF_MSGSIZE);
        if (len == -1)
        {
            Log(LOG_LEVEL_WARNING, "Failed to GET file %s:%s",
                conn->this_server, remote_path);
            success = false;
            break;
        }
        else if (len > CF_MSGSIZE)
        {
            Log(LOG_LEVEL_WARNING,
                "Incorrect length of incoming packet "
                "while retrieving %s:%s, %d > %d",
                conn->this_server, remote_path, len, CF_MSGSIZE);
            success = false;
            break;
        }

        if (BadProtoReply(buf))
        {
            Log(LOG_LEVEL_ERR,
                "Error from server while retrieving file %s:%s: %s",
                conn->this_server, remote_path, buf);
            success = false;
            break;
        }

        if (StringSafeEqualN(buf, cfchangedstr, sizeof(cfchangedstr) - 1))
        {
            Log(LOG_LEVEL_ERR,
                "Remote file %s:%s changed during file transfer",
                conn->this_server, remote_path);
            success = false;
            break;
        }

        ret = fwrite(buf, sizeof(char), len, file_ptr);
        if (ret < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to write during retrieval of file %s:%s (fwrite: %s)",
                conn->this_server, remote_path, GetErrorStr());
            success = false;
            break;
        }

        received_bytes += len;
    }

    if (!success)
    {
        unlink(local_path);
    }

    fclose(file_ptr);
    return success;
}

bool ProtocolStatGet(AgentConnection *conn, const char *remote_path,
                     const char *local_path, int perms)
{
    assert(conn != NULL);
    assert(remote_path != NULL);

    struct stat sb;
    int ret = cf_remote_stat(conn, false, remote_path, &sb, "file");
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Failed to stat remote file %s:%s",
            __func__, conn->this_server, remote_path);
        return false;
    }

    return ProtocolGet(conn, remote_path, local_path, sb.st_size, perms);
}
