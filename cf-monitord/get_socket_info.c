/*
  Copyright 2020 Northern.tech AS

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

#include <stdio.h>
#include <arpa/inet.h>

#include <sequence.h>
#include <file_lib.h>
#include <logging.h>
#include <alloc.h>

#include <proc_net_parsing.h>

int main()
{
    FILE *fp = fopen("/proc/net/tcp", "r");

    size_t buff_size = 256;
    char *buff = xmalloc(buff_size);

    /* Read the header */
    ssize_t ret = CfReadLine(&buff, &buff_size, fp);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp");
        free(buff);
        return 1;
    }

    /* Read real data */
    Seq *lines = SeqNew(64, free);
    ret = CfReadLines(&buff, &buff_size, fp, lines);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp");
        SeqDestroy(lines);
        free(buff);
        return 1;
    }

    /* else */
    char local_addr[INET_ADDRSTRLEN];
    char remote_addr[INET_ADDRSTRLEN];
    uint32_t l_port, r_port;
    SocketState state;
    for (size_t i = 0; i < ret; i++)
    {
        char *line = SeqAt(lines,i);
        if (ParseIPv4SocketInfo(line, local_addr, &l_port, remote_addr, &r_port, &state))
        {
            printf("%s:%d -> %s:%d%s\n",
                   local_addr, l_port,
                   remote_addr, r_port,
                   (state == SOCK_STATE_LISTEN) ? " [LISTEN]" : "");
        }
    }
    SeqClear(lines);

    char local_addr6[INET6_ADDRSTRLEN];
    char remote_addr6[INET6_ADDRSTRLEN];
    fp = fopen("/proc/net/tcp6", "r");
    if (fp != NULL)
    {
        ret = CfReadLine(&buff, &buff_size, fp);
        if (ret == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp6");
            SeqDestroy(lines);
            free(buff);
            return 1;
        }

        /* Read real data */
        ret = CfReadLines(&buff, &buff_size, fp, lines);
        if (ret == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp6");
            SeqDestroy(lines);
            free(buff);
            return 1;
        }

        /* else */
        for (size_t i = 0; i < ret; i++)
        {
            char *line = SeqAt(lines,i);
            if (ParseIPv6SocketInfo(line, local_addr6, &l_port, remote_addr6, &r_port, &state))
            {
                printf("%s:%d -> %s:%d%s\n",
                       local_addr6, l_port,
                       remote_addr6, r_port,
                       (state == SOCK_STATE_LISTEN) ? " [LISTEN]" : "");
            }
        }
        SeqClear(lines);
    }

    fp = fopen("/proc/net/udp", "r");
    ret = CfReadLine(&buff, &buff_size, fp);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp");
        SeqDestroy(lines);
        free(buff);
        return 1;
    }

    /* Read real data */
    ret = CfReadLines(&buff, &buff_size, fp, lines);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp");
        SeqDestroy(lines);
        free(buff);
        return 1;
    }

    /* else */
    for (size_t i = 0; i < ret; i++)
    {
        char *line = SeqAt(lines,i);
        if (ParseIPv4SocketInfo(line, local_addr, &l_port, remote_addr, &r_port, &state))
        {
            printf("%s:%d -> %s:%d [udp]\n",
                   local_addr, l_port,
                   remote_addr, r_port);
        }
    }
    SeqClear(lines);

    fp = fopen("/proc/net/udp6", "r");
    if (fp != NULL)
    {
        ret = CfReadLine(&buff, &buff_size, fp);
        if (ret == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp6");
            SeqDestroy(lines);
            free(buff);
            return 1;
        }

        /* Read real data */
        ret = CfReadLines(&buff, &buff_size, fp, lines);
        if (ret == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp6");
            SeqDestroy(lines);
            free(buff);
            return 1;
        }

        /* else */
        for (size_t i = 0; i < ret; i++)
        {
            char *line = SeqAt(lines,i);
            if (ParseIPv6SocketInfo(line, local_addr6, &l_port, remote_addr6, &r_port, &state))
            {
                printf("%s:%d -> %s:%d [udp6]\n",
                       local_addr6, l_port,
                       remote_addr6, r_port);
            }
        }
    }
    SeqDestroy(lines);
    free(buff);
    return 0;
}
