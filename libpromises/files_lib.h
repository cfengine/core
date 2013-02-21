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

#ifndef CFENGINE_FILES_LIB_H
#define CFENGINE_FILES_LIB_H

#include "cf3.defs.h"

bool FileCanOpen(const char *path, const char *modes);
void PurgeItemList(Item **list, char *name);
ssize_t FileRead(const char *filename, char *buffer, size_t bufsize);
ssize_t FileReadMax(char **output, char *filename, size_t size_max);
bool FileWriteOver(char *filename, char *contents);

int LoadFileAsItemList(Item **liststart, const char *file, Attributes a, const Promise *pp);

int MakeParentDirectory(char *parentandchild, int force);
int MakeParentDirectory2(char *parentandchild, int force, bool enforce_promise);

int FileSanityChecks(char *path, Attributes a, Promise *pp);

int LoadFileAsItemList(Item **liststart, const char *file, Attributes a, const Promise *pp);

void RotateFiles(char *name, int number);
void CreateEmptyFile(char *name);

void LogHashChange(char *file, FileState status, char *msg, Promise *pp);

#endif
