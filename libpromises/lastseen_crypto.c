/*
  Copyright 2020 Northern.tech AS

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

#include <hash.h>
#include <crypto.h>
#include <lastseen.h>

/**
 * @brief Search for a key given an IP address, by getting the
 *        key hash value from lastseen db.
 * @return NULL if the key was not found in any form.
 */
RSA *HavePublicKeyByIP(const char *username, const char *ipaddress)
{
    char hash[CF_HOSTKEY_STRING_SIZE];

    /* Get the key hash for that address from lastseen db. */
    bool found = Address2Hostkey(hash, sizeof(hash), ipaddress);

    /* If not found, by passing "" as digest, we effectively look only for
     * the old-style key file, e.g. root-1.2.3.4.pub. */
    return HavePublicKey(username, ipaddress,
                         found ? hash : "");
}

/**
 * Trust the given key.  If #ipaddress is not NULL, then also
 * update the "last seen" database.  The IP address is required for
 * trusting a server key (on the client); it is -currently- optional
 * for trusting a client key (on the server).
 */
bool TrustKey(const char *filename, const char *ipaddress, const char *username)
{
    RSA* key;
    char *digest;

    key = LoadPublicKey(filename);
    if (key == NULL)
    {
        return false;
    }

    digest = GetPubkeyDigest(key);
    if (digest == NULL)
    {
        RSA_free(key);
        return false;
    }

    if (ipaddress != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Adding a CONNECT entry in lastseen db: IP '%s', key '%s'",
            ipaddress, digest);
        LastSaw1(ipaddress, digest, LAST_SEEN_ROLE_CONNECT);
    }

    bool ret = SavePublicKey(username, digest, key);
    RSA_free(key);
    free(digest);

    return ret;
}
