/*
   Copyright 2017 Northern.tech AS

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


#ifndef CFENGINE_TLS_GENERIC_H
#define CFENGINE_TLS_GENERIC_H


#include <cfnet.h>

#include <openssl/ssl.h>

#include <logging.h>                                            /* LogLevel */


extern int CONNECTIONINFO_SSL_IDX;


bool TLSGenericInitialize(void);
int TLSVerifyCallback(X509_STORE_CTX *ctx, void *arg);
int TLSVerifyPeer(ConnectionInfo *conn_info, const char *remoteip, const char *username);
X509 *TLSGenerateCertFromPrivKey(RSA *privkey);
int TLSLogError(SSL *ssl, LogLevel level, const char *prepend, int code);
int TLSSend(SSL *ssl, const char *buffer, int length);
int TLSRecv(SSL *ssl, char *buffer, int toget);
int TLSRecvLines(SSL *ssl, char *buf, size_t buf_size);
void TLSSetDefaultOptions(SSL_CTX *ssl_ctx, const char *min_version);
const char *TLSErrorString(intmax_t errcode);

#endif
