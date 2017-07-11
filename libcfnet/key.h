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

#ifndef KEY_H
#define KEY_H

#include <hash.h>
#include <openssl/rsa.h>

/**
  @brief Structure to simplify the key management.

  */
typedef struct Key Key;

/**
  @brief Creates a new Key structure.
  @param key RSA structure
  @param hash Hash method to use when hashing the key.
  @return A fully initialized Key structure or NULL in case of error.
  */
Key *KeyNew(RSA *rsa, HashMethod method);
/**
  @brief Destroys a structure of type Key.
  @param key Structure to be destroyed.
  */
void KeyDestroy(Key **key);
/**
  @brief Constant pointer to the key data.
  @param key Key
  @return A pointer to the RSA structure.
  */
RSA *KeyRSA(const Key *key);
/**
  @brief Binary hash of the key
  @param key Key structure
  @param length Length of the binary hash
  @return A pointer to the binary hash or NULL in case of error.
  */
const unsigned char *KeyBinaryHash(const Key *key, unsigned int *length);
/**
  @brief Printable hash of the key.
  @param key
  @return A pointer to the printable hash of the key.
  */
const char *KeyPrintableHash(const Key *key);
/**
  @brief Method use to hash the key.
  @param key Structure
  @return Method used to hash the key.
  */
HashMethod KeyHashMethod(const Key *key);
/**
  @brief Changes the method used to hash the key.

  This method triggers a rehashing of the key. This can be an expensive operation.
  @param key Structure
  @param hash New hashing mechanism.
  @return 0 if successful, -1 in case of error.
  */
int KeySetHashMethod(Key *key, HashMethod method);
/**
  @brief Internal Hash data
  @param key Structure
  @return A pointer to the Hash structure or NULL in case of error.
  */
const Hash *KeyData(Key *key);

#endif // KEY_H
