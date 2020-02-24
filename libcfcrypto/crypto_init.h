#ifndef CFCRYPTO_CRYPTO_INIT_H
#define CFCRYPTO_CRYPTO_INIT_H

#include <enterprise_extension.h>
#include <hash_method.h>

void CryptoInitialize(time_t start_time, const char *host);
void CryptoDeInitialize(void);
ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, GenericAgentSetDefaultDigest, HashMethod *, digest, int *, digest_len);

#endif  /* CFCRYPTO_CRYPTO_INIT_H */
