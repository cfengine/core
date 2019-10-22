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
#include <string_lib.h>
#include <tls_generic.h>

ProtocolVersion ParseProtocolVersionNetwork(const char *const s)
{
    int version;
    const int ret = sscanf(s, "CFE_v%d", &version);
    if (ret != 1 || version <= CF_PROTOCOL_UNDEFINED)
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    // Note that `version` may be above CF_PROTOCOL_LATEST, if the other side
    // supports a newer protocol
    return version;
}

ProtocolVersion ParseProtocolVersionPolicy(const char *const s)
{
    if ((s == NULL) || (strcmp(s, "0") == 0) || (strcmp(s, "undefined") == 0))
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    if ((strcmp(s, "1") == 0) || (strcmp(s, "classic") == 0))
    {
        return CF_PROTOCOL_CLASSIC;
    }
    else if ((strcmp(s, "2") == 0) || (strcmp(s, "tls") == 0))
    {
        return CF_PROTOCOL_TLS;
    }
    else if (strcmp(s, "latest") == 0)
    {
        return CF_PROTOCOL_LATEST;
    }
    else
    {
        return CF_PROTOCOL_UNDEFINED;
    }
}

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
