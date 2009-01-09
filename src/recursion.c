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
/* File: recursion.c                                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


/*********************************************************************/
/* Files to be ignored when parsing directories                      */
/*********************************************************************/

char *VSKIPFILES[] =
   {
   ".",
   "..",
   "lost+found",
   ".cfengine.rm",
   NULL
   };

/*********************************************************************/

int SensibleFile(char *nodename,char *path,struct Attributes attr,struct Promise *pp)

{ int i, suspicious = true;
  struct stat statbuf; 
  unsigned char *sp, newname[CF_BUFSIZE],vbuff[CF_BUFSIZE];
  
if (strlen(nodename) < 1)
   {
   CfOut(cf_error,"","Empty (null) filename detected in %s\n",path);
   return true;
   }

if (IsItemIn(SUSPICIOUSLIST,nodename))
   {
   struct stat statbuf;

   if (stat(nodename,&statbuf) != -1)
      {
      if (S_ISREG(statbuf.st_mode))
         {
         cfPS(cf_error,CF_CHG,"",pp,attr," !! Suspicious file %s found in %s\n",nodename,path);
         return false;
         }
      }
   }
 
if (strcmp(nodename,"...") == 0)
   {
   Verbose("Possible FS cell node detected in %s...\n",path);
   return true;
   }
  
for (i = 0; VSKIPFILES[i] != NULL; i++)
   {
   if (strcmp(nodename,VSKIPFILES[i]) == 0)
      {
      Debug("Filename %s/%s is classified as ignorable\n",path,nodename);
      return false;
      }
   }

if ((strcmp("[",nodename) == 0) && (strcmp("/usr/bin",path) == 0))
   {
   if (VSYSTEMHARDCLASS == linuxx)
      {
      return true;
      }
   }

suspicious = true;

for (sp = nodename; *sp != '\0'; sp++)
   {
   if ((*sp > 31) && (*sp < 127))
      {
      suspicious = false;
      break;
      }
   }

strcpy(vbuff,path);
AddSlash(vbuff);
strcat(vbuff,nodename); 

if (suspicious && NONALPHAFILES)
   {
   cfPS(cf_error,CF_CHG,"",pp,attr," !! Suspicious filename %s in %s has no alphanumeric content (security)",CanonifyName(nodename),path);
   strcpy(newname,vbuff);
   
   for (sp = newname+strlen(path); *sp != '\0'; sp++)
      {
      if ((*sp > 126) || (*sp < 33))
         {
         *sp = 50 + (*sp / 50);  /* Create a visible ASCII interpretation */
         }
      }
   
   strcat(newname,".cf-nonalpha");
   
   CfOut(cf_inform,"","Renaming file %s to %s",vbuff,newname);
   
   if (rename(vbuff,newname) == -1)
      {
      CfOut(cf_verbose,"rename","Rename failed - foreign filesystem?\n");
      }

   if (chmod(newname,0644) == -1)
      {
      CfOut(cf_verbose,"chmod","Mode change failed - foreign filesystem?\n");
      }

   return false;
   }

for (sp = nodename; *sp != '\0'; sp++) /* Check for files like ".. ." */
   {
   if ((*sp != '.') && ! isspace(*sp))
      {
      suspicious = false;
      return true;
      }
   }

/* removed if (EXTENSIONLIST==NULL) mb */ 

if (cf_lstat(vbuff,&statbuf,attr,pp) == -1)
   {
   CfOut(cf_verbose,"cf_lstat","Couldn't stat %s",vbuff);
   return true;
   }

if (statbuf.st_size == 0 && ! (VERBOSE||INFORM)) /* No sense in warning about empty files */
   {
   return false;
   }
 
cfPS(cf_error,CF_CHG,"",pp,attr," !! Suspicious looking file object \"%s\" masquerading as hidden file in %s\n",nodename,path);
 
if (S_ISLNK(statbuf.st_mode))
   {
   CfOut(cf_error,""," !!- %s is a symbolic link\n",nodename);
   }
else if (S_ISDIR(statbuf.st_mode))
   {
   CfOut(cf_error,""," !!- %s is a directory\n",nodename);
   }
else
   {
   CfOut(cf_error,""," !!- %s has size %ld and full mode %o\n",nodename,(unsigned long)(statbuf.st_size),(unsigned int)(statbuf.st_mode));
   }
 
return true;
}

/*********************************************************************/
/* Depth searches                                                    */
/*********************************************************************/

int DepthSearch(char *name,struct stat *sb,int rlevel,struct Attributes attr,struct Promise *pp)
    
{ DIR *dirh;
  int goback; 
  struct dirent *dirp;
  char path[CF_BUFSIZE];
  struct stat lsb;
  
if (!attr.havedepthsearch)  /* if the search is trivial, make sure that we are in the parent dir of the leaf */
   {
   char basedir[CF_BUFSIZE];

   Debug(" -> Direct file reference %s, no search implied\n",name);
   strcpy(basedir,name);
   ChopLastNode(basedir);
   chdir(basedir);
   return VerifyFileLeaf(name,sb,attr,pp);
   }

if (rlevel > CF_RECURSION_LIMIT)
   {
   CfOut(cf_error,"","WARNING: Very deep nesting of directories (>%d deep): %s (Aborting files)",rlevel,name);
   return false;
   }
 
memset(path,0,CF_BUFSIZE); 

Debug("To iterate is Human, to recurse is divine...(%s)\n",name);

if (!PushDirState(name,sb))
   {
   return false;
   }
 
if ((dirh = opendir(".")) == NULL)
   {
   CfOut(cf_inform,"opendir","Could not open existing directory %s\n",name);
   return false;
   }

for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
   {
   if (!SensibleFile(dirp->d_name,name,attr,pp))
      {
      continue;
      }

   strcpy(path,name);
   AddSlash(path);

   if (!JoinPath(path,dirp->d_name))
      {
      closedir(dirh);
      return true;
      }
   
   if (lstat(dirp->d_name,&lsb) == -1)
      {
      CfOut(cf_verbose,"lstat","Recurse was looking at %s when an error occurred:\n",path);
      continue;
      }

   if (S_ISLNK(lsb.st_mode))            /* should we ignore links? */
      {
      if (!KillGhostLink(path,attr,pp))
         {
         VerifyFileLeaf(path,&lsb,attr,pp);
         }
      else
         {
         continue;
         }
      }
   
   /* See if we are supposed to treat links to dirs as dirs and descend */
   
   if (attr.recursion.travlinks && S_ISLNK(lsb.st_mode))
      {
      if (lsb.st_uid != 0 && lsb.st_uid != getuid())
         {
         CfOut(cf_inform,"","File %s is an untrusted link: cfengine will not follow it with a destructive operation",path);
         continue;
         }

      /* if so, hide the difference by replacing with actual object */
      
      if (stat(dirp->d_name,&lsb) == -1)
         {
         CfOut(cf_error,"stat","Recurse was working on %s when this failed:\n",path);
         continue;
         }
      }
   
   if (attr.recursion.xdev && DeviceBoundary(&lsb,pp))
      {
      Verbose("Skipping %s on different device - use xdev option to change this\n",path);
      continue;
      }

   if (S_ISDIR(lsb.st_mode))
      {
      if (SkipDirLinks(path,dirp->d_name,attr.recursion))
         {
         continue;
         }
      
      if (attr.recursion.depth > 1)
         {
         goback = DepthSearch(path,&lsb,rlevel+1,attr,pp);
         PopDirState(goback,name,sb,attr.recursion);
         VerifyFileLeaf(path,&lsb,attr,pp);
         }
      else
         {
         VerifyFileLeaf(path,&lsb,attr,pp);
         }
      }
   else
      {
      VerifyFileLeaf(path,&lsb,attr,pp);
      }
   }

closedir(dirh);
return true; 
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

int PushDirState(char *name,struct stat *sb)

{
if (chdir(name) == -1)
   {
   CfOut(cf_inform,"chdir","Could not change to directory %s, mode %o in tidy",name,sb->st_mode & 07777);
   return false;
   }
else
   {
   Debug("Changed directory to %s\n",name);
   }

CheckLinkSecurity(sb,name);
return true; 
}

/**********************************************************************/

void PopDirState(int goback,char *name,struct stat *sb,struct Recursion r)

{
if (goback && r.travlinks)
   {
   if (chdir(name) == -1)
      {
      CfOut(cf_error,"chdir","Error in backing out of recursive travlink descent securely to %s",name);
      HandleSignals(SIGTERM);
      }
   
   CheckLinkSecurity(sb,name); 
   }
else if (goback)
   {
   if (chdir("..") == -1)
      {
      CfOut(cf_error,"chdir","Error in backing out of recursive descent securely to %s",name);
      HandleSignals(SIGTERM);
      }
   }
}

/**********************************************************************/

int SkipDirLinks(char *path,char *lastnode,struct Recursion r)

{
Debug("SkipDirLinks(%s,%s)\n",path,lastnode);

if ((r.include_dirs != NULL) && !(MatchRlistItem(r.include_dirs,path) || MatchRlistItem(r.include_dirs,lastnode)))
   {
   Debug("Skipping matched non-included directory %s\n",path);
   return true;
   }

if (MatchRlistItem(r.exclude_dirs,path) || MatchRlistItem(r.exclude_dirs,lastnode))
   {
   Debug("Skipping matched excluded directory %s\n",path);
   return true;
   }       

return false;
}


/**********************************************************************/

void CheckLinkSecurity(struct stat *sb,char *name)

{ struct stat security;

Debug("Checking the inode and device to make sure we are where we think we are...\n"); 

if (stat(".",&security) == -1)
   {
   CfOut(cf_error,"stat","Could not stat directory %s after entering!",name);
   return;
   }

if ((sb->st_dev != security.st_dev) || (sb->st_ino != security.st_ino))
   {
   CfOut(cf_error,"","SERIOUS SECURITY ALERT: path race exploited in recursion to/from %s. Not safe for agent to continue - aborting",name);
   HandleSignals(SIGTERM);
   /* Exits */
   }
}

