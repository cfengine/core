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

#ifndef CFENGINE_CRYPTO_H
#define CFENGINE_CRYPTO_H

#include <platform.h>

#include <openssl/rsa.h>

#include <logging.h>


void CryptoInitialize(void);
void CryptoDeInitialize(void);

const char *CryptoLastErrorString(void);
void DebugBinOut(char *buffer, int len, char *com);
bool LoadSecretKeys(void);
void PolicyHubUpdateKeys(const char *policy_server);
int EncryptString(char type, const char *in, char *out, unsigned char *key, int len);
int DecryptString(char type, const char *in, char *out, unsigned char *key, int len);
RSA *HavePublicKey(const char *username, const char *ipaddress, const char *digest);
RSA *HavePublicKeyByIP(const char *username, const char *ipaddress);
bool SavePublicKey(const char *username, const char *digest, const RSA *key);

char *PublicKeyFile(const char *workdir);
char *PrivateKeyFile(const char *workdir);
LogLevel CryptoGetMissingKeyLogLevel(void);

#endif
