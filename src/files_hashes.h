#ifndef CFENGINE_FILES_HASHES_H
#define CFENGINE_FILES_HASHES_H

#include "cf3.defs.h"

int FileHashChanged(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], int warnlevel, enum cfhashes type,
                    Attributes attr, Promise *pp);
void PurgeHashes(char *file, Attributes attr, Promise *pp);
void DeleteHash(CF_DB *dbp, enum cfhashes type, char *name);
ChecksumValue *NewHashValue(unsigned char digest[EVP_MAX_MD_SIZE + 1]);
int CompareFileHashes(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp);
int CompareBinaryFiles(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp);
void HashFile(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type);
void HashString(const char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type);
int HashesMatch(unsigned char digest1[EVP_MAX_MD_SIZE + 1], unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                enum cfhashes type);
char *HashPrint(enum cfhashes type, unsigned char digest[EVP_MAX_MD_SIZE + 1]);
char *HashPrintSafe(enum cfhashes type, unsigned char digest[EVP_MAX_MD_SIZE + 1], char buffer[EVP_MAX_MD_SIZE * 4]);
char *SkipHashType(char *hash);
const char *FileHashName(enum cfhashes id);
void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type);

#endif
