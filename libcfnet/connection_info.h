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

#ifndef CONNECTION_INFO_H
#define CONNECTION_INFO_H

#include <platform.h>
#include <openssl/ssl.h>
#include <key.h>

/**
  @brief ConnectionInfo Structure and support routines

  ConnectionInfo is used to abstract the underlying type of connection from our protocol implementation.
  It can hold both a normal socket connection and a TLS stream.

  Notice that despite being reference counted, we will not detach this structure once shared in order to
  modify it. This arises from the fact that we handle a couple of structures that are opaque to us, such
  as RSA and SSL. We cannot copy those structures since they are completely opaque, therefore we cannot
  modify this structure once it has been shared. As a safety measure, the copy will fail if the structure
  is not ready.
  */

/**
  @brief Available protocol versions
  */
typedef enum
{
    /* When connection is initialised ProtocolVersion is 0, i.e. undefined. */
    CF_PROTOCOL_UNDEFINED, /*!< Protocol not defined yet */
    CF_PROTOCOL_CLASSIC, /*!< Normal CFEngine protocol */
    CF_PROTOCOL_TLS /*!< TLS protocol */
} ProtocolVersion;

/**
  @brief States of the connection.
  */
typedef enum
{
    /* Status of the connection so we can detect if we need to negotiate a new connection or not */
    CF_CONNECTION_NOT_ESTABLISHED, /*!< Connection not established yet */
    CF_CONNECTION_ESTABLISHED /*!< Connection established */
} ConnectionStatus;

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
  @brief Connection status
  @param info ConnectionInfo structure
  @return Returns the status of the connection or CF_CONNECTION_NOT_ESTABLISHED in case of error.
*/
ConnectionStatus ConnectionInfoConnectionStatus(const ConnectionInfo *info);
/**
  @brief Sets the connection status.
  @param info ConnectionInfo structure.
  @param status New status
  */
void ConnectionInfoSetConnectionStatus(ConnectionInfo *info, ConnectionStatus status);
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
