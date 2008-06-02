/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: files_repository.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern pthread_mutex_t MUTEX_GETADDR;

/*********************************************************************/

int ArchiveToRepository(char *file,struct Attributes attr,struct Promise *pp)

 /* Returns true if the file was backup up and false if not */

{ char destination[CF_BUFSIZE];
  char localrepository[CF_BUFSIZE]; 
  char node[CF_BUFSIZE];
  struct stat sb, dsb;
  char *sp;
  struct Image dummy;
  short imagecopy;

if (attr.repository == NULL && VREPOSITORY == NULL)
   {
   return false;
   }

if (attr.repository != NULL)
   {
   strncpy(localrepository,attr.repository,CF_BUFSIZE);
   }
else if (attr.repository == NULL && VREPOSITORY != NULL)
   {
   strncpy(localrepository,VREPOSITORY,CF_BUFSIZE);
   }

if (attr.copy.backup == cfa_nobackup)
   {
   return true;
   }

if (IsItemIn(VREPOSLIST,file))
   {
   snprintf(OUTPUT,CF_BUFSIZE,"The file %s has already been moved to the repository once. Multiple update will cause loss of backup.",file);
   CfLog(cfinform,OUTPUT,"");
   return true;
   }

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_lock(&MUTEX_GETADDR) != 0)
   {
   CfLog(cferror,"pthread_mutex_lock failed","unlock");
   exit(1);
   }
#endif

PrependItem(&VREPOSLIST,file,NULL);

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_unlock(&MUTEX_GETADDR) != 0)
   {
   CfLog(cferror,"pthread_mutex_unlock failed","unlock");
   exit(1);
   }
#endif

Debug("Repository(%s)\n",file);

strcpy (node,file);

destination[0] = '\0';

for (sp = node; *sp != '\0'; sp++)
   {
   if (*sp == '/')
      {
      *sp = REPOSCHAR;
      }
   }

strncpy(destination,localrepository,CF_BUFSIZE-2);

if (!JoinPath(destination,node))
   {
   CfLog(cferror,"Buffer overflow for long filename\n","");
   return false;
   }

if (!MakeParentDirectory(destination,attr.move_obstructions))
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Repository (%s),testfile (%s)",localrepository,destination);
   }

if (stat(file,&sb) == -1)
   {
   Debug2("File %s promised to archive to the repository but it disappeared!\n",file);
   return true;
   }

stat(destination,&dsb);

attr.copy.servers = NULL;
attr.copy.backup = cfa_repos_store; // cfa_nobackup;
attr.copy.stealth = false;
attr.copy.verify = false;
attr.copy.preserve = false;

CheckForFileHoles(&sb,attr,pp);

if (CopyRegularFile(file,destination,sb,dsb,attr,pp))
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Moved %s to repository location %s\n",file,destination);
   CfLog(cfinform,OUTPUT,"");
   return true;
   }
else
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Failed to move %s to repository location %s\n",file,destination);
   CfLog(cfinform,OUTPUT,"");
   return false;
   }
}

