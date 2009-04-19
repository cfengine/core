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
/* File: files_properties.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/
/* Files to be ignored when parsing directories                      */
/*********************************************************************/


/*********************************************************************/

int ConsiderFile(char *nodename,char *path,struct Attributes attr,struct Promise *pp)

{ int i, suspicious = true;
  struct stat statbuf; 
  unsigned char *sp, newname[CF_BUFSIZE],vbuff[CF_BUFSIZE];
  static char *skipfiles[] =
      {
      ".",
      "..",
      "lost+found",
      ".cfengine.rm",
      NULL
      };

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
         CfOut(cf_error,"","Suspicious file %s found in %s\n",nodename,path);
         return false;
         }
      }
   }
 
if (strcmp(nodename,"...") == 0)
   {
   CfOut(cf_verbose,"","Possible DFS/FS cell node detected in %s...\n",path);
   return true;
   }
  
for (i = 0; skipfiles[i] != NULL; i++)
   {
   if (strcmp(nodename,skipfiles[i]) == 0)
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
   CfOut(cf_error,"","Suspicious filename %s in %s has no alphanumeric content (security)",CanonifyName(nodename),path);
   strcpy(newname,vbuff);

   for (sp = newname+strlen(path); *sp != '\0'; sp++)
      {
      if ((*sp > 126) || (*sp < 33))
         {
         *sp = 50 + (*sp / 50);  /* Create a visible ASCII interpretation */
         }
      }
   
   strcat(newname,".cf-nonalpha");
   
   CfOut(cf_error,"","Renaming file %s to %s",vbuff,newname);
   
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

if (strstr(nodename,".") && (EXTENSIONLIST != NULL))
   {
   if (cf_lstat(vbuff,&statbuf,attr,pp) == -1)
      {
      CfOut(cf_verbose,"lstat","Couldn't examine %s - foreign filesystem?\n",vbuff);
      return true;
      }

   if (S_ISDIR(statbuf.st_mode))
      {
      if (strcmp(nodename,"...") == 0)
         {
         CfOut(cf_verbose,"","Hidden directory ... found in %s\n",path);
         return true;
         }
      
      for (sp = nodename+strlen(nodename)-1; *sp != '.'; sp--)
         {
         }
      
      if ((char *)sp != nodename) /* Don't get .dir */
         {
         sp++; /* Find file extension, look for known plain files  */
         
         if ((strlen(sp) > 0) && IsItemIn(EXTENSIONLIST,sp))
            {
            CfOut(cf_error,"","Suspicious directory %s in %s looks like plain file with extension .%s",nodename,path,sp);
            return false;
            }
         }
      }
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
   CfOut(cf_verbose,"lstat","Couldn't stat %s",vbuff);
   return true;
   }

if (statbuf.st_size == 0 && ! (VERBOSE||INFORM)) /* No sense in warning about empty files */
   {
   return false;
   }
 
CfOut(cf_error,"","Suspicious looking file object \"%s\" masquerading as hidden file in %s\n",nodename,path);
Debug("Filename looks suspicious\n"); 
 
if (S_ISLNK(statbuf.st_mode))
   {
   CfOut(cf_inform,"","   %s is a symbolic link\n",nodename);
   }
else if (S_ISDIR(statbuf.st_mode))
   {
   CfOut(cf_inform,"","   %s is a directory\n",nodename);
   }

CfOut(cf_verbose,"","[%s] has size %ld and full mode %o\n",nodename,(unsigned long)(statbuf.st_size),(unsigned int)(statbuf.st_mode)); 
return true;
}


/********************************************************************/

void SetSearchDevice(struct stat *sb,struct Promise *pp)

{
Debug("Registering root device as %d\n",sb->st_dev);
pp->rootdevice = sb->st_dev;
}


/********************************************************************/

int DeviceBoundary(struct stat *sb,struct Promise *pp)

{
if (sb->st_dev == pp->rootdevice)
   {
   return false;
   }
else
   {
   CfOut(cf_verbose,"","Device change from %d to %d\n",pp->rootdevice,sb->st_dev);
   return true;
   }
}
