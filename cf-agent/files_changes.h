/*
  Copyright 2024 Northern.tech AS

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

#ifndef CFENGINE_FILES_CHANGES_H
#define CFENGINE_FILES_CHANGES_H

#include <promises.h>

/**
 * Returns true if *every* one of the given change categories is silenced by
 * the 'silence' attribute of the promise's changes body. An empty category
 * set is never silenced.
 */
bool IsChangeSilenced(const Attributes *attr, FileChangeSilence categories);

/**
 * Like RecordChange(), but omits the log message when the change belongs
 * entirely to silenced categories. The promise outcome classes are set either
 * way, so silencing never hides a change from the policy itself.
 */
void RecordFileChange(EvalContext *ctx, const Promise *pp, const Attributes *attr,
                      FileChangeSilence categories, const char *fmt, ...)
    FUNC_ATTR_PRINTF(5, 6);

typedef enum
{
    FILE_STATE_NEW,
    FILE_STATE_REMOVED,
    FILE_STATE_CONTENT_CHANGED,
    FILE_STATE_STATS_CHANGED
} FileState;

bool FileChangesLogChange(const char *file, FileState status, char *msg, const Promise *pp);
bool FileChangesCheckAndUpdateHash(EvalContext *ctx,
                                   const char *filename,
                                   unsigned char digest[EVP_MAX_MD_SIZE + 1],
                                   HashMethod type,
                                   const Attributes *attr,
                                   const Promise *pp,
                                   PromiseResult *result);
bool FileChangesGetDirectoryList(const char *path, Seq *files);
bool FileChangesLogNewFile(const char *path, const Promise *pp, bool silent);
void FileChangesCheckAndUpdateDirectory(EvalContext *ctx, const Attributes *attr,
                                        const char *name, const Seq *file_set, const Seq *db_file_set,
                                        bool update, const Promise *pp, PromiseResult *result);
void FileChangesCheckAndUpdateStats(EvalContext *ctx,
                                    const char *file,
                                    const struct stat *sb,
                                    bool update,
                                    const Attributes *attr,
                                    const Promise *pp,
                                    PromiseResult *result);

#endif
