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

#ifndef CONNECTION_INFO_H
#define CONNECTION_INFO_H


#include <platform.h>
#include <openssl/ssl.h>
#include <key.h>


/**
  @brief ConnectionInfo Structure and support routines

  ConnectionInfo is used to abstract the underlying type of connection from our protocol implementation.
  It can hold both a normal socket connection and a TLS stream.
 */


/**
 * @brief Status of the connection, for the connection cache and for
 *        propagating errors up in function callers.
 */
typedef enum
{
    CONNECTIONINFO_STATUS_NOT_ESTABLISHED,
    CONNECTIONINFO_STATUS_ESTABLISHED,
    /* used to propagate connection errors up in function calls */
    CONNECTIONINFO_STATUS_BROKEN
    /* TODO ESTABLISHED==IDLE, BUSY, OFFLINE */
} ConnectionStatus;

struct ConnectionInfo {
    ProtocolVersion protocol;
    ConnectionStatus status;
    int sd;                           /* Socket descriptor */
    SSL *ssl;                         /* OpenSSL struct for TLS connections */
    Key *remote_key;
    socklen_t ss_len;
    struct sockaddr_storage ss;
    bool is_call_collect;       /* Maybe replace with a bitfield later ... */
};

typedef struct ConnectionInfo ConnectionInfo;


/**
  @brief Creates a new ConnectionInfo structure.
  @return A initialized ConnectionInfo structure, needs to be populated.
  */
ConnectionInfo *ConnectionInfoNew(void);

/**
  @brief Destroys a ConnectionInfo structure.
  @param info Pointer to the ConectionInfo structure to be destroyed.
  */
void ConnectionInfoDestroy(ConnectionInfo **info);

/**
  @brief Protocol Version
  @param info ConnectionInfo structure
  @return Returns the protocol version or CF_PROTOCOL_UNDEFINED in case of error.
  */
ProtocolVersion ConnectionInfoProtocolVersion(const ConnectionInfo *info);

/**
  @brief Sets the protocol version

  Notice that if an invalid protocol version is passed, the value will not be changed.
  @param info ConnectionInfo structure.
  @param version New protocol version
  */
void ConnectionInfoSetProtocolVersion(ConnectionInfo *info, ProtocolVersion version);

/**
  @brief Connection socket

  For practical reasons there is no difference between an invalid socket and an error on this routine.
  @param info ConnectionInfo structure.
  @return Returns the connection socket or -1 in case of error.
  */
int ConnectionInfoSocket(const ConnectionInfo *info);

/**
  @brief Sets the connection socket.
  @param info ConnectionInfo structure.
  @param s New connection socket.
  */
void ConnectionInfoSetSocket(ConnectionInfo *info, int s);

/**
  @brief SSL structure.
  @param info ConnectionInfo structure.
  @return The SSL structure attached to this connection or NULL in case of error.
  */
SSL *ConnectionInfoSSL(const ConnectionInfo *info);

/**
  @brief Sets the SSL structure.
  @param info ConnectionInfo structure.
  @param ssl SSL structure to attached to this connection.
  */
void ConnectionInfoSetSSL(ConnectionInfo *info, SSL *ssl);

/**
  @brief RSA key
  @param info ConnectionInfo structure.
  @return Returns the RSA key or NULL in case of error.
  */
const Key *ConnectionInfoKey(const ConnectionInfo *info);

/**
  @brief Sets the key for the connection structure.

  This triggers a calculation of two other fields.
  @param info ConnectionInfo structure.
  @param key RSA key.
  */
void ConnectionInfoSetKey(ConnectionInfo *info, Key *key);

/**
  @brief A constant pointer to the binary hash of the key
  @param info ConnectionInfo structure
  @param length Length of the hash
  @return Returns a constant pointer to the binary hash and if length is not NULL the size is stored there.
  */
const unsigned char *ConnectionInfoBinaryKeyHash(ConnectionInfo *info, unsigned int *length);

/**
  @brief A constant pointer to the binary hash of the key
  @param info ConnectionInfo structure
  @return Returns a printable representation of the hash. The string is '\0' terminated or NULL in case of failure.
  */
const char *ConnectionInfoPrintableKeyHash(ConnectionInfo *info);


#endif // CONNECTION_INFO_H
