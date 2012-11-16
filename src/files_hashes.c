
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

#include "files_hashes.h"

#include "dbm_api.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "cfstream.h"
#include "client_code.h"

static int ReadHash(CF_DB *dbp, enum cfhashes type, char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1]);
static int WriteHash(CF_DB *dbp, enum cfhashes type, char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1]);
static char *NewIndexKey(char type, char *name, int *size);
static void DeleteIndexKey(char *key);
static void DeleteHashValue(ChecksumValue *value);
static int FileHashSize(enum cfhashes id);

/*******************************************************************/

int RefHash(char *name)         // This function wants HASHTABLESIZE to be prime
{
    int i, slot = 0;

    for (i = 0; name[i] != '\0'; i++)
    {
        slot = (CF_MACROALPHABET * slot + name[i]) % CF_HASHTABLESIZE;
    }

    return slot;
}

/*******************************************************************/

int ElfHash(char *key)
{
    unsigned char *p = key;
    int len = strlen(key);
    unsigned h = 0, g;
    unsigned int hashtablesize = CF_HASHTABLESIZE;
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

    return (h & (hashtablesize - 1));
}

/*******************************************************************/

int OatHash(const char *key)
{
    unsigned int hashtablesize = CF_HASHTABLESIZE;
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

    return (h & (hashtablesize - 1));
}

/*****************************************************************************/

int FileHashChanged(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], int warnlevel, enum cfhashes type,
                    Attributes attr, Promise *pp)
/* Returns false if filename never seen before, and adds a checksum
   to the database. Returns true if hashes do not match and also potentially
   updates database to the new value */
{
    int i, size = 21;
    unsigned char dbdigest[EVP_MAX_MD_SIZE + 1];
    CF_DB *dbp;

    CfDebug("HashChanged: key %s (type=%d) with data %s\n", filename, type, HashPrint(type, digest));

    size = FileHashSize(type);

    if (!OpenDB(&dbp, dbid_checksums))
    {
        cfPS(cf_error, CF_FAIL, "", pp, attr, "Unable to open the hash database!");
        return false;
    }

    if (ReadHash(dbp, type, filename, dbdigest))
    {
        for (i = 0; i < size; i++)
        {
            if (digest[i] != dbdigest[i])
            {
                CfDebug("Found cryptohash for %s in database but it didn't match\n", filename);

                if (EXCLAIM)
                {
                    CfOut(warnlevel, "", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                }

                CfOut(warnlevel, "", "ALERT: Hash (%s) for %s changed!", FileHashName(type), filename);

                if (pp->ref)
                {
                    CfOut(warnlevel, "", "Preceding promise: %s", pp->ref);
                }

                if (EXCLAIM)
                {
                    CfOut(warnlevel, "", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                }

                if (attr.change.update)
                {
                    cfPS(warnlevel, CF_CHG, "", pp, attr, " -> Updating hash for %s to %s", filename,
                         HashPrint(type, digest));

                    DeleteHash(dbp, type, filename);
                    WriteHash(dbp, type, filename, digest);
                }
                else
                {
                    cfPS(warnlevel, CF_FAIL, "", pp, attr, "!! Hash for file \"%s\" changed", filename);
                }

                CloseDB(dbp);
                return true;
            }
        }

        cfPS(cf_verbose, CF_NOP, "", pp, attr, " -> File hash for %s is correct", filename);
        CloseDB(dbp);
        return false;
    }
    else
    {
        /* Key was not found, so install it */
        cfPS(warnlevel, CF_CHG, "", pp, attr, " !! File %s was not in %s database - new file found", filename,
             FileHashName(type));
        CfDebug("Storing checksum for %s in database %s\n", filename, HashPrint(type, digest));
        WriteHash(dbp, type, filename, digest);

        LogHashChange(filename, cf_file_new, "New file found", pp);

        CloseDB(dbp);
        return false;
    }
}

/*******************************************************************/

int CompareFileHashes(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp)
{
    static unsigned char digest1[EVP_MAX_MD_SIZE + 1], digest2[EVP_MAX_MD_SIZE + 1];
    int i;

    CfDebug("CompareFileHashes(%s,%s)\n", file1, file2);

    if (sstat->st_size != dstat->st_size)
    {
        CfDebug("File sizes differ, no need to compute checksum\n");
        return true;
    }

    if ((attr.copy.servers == NULL) || (strcmp(attr.copy.servers->item, "localhost") == 0))
    {
        HashFile(file1, digest1, CF_DEFAULT_DIGEST);
        HashFile(file2, digest2, CF_DEFAULT_DIGEST);

        for (i = 0; i < EVP_MAX_MD_SIZE; i++)
        {
            if (digest1[i] != digest2[i])
            {
                return true;
            }
        }

        CfDebug("Files were identical\n");
        return false;           /* only if files are identical */
    }
    else
    {
        return CompareHashNet(file1, file2, attr, pp);  /* client.c */
    }
}

/*******************************************************************/

int CompareBinaryFiles(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp)
{
    int fd1, fd2, bytes1, bytes2;
    char buff1[BUFSIZ], buff2[BUFSIZ];

    CfDebug("CompareBinarySums(%s,%s)\n", file1, file2);

    if (sstat->st_size != dstat->st_size)
    {
        CfDebug("File sizes differ, no need to compute checksum\n");
        return true;
    }

    if ((attr.copy.servers == NULL) || (strcmp(attr.copy.servers->item, "localhost") == 0))
    {
        fd1 = open(file1, O_RDONLY | O_BINARY, 0400);
        fd2 = open(file2, O_RDONLY | O_BINARY, 0400);

        do
        {
            bytes1 = read(fd1, buff1, BUFSIZ);
            bytes2 = read(fd2, buff2, BUFSIZ);

            if ((bytes1 != bytes2) || (memcmp(buff1, buff2, bytes1) != 0))
            {
                CfOut(cf_verbose, "", "Binary Comparison mismatch...\n");
                close(fd2);
                close(fd1);
                return true;
            }
        }
        while (bytes1 > 0);

        close(fd2);
        close(fd1);

        return false;           /* only if files are identical */
    }
    else
    {
        CfDebug("Using network checksum instead\n");
        return CompareHashNet(file1, file2, attr, pp);  /* client.c */
    }
}

/*******************************************************************/

void HashFile(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type)
{
    FILE *file;
    EVP_MD_CTX context;
    int len, md_len;
    unsigned char buffer[1024];
    const EVP_MD *md = NULL;

    CfDebug("HashFile(%d,%s)\n", type, filename);

    if ((file = fopen(filename, "rb")) == NULL)
    {
        CfOut(cf_inform, "fopen", "%s can't be opened\n", filename);
    }
    else
    {
        md = EVP_get_digestbyname(FileHashName(type));

        EVP_DigestInit(&context, md);

        while ((len = fread(buffer, 1, 1024, file)))
        {
            EVP_DigestUpdate(&context, buffer, len);
        }

        EVP_DigestFinal(&context, digest, &md_len);

        /* Digest length stored in md_len */
        fclose(file);
    }
}

/*******************************************************************/

void HashString(const char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type)
{
    EVP_MD_CTX context;
    const EVP_MD *md = NULL;
    int md_len;

    CfDebug("HashString(%c)\n", type);

    switch (type)
    {
    case cf_crypt:
        CfOut(cf_error, "", "The crypt support is not presently implemented, please use another algorithm instead");
        memset(digest, 0, EVP_MAX_MD_SIZE + 1);
        break;

    default:
        md = EVP_get_digestbyname(FileHashName(type));

        if (md == NULL)
        {
            CfOut(cf_inform, "", " !! Digest type %s not supported by OpenSSL library", CF_DIGEST_TYPES[type][0]);
        }

        EVP_DigestInit(&context, md);
        EVP_DigestUpdate(&context, (unsigned char *) buffer, (size_t) len);
        EVP_DigestFinal(&context, digest, &md_len);
        break;
    }
}

/*******************************************************************/

void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type)
{
    EVP_MD_CTX context;
    const EVP_MD *md = NULL;
    int md_len, i, buf_len, actlen;
    unsigned char *buffer;

    CfDebug("HashPubKey(%d)\n", type);

    if (key->n)
    {
        buf_len = (size_t) BN_num_bytes(key->n);
    }
    else
    {
        buf_len = 0;
    }

    if (key->e)
    {
        if (buf_len < (i = (size_t) BN_num_bytes(key->e)))
        {
            buf_len = i;
        }
    }

    buffer = xmalloc(buf_len + 10);

    switch (type)
    {
    case cf_crypt:
        CfOut(cf_error, "", "The crypt support is not presently implemented, please use sha256 instead");
        break;

    default:
        md = EVP_get_digestbyname(FileHashName(type));

        if (md == NULL)
        {
            CfOut(cf_inform, "", " !! Digest type %s not supported by OpenSSL library", CF_DIGEST_TYPES[type][0]);
        }

        EVP_DigestInit(&context, md);

        actlen = BN_bn2bin(key->n, buffer);
        EVP_DigestUpdate(&context, buffer, actlen);
        actlen = BN_bn2bin(key->e, buffer);
        EVP_DigestUpdate(&context, buffer, actlen);
        EVP_DigestFinal(&context, digest, &md_len);
        break;
    }

    free(buffer);
}

/*******************************************************************/

int HashesMatch(unsigned char digest1[EVP_MAX_MD_SIZE + 1], unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                enum cfhashes type)
{
    int i, size = EVP_MAX_MD_SIZE;

    size = FileHashSize(type);

    CfDebug("1. CHECKING DIGEST type %d - size %d (%s)\n", type, size, HashPrint(type, digest1));
    CfDebug("2. CHECKING DIGEST type %d - size %d (%s)\n", type, size, HashPrint(type, digest2));

    for (i = 0; i < size; i++)
    {
        if (digest1[i] != digest2[i])
        {
            return false;
        }
    }

    return true;
}

/*********************************************************************/

char *HashPrint(enum cfhashes type, unsigned char digest[EVP_MAX_MD_SIZE + 1])
/** 
 * WARNING: Not thread-safe (returns pointer to global memory).
 * Use HashPrintSafe for a thread-safe equivalent
 */
{
    static char buffer[EVP_MAX_MD_SIZE * 4];

    HashPrintSafe(type, digest, buffer);

    return buffer;
}

/*********************************************************************/

char *HashPrintSafe(enum cfhashes type, unsigned char digest[EVP_MAX_MD_SIZE + 1], char buffer[EVP_MAX_MD_SIZE * 4])
/**
 * Thread safe. Note the buffer size.
 */
{
    unsigned int i;

    switch (type)
    {
    case cf_md5:
        sprintf(buffer, "MD5=  ");
        break;
    default:
        sprintf(buffer, "SHA=  ");
        break;
    }

    for (i = 0; i < CF_DIGEST_SIZES[type]; i++)
    {
        sprintf((char *) (buffer + 4 + 2 * i), "%02x", digest[i]);
    }

    return buffer;
}


char *SkipHashType(char *hash)
{
    char *str = hash;

    if(BEGINSWITH(hash, "MD5=") || BEGINSWITH(hash, "SHA="))
    {
        str = hash + 4;
    }

    return str;
}

/***************************************************************/

void PurgeHashes(char *path, Attributes attr, Promise *pp)
/* Go through the database and purge records about non-existent files */
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    struct stat statbuf;
    int ksize, vsize;
    char *key;
    void *value;

    if (!OpenDB(&dbp,dbid_checksums))
    {
        return;
    }

    if (path)
    {
        if (cfstat(path, &statbuf) == -1)
        {
            DeleteDB(dbp, path);
        }
        CloseDB(dbp);
        return;
    }

/* Acquire a cursor for the database. */

    if (!NewDBCursor(dbp, &dbcp))
    {
        CfOut(cf_inform, "", " !! Unable to scan hash database");
        CloseDB(dbp);
        return;
    }

    /* Walk through the database and print out the key/data pairs. */

    while (NextDB(dbp, dbcp, &key, &ksize, &value, &vsize))
    {
        char *obj = (char *) key + CF_INDEX_OFFSET;

        if (cfstat(obj, &statbuf) == -1)
        {
            if (attr.change.update)
            {
                DBCursorDeleteEntry(dbcp);
            }
            else
            {
                cfPS(cf_error, CF_WARN, "", pp, attr, "ALERT: File %s no longer exists!", obj);
            }

            LogHashChange(obj, cf_file_removed, "File removed", pp);
        }

        memset(&key, 0, sizeof(key));
        memset(&value, 0, sizeof(value));
    }

    DeleteDBCursor(dbp, dbcp);
    CloseDB(dbp);
}

/*****************************************************************************/

static int ReadHash(CF_DB *dbp, enum cfhashes type, char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    char *key;
    int size;
    ChecksumValue chk_val;

    key = NewIndexKey(type, name, &size);

    if (ReadComplexKeyDB(dbp, key, size, (void *) &chk_val, sizeof(ChecksumValue)))
    {
        memset(digest, 0, EVP_MAX_MD_SIZE + 1);
        memcpy(digest, chk_val.mess_digest, EVP_MAX_MD_SIZE + 1);
        DeleteIndexKey(key);
        return true;
    }
    else
    {
        DeleteIndexKey(key);
        return false;
    }
}

/*****************************************************************************/

static int WriteHash(CF_DB *dbp, enum cfhashes type, char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    char *key;
    ChecksumValue *value;
    int ret, keysize;

    key = NewIndexKey(type, name, &keysize);
    value = NewHashValue(digest);
    ret = WriteComplexKeyDB(dbp, key, keysize, value, sizeof(ChecksumValue));
    DeleteIndexKey(key);
    DeleteHashValue(value);
    return ret;
}

/*****************************************************************************/

void DeleteHash(CF_DB *dbp, enum cfhashes type, char *name)
{
    int size;
    char *key;

    key = NewIndexKey(type, name, &size);
    DeleteComplexKeyDB(dbp, key, size);
    DeleteIndexKey(key);
}

/*****************************************************************************/
/* level                                                                     */
/*****************************************************************************/

static char *NewIndexKey(char type, char *name, int *size)
{
    char *chk_key;

// Filename plus index_str in one block + \0

    *size = strlen(name) + CF_INDEX_OFFSET + 1;

    chk_key = xcalloc(1, *size);

// Data start after offset for index

    strncpy(chk_key, FileHashName(type), CF_INDEX_FIELD_LEN);
    strncpy(chk_key + CF_INDEX_OFFSET, name, strlen(name));
    return chk_key;
}

/*****************************************************************************/

static void DeleteIndexKey(char *key)
{
    free(key);
}

/*****************************************************************************/

ChecksumValue *NewHashValue(unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    ChecksumValue *chk_val;

    chk_val = xcalloc(1, sizeof(ChecksumValue));

    memcpy(chk_val->mess_digest, digest, EVP_MAX_MD_SIZE + 1);

/* memcpy(chk_val->attr_digest,attr,EVP_MAX_MD_SIZE+1); depricated */

    return chk_val;
}

/*****************************************************************************/

static void DeleteHashValue(ChecksumValue *chk_val)
{
    free((char *) chk_val);
}

/*********************************************************************/

const char *FileHashName(enum cfhashes id)
{
    return CF_DIGEST_TYPES[id][0];
}

/***************************************************************************/

static int FileHashSize(enum cfhashes id)
{
    return CF_DIGEST_SIZES[id];
}
