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

#ifndef CFENGINE_TLS_H
#define CFENGINE_TLS_H

#include "cfnet.h"

/** 
 @brief TLS routines for CFEngine

 CFEngine uses TLS only as a secondary transport layer. It is possible that we will use TLS as the primary transport layer
 in the future, but for now we start a normal connection and then we switch to TLS.
 */

int ClientStartTLS();
int ClientStopTLS();
/**
  @brief Start a TLS session with the server.

  If this routine fails, the underlying classic connection will be closed.
  @param connection ConnectionInfo structure
  @return 0 if the connection was established and -1 in case of error.
  */
int ServerStartTLS(ConnectionInfo *connection);
int ServerStopTLS();
/**
  @brief Sends the data stored on the buffer using a TLS session.
  @param ssl SSL information.
  @param buffer Data to send.
  @param length Length of the data to send.
  @return The length of the data sent (which could be smaller than the requested length) or -1 in case of error.
  */
int SendTLS(SSL *ssl, const char *buffer, int length);
/**
  @brief Receives data from the SSL session and stores it on the buffer.
  @param ssl SSL information.
  @param buffer Buffer to store the received data.
  @param length Length of the data to receive.
  @return The length of the received data, which could be smaller than the requested or -1 in case of error.
  */
int ReceiveTLS(SSL *ssl, char *buffer, int length);

#endif // CFENGINE_TLS_H

