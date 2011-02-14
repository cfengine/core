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

#include "cf3.defs.h"
#include "cf3.extern.h"

/* Maintenance of last-seen database */

/*
 * Last-seen database maps key IDs to client IPs. Also the [FIXME: describe Q].
 *
 * Given DBM cannot be indexed by anything beside keys, auixiliary memory cache
 * is constructed, mapping client IPs to key IDs. This cache is used to avoid
 * linear database search in case mapping of IP to ID is requested and operates
 * under the following assumptions:
 *
 * - No other process *adds* entries to the database, it only may remove it.
 *
 * In case entry is found in memory cache it's validity is tested by checking
 * with database.
 */

/*
 * Database schema:
 *
 * key_hash -> (host_IP, q)
 *
 * In-memory cache schema:
 *
 * host_IP -> key_hash
 *
 */

typedef struct LastSeen
{
    CF_DB *db;
} LastSeen;

LastSeen *last_seen;


static void InitLastSeen()
{
    last_seen = calloc(1, sizeof(LastSeen));
    if (!last_seen)
    {
        CfOut(cf_error, "malloc", "Unable to allocate memory for lastseen DB");
        FatalError("");
    }

    snprintf(name, CF_BUFSIZE-1, "%s%c%s",
             CFWORKDIR, FILE_SEPARATOR, CF_LASTDB_FILE);

    last_seen->db = OpenDB(
}

void FreeHostKey(HostKey *key)
{
    free(key->key_digest);
    free(key->host_ip);
    free(key);
}

HostKey *LookupHostKeyByIP(const char *ip)
{
    /* FIXME */
    return NULL;
}

HostKey *LookupHostKeyByKeyDigest(const char *key_digest)
{
    ThreadLock(cft_server_keyseen);

    if (!last_seen)
        InitLastSeen();

    ThreadUnlock(cft_server_keyseen);

    return NULL;
}

void LastSaw(const char *username, const char *ip_address,
             unsigned char digest[EVP_MAX_MD_SIZE+1], enum roles role)
{
}

/*
 * This function stands out from the rest of the module, as it is called by
 * cf-key agent and hence does not care about memory cache in
 * cf-serverd. Validity of cache in cf-serverd is maintaned by checking with
 * actual DB this function updates.
 */

void ForgetAllKeysForIp(const char *ip_address)
{
}
