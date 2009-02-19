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
/* File: verify_storage.c                                                    */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void *FindAndVerifyStoragePromises(struct Promise *pp)

{
PromiseBanner(pp); 
FindStoragePromiserObjects(pp);

return (void *)NULL;
}

/*****************************************************************************/

void FindStoragePromiserObjects(struct Promise *pp)

{
/* Check if we are searching over a regular expression */
 
LocateFilePromiserGroup(pp->promiser,pp,VerifyStoragePromise);
}

/*****************************************************************************/

void VerifyStoragePromise(char *path,struct Promise *pp)

{ struct stat osb,oslb,dsb,dslb;
  struct Attributes a;
  struct CfLock thislock;
  int success,rlevel = 0,isthere;

a = GetStorageAttributes(pp);

/* No parameter conflicts here */

if (a.mount.unmount)
   {
   if (a.mount.mount_source || a.mount.mount_server)
      {
      CfOut(cf_verbose,""," !! An unmount promise indicates a mount-source information - probably in error\n");
      }
   }
else
   {
   if (a.mount.mount_source == NULL || a.mount.mount_server == NULL)
      {
      CfOut(cf_error,""," !! Insufficient specification in mount promise - need source and server\n");
      return;
      }
   }


thislock = AcquireLock(path,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

/* Do mounts first */

if (a.havemount)
   {
   if (!MOUNTEDFSLIST && !LoadMountInfo(&MOUNTEDFSLIST))
      {
      CfOut(cf_error,"","Couldn't obtain a list of mounted filesystems - aborting\n");
      return;
      }

   VerifyMountPromise(path,a,pp);
   }

/* Then check file system */

if (a.havevolume)
   {
   VerifyFileSystem(path,a,pp);   
   VerifyFreeSpace(path,a,pp);
   
   if (a.volume.scan_arrivals)
      {
      VolumeScanArrivals(path,a,pp);
      }
   }

YieldCurrentLock(thislock);
}


/*******************************************************************/
/** Level                                                          */
/*******************************************************************/

int VerifyMountPromise(char *name,struct Attributes a,struct Promise *pp)

{ struct CfMount mount;
  char *options;
  char dir[CF_BUFSIZE];
  int changes = 0;
 
CfOut(cf_verbose,""," -> Verifying mounted file systems on %s\n",name);

snprintf(dir,CF_BUFSIZE,"%s/.",name);

if (!IsPrivileged())                            
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a,"Only root can mount filesystems.\n","");
   return false;
   }

options = Rlist2String(a.mount.mount_options,",");

if (!FileSystemMountedCorrectly(MOUNTEDFSLIST,name,options,a,pp))
   {
   if (!a.mount.unmount)
      {
      if (!MakeParentDirectory(dir,a.move_obstructions))
         {
         }

      if (a.mount.editfstab)
         {
         changes += VerifyInFstab(name,a,pp);
         }
      else
         {
         cfPS(cf_inform,CF_FAIL,"",pp,a," -> Filesystem %s was not mounted as promised, and no edits were promised in %s\n",name,VFSTAB[VSYSTEMHARDCLASS]);
         // Mount explicitly
         }
      }
   else
      {
      if (a.mount.editfstab)
         {
         changes += VerifyNotInFstab(name,a,pp);
         }
      }

   if (changes)
      {
      CF_MOUNTALL = true;
      CF_SAVEFSTAB = true;
      }
   }
else
   {
   if (a.mount.unmount)
      {
      VerifyUnmount(name,a,pp);
      }
   
   if (a.mount.editfstab)
      {
      if (VerifyNotInFstab(name,a,pp))
         {
         CF_SAVEFSTAB = true;
         }
      }
   else
      {
      cfPS(cf_inform,CF_NOP,"",pp,a," -> Filesystem %s seems to be mounted as promised\n",name);
      }
   }

free(options);
return true;
}

/*******************************************************************/

int VerifyFileSystem(char *name,struct Attributes a,struct Promise *pp)

{ struct stat statbuf, localstat;
  DIR *dirh;
  struct dirent *dirp;
  long sizeinbytes = 0, filecount = 0;
  char buff[CF_BUFSIZE];

CfOut(cf_verbose,""," -> Checking required filesystem %s\n",name);

if (stat(name,&statbuf) == -1)
   {
   return(false);
   }

if (S_ISLNK(statbuf.st_mode))
   {
   KillGhostLink(name,a,pp);
   return(true);
   }

if (S_ISDIR(statbuf.st_mode))
   {
   if ((dirh = opendir(name)) == NULL)
      {
      CfOut(cf_error,"opendir","Can't open directory %s which checking required/disk\n",name);
      return false;
      }

   for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
      {
      if (!ConsiderFile(dirp->d_name,name,a,pp))
         {
         continue;
         }

      filecount++;

      strcpy(buff,name);

      if (buff[strlen(buff)] != '/')
         {
         strcat(buff,"/");
         }

      strcat(buff,dirp->d_name);

      if (lstat(buff,&localstat) == -1)
         {
         if (S_ISLNK(localstat.st_mode))
            {
            KillGhostLink(buff,a,pp);
            continue;
            }

         CfOut(cf_error,"lstat","Can't stat volume %s\n",buff);
         continue;
         }

      sizeinbytes += localstat.st_size;
      }

   closedir(dirh);

   if (sizeinbytes < 0)
      {
      CfOut(cf_verbose,"","Internal error: count of byte size was less than zero!\n");
      return true;
      }

   if (sizeinbytes < SENSIBLEFSSIZE)
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a," !! File system %s is suspiciously small! (%d bytes)\n",name,sizeinbytes);
      return(false);
      }

   if (filecount < SENSIBLEFILECOUNT)
      {
      cfPS(cf_error,CF_INTERPT,"",pp,a," !! Filesystem %s has only %d files/directories.\n",name,filecount);
      return(false);
      }
   }

cfPS(cf_inform,CF_NOP,"",pp,a," -> Filesystem %s's content seems to be sensible as promised\n",name);
return(true);
}

/*******************************************************************/

int VerifyFreeSpace(char *file,struct Attributes a,struct Promise *pp)

{ struct stat statbuf;
  int free;
  int kilobytes;

if (stat(file,&statbuf) == -1)
   {
   CfOut(cf_error,"stat","Couldn't stat %s checking diskspace\n",file);
   return true;
   }

if (!a.volume.check_foreign)
   {
   if (IsForeignFileSystem(&statbuf,file))
      {
      CfOut(cf_inform,"","Filesystem %s is mounted from a foreign system, so skipping it",file);
      return true;
      }
   }

kilobytes = a.volume.freespace;

if (kilobytes < 0)
   {
   free = GetDiskUsage(file,cfpercent);
   kilobytes = -1 * kilobytes;

   if (free < kilobytes)
      {
      cfPS(cf_error,CF_CHG,"",pp,a," !! Free disk space is under %d%% for volume containing %s (%d%% free)\n",kilobytes,file,free);
      return false;
      }
   }
else
   {
   free = GetDiskUsage(file, cfabs);

   if (free < kilobytes)
      {
      cfPS(cf_error,CF_CHG,"",pp,a," !! Disk space under %d kB for volume containing %s (%d kB free)\n",kilobytes,file,free);
      return false;
      }
   }

return true;
}

/*******************************************************************/

void VolumeScanArrivals(char *file,struct Attributes a,struct Promise *pp)

{
 CfOut(cf_verbose,"","Scan arrival sequence . not yet implemented\n");
}

/*******************************************************************/

int FileSystemMountedCorrectly(struct Rlist *list,char *name,char *options,struct Attributes a,struct Promise *pp)

{ struct Rlist *rp;
  struct CfMount *mp;
  int found = false;

for (rp = list; rp != NULL; rp=rp->next)
   {
   mp = (struct CfMount *)rp->item;

   if (mp == NULL)
      {
      continue;
      }

   /* Give primacy to the promised / affected object */
   
   if (strcmp(name,mp->mounton) == 0)
      {
      /* We have found something mounted on the promiser dir */

      found = true;
      
      if (strcmp(mp->source,a.mount.mount_source) != 0)
         {
         CfOut(cf_inform,"","A different files system (%s:%s) is mounted on %s than what is promised\n",mp->host,mp->source,name);
         return false;
         }
      else
         {
         CfOut(cf_verbose,""," -> File system %s seems to be mounted correctly\n",mp->source);
         break;
         }
      }
   }

if (!found)
   {
   CfOut(cf_verbose,""," !! File system %s seems not to be mounted correctly\n",name);

   if (! a.mount.unmount)
      {
      CF_MOUNTALL = true;
      }
   }

return found;
}

/*********************************************************************/
/* Mounting */
/*********************************************************************/

int IsForeignFileSystem (struct stat *childstat,char *dir)

 /* Is FS NFS mounted ? */

{ struct stat parentstat;
  char host[CF_MAXVARSIZE], vbuff[CF_BUFSIZE];
 
strncpy(vbuff,dir,CF_BUFSIZE-1);

if (vbuff[strlen(vbuff)-1] == '/')
   {
   strcat(vbuff,"..");
   }
else
   {
   strcat(vbuff,"/..");
   }

if (stat(vbuff,&parentstat) == -1)
   {
   Debug2("File %s couldn't stat its parent directory! Assuming permission\n",dir);
   Debug2("is denied because the file system is mounted from another host.\n");
   return(true);
   }

if (childstat->st_dev != parentstat.st_dev)
   {
   Debug2("[%s is on a different file system, not descending]\n",dir);
   return (true);
   }

Debug("NotMountedFileSystem\n");
return(false);
}
