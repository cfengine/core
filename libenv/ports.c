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
#include <ports.h>

#include <files_interfaces.h>
#include <pipes.h>

Buffer* PortsGetNetstatCommand(const PlatformContext platform)
{
    char *command = "-";

    switch (platform)
    {
    case PLATFORM_CONTEXT_OPENVZ: /* virt_host_vz_vzps */
    case PLATFORM_CONTEXT_LINUX:  /* linux */
        command = "/bin/netstat";
        break;

    case PLATFORM_CONTEXT_HP:        /* hpux */
    case PLATFORM_CONTEXT_AIX:       /* aix */
    case PLATFORM_CONTEXT_SOLARIS:   /* solaris */
    case PLATFORM_CONTEXT_FREEBSD:   /* freebsd */
    case PLATFORM_CONTEXT_NETBSD:    /* netbsd */
    case PLATFORM_CONTEXT_SYSTEMV:   /* Unixware */
    case PLATFORM_CONTEXT_OPENBSD:   /* openbsd */
    case PLATFORM_CONTEXT_CFSCO:     /* sco */
    case PLATFORM_CONTEXT_QNX:       /* qnx */
    case PLATFORM_CONTEXT_DRAGONFLY: /* dragonfly */
    case PLATFORM_CONTEXT_VMWARE:    /* vmware */
        command = "/usr/bin/netstat";
        break;

        /* collect them all! */
    case PLATFORM_CONTEXT_CRAYOS:     command = "/usr/ucb/netstat";                   break;     /* cray */
    case PLATFORM_CONTEXT_WINDOWS_NT: command = "/cygdrive/c/WINNT/System32/netstat"; break;     /* NT */
    case PLATFORM_CONTEXT_DARWIN:     command = "/usr/sbin/netstat";                  break;     /* darwin */
    case PLATFORM_CONTEXT_MINGW:      command = "mingw-invalid";                      break;     /* mingw */

    default: break;
    }
    return BufferNewFrom(command, strlen(command));
}

void PortsFindListening(const PlatformContext platform, PortProcessorFn port_processor, void *callback_context)
{
    cf_netstat_type type = cfn_new;
    cf_packet_type packet = cfn_tcp4;
    char local[CF_BUFSIZE], remote[CF_BUFSIZE];
    char *sp;

    Buffer *command = PortsGetNetstatCommand(platform);
    BufferAppend(command, " -an", 4);

    FILE *pp = cf_popen((char*)BufferData(command), "r", true);
    BufferDestroy(command);

    if (NULL == pp)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not run netstat command");
        return;
    }

    size_t vbuff_size = CF_BUFSIZE;
    char *vbuff = xmalloc(vbuff_size);

    int scan_count;

    for (;;)
    {
        memset(local, 0, CF_BUFSIZE);
        memset(remote, 0, CF_BUFSIZE);

        size_t res = CfReadLine(&vbuff, &vbuff_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                /* FIXME: no logging */
                cf_pclose(pp);
                free(vbuff);
                return;
            }
            else
            {
                break;
            }
        }

        if (strstr(vbuff, "UNIX"))
        {
            break;
        }

        if (!((strstr(vbuff, ":")) || (strstr(vbuff, "."))))
        {
            continue;
        }

        cf_port_state port_state = strstr(vbuff, "LISTEN") ? cfn_listen : cfn_not_listen;

        /* Different formats here ... ugh.. pick a model */

        // If this is old style, we look for chapter headings, e.g. "TCP: IPv4"

        if ((strncmp(vbuff,"UDP:",4) == 0) && (strstr(vbuff+4,"6")))
        {
            packet = cfn_udp6;
            type = cfn_old;
            continue;
        }
        else if ((strncmp(vbuff,"TCP:",4) == 0) && (strstr(vbuff+4,"6")))
        {
            packet = cfn_tcp6;
            type = cfn_old;
            continue;
        }
        else if ((strncmp(vbuff,"UDP:",4) == 0) && (strstr(vbuff+4,"4")))
        {
            packet = cfn_udp4;
            type = cfn_old;
            continue;
        }
        else if ((strncmp(vbuff,"TCP:",4) == 0) && (strstr(vbuff+4,"4")))
        {
            packet = cfn_tcp4;
            type = cfn_old;
            continue;
        }

        // Line by line state in modern/linux output

        if (strncmp(vbuff,"udp6",4) == 0)
        {
            packet = cfn_udp6;
            type = cfn_new;
        }
        else if (strncmp(vbuff,"tcp6",4) == 0)
        {
            packet = cfn_tcp6;
            type = cfn_new;
        }
        else if (strncmp(vbuff,"udp",3) == 0)
        {
            packet = cfn_udp4;
            type = cfn_new;
        }
        else if (strncmp(vbuff,"tcp",3) == 0)
        {
            packet = cfn_tcp4;
            type = cfn_new;
        }

        // End extract type
        
        switch (type)
        {
        case cfn_new:
            /* linux-like */
            scan_count = sscanf(vbuff, "%*s %*s %*s %s %s", local, remote);
            break;
            
        case cfn_old:
            scan_count = sscanf(vbuff, "%s %s", local, remote);
            break;
        }

        if ('\0' == *local || scan_count < 2)
        {
            continue;
        }

        // Extract the port number from the end of the string
        for (sp = local + strlen(local); (*sp != '.') && (*sp != ':')  && (sp > local); sp--)
        {
        }

        *sp = '\0'; // Separate address from port number
        sp++;

        char *localport = sp;

        // Now look at outgoing
       
        for (sp = remote + strlen(remote) - 1; (sp >= remote) && (isdigit((int) *sp)); sp--)
        {
        }

        char *remote_trim = sp;
        for (; (*remote_trim != '.') && (*remote_trim != ':') && (remote_trim > remote); remote_trim--)
        {
        }
        *remote_trim = '\0'; // Separate address from port number
        
        sp++;
        char *remoteport = sp;

        (*port_processor)(type, packet, port_state, vbuff, local, localport, remote, remoteport, callback_context);
    }

    cf_pclose(pp);
    free(vbuff);
}
