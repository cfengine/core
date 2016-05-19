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

#ifndef CFENGINE_FILES_NAMES_H
#define CFENGINE_FILES_NAMES_H

#include <cf3.defs.h>

typedef enum
{
    FILE_PATH_TYPE_ABSOLUTE, // /foo.cf
    FILE_PATH_TYPE_RELATIVE, // ./../foo.cf
    FILE_PATH_TYPE_NON_ANCHORED, // foo.cf
} FilePathType;

FilePathType FilePathGetType(const char *file_path);

int IsNewerFileTree(const char *dir, time_t reftime);
int CompareCSVName(const char *s1, const char *s2);
int IsDir(const char *path);
char *JoinSuffix(char *path, size_t path_size, const char *leaf);
int IsAbsPath(const char *path);
void AddSlash(char *str);
char *GetParentDirectoryCopy(const char *path);
void DeleteSlash(char *str);
void DeleteRedundantSlashes(char *str);
const char *FirstFileSeparator(const char *str);
const char *LastFileSeparator(const char *str);
bool ChopLastNode(char *str);
char *CanonifyName(const char *str);
void TransformNameInPlace(char *s, char from, char to);
char *CanonifyChar(const char *str, char ch);
const char *ReadLastNode(const char *str);
bool CompressPath(char *dest, size_t dest_size, const char *src);
bool IsFileOutsideDefaultRepository(const char *f);
int RootDirLength(const char *f);
const char *GetSoftwareCacheFilename(char *buffer);
const char *GetSoftwarePatchesFilename(char *buffer);

/**
 * Detect whether package manager starts with an env command instead of package manager,
 * and if so, return the real package manager.
 */
const char *RealPackageManager(const char *manager);

#endif
