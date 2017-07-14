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
#include <tokyo_check.h>

#ifdef HAVE_CONFIG_H
#include  <config.h>
#endif

#include <array_map_priv.h>
#include <hash_map_priv.h>
#include <map.h>
#include <string_lib.h>
#include <logging.h>
#include <cf3.defs.h>

#ifdef TCDB

/*
 * The idea behind the following code comes from : copiousfreetime@github
 */

/*
 * Fields we will need from a TC record
 */
typedef struct TokyoCabinetRecord
{
    uint64_t offset;
    uint64_t length;

    uint64_t left;
    uint64_t right;

    uint32_t key_size;
    uint32_t rec_size;
    uint16_t pad_size;

    uint8_t magic;
    uint8_t hash;
} TokyoCabinetRecord;

/* meta information from the Hash Database
 * used to coordinate the other operations
 */
typedef struct DBMeta
{
    uint64_t bucket_count;      /* Number of hash buckets                */
    uint64_t bucket_offset;     /* Start of the bucket list              */

    uint64_t record_count;      /* Number of records                     */
    uint64_t record_offset;     /* First record  offset in file          */

    short alignment_pow;        /* power of 2 for calculating offsets */
    short bytes_per;            /* 4 or 8 */
    char dbpath[PATH_MAX + 1];  /* full pathname to the database file */

    int fd;

    StringMap *offset_map;
    StringMap *record_map;
} DBMeta;

static DBMeta *DBMetaNewDirect(const char *dbfilename)
{
    char hbuf[256];
    DBMeta *dbmeta;

    dbmeta = (DBMeta *) xcalloc(1, sizeof(DBMeta));

    realpath(dbfilename, dbmeta->dbpath);
    if (-1 == (dbmeta->fd = open(dbmeta->dbpath, O_RDONLY)))
    {
        Log(LOG_LEVEL_ERR, "Failure opening file '%s'. (open: %s)", dbmeta->dbpath,
                GetErrorStr());
        if (dbmeta)
        {
            free(dbmeta);
        }
        return NULL;
    }

    if (256 != read(dbmeta->fd, hbuf, 256))
    {
        Log(LOG_LEVEL_ERR, "Failure reading from database '%s'. (read: %s)",
                dbmeta->dbpath, GetErrorStr());
        close(dbmeta->fd);
        if (dbmeta)
        {
            free(dbmeta);
        }
        return NULL;
    }

    memcpy(&(dbmeta->bucket_count), hbuf + 40, sizeof(uint64_t));
    dbmeta->bucket_offset = 256;
    uint8_t opts;
    memcpy(&opts, hbuf + 36, sizeof(uint8_t));
    dbmeta->bytes_per =
        (opts & (1 << 0)) ? sizeof(uint64_t) : sizeof(uint32_t);

    memcpy(&(dbmeta->record_count), hbuf + 48, sizeof(uint64_t));
    memcpy(&(dbmeta->record_offset), hbuf + 64, sizeof(uint64_t));
    memcpy(&(dbmeta->alignment_pow), hbuf + 34, sizeof(uint8_t));
    dbmeta->offset_map = StringMapNew();
    dbmeta->record_map = StringMapNew();

    Log(LOG_LEVEL_VERBOSE, "Database            : %s", dbmeta->dbpath);
    Log(LOG_LEVEL_VERBOSE, "  number of buckets : %" PRIu64,
            dbmeta->bucket_count);
    Log(LOG_LEVEL_VERBOSE, "  offset of buckets : %" PRIu64,
            dbmeta->bucket_offset);
    Log(LOG_LEVEL_VERBOSE, "  bytes per pointer : %hd",
            dbmeta->bytes_per);
    Log(LOG_LEVEL_VERBOSE, "  alignment power   : %hd",
            dbmeta->alignment_pow);
    Log(LOG_LEVEL_VERBOSE, "  number of records : %" PRIu64,
            dbmeta->record_count);
    Log(LOG_LEVEL_VERBOSE, "  offset of records : %" PRIu64,
            dbmeta->record_offset);

    return dbmeta;
}

static void DBMetaFree(DBMeta * dbmeta)
{
    StringMapDestroy(dbmeta->offset_map);
    StringMapDestroy(dbmeta->record_map);

    close(dbmeta->fd);

    if (dbmeta)
    {
        free(dbmeta);
    }
}

static int AddOffsetToMapUnlessExists(StringMap ** tree, uint64_t offset,
                                      int64_t bucket_index)
{
    char *tmp;
    xasprintf(&tmp, "%" PRIu64, offset);
    char *val;
    if (StringMapHasKey(*tree, tmp) == false)
    {
        xasprintf(&val, "%" PRIu64, bucket_index);
        StringMapInsert(*tree, tmp, val);
    }
    else
    {
        Log(LOG_LEVEL_ERR,
            "Duplicate offset for value %" PRIu64 " at index %" PRId64 ", other value %" PRIu64 ", other index '%s'",
             offset, bucket_index,
             offset, (char *) StringMapGet(*tree, tmp));
        free(tmp);
    }
    return 0;
}

static int DBMetaPopulateOffsetMap(DBMeta * dbmeta)
{
    uint64_t i;

    if (lseek(dbmeta->fd, dbmeta->bucket_offset, SEEK_SET) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Error traversing bucket section to find record offsets '%s'",
             strerror(errno));
        return 1;
    }

    for (i = 0; i < dbmeta->bucket_count; i++)
    {
        uint64_t offset = 0LL;
        int b = read(dbmeta->fd, &offset, dbmeta->bytes_per);

        if (b != dbmeta->bytes_per)
        {
            Log(LOG_LEVEL_ERR, "Read the wrong number of bytes (%d)", b);
            return 2;
        }

        /* if the value is > 0 then we have a number so do something with it */
        if (offset > 0)
        {
            offset = offset << dbmeta->alignment_pow;
            if (AddOffsetToMapUnlessExists(&(dbmeta->offset_map), offset, i))
            {
                return 3;
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Found %zu buckets with offsets",
            StringMapSize(dbmeta->offset_map));
    return 0;
}

typedef enum
{                               // enumeration for magic data
    MAGIC_DATA_BLOCK = 0xc8,    // for data block
    MAGIC_FREE_BLOCK = 0xb0     // for free block
} TypeOfBlock;

static int TCReadVaryInt(int fd, uint32_t * result)
{
    uint64_t num = 0;
    unsigned int base = 1;
    unsigned int i = 0;
    int read_bytes = 0;
    char c;

    while (true)
    {
        read_bytes += read(fd, &c, 1);
        if (c >= 0)
        {
            num += (c * base);
            break;
        }
        num += (base * (c + 1) * -1);
        base <<= 7;
        i += 1;
    }

    *result = num;

    return read_bytes;
}


static bool DBMetaReadOneRecord(DBMeta * dbmeta, TokyoCabinetRecord * rec)
{
    if (lseek(dbmeta->fd, rec->offset, SEEK_SET) == -1)
    {
        Log(LOG_LEVEL_ERR, "Error traversing record section to find records : ");
    }

    while (true)
    {
        // get the location of the current read
        rec->offset = lseek(dbmeta->fd, 0, SEEK_CUR);
        if (rec->offset == (off_t) - 1)
        {
            Log(LOG_LEVEL_ERR,
                "Error traversing record section to find records");
        }

        if (1 != read(dbmeta->fd, &(rec->magic), 1))
        {
            Log(LOG_LEVEL_ERR, "Failure reading 1 byte, (read: %s)",
                    GetErrorStr());
            return false;
        }

        if (MAGIC_DATA_BLOCK == rec->magic)
        {
            Log(LOG_LEVEL_VERBOSE, "off=%" PRIu64 "[c8]", rec->offset);
            int length = 1;

            length += read(dbmeta->fd, &(rec->hash), 1);
            length += read(dbmeta->fd, &(rec->left), dbmeta->bytes_per);
            rec->left = rec->left << dbmeta->alignment_pow;

            length += read(dbmeta->fd, &(rec->right), dbmeta->bytes_per);
            rec->right = rec->right << dbmeta->alignment_pow;

            length += read(dbmeta->fd, &(rec->pad_size), 2);
            length += rec->pad_size;

            length += TCReadVaryInt(dbmeta->fd, &(rec->key_size));
            length += TCReadVaryInt(dbmeta->fd, &(rec->rec_size));

            rec->length = length + rec->key_size + rec->rec_size;
            return true;

        }
        else if (MAGIC_FREE_BLOCK == rec->magic)
        {
            Log(LOG_LEVEL_VERBOSE, "off=%" PRIu64 "[b0]", rec->offset);
            uint32_t length;
            rec->length = 1;
            rec->length += read(dbmeta->fd, &length, sizeof(length));
            rec->length += length;
            return true;

        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Read a non-magic byte (skip it)");
        }
    }
    Log(LOG_LEVEL_ERR, "Read loop reached here");
    return false;
}

static int DBMetaPopulateRecordMap(DBMeta * dbmeta)
{
    off_t offset;
    uint64_t data_blocks = 0;
    uint64_t free_blocks = 0;
    struct stat st;

    offset = dbmeta->record_offset;
    if (fstat(dbmeta->fd, &st) == -1)
    {
        Log(LOG_LEVEL_ERR, "Error getting file stats. (fstat: %s)", GetErrorStr());
        return 1;
    }

    while (offset < st.st_size)
    {

        TokyoCabinetRecord new_rec;
        memset(&new_rec, 0, sizeof(TokyoCabinetRecord));
        new_rec.offset = offset;

        // read a variable-length record
        if (!DBMetaReadOneRecord(dbmeta, &new_rec))
        {
            Log(LOG_LEVEL_ERR, "Unable to fetch a new record from DB file");
            return 2;
        }
        else
        {
            offset = new_rec.offset + new_rec.length;
        }

        // if it is a data record then:
        // for the record, its left and right do:
        // look up that record in the offset tree
        // 1) remove it if it exists
        // 2) add it to the record_tree if it doesn't

        if (MAGIC_DATA_BLOCK == new_rec.magic)
        {

            if (new_rec.offset > 0)
            {

                char *key;
                xasprintf(&key, "%" PRIu64, new_rec.offset);
                if (StringMapHasKey(dbmeta->offset_map, key) == true)
                {
                    if (key)
                    {
                        free(key);
                    }
                }
                else
                {
                    StringMapInsert(dbmeta->record_map, key, xstrdup("0"));
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "new_rec.offset cannot be <= 0 ???");
            }

            if (new_rec.left > 0)
            {
                Log(LOG_LEVEL_VERBOSE, "handle left %" PRIu64, new_rec.left);
                if (AddOffsetToMapUnlessExists
                    (&(dbmeta->offset_map), new_rec.left, -1))
                {
                    return 4;
                }
            }

            if (new_rec.right > 0)
            {
                Log(LOG_LEVEL_VERBOSE, "handle right %" PRIu64, new_rec.right);
                if (AddOffsetToMapUnlessExists
                    (&(dbmeta->offset_map), new_rec.right, -1))
                {
                    return 4;
                }
            }

            data_blocks++;
        }
        else if (MAGIC_FREE_BLOCK == new_rec.magic)
        {
            // if it is a fragment record, then skip it
            free_blocks++;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "NO record found at offset %" PRIu64,
                    new_rec.offset);
        }
    }

    // if we are not at the end of the file, output the current file offset
    // with an appropriate message and return
    Log(LOG_LEVEL_VERBOSE, "Found %" PRIu64 " data records and "
        "%" PRIu64 " free block records", data_blocks, free_blocks);

    return 0;
}

static int DBMetaGetResults(DBMeta * dbmeta)
{
    uint64_t buckets_no_record = StringMapSize(dbmeta->offset_map);
    uint64_t records_no_bucket = StringMapSize(dbmeta->record_map);
    int ret = 0;

    Log(LOG_LEVEL_VERBOSE,
        "Found %" PRIu64 " offsets listed in buckets that do not have records",
         buckets_no_record);
    Log(LOG_LEVEL_VERBOSE,
        "Found %" PRIu64 " records in data that do not have an offset pointing to them",
         records_no_bucket);

    if (buckets_no_record > 0)
    {
        ret += 1;
    }

    if (records_no_bucket > 0)
    {
        ret += 2;
    }
    return ret;
}

int CheckTokyoDBCoherence(const char *path)
{
    int ret = 0;
    DBMeta *dbmeta;

    dbmeta = DBMetaNewDirect(path);
    if (dbmeta == NULL)
    {
        return 1;
    }

    Log(LOG_LEVEL_VERBOSE, "Populating with bucket section offsets");
    ret = DBMetaPopulateOffsetMap(dbmeta);
    if (ret)
    {
        goto clean;
    }

    Log(LOG_LEVEL_VERBOSE, "Populating with record section offsets");
    ret = DBMetaPopulateRecordMap(dbmeta);
    if (ret)
    {
        goto clean;
    }

    ret = DBMetaGetResults(dbmeta);

  clean:
    if (dbmeta)
    {
        DBMetaFree(dbmeta);
    }

    return ret;
}

#else
int CheckTokyoDBCoherence(ARG_UNUSED const char *path)
{
  return 0;
}
#endif
