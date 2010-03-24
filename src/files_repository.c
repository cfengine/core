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

/*****************************************************************************/
/*                                                                           */
/* File: files_repository.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/

int ArchiveToRepository(char *file,struct Attributes attr,struct Promise *pp)

 /* Returns true if the file was backup up and false if not */

{ char destination[CF_BUFSIZE];
  char localrepository[CF_BUFSIZE]; 
  char node[CF_BUFSIZE];
  struct stat sb, dsb;
  char *sp;
  short imagecopy;

if (attr.repository == NULL && VREPOSITORY == NULL)
   {
   return false;
   }

if (attr.repository != NULL)
   {
   strncpy(localrepository,attr.repository,CF_BUFSIZE);
   }
else if (VREPOSITORY != NULL)
   {
   strncpy(localrepository,VREPOSITORY,CF_BUFSIZE);
   }

if (attr.copy.backup == cfa_nobackup)
   {
   return true;
   }

if (IsItemIn(VREPOSLIST,file))
   {
   CfOut(cf_inform,"","The file %s has already been moved to the repository once. Multiple update will cause loss of backup.",file);
   return true;
   }

ThreadLock(cft_getaddr);
PrependItemList(&VREPOSLIST,file);
ThreadUnlock(cft_getaddr);

Debug("Repository(%s)\n",file);

strcpy (node,file);

destination[0] = '\0';

for (sp = node; *sp != '\0'; sp++)
   {
   if (*sp == FILE_SEPARATOR)
      {
      *sp = REPOSCHAR;
      }
   }

strncpy(destination,localrepository,CF_BUFSIZE-2);

if (!JoinPath(destination,node))
   {
   CfOut(cf_error,"","Buffer overflow for long filename\n");
   return false;
   }

if (!MakeParentDirectory(destination,attr.move_obstructions))
   {
   }

if (cfstat(file,&sb) == -1)
   {
   Debug("File %s promised to archive to the repository but it disappeared!\n",file);
   return true;
   }

cfstat(destination,&dsb);

attr.copy.servers = NULL;
attr.copy.backup = cfa_repos_store; // cfa_nobackup;
attr.copy.stealth = false;
attr.copy.verify = false;
attr.copy.preserve = false;

CheckForFileHoles(&sb,pp);

if (CopyRegularFileDisk(file,destination,attr,pp))
   {
   CfOut(cf_inform,"","Moved %s to repository location %s\n",file,destination);
   return true;
   }
else
   {
   CfOut(cf_inform,"","Failed to move %s to repository location %s\n",file,destination);
   return false;
   }
}

