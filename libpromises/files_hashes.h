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

#ifndef CFENGINE_FILES_HASHES_H
#define CFENGINE_FILES_HASHES_H

#include "cf3.defs.h"

int FileHashChanged(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], int warnlevel, HashMethod type,
                    Attributes attr, Promise *pp);
void PurgeHashes(char *file, Attributes attr, Promise *pp);
int CompareFileHashes(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp);
int CompareBinaryFiles(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp);
void HashFile(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);
void HashString(const char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);
int HashesMatch(unsigned char digest1[EVP_MAX_MD_SIZE + 1], unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                HashMethod type);
char *HashPrint(HashMethod type, unsigned char digest[EVP_MAX_MD_SIZE + 1]);
char *HashPrintSafe(HashMethod type, unsigned char digest[EVP_MAX_MD_SIZE + 1], char buffer[EVP_MAX_MD_SIZE * 4]);
char *SkipHashType(char *hash);
const char *FileHashName(HashMethod id);
void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);
HashMethod HashMethodFromString(char *typestr);

#endif
