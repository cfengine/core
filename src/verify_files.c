/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
/* File: verify_files.c                                                      */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void *FindAndVerifyFilesPromises(struct Promise *pp)

{
PromiseBanner(pp); 
FindFilePromiserObjects(pp);

if (AM_BACKGROUND_PROCESS && !pp->done)
   {
   CfOut(cf_inform,"","Exiting backgrounded promise");
   PromiseRef(cf_inform,pp);
   exit(0);
   }

return (void *)NULL;
}

/*****************************************************************************/

void FindFilePromiserObjects(struct Promise *pp)

{ char *val = GetConstraint("pathtype",pp->conlist,CF_SCALAR);
  int literal = GetBooleanConstraint("copy_from",pp->conlist) ||
                GetBooleanConstraint("create",pp->conlist) ||
                ((val != NULL) && (strcmp(val,"literal") == 0));

/* Check if we are searching over a regular expression */

if (literal)
   {
   VerifyFilePromise(pp->promiser,pp);
   return;
   }
else // Default is to expand regex paths
   {
   LocateFilePromiserGroup(pp->promiser,pp,VerifyFilePromise);
   }
}

/*****************************************************************************/

void LocateFilePromiserGroup(char *wildpath,struct Promise *pp,void (*fnptr)(char *path, struct Promise *ptr))

{ struct Item *path,*ip,*remainder = NULL;
  char pbuffer[CF_BUFSIZE];
  struct stat statbuf;
  int count = 0,lastnode = false, expandregex = false;
  uid_t agentuid = getuid();
  int create = GetBooleanConstraint("create",pp->conlist);

Debug("LocateFilePromiserGroup(%s)\n",wildpath);

/* Do a search for promiser objects matching wildpath */

if (!IsPathRegex(wildpath))
   {
   CfOut(cf_verbose,""," -> Using literal pathtype for %s\n",wildpath);
   (*fnptr)(wildpath,pp);
   return;
   }
else
   {
   CfOut(cf_verbose,""," -> Using regex pathtype for %s (see pathtype)\n",wildpath);
   }

pbuffer[0] = '\0';
path = SplitString(wildpath,FILE_SEPARATOR);

for (ip = path; ip != NULL; ip=ip->next)
   {
   if (ip->name == NULL || strlen(ip->name) == 0)
      {
      continue;
      }

   if (ip->next == NULL)
      {
      lastnode = true;
      }

   /* No need to chdir as in recursive descent, since we know about the path here */
   
   if (IsRegex(ip->name))
      {
      remainder = ip->next;
      expandregex = true;
      break;
      }
   else
      {
      expandregex = false;
      }

   if (!JoinPath(pbuffer,ip->name))
      {
      CfOut(cf_error,"","Buffer overflow in LocateFilePromiserGroup\n");
      return;
      }

   if (stat(pbuffer,&statbuf) != -1)
      {
      if (S_ISDIR(statbuf.st_mode) && statbuf.st_uid != agentuid && statbuf.st_uid != 0)
         {
         CfOut(cf_inform,"","Directory %s in search path %s is controlled by another user - trusting its content is potentially risky (possible race)\n",pbuffer,wildpath);
         PromiseRef(cf_inform,pp);
         }
      }
   }

if (expandregex) /* Expand one regex link and hand down */
   {
   char nextbuffer[CF_BUFSIZE],regex[CF_BUFSIZE];
   struct dirent *dirp;
   DIR *dirh;
   struct Attributes dummyattr;

   memset(&dummyattr,0,sizeof(dummyattr));
 
   strncpy(regex,ip->name,CF_BUFSIZE-1);

   if ((dirh=opendir(pbuffer)) == NULL)
      {
      cfPS(cf_verbose,CF_FAIL,"opendir",pp,dummyattr,"Could not expand promise makers in %s because %s could not be read\n",pp->promiser,pbuffer);
      return;
      }
   
   count = 0;
   
   for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
      {
      if (!ConsiderFile(dirp->d_name,pbuffer,dummyattr,pp))
         {
         continue;
         }

      if (!lastnode && !S_ISDIR(statbuf.st_mode))
         {
         Debug("Skipping non-directory %s\n",dirp->d_name);
         continue;
         }

      if (FullTextMatch(regex,dirp->d_name))
         {
         Debug("Link %s matched regex %s\n",dirp->d_name,regex);
         }
      else
         {
         continue;
         }

      count++;

      strncpy(nextbuffer,pbuffer,CF_BUFSIZE-1);
      AddSlash(nextbuffer);
      strcat(nextbuffer,dirp->d_name);

      for (ip = remainder; ip != NULL; ip=ip->next)
         {
         AddSlash(nextbuffer);
         strcat(nextbuffer,ip->name);
         }
      
      /* The next level might still contain regexs, so go again as long as expansion is not nullpotent */

      if (!lastnode && (strcmp(nextbuffer,wildpath) != 0))
         {
         LocateFilePromiserGroup(nextbuffer,pp,fnptr);
         }
      else
         {
         CfOut(cf_verbose,""," -> Using expanded file base path %s\n",nextbuffer);
         (*fnptr)(nextbuffer,pp);
         }
      }
   
   closedir(dirh);
   }
else
   {
   CfOut(cf_verbose,""," -> Using file base path %s\n",pbuffer);
   (*fnptr)(pbuffer,pp);
   }

if (count == 0)
   {
   CfOut(cf_verbose,"","No promiser file objects matched as regular expression %s\n",wildpath);

   if (create)
      {
      VerifyFilePromise(pp->promiser,pp);      
      }
   }

DeleteItemList(path);
}

