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

#ifndef CFENGINE_FILES_LIB_H
#define CFENGINE_FILES_LIB_H

#include <cf3.defs.h>
#include <file_lib.h>

void PurgeItemList(Item **list, char *name);
bool FileWriteOver(char *filename, char *contents);

int LoadFileAsItemList(Item **liststart, const char *file, EditDefaults edits);

bool MakeParentDirectory(const char *parentandchild, bool force);
int MakeParentDirectory2(char *parentandchild, int force, bool enforce_promise);

void RotateFiles(char *name, int number);
void CreateEmptyFile(char *name);


/**
 * @brief This is a somewhat simpler version of nftw that support user_data.
 *        Callback function must return 0 to indicate success, -1 for failure.
 * @param path Path to traverse
 * @param user_data User data carry
 * @return True if successful
 */
bool TraverseDirectoryTree(const char *path,
                           int (*callback)(const char *path, const struct stat *sb, void *user_data),
                           void *user_data);

bool HashDirectoryTree(const char *path,
                       const char **extensions_filter,
                       EVP_MD_CTX *crypto_context);

#include <file_lib.h>

#endif
