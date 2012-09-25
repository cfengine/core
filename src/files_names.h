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

#ifndef CFENGINE_FILES_NAMES_H
#define CFENGINE_FILES_NAMES_H

#include "platform.h"

int IsNewerFileTree(char *dir, time_t reftime);
int DeEscapeQuotedString(const char *in, char *out);
int CompareCSVName(const char *s1, const char *s2);
int IsDir(char *path);
int EmptyString(char *s);
char *JoinPath(char *path, const char *leaf);
char *JoinSuffix(char *path, char *leaf);
int JoinMargin(char *path, const char *leaf, char **nextFree, int bufsize, int margin);
int StartJoin(char *path, char *leaf, int bufsize);
int Join(char *path, const char *leaf, int bufsize);
int JoinSilent(char *path, const char *leaf, int bufsize);
int EndJoin(char *path, char *leaf, int bufsize);
int IsAbsPath(char *path);
void AddSlash(char *str);
char *GetParentDirectoryCopy(const char *path);
void DeleteSlash(char *str);
const char *FirstFileSeparator(const char *str);
const char *LastFileSeparator(const char *str);
int ChopLastNode(char *str);
char *CanonifyName(const char *str);
void CanonifyNameInPlace(char *str);
void TransformNameInPlace(char *s, char from, char to);
char *CanonifyChar(const char *str, char ch);
const char *ReadLastNode(const char *str);
int CompressPath(char *dest, char *src);
void Chop(char *str);
void StripTrailingNewline(char *str);
char *ScanPastChars(char *scanpast, char *input);
int IsAbsoluteFileName(const char *f);
bool IsFileOutsideDefaultRepository(const char *f);
int RootDirLength(char *f);
const char *GetSoftwareCacheFilename(char *buffer);
int StringInArray(char **array, char *string);
#endif
