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

#ifndef CFENGINE_VERIFY_FILES_UTILS_H
#define CFENGINE_VERIFY_FILES_UTILS_H

#include "cf3.defs.h"

extern Item *VSETUIDLIST;
extern Rlist *SINGLE_COPY_LIST;

void SetFileAutoDefineList(Rlist *auto_define_list);

int VerifyFileLeaf(char *path, struct stat *sb, Attributes attr, Promise *pp);
int DepthSearch(char *name, struct stat *sb, int rlevel, Attributes attr, Promise *pp);
int CfCreateFile(char *file, Promise *pp, Attributes attr);
void SetSearchDevice(struct stat *sb, Promise *pp);

int ScheduleCopyOperation(char *destination, Attributes attr, Promise *pp);
int ScheduleLinkChildrenOperation(char *destination, char *source, int rec, Attributes attr, Promise *pp);
int ScheduleLinkOperation(char *destination, char *source, Attributes attr, Promise *pp);
int ScheduleEditOperation(char *filename, Attributes attr, Promise *pp, const ReportContext *report_context);

int CopyRegularFile(char *source, char *dest, struct stat sstat, struct stat dstat, Attributes attr, Promise *pp);

#endif
