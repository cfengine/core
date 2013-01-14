/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "hashes.h"

int GetHash(const char *key, unsigned int max)
{
    return OatHash(key, max);
}

/*****************************************************************************/

int OatHash(const char *key, unsigned int max)
{
    unsigned const char *p = key;
    unsigned h = 0;
    int i, len = strlen(key);

    for (i = 0; i < len; i++)
    {
        h += p[i];
        h += (h << 10);
        h ^= (h >> 6);
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    return (h & (max - 1));
}

/*****************************************************************************/

int RefHash(char *name, unsigned int max)         // This function wants max to be prime
{
    int i, slot = 0;
    unsigned int macro_alphabet_size = 61;

    for (i = 0; name[i] != '\0'; i++)
    {
        slot = (macro_alphabet_size * slot + name[i]) % max;
    }

    return slot;
}

/*****************************************************************************/

int ElfHash(char *key, unsigned int max)
{
    unsigned char *p = key;
    int len = strlen(key);
    unsigned h = 0, g;
    int i;

    for (i = 0; i < len; i++)
    {
        h = (h << 4) + p[i];
        g = h & 0xf0000000L;

        if (g != 0)
        {
            h ^= g >> 24;
        }

        h &= ~g;
    }

    return (h & (max - 1));
}
