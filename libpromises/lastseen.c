/*
   Copyright (C) CFEngine AS

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

#include <cf3.defs.h>

#include <lastseen.h>
#include <conversion.h>
#include <files_hashes.h>
#include <locks.h>
#include <item_lib.h>
#include <known_dirs.h>
#ifdef LMDB
#include <lmdb.h>
#endif

void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp);

/*
 * Lastseen database schema (version 1):
 *
 * Version entry
 *
 * key: "version\0"
 * value: "1\0"
 *
 * "Quality of connection" entries
 *
 * key: q<direction><hostkey> (direction: 'i' for incoming, 'o' for outgoing)
 * value: struct KeyHostSeen
 *
 * "Hostkey" entries
 *
 * key: k<hostkey> ("MD5-ffffefefeefef..." or "SHA-abacabaca...")
 * value: <address> (IPv4 or IPv6)
 *
 * "Address", or reverse, entries (auxiliary)
 *
 * key: a<address> (IPv6 or IPv6)
 * value: <hostkey>
 *
 *
 *
 * Schema version 0 mapped direction + hostkey to address + quality of
 * connection. This approach had a number of drawbacks:
 *  - There were two potentially conflicting addresses for given hostkey.
 *  - There was no way to quickly lookup hostkey by address.
 *  - Address update required traversal of the whole database.
 *
 * In order to overcome these limitations, new schema normalized (in relational
 * algebra sense) the data relations.
 */

/*****************************************************************************/

/**
 * @brief Same as LastSaw() but the digest parameter is the hash as a
 *        "SHA=..." string, to avoid converting twice.
 */
void LastSaw1(const char *ipaddress, const char *hashstr,
              LastSeenRole role)
{
    const char *mapip = MapAddress(ipaddress);
    UpdateLastSawHost(hashstr, mapip, role == LAST_SEEN_ROLE_ACCEPT, time(NULL));
}

void LastSaw(const char *ipaddress, const char *digest, LastSeenRole role)
{
    char databuf[CF_HOSTKEY_STRING_SIZE];

    if (strlen(ipaddress) == 0)
    {
        Log(LOG_LEVEL_INFO, "LastSeen registry for empty IP with role %d", role);
        return;
    }

    HashPrintSafe(databuf, sizeof(databuf), digest, CF_DEFAULT_DIGEST, true);

    const char *mapip = MapAddress(ipaddress);

    UpdateLastSawHost(databuf, mapip, role == LAST_SEEN_ROLE_ACCEPT, time(NULL));
}

/*****************************************************************************/

void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp)
{
    DBHandle *db = NULL;
    if (!OpenDB(&db, dbid_lastseen))
    {
        Log(LOG_LEVEL_ERR, "Unable to open last seen db");
        return;
    }

    /* Update quality-of-connection entry */

    char quality_key[CF_BUFSIZE];
    snprintf(quality_key, CF_BUFSIZE, "q%c%s", incoming ? 'i' : 'o', hostkey);

    KeyHostSeen newq = { .lastseen = timestamp };

    KeyHostSeen q;
    if (ReadDB(db, quality_key, &q, sizeof(q)))
    {
        newq.Q = QAverage(q.Q, newq.lastseen - q.lastseen, 0.4);
    }
    else
    {
        /* FIXME: more meaningful default value? */
        newq.Q = QDefinite(0);
    }
    WriteDB(db, quality_key, &newq, sizeof(newq));

    /* Update forward mapping */

    char hostkey_key[CF_BUFSIZE];
    snprintf(hostkey_key, CF_BUFSIZE, "k%s", hostkey);

    WriteDB(db, hostkey_key, address, strlen(address) + 1);

    /* Update reverse mapping */

    char address_key[CF_BUFSIZE];
    snprintf(address_key, CF_BUFSIZE, "a%s", address);

    WriteDB(db, address_key, hostkey, strlen(hostkey) + 1);

    CloseDB(db);
}
/*****************************************************************************/

static bool Address2HostkeyInDB(DBHandle *db, const char *address, char *result)
{
    char address_key[CF_BUFSIZE];
    char hostkey[CF_BUFSIZE];

    /* Address key: "a" + address */
    snprintf(address_key, CF_BUFSIZE, "a%s", address);

    if (!ReadDB(db, address_key, &hostkey, sizeof(hostkey)))
    {
        return false;
    }

    char hostkey_key[CF_BUFSIZE];
    char back_address[CF_BUFSIZE];

    /* Hostkey key: "k" + hostkey */
    snprintf(hostkey_key, CF_BUFSIZE, "k%s", hostkey);

    if (!ReadDB(db, hostkey_key, &back_address, sizeof(back_address)))
    {
        /* There is no key -> address mapping. Remove reverse mapping and return failure. */
        DeleteDB(db, address_key);
        return false;
    }

    if (strcmp(address, back_address) != 0)
    {
        /* Forward and reverse mappings do not match. Remove reverse mapping and return failure. */
        DeleteDB(db, address_key);
        return false;
    }

    strlcpy(result, hostkey, CF_BUFSIZE);
    return true;
}

/*****************************************************************************/

bool Address2Hostkey(char *dst, size_t dst_size, const char *address)
{
    dst[0] = '\0';
    if ((strcmp(address, "127.0.0.1") == 0) ||
        (strcmp(address, "::1") == 0) ||
        (strcmp(address, VIPADDRESS) == 0))
    {
        if (PUBKEY)
        {
            unsigned char digest[EVP_MAX_MD_SIZE + 1];
            HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
            HashPrintSafe(dst, dst_size, digest,
                          CF_DEFAULT_DIGEST, true);
            return true;
        }
        else
        {
            return false;
        }
    }

    DBHandle *db;
    if (!OpenDB(&db, dbid_lastseen))
    {
        return false;
    }

    bool ret = Address2HostkeyInDB(db, address, dst);
    CloseDB(db);
    return ret;
}
/**
 * @brief detects whether input is a host/ip name or a key digest
 *
 * @param[in] key digest (SHA/MD5 format) or free host name string
 *            (character '=' is optional but recommended)
 * @retval true if a key digest, false otherwise
 */
static bool IsDigestOrHost(const char *input)
{
    if (strncmp(input, "SHA=", 3) == 0)
    {
        return true;
    }
    else if (strncmp(input, "MD5=", 3) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * @brief check whether the lastseen DB is coherent or not
 * 
 * A DB is coherent mainly if all the entries are valid and if there is
 * a strict one-to-one correspondance between hosts and key digests
 * (whether in MD5 or SHA1 format).
 *
 * @retval true if the lastseen DB is coherent, false otherwise
 */
bool IsLastSeenCoherent(void)
{
    DBHandle *db;
    DBCursor *cursor;
    bool res = true;

    if (!OpenDB(&db, dbid_lastseen))
    {
        Log(LOG_LEVEL_ERR, "Unable to open lastseen database");
        return false;
    }

    if (!NewDBCursor(db, &cursor))
    {
        Log(LOG_LEVEL_ERR, "Unable to create lastseen database cursor");
        CloseDB(db);
        return false;
    }

    char *key;
    void *value;
    int ksize, vsize;

    Item *qkeys=NULL;
    Item *akeys=NULL;
    Item *kkeys=NULL;
    Item *ahosts=NULL;
    Item *khosts=NULL;

    while (NextDB(cursor, &key, &ksize, &value, &vsize))
    {
        if (key[0] != 'k' && key[0] != 'q' && key[0] != 'a' )
        {
            continue;
        }

        if (key[0] == 'q' )
        {
            if (strncmp(key,"qiSHA=",5)==0 || strncmp(key,"qoSHA=",5)==0 ||
                strncmp(key,"qiMD5=",5)==0 || strncmp(key,"qoMD5=",5)==0)
            {
                if (IsItemIn(qkeys, key+2)==false)
                {
                    PrependItem(&qkeys, key+2, NULL);
                }
            }
        }

        if (key[0] == 'k' )
        {
            if (strncmp(key, "kSHA=", 4)==0 || strncmp(key, "kMD5=", 4)==0)
            {
                if (IsItemIn(kkeys, key+1)==false)
                {
                    PrependItem(&kkeys, key+1, NULL);
                }
                if (IsItemIn(khosts, value)==false)
                {
                    PrependItem(&khosts, value, NULL);
                }
            }
        }

        if (key[0] == 'a' )
        {
            if (IsItemIn(ahosts, key+1)==false)
            {
                PrependItem(&ahosts, key+1, NULL);
            }
            if (IsItemIn(akeys, value)==false)
            {
                PrependItem(&akeys, value, NULL);
            }
        }
    }

    DeleteDBCursor(cursor);
    CloseDB(db);

    if (ListsCompare(ahosts, khosts) == false)
    {
        res = false;
        goto clean;
    }
    if (ListsCompare(akeys, kkeys) == false)
    {
        res = false;
        goto clean;
    }

clean:
    DeleteItemList(qkeys);
    DeleteItemList(akeys);
    DeleteItemList(kkeys);
    DeleteItemList(ahosts);
    DeleteItemList(khosts);

    return res;
}

/**
 * @brief removes all traces of host 'ip' from lastseen DB
 *
 * @param[in]     ip : either in (SHA/MD5 format)
 * @param[in,out] digest: return corresponding digest of input host.
 *                        If NULL, return nothing
 * @retval true if entry was deleted, false otherwise
 */
bool DeleteIpFromLastSeen(const char *ip, char *digest)
{
    DBHandle *db;
    bool res = false;

    if (!OpenDB(&db, dbid_lastseen))
    {
        Log(LOG_LEVEL_ERR, "Unable to open lastseen database");
        return false;
    }

    char bufkey[CF_BUFSIZE + 1];
    char bufhost[CF_BUFSIZE + 1];

    strcpy(bufhost, "a");
    strlcat(bufhost, ip, CF_BUFSIZE);

    char key[CF_BUFSIZE];
    if (ReadDB(db, bufhost, &key, sizeof(key)) == true)
    {
        strcpy(bufkey, "k");
        strlcat(bufkey, key, CF_BUFSIZE);
        if (HasKeyDB(db, bufkey, strlen(bufkey) + 1) == false)
        {
            res = false;
            goto clean;
        }
        else
        {
            if (digest != NULL)
            {
                strcpy(digest, bufkey + 1);
            }
            DeleteDB(db, bufkey);
            DeleteDB(db, bufhost);
            res = true;
        }
    }
    else
    {
        res = false;
        goto clean;
    }

    strcpy(bufkey, "qi");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

    strcpy(bufkey, "qo");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

clean:
    CloseDB(db);
    return res;
}

/**
 * @brief removes all traces of key digest 'key' from lastseen DB
 *
 * @param[in]     key : either in (SHA/MD5 format)
 * @param[in,out] ip  : return the key corresponding host.
 *                      If NULL, return nothing
 * @retval true if entry was deleted, false otherwise
 */
bool DeleteDigestFromLastSeen(const char *key, char *ip)
{
    DBHandle *db;
    bool res = false;

    if (!OpenDB(&db, dbid_lastseen))
    {
        Log(LOG_LEVEL_ERR, "Unable to open lastseen database");
        return false;
    }
    char bufkey[CF_BUFSIZE + 1];
    char bufhost[CF_BUFSIZE + 1];

    strcpy(bufkey, "k");
    strlcat(bufkey, key, CF_BUFSIZE);

    char host[CF_BUFSIZE];
    if (ReadDB(db, bufkey, &host, sizeof(host)) == true)
    {
        strcpy(bufhost, "a");
        strlcat(bufhost, host, CF_BUFSIZE);
        if (HasKeyDB(db, bufhost, strlen(bufhost) + 1) == false)
        {
            res = false;
            goto clean;
        }
        else
        {
            if (ip != NULL)
            {
                strcpy(ip, host);
            }
            DeleteDB(db, bufhost);
            DeleteDB(db, bufkey);
            res = true;
        }
    }
    else
    {
        res = false;
        goto clean;
    }

    strcpy(bufkey, "qi");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

    strcpy(bufkey, "qo");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

clean:
    CloseDB(db);
    return res;
}

/*****************************************************************************/
bool ScanLastSeenQuality(LastSeenQualityCallback callback, void *ctx)
{
    StringMap *lastseen_db = LoadDatabaseToStringMap(dbid_lastseen);
    if (!lastseen_db)
    {
        return false;
    }
    MapIterator it = MapIteratorInit(lastseen_db->impl);
    MapKeyValue *item;

    Seq *hostkeys = SeqNew(100, free);
    while ((item = MapIteratorNext(&it)) != NULL)
    {
        char *key = item->key;
        /* Only look for "keyhost" entries */
        if (key[0] != 'k')
        {
            continue;
        }

        SeqAppend(hostkeys, xstrdup(key + 1));
    }
    for (int i = 0; i < SeqLength(hostkeys); ++i)
    {
        const char *hostkey = SeqAt(hostkeys, i);

        char keyhost_key[CF_BUFSIZE];
        snprintf(keyhost_key, CF_BUFSIZE, "k%s", hostkey);
        char *address = NULL;
        address = (char*)StringMapGet(lastseen_db, keyhost_key);
        if (!address)
        {
            Log(LOG_LEVEL_ERR, "Failed to read address for key '%s'.", hostkey);
            continue;
        }

        char incoming_key[CF_BUFSIZE];
        snprintf(incoming_key, CF_BUFSIZE, "qi%s", hostkey);
        KeyHostSeen *incoming = NULL;
        incoming = (KeyHostSeen*)StringMapGet(lastseen_db, incoming_key);
        if (incoming)
        {
            if (!(*callback)(hostkey, address, true, incoming, ctx))
            {
                break;
            }
        }

        char outgoing_key[CF_BUFSIZE];
        snprintf(outgoing_key, CF_BUFSIZE, "qo%s", hostkey);
        KeyHostSeen *outgoing = NULL;
        outgoing = (KeyHostSeen*)StringMapGet(lastseen_db, outgoing_key);
        if (outgoing)
        {
            if (!(*callback)(hostkey, address, false, outgoing, ctx))
            {
                break;
            }
        }
    }

    StringMapDestroy(lastseen_db);
    SeqDestroy(hostkeys);

    return true;
}

/*****************************************************************************/

int LastSeenHostKeyCount(void)
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    QPoint entry;
    char *key;
    void *value;
    int ksize, vsize;

    int count = 0;

    if (OpenDB(&dbp, dbid_lastseen))
    {
        memset(&entry, 0, sizeof(entry));

        if (NewDBCursor(dbp, &dbcp))
        {
            while (NextDB(dbcp, &key, &ksize, &value, &vsize))
            {
                /* Only look for valid "hostkey" entries */

                if ((key[0] != 'k') || (value == NULL))
                {
                    continue;
                }

                count++;
            }

            DeleteDBCursor(dbcp);
        }

        CloseDB(dbp);
    }

    return count;
}
/**
 * @brief removes all traces of entry 'input' from lastseen DB
 *
 * @param[in] key digest (SHA/MD5 format) or free host name string
 * @param[in] must_be_coherent. false : delete if lastseen is incoherent, 
 *                              true :  don't if lastseen is incoherent
 * @param[out] equivalent. If input is a host, return its corresponding
 *                         digest. If input is a digest, return its
 *                         corresponding host. CAN BE NULL! If equivalent
 *                         is null, it stays as NULL
 * @retval 0 if entry was deleted, <>0 otherwise
 */
int RemoveKeysFromLastSeen(const char *input, bool must_be_coherent,
                           char *equivalent)
{
    bool is_coherent = false;

    if (must_be_coherent == true)
    {
        is_coherent = IsLastSeenCoherent();
        if (is_coherent == false)
        {
            Log(LOG_LEVEL_ERR, "Lastseen database is incoherent (there is not a 1-to-1 relationship between hosts and keys) and coherence check is enforced. Will not proceed to remove entries from it.");
            return 254;
        }
    }

    bool is_digest;
    is_digest = IsDigestOrHost(input);

    if (is_digest == true)
    {
        Log(LOG_LEVEL_VERBOSE, "Removing digest '%s' from lastseen database\n", input);
        if (DeleteDigestFromLastSeen(input, equivalent) == false)
        {
            Log(LOG_LEVEL_ERR, "Unable to remove digest from lastseen database.");
            return 252;
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Removing host '%s' from lastseen database\n", input);
        if (DeleteIpFromLastSeen(input, equivalent) == false)
        {
            Log(LOG_LEVEL_ERR, "Unable to remove host from lastseen database.");
            return 253;
        }
    }

    Log(LOG_LEVEL_INFO, "Removed corresponding entries from lastseen database.");

    return 0;
}

