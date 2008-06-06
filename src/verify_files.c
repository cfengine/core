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
/* File: verify_files.c                                                      */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void FindAndVerifyFilesPromises(struct Promise *pp)

{
PromiseBanner(pp);
FindFilePromiserObjects(pp);
}

/*****************************************************************************/

void FindFilePromiserObjects(struct Promise *pp)

{ char *val = GetConstraint("pathtype",pp->conlist,CF_SCALAR);

/* Check if we are searching over a regular expression */
 
if ((val != NULL) && (strcmp(val,"literal") == 0))
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

Debug("LocateFilePromiserGroup(%s)\n",wildpath);

/* Do a search for promiser objects matching wildpath */

if (!IsPathRegex(wildpath))
   {
   Verbose(" -> Using file literal base path %s\n",wildpath);
   (*fnptr)(wildpath,pp);
   return;
   }

pbuffer[0] = '\0';
path = SplitString(wildpath,FILE_SEPARATOR);

for (ip = path; ip != NULL; ip=ip->next)
   {
   if (strlen(ip->name) == 0)
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

   if (BufferOverflow(pbuffer,ip->name))
      {
      CfOut(cf_error,"","Buffer overflow in LocateFilePromiserGroup\n");
      return;
      }

   AddSlash(pbuffer);
   strcat(pbuffer,ip->name);
   
   if (stat(pbuffer,&statbuf) != -1)
      {
      if (S_ISDIR(statbuf.st_mode) && statbuf.st_uid != agentuid)
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
 
   strncpy(regex,ip->name,CF_BUFSIZE-1);

   if ((dirh=opendir(pbuffer)) == NULL)
      {
      struct Attributes dummyattr;

      memset(&dummyattr,0,sizeof(dummyattr));
      cfPS(cf_verbose,CF_FAIL,"opendir",pp,dummyattr,"Could not expand promise makers in %s because %s could not be read\n",pp->promiser,pbuffer);
      return;
      }
   
   count = 0;
   
   for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
      {
      if (!SensibleFile(dirp->d_name,pbuffer,NULL))
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
      
      /* The next level might still contain regexs, so go again*/

      if (!lastnode)
         {
         LocateFilePromiserGroup(nextbuffer,pp,fnptr);
         }
      else
         {
         Verbose(" -> Using expanded file base path %s\n",nextbuffer);
         (*fnptr)(nextbuffer,pp);
         }
      }
   
   closedir(dirh);
   }
else
   {
   Verbose(" -> Using file base path %s\n",pbuffer);
   (*fnptr)(pbuffer,pp);
   }

if (count == 0)
   {
   Verbose("No promiser file objects matched as regular expression %s\n",wildpath);
   }

DeleteItemList(path);
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

void VerifyFilePromise(char *path,struct Promise *pp)

{ struct stat osb,oslb,dsb,dslb;
  struct Attributes a;
  struct CfLock thislock;
  int success,rlevel = 0,isthere;

a = GetFilesAttributes(pp);

if (!SanityChecks(path,a,pp))
   {
   return;
   }

if (stat(path,&osb) == -1)
   {
   if (a.create||a.touch)
      {
      if (!CreateFile(path,pp,a))
         {
         return;
         }
      }
   }
else
   {
   if (!S_ISDIR(osb.st_mode))
      {
      if (a.havedepthsearch)
         {
         CfOut(cf_error,"stat","depth_search (recursion) is promised for a base object %s that is not a directory",path);
         return;
         }
      }
   }

thislock = AcquireLock(ASUniqueName("files"),CanonifyName(path),VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

/* Phase 1 - */

if (a.havedelete||a.haverename||a.haveperms||a.havechange||a.transformer)
   {
   lstat(path,&osb); /* if doesn't exist have to stat again anyway */
   
   if (a.havedepthsearch)
      {
      SetSearchDevice(&osb,pp);
      }
   
   success = DepthSearch(path,&osb,rlevel,a,pp);

   /* normally searches do not include the base directory */
   
   if (a.recursion.include_basedir)
      {
      int save_search = a.havedepthsearch;

      /* Handle this node specially */

      a.havedepthsearch = false;
      success = DepthSearch(path,&osb,rlevel,a,pp);
      a.havedepthsearch = save_search;
      }
   }

/* Phase 2a - copying is potentially threadable if no followup actions */

if (a.havecopy)
   {
   ScheduleCopyOperation(path,a,pp);
   }

/* Phase 2b link after copy in case need file first */

if (a.havelink)
   {
   ScheduleLinkOperation(path,a,pp);
   }

/* Phase 3 - content editing */

ScheduleEditOperation(path,a,pp);

YieldCurrentLock(thislock);
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

int SanityChecks(char *path,struct Attributes a,struct Promise *pp)

{
if (a.havelink && a.havecopy)
   {
   CfOut(cf_error,"","Promise constraint conflicts - %s file cannot both be a copy of and a link to the source",path);
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.haveeditline && a.haveeditxml)
   {
   CfOut(cf_error,"","Promise constraint conflicts - %s editing file as both line and xml makes no sense",path);
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.havedepthsearch && a.haveedit)
   {
   CfOut(cf_error,"","Recursive depth_searches are not compatible with general file editing",path);
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.havedelete && (a.create||a.havecopy||a.haveedit||a.haverename))
   {
   CfOut(cf_error,"","Promise constraint conflicts - %s cannot be deleted and exist at the same time",path);
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.haverename && (a.create||a.havecopy||a.haveedit))
   {
   CfOut(cf_error,"","Promise constraint conflicts - %s cannot be renamed/moved and exist there at the same time",path);
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.havedelete && a.havedepthsearch && !a.haveselect)
   {
   CfOut(cf_error,"","Dangerous or ambiguous promise - %s specifices recursive depth search but has no file selection criteria",path);
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.havedelete && a.haverename)
   {
   CfOut(cf_error,"","File %s cannot promise both deletion and renaming",path);
   PromiseRef(cf_error,pp);
   return false;
   }

if (a.havecopy && a.havedepthsearch && a.havedelete)
   {
   CfOut(cf_inform,"","Warning: depth_search of %s applies to both delete and copy, but these refer to different searches (source/destination)",pp->promiser);
   PromiseRef(cf_inform,pp);
   }

if (a.transaction.background && a.transaction.audit)
   {
   CfOut(cf_error,"","Auditing cannot be performed on backgrounded promises (this might change).",pp->promiser);
   PromiseRef(cf_inform,pp);
   return false;
   }

if ((a.havecopy || a.havelink) && a.transformer)
   {
   CfOut(cf_error,"","File object(s) %s cannot both be a copy of source and transformed simultaneously",pp->promiser);
   PromiseRef(cf_inform,pp);
   return false;
   }

return true;
}
