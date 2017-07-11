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

/* TODO #ifndef NDEBUG check, report loudly, and fix consistency issues in every operation. */

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

/* Lookup a reverse entry (IP->KeyHash) in lastseen database. */
static bool Address2HostkeyInDB(DBHandle *db, const char *address, char *result, size_t result_size)
{
    char address_key[CF_BUFSIZE];
    char hostkey[CF_BUFSIZE];

    /* Address key: "a" + address */
    snprintf(address_key, CF_BUFSIZE, "a%s", address);

    if (!ReadDB(db, address_key, &hostkey, sizeof(hostkey)))
    {
        return false;
    }

#ifndef NDEBUG
    /* Check for inconsistencies. Return success even if db is found
     * inconsistent, since the reverse entry is already found. */

    char hostkey_key[CF_BUFSIZE];
    char back_address[CF_BUFSIZE];

    /* Hostkey key: "k" + hostkey */
    snprintf(hostkey_key, CF_BUFSIZE, "k%s", hostkey);

    if (!ReadDB(db, hostkey_key, &back_address, sizeof(back_address)))
    {
        Log(LOG_LEVEL_WARNING, "Lastseen db inconsistency: "
            "no key entry '%s' for existing host entry '%s'",
            hostkey_key, address_key);
    }
#endif

    strlcpy(result, hostkey, result_size);
    return true;
}

/*****************************************************************************/

/* Given an address it returns a key - its own key if address is 127.0.0.1,
 * else it looks the "aADDRESS" entry in lastseen. */
bool Address2Hostkey(char *dst, size_t dst_size, const char *address)
{
    bool retval = false;
    dst[0] = '\0';

    if ((strcmp(address, "127.0.0.1") == 0) ||
        (strcmp(address, "::1") == 0) ||
        (strcmp(address, VIPADDRESS) == 0))
    {
        Log(LOG_LEVEL_DEBUG,
            "Address2Hostkey: Returning local key for address %s",
            address);

        if (PUBKEY)
        {
            unsigned char digest[EVP_MAX_MD_SIZE + 1];
            HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
            HashPrintSafe(dst, dst_size, digest,
                          CF_DEFAULT_DIGEST, true);
            retval = true;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE,
                "Local key not found, generate one with cf-key?");
            retval = false;
        }
    }
    else                                                 /* lastseen lookup */
    {
        DBHandle *db;
        if (OpenDB(&db, dbid_lastseen))
        {
            retval = Address2HostkeyInDB(db, address, dst, dst_size);
            CloseDB(db);

            if (!retval)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Key digest for address '%s' was not found in lastseen db!",
                    address);
            }
        }
    }

    return retval;
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
 * @brief check whether the lastseen DB is coherent or not.
 *
 * It is allowed for a aIP1 -> KEY1 to not have a reverse kKEY1 -> IP.
 * kKEY1 *must* exist, but may point to another IP.
 * Same for IP values, they *must* appear as aIP entries, but we don't
 * care where they point to.
 * So for every aIP->KEY1 entry there should be a kKEY1->whatever entry.
 * And for every kKEY->IP1 entry there should be a aIP1->whatever entry.
 *
 * If a host changes IP, then we have a new entry aIP2 -> KEY1 together
 * with the aIP1 -> KEY1 entry. ALLOWED.
 *
 * If a host changes key, then its entry will become aIP1 -> KEY2.
 * Then still it will exist kKEY1 -> IP1 but also kKEY2 -> IP1. ALLOWED
 *
 * Can I have a IP value of some kKEY that does not have any aIP entry?
 * NO because at some time aIP it was written in the database.
 * SO EVERY kIP must be found in aIPS.
 * kIPS SUBSET OF aIPS
 *
 * Can I have a KEY value of some aIP that does not have any kKEY entry?
 * NO for the same reason.
 * SO EVERY akey must be found in kkeys.
 * aKEYS SUBSET OF kKEYS
 *
 * FIN
 *
 * @TODO P.S. redesign lastseen. Really, these whole requirements are
 *       implemented on top of a simple key-value store, no wonder it's such a
 *       mess. I believe that reverse mapping is not even needed since only
 *       aIP entries are ever looked up. kKEY entries can be deprecated and
 *       forget all the false notion of "schema consistency" in this key-value
 *       store...
 *
 * @retval true if the lastseen DB is coherent, false otherwise.
 */
bool IsLastSeenCoherent(void)
{
    DBHandle *db;
    DBCursor *cursor;

    if (!OpenDB(&db, dbid_lastseen))
    {
        char *db_path = DBIdToPath(dbid_lastseen);
        Log(LOG_LEVEL_ERR, "Unable to open lastseen database '%s'", db_path);
        free(db_path);
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

    Item *qKEYS = NULL;
    Item *aKEYS = NULL;
    Item *kKEYS = NULL;
    Item *aIPS = NULL;
    Item *kIPS = NULL;

    bool result = true;
    while (NextDB(cursor, &key, &ksize, &value, &vsize))
    {
        if (strcmp(key, "version") != 0 &&
            strncmp(key, "qi", 2) != 0 &&
            strncmp(key, "qo", 2) != 0 &&
            key[0] != 'k' &&
            key[0] != 'a')
        {
            Log(LOG_LEVEL_WARNING,
                "lastseen db inconsistency, unexpected key: %s",
                key);
            result = false;
        }

        if (key[0] == 'q' )
        {
            if (strncmp(key,"qiSHA=",5)==0 || strncmp(key,"qoSHA=",5)==0 ||
                strncmp(key,"qiMD5=",5)==0 || strncmp(key,"qoMD5=",5)==0)
            {
                if (!IsItemIn(qKEYS, key+2))
                {
                    PrependItem(&qKEYS, key+2, NULL);
                }
            }
        }

        if (key[0] == 'k' )
        {
            if (strncmp(key, "kSHA=", 4)==0 || strncmp(key, "kMD5=", 4)==0)
            {
                if (!IsItemIn(kKEYS, key+1))
                {
                    PrependItem(&kKEYS, key+1, NULL);
                }
                if (!IsItemIn(kIPS, value))
                {
                    PrependItem(&kIPS, value, NULL);
                }
            }
        }

        if (key[0] == 'a' )
        {
            if (!IsItemIn(aIPS, key+1))
            {
                PrependItem(&aIPS, key+1, NULL);
            }
            if (!IsItemIn(aKEYS, value))
            {
                PrependItem(&aKEYS, value, NULL);
            }
        }
    }

    DeleteDBCursor(cursor);
    CloseDB(db);


    /* For every kKEY->IP1 entry there should be a aIP1->whatever entry.
     * So basically: kIPS SUBSET OF aIPS. */
    Item *kip = kIPS;
    while (kip != NULL)
    {
        if (!IsItemIn(aIPS, kip->name))
        {
            Log(LOG_LEVEL_WARNING,
                "lastseen db inconsistency, found kKEY -> '%s' entry, "
                "but no 'a%s' -> any key entry exists!",
                kip->name, kip->name);

            result = false;
        }

        kip = kip->next;
    }

    /* For every aIP->KEY1 entry there should be a kKEY1->whatever entry.
     * So basically: aKEYS SUBSET OF kKEYS. */
    Item *akey = aKEYS;
    while (akey != NULL)
    {
        if (!IsItemIn(kKEYS, akey->name))
        {
            Log(LOG_LEVEL_WARNING,
                "lastseen db inconsistency, found aIP -> '%s' entry, "
                "but no 'k%s' -> any ip entry exists!",
                akey->name, akey->name);

            result = false;
        }

        akey = akey->next;
    }

    DeleteItemList(qKEYS);
    DeleteItemList(aKEYS);
    DeleteItemList(kKEYS);
    DeleteItemList(aIPS);
    DeleteItemList(kIPS);

    return result;
}

/**
 * @brief removes all traces of host 'ip' from lastseen DB
 *
 * @param[in]     ip : either in (SHA/MD5 format)
 * @param[in,out] digest: return corresponding digest of input host.
 *                        If NULL, return nothing
 * @param[in] digest_size: size of digest parameter
 * @retval true if entry was deleted, false otherwise
 */
bool DeleteIpFromLastSeen(const char *ip, char *digest, size_t digest_size)
{
    DBHandle *db;
    bool res = false;

    if (!OpenDB(&db, dbid_lastseen))
    {
        char *db_path = DBIdToPath(dbid_lastseen);
        Log(LOG_LEVEL_ERR, "Unable to open lastseen database '%s'", db_path);
        free(db_path);
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
                strlcpy(digest, bufkey + 1, digest_size);
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
 * @param[in] ip_size : length of ip parameter
 * @retval true if entry was deleted, false otherwise
 */
bool DeleteDigestFromLastSeen(const char *key, char *ip, size_t ip_size)
{
    DBHandle *db;
    bool res = false;

    if (!OpenDB(&db, dbid_lastseen))
    {
        char *db_path = DBIdToPath(dbid_lastseen);
        Log(LOG_LEVEL_ERR, "Unable to open lastseen database '%s'", db_path);
        free(db_path);
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
                strlcpy(ip, host, ip_size);
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
                           char *equivalent, size_t equivalent_size)
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
        if (DeleteDigestFromLastSeen(input, equivalent, equivalent_size) == false)
        {
            Log(LOG_LEVEL_ERR, "Unable to remove digest from lastseen database.");
            return 252;
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Removing host '%s' from lastseen database\n", input);
        if (DeleteIpFromLastSeen(input, equivalent, equivalent_size) == false)
        {
            Log(LOG_LEVEL_ERR, "Unable to remove host from lastseen database.");
            return 253;
        }
    }

    Log(LOG_LEVEL_INFO, "Removed corresponding entries from lastseen database.");

    return 0;
}

