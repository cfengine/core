#ifndef CFCRYPTO_CRYPTO_INIT_H
#define CFCRYPTO_CRYPTO_INIT_H

#include <enterprise_extension.h>
#include <hash_method.h>

extern HashMethod CF_DEFAULT_DIGEST;
extern int CF_DEFAULT_DIGEST_LEN;

extern bool FIPS_MODE;

void CryptoInitialize(time_t start_time, const char *host);
void CryptoDeInitialize(void);

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, GenericAgentSetDefaultDigest, HashMethod *, digest, int *, digest_len);
ENTERPRISE_FUNC_1ARG_DECLARE(const EVP_CIPHER *, CfengineCipher, char, type);
ENTERPRISE_FUNC_1ARG_DECLARE(int, CfSessionKeySize, char, c);
ENTERPRISE_FUNC_0ARG_DECLARE(char, CfEnterpriseOptions);

#endif  /* CFCRYPTO_CRYPTO_INIT_H */
