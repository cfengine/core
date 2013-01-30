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

#ifndef CFENGINE_NFS_H
#define CFENGINE_NFS_H

#include "cf3.defs.h"

extern int FSTAB_EDITS;
extern Item *FSTABLIST;

#ifndef __MINGW32__
int LoadMountInfo(Rlist **list);
void DeleteMountInfo(Rlist *list);
int VerifyNotInFstab(char *name, Attributes a, Promise *pp);
int VerifyInFstab(char *name, Attributes a, Promise *pp);
int VerifyMount(char *name, Attributes a, Promise *pp);
int VerifyUnmount(char *name, Attributes a, Promise *pp);
void MountAll(void);
#endif /* !__MINGW32__ */

#endif
