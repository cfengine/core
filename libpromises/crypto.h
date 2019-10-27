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

#ifndef CFENGINE_CRYPTO_H
#define CFENGINE_CRYPTO_H

#include <platform.h>

#include <openssl/rsa.h>

#include <logging.h>

// This passphrase was used to encrypt private keys.
// We no longer encrypt new keys, but the passphrase is kept
// for backwards compatibility - old encrypted keys will still work.
#define PRIVKEY_PASSPHRASE "Cfengine passphrase"
#define PRIVKEY_PASSPHRASE_LEN 19

void CryptoInitialize(void);
void CryptoDeInitialize(void);

const char *CryptoLastErrorString(void);
void DebugBinOut(char *buffer, int len, char *com);
bool LoadSecretKeys(const char *const priv_key_path,
                    const char *const pub_key_path,
                    RSA **priv_key, RSA **pub_key);
void PolicyHubUpdateKeys(const char *policy_server);
int EncryptString(char *out, size_t out_size, const char *in, int plainlen,
                  char type, unsigned char *key);
size_t CipherBlockSizeBytes(const EVP_CIPHER *cipher);
size_t CipherTextSizeMax(const EVP_CIPHER* cipher, size_t plaintext_size);
size_t PlainTextSizeMax(const EVP_CIPHER* cipher, size_t ciphertext_size);
int DecryptString(char *out, size_t out_size, const char *in, int cipherlen,
                  char type, unsigned char *key);
RSA *HavePublicKey(const char *username, const char *ipaddress, const char *digest);
RSA *HavePublicKeyByIP(const char *username, const char *ipaddress);
bool SavePublicKey(const char *username, const char *digest, const RSA *key);
RSA *LoadPublicKey(const char *filename);
char *LoadPubkeyDigest(const char *pubkey);
char *GetPubkeyDigest(RSA *pubkey);
bool TrustKey(const char *pubkey, const char *ipaddress, const char *username);

char *PublicKeyFile(const char *workdir);
char *PrivateKeyFile(const char *workdir);
LogLevel CryptoGetMissingKeyLogLevel(void);

#endif
