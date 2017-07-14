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

#include <alloc.h>
#include <key.h>

struct Key {
    RSA *key;
    Hash *hash;
};

Key *KeyNew(RSA *rsa, HashMethod method)
{
    if (!rsa)
    {
        return NULL;
    }
    Key *key = xmalloc (sizeof(Key));
    key->key = rsa;
    /* Hash the key */
    key->hash = HashNewFromKey(rsa, method);
    if (key->hash == NULL)
    {
        free (key);
        return NULL;
    }
    return key;
}

void KeyDestroy(Key **key)
{
    if (!key || !*key)
    {
        return;
    }
    if ((*key)->key)
    {
        RSA_free((*key)->key);
    }
    HashDestroy(&(*key)->hash);
    free (*key);
    *key = NULL;
}

RSA *KeyRSA(const Key *key)
{
    return key ? key->key : NULL;
}

const unsigned char *KeyBinaryHash(const Key *key, unsigned int *length)
{
    if (!key || !length)
    {
        return NULL;
    }
    return HashData(key->hash, length);
}

const char *KeyPrintableHash(const Key *key)
{
    return key ? HashPrintable(key->hash) : NULL;
}

HashMethod KeyHashMethod(const Key *key)
{
    return key ? HashType(key->hash) : HASH_METHOD_NONE;
}

int KeySetHashMethod(Key *key, HashMethod method)
{
    if (!key)
    {
        return -1;
    }
    /* We calculate the new hash before changing the value, in case there is an error. */
    Hash *hash = NULL;
    hash = HashNewFromKey(key->key, method);
    if (hash == NULL)
    {
        return -1;
    }
    if (key->hash)
    {
        HashDestroy(&key->hash);
    }
    key->hash = hash;
    return 0;
}

const Hash *KeyData(Key *key)
{
    return key ? key->hash : NULL;
}
