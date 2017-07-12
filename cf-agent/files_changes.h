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

#ifndef CFENGINE_FILES_CHANGES_H
#define CFENGINE_FILES_CHANGES_H

#include <promises.h>

typedef enum
{
    FILE_STATE_NEW,
    FILE_STATE_REMOVED,
    FILE_STATE_CONTENT_CHANGED,
    FILE_STATE_STATS_CHANGED
} FileState;

void FileChangesLogChange(const char *file, FileState status, char *msg, const Promise *pp);
bool FileChangesCheckAndUpdateHash(EvalContext *ctx,
                                   const char *filename,
                                   unsigned char digest[EVP_MAX_MD_SIZE + 1],
                                   HashMethod type,
                                   Attributes attr,
                                   const Promise *pp,
                                   PromiseResult *result);
bool FileChangesGetDirectoryList(const char *path, Seq *files);
void FileChangesLogNewFile(const char *path, const Promise *pp);
void FileChangesCheckAndUpdateDirectory(const char *name, const Seq *file_set, const Seq *db_file_set,
                                        bool update, const Promise *pp, PromiseResult *result);
void FileChangesCheckAndUpdateStats(const char *file, struct stat *sb, bool update, const Promise *pp);

#endif
