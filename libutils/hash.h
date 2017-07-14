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

#ifndef CFENGINE_HASH_H
#define CFENGINE_HASH_H

/**
  @brief Hash implementations
  */

#include <openssl/rsa.h>

#include <hash_method.h>                            /* HashMethod, HashSize */


typedef struct Hash Hash;

/**
  @brief Creates a new structure of type Hash.
  @param data String to hash.
  @param length Length of the string to hash.
  @param method Hash method.
  @return A structure of type Hash or NULL in case of error.
  */
Hash *HashNew(const char *data, const unsigned int length, HashMethod method);

/**
  @brief Creates a new structure of type Hash.
  @param descriptor Either file descriptor or socket descriptor.
  @param method Hash method.
  @return A structure of type Hash or NULL in case of error.
  */
Hash *HashNewFromDescriptor(const int descriptor, HashMethod method);

/**
  @brief Creates a new structure of type Hash.
  @param rsa RSA key to be hashed.
  @param method Hash method.
  @return A structure of type Hash or NULL in case of error.
  */
Hash *HashNewFromKey(const RSA *rsa, HashMethod method);

/**
  @brief Destroys a structure of type Hash.
  @param hash The structure to be destroyed.
  */
void HashDestroy(Hash **hash);

/**
  @brief Copy a hash
  @param origin Hash to be copied.
  @param destination Hash to be copied to.
  @return 0 if successful, -1 in any other case.
  */
int HashCopy(Hash *origin, Hash **destination);

/**
  @brief Checks if two hashes are equal.
  @param a 1st hash to be compared.
  @param b 2nd hash to be compared.
  @return True if both hashes are equal and false in any other case.
  */
int HashEqual(const Hash *a, const Hash *b);

/**
  @brief Pointer to the raw digest data.
  @note Notice that this is a binary representation and not '\0' terminated.
  @param hash Hash structure.
  @param length Pointer to an unsigned int to hold the length of the data.
  @return A pointer to the raw digest data.
  */
const unsigned  char *HashData(const Hash *hash, unsigned int *length);

/**
  @brief Printable hash representation.
  @param hash Hash structure.
  @return A pointer to the printable digest representation.
  */
const char *HashPrintable(const Hash *hash);

/**
  @brief Hash type.
  @param hash Hash structure
  @return The hash method used by this hash structure.
  */
HashMethod HashType(const Hash *hash);

/**
  @brief Hash length in bytes.
  @param hash Hash structure
  @return The hash length in bytes.
  */
HashSize HashLength(const Hash *hash);

/**
  @brief Returns the ID of the hash based on the name
  @param hash_name Name of the hash.
  @return Returns the ID of the hash from the name.
  */
HashMethod HashIdFromName(const char *hash_name);

/**
  @brief Returns the name of the hash based on the ID.
  @param hash_id Id of the hash.
  @return Returns the name of the hash.
  */
const char *HashNameFromId(HashMethod hash_id);

/**
  @brief Size of the hash
  @param method Hash method
  @return Returns the size of the hash or 0 in case of error.
  */
HashSize HashSizeFromId(HashMethod hash_id);

#endif // CFENGINE_HASH_H
