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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <alloc.h>
#include <cfnet.h>
#include <refcount.h>
#include <connection_info.h>

/*
 * We define the structure on the C file to force people to use the API.
 * Do not move this declaration to the header file, otherwise this structure
 * will become modifiable without accessing the API.
 */
struct ConnectionInfoData {
    ProtocolVersion type;
    ConnectionStatus status;
    int sd;                           /* Socket descriptor */
    SSL *ssl;                         /* OpenSSL struct for TLS connections */
    Key *remote_key;
};

struct ConnectionInfo {
    struct ConnectionInfoData *data;
};

ConnectionInfo *ConnectionInfoNew(void)
{
    struct ConnectionInfoData *data = NULL;
    data = xmalloc(sizeof(struct ConnectionInfoData));
    data->remote_key = NULL;
    data->ssl = NULL;
    data->sd = SOCKET_INVALID;
    data->type = CF_PROTOCOL_UNDEFINED;
    data->status = CF_CONNECTION_NOT_ESTABLISHED;

    ConnectionInfo *info = NULL;
    info = xmalloc(sizeof(ConnectionInfo));
    info->data = data;

    return info;
}

void ConnectionInfoDestroy(ConnectionInfo **info)
{
    if (!info || !*info)
    {
        return;
    }
    /* Destroy everything */
    if ((*info)->data)
    {
        if ((*info)->data->ssl)
        {
            SSL_free((*info)->data->ssl);
        }
        if ((*info)->data->remote_key)
        {
            KeyDestroy(&(*info)->data->remote_key);
        }
    }
    free ((*info)->data);
    free (*info);
    *info = NULL;
}

ProtocolVersion ConnectionInfoProtocolVersion(const ConnectionInfo *info)
{
    if (!info)
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    if (!info->data)
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    return info->data->type;
}

void ConnectionInfoSetProtocolVersion(ConnectionInfo *info, ProtocolVersion version)
{
    if (!info)
    {
        return;
    }
    if (!info->data)
    {
        return;
    }
    switch (version)
    {
    case CF_PROTOCOL_UNDEFINED:
    case CF_PROTOCOL_CLASSIC:
    case CF_PROTOCOL_TLS:
        info->data->type = version;
        break;
    default:
        break;
    }
}

ConnectionStatus ConnectionInfoConnectionStatus(const ConnectionInfo *info)
{
    if (!info)
    {
        return CF_CONNECTION_NOT_ESTABLISHED;
    }
    if (!info->data)
    {
        return CF_CONNECTION_NOT_ESTABLISHED;
    }
    return info->data->status;
}

void ConnectionInfoSetConnectionStatus(ConnectionInfo *info, ConnectionStatus status)
{
    if (!info)
    {
        return;
    }
    if (!info->data)
    {
        return;
    }
    switch (status)
    {
    case CF_CONNECTION_NOT_ESTABLISHED:
    case CF_CONNECTION_ESTABLISHED:
        info->data->status = status;
    default:
        break;
    }
}

int ConnectionInfoSocket(const ConnectionInfo *info)
{
    if (!info)
    {
        return -1;
    }
    if (!info->data)
    {
        return -1;
    }
    return info->data->sd;
}

void ConnectionInfoSetSocket(ConnectionInfo *info, int s)
{
    if (!info)
    {
        return;
    }
    if (!info->data)
    {
        return;
    }
    info->data->sd = s;
}

SSL *ConnectionInfoSSL(const ConnectionInfo *info)
{
    if (!info)
    {
        return NULL;
    }
    if (!info->data)
    {
        return NULL;
    }
    return info->data->ssl;
}

void ConnectionInfoSetSSL(ConnectionInfo *info, SSL *ssl)
{
    if (!info)
    {
        return;
    }
    if (!info->data)
    {
        return;
    }
    info->data->ssl = ssl;
}

const Key *ConnectionInfoKey(const ConnectionInfo *info)
{
    if (!info)
    {
        return NULL;
    }
    if (!info->data)
    {
        return NULL;
    }
    const Key *key = info->data->remote_key;
    return key;
}

void ConnectionInfoSetKey(ConnectionInfo *info, Key *key)
{
    if (!info)
    {
        return;
    }
    if (!info->data)
    {
        return;
    }
    /* The key can be assigned only once on a session */
    if (info->data->remote_key)
    {
        return;
    }
    if (!key)
    {
        return;
    }
    info->data->remote_key = key;
}

const unsigned char *ConnectionInfoBinaryKeyHash(ConnectionInfo *info, unsigned int *length)
{
    if (!info)
    {
        return NULL;
    }
    if (!info->data)
    {
        return NULL;
    }
    Key *connection_key = info->data->remote_key;
    unsigned int real_length = 0;
    const char *binary = KeyBinaryHash(connection_key, &real_length);
    if (length)
    {
        *length = real_length;
    }
    return binary;
}

const unsigned char *ConnectionInfoPrintableKeyHash(ConnectionInfo *info)
{
    if (!info)
    {
        return NULL;
    }
    if (!info->data)
    {
        return NULL;
    }
    Key *connection_key = info->data->remote_key;
    return KeyPrintableHash(connection_key);
}
