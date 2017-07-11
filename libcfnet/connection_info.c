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

#include <alloc.h>
#include <cfnet.h>
#include <refcount.h>
#include <connection_info.h>


ConnectionInfo *ConnectionInfoNew(void)
{
    struct ConnectionInfo *info = xcalloc(1, sizeof(struct ConnectionInfo));
    info->sd = SOCKET_INVALID;

    return info;
}

void ConnectionInfoDestroy(ConnectionInfo **info)
{
    if (!info || !*info)
    {
        return;
    }
    /* Destroy everything */
    if ((*info)->ssl)
    {
        SSL_free((*info)->ssl);
    }
    KeyDestroy(&(*info)->remote_key);
    free(*info);
    *info = NULL;
}

ProtocolVersion ConnectionInfoProtocolVersion(const ConnectionInfo *info)
{
    return info ? info->protocol : CF_PROTOCOL_UNDEFINED;
}

void ConnectionInfoSetProtocolVersion(ConnectionInfo *info, ProtocolVersion version)
{
    if (!info)
    {
        return;
    }
    switch (version)
    {
    case CF_PROTOCOL_UNDEFINED:
    case CF_PROTOCOL_CLASSIC:
    case CF_PROTOCOL_TLS:
        info->protocol = version;
        break;
    default:
        break;
    }
}

int ConnectionInfoSocket(const ConnectionInfo *info)
{
    return info ? info->sd : -1;
}

void ConnectionInfoSetSocket(ConnectionInfo *info, int s)
{
    if (!info)
    {
        return;
    }
    info->sd = s;
}

SSL *ConnectionInfoSSL(const ConnectionInfo *info)
{
    return info ? info->ssl : NULL;
}

void ConnectionInfoSetSSL(ConnectionInfo *info, SSL *ssl)
{
    if (!info)
    {
        return;
    }
    info->ssl = ssl;
}

const Key *ConnectionInfoKey(const ConnectionInfo *info)
{
    const Key *key = info ? info->remote_key : NULL;
    return key;
}

void ConnectionInfoSetKey(ConnectionInfo *info, Key *key)
{
    if (!info)
    {
        return;
    }
    /* The key can be assigned only once on a session */
    if (info->remote_key)
    {
        return;
    }
    if (!key)
    {
        return;
    }
    info->remote_key = key;
}

const unsigned char *ConnectionInfoBinaryKeyHash(ConnectionInfo *info, unsigned int *length)
{
    if (!info)
    {
        return NULL;
    }
    Key *connection_key = info->remote_key;
    unsigned int real_length = 0;
    const char *binary = KeyBinaryHash(connection_key, &real_length);
    if (length)
    {
        *length = real_length;
    }
    return binary;
}

const char *ConnectionInfoPrintableKeyHash(ConnectionInfo *info)
{
    return info ? KeyPrintableHash(info->remote_key) : NULL;
}
