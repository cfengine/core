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
/* File: nfs.c                                                               */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

#ifndef MINGW  // use samba on windows ?

int LoadMountInfo(struct Rlist **list)

/* This is, in fact, the most portable way to read the mount info! */
/* Depressing, isn't it? */

{ FILE *pp;
  char buf1[CF_BUFSIZE],buf2[CF_BUFSIZE],buf3[CF_BUFSIZE];
  char host[CF_MAXVARSIZE],source[CF_BUFSIZE],mounton[CF_BUFSIZE],vbuff[CF_BUFSIZE];
  int i, nfs = false;

for (i=0; VMOUNTCOMM[VSYSTEMHARDCLASS][i] != ' '; i++)
   {
   buf1[i] = VMOUNTCOMM[VSYSTEMHARDCLASS][i];
   }

buf1[i] = '\0';

SetTimeOut(RPCTIMEOUT);

if ((pp = cf_popen(buf1,"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","Can't open %s\n",buf1);
   return false;
   }

do
   {
   vbuff[0] = buf1[0] = buf2[0] = buf3[0] = source[0] = '\0';

   if (ferror(pp))  /* abortable */
      {
      CfOut(cf_error,"ferror","Error getting mount info\n");
      break;
      }
   
   CfReadLine(vbuff,CF_BUFSIZE,pp);

   if (ferror(pp))  /* abortable */
      {
      CfOut(cf_error,"ferror","Error getting mount info\n");
      break;
      }

   if (strstr(vbuff,"nfs"))
      {
      nfs = true;
      }
   
   sscanf(vbuff,"%s%s%s",buf1,buf2,buf3);

   if (vbuff[0] == '\n')
      {
      break;
      }

   if (strstr(vbuff,"not responding"))
      {
      CfOut(cf_error,"","%s\n",vbuff);
      }

   if (strstr(vbuff,"be root"))
      {
      CfOut(cf_error,"","Mount access is denied. You must be root.\n");
      CfOut(cf_error,"","Use the -n option to run safely.");
      }

   if (strstr(vbuff,"retrying") || strstr(vbuff,"denied") || strstr(vbuff,"backgrounding"))
      {
      continue;
      }

   if (strstr(vbuff,"exceeded") || strstr(vbuff,"busy"))
      {
      continue;
      }

   if (strstr(vbuff,"RPC"))
      {
      CfOut(cf_inform,"","There was an RPC timeout. Aborting mount operations.\n");
      CfOut(cf_inform,"","Session failed while trying to talk to remote host\n");
      CfOut(cf_inform,"","%s\n",vbuff);
      cf_pclose(pp);
      return false;
      }

   switch (VSYSTEMHARDCLASS)
      {
      case darwin:
      case sun4:
      case sun3:
      case ultrx: 
      case irix:
      case irix4:
      case irix64:
      case linuxx:
      case GnU:
      case unix_sv:
      case freebsd:
      case netbsd:
      case openbsd:
      case bsd_i:
      case nextstep:
      case bsd4_3:
      case newsos:
      case aos:
      case osf:
      case qnx:
      case crayos:
      case dragonfly:
          if (IsAbsoluteFileName(buf1))
             {
             strcpy(host,"localhost");
             strcpy(mounton,buf3);
             }
          else
             {
             sscanf(buf1,"%[^:]:%s",host,source);
             strcpy(mounton,buf3);
             }
          
          break;
      case solaris:
      case solarisx86:

      case hp:      
          if (IsAbsoluteFileName(buf3))
             {
             strcpy(host,"localhost");
             strcpy(mounton,buf1);
             }
          else
             {
             sscanf(buf1,"%[^:]:%s",host,source);
             strcpy(mounton,buf1);
             }
          
          break;
      case aix:
          /* skip header */
          
          if (IsAbsoluteFileName(buf1))
             {
             strcpy(host,"localhost");
             strcpy(mounton,buf2);
             }
          else
             {
             strcpy(host,buf1);
             strcpy(source,buf1);
             strcpy(mounton,buf3);
             }
          break;
          
      case cfnt:
          strcpy(mounton,buf2);
          strcpy(host,buf1);
          break;
      case unused2:
      case unused3:
          break;
          
      case cfsco: CfOut(cf_error,"","Don't understand SCO mount format, no data");

      default:
          printf("cfengine software error: case %d = %s\n",VSYSTEMHARDCLASS,CLASSTEXT[VSYSTEMHARDCLASS]);
          FatalError("System error in GetMountInfo - no such class!");
      }

   Debug("GOT: host=%s, source=%s, mounton=%s\n",host,source,mounton);

   if (nfs)
      {
      AugmentMountInfo(list,host,source,mounton,"nfs");
      }
   else
      {
      AugmentMountInfo(list,host,source,mounton,NULL);
      }
   }
while (!feof(pp));

alarm(0);
signal(SIGALRM,SIG_DFL);
cf_pclose(pp);
return true;
}


/*******************************************************************/

void AugmentMountInfo(struct Rlist **list,char *host,char *source,char *mounton,char *options)

{ struct CfMount *entry;

if ((entry = malloc(sizeof(struct CfMount))) == NULL)
   {
   CfOut(cf_error,"malloc","Memory allocation error - augmenting mount info");
   return;
   }

entry->host = entry->source = entry->mounton = entry->options = NULL;

if (host)
   {
   entry->host = strdup(host);
   }

if (source)
   {
   entry->source = strdup(source);
   }

if (mounton)
   {
   entry->mounton = strdup(mounton);
   }

if (options)
   {
   entry->options = strdup(options);
   }

AppendRlistAlien(list,(void *)entry);
}

/*******************************************************************/

void DeleteMountInfo(struct Rlist *list)

{ struct Rlist *rp, *sp;
  struct CfMount *entry;

for (rp = list; rp != NULL; rp = sp)
   {
   sp = rp->next;
   entry = (struct CfMount *)rp->item;
   
   if (entry->host)
      {
      free (entry->host);
      }

   if (entry->source)
      {
      free (entry->source);
      }

   if (entry->mounton)
      {
      free (entry->mounton);
      }

   if (entry->options)
      {
      free (entry->options);
      }

   free((char *)entry);
   }
}

/*******************************************************************/

int VerifyInFstab(char *name,struct Attributes a,struct Promise *pp)

/* Ensure filesystem IS in fstab, and return no of changes */
    
{ char fstab[CF_BUFSIZE];
  char *host,*rmountpt,*mountpt,*fstype,*opts;

if (!FSTABLIST)
   {
   if (!LoadFileAsItemList(&FSTABLIST,VFSTAB[VSYSTEMHARDCLASS],a,pp))
      {
      CfOut(cf_error,"","Couldn't open %s!\n",VFSTAB[VSYSTEMHARDCLASS]);
      return false;
      }
   else
      {
      FSTAB_EDITS = 0;
      }
   }
  
if (a.mount.mount_options)
   {
   opts = Rlist2String(a.mount.mount_options,",");
   }
else
   {
   opts = strdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
   }

host = a.mount.mount_server;
rmountpt = a.mount.mount_source;
mountpt = name;
fstype = a.mount.mount_type;

switch (VSYSTEMHARDCLASS)
   {
   case osf:
   case bsd4_3:
   case irix:
   case irix4:
   case irix64:
   case sun3:
   case aos:
   case nextstep:
   case newsos:
   case qnx:
   case sun4:    snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s %s\t%s 0 0",host,rmountpt,mountpt,fstype,opts);
                 break;

   case crayos:
                 snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s %s\t%s",host,rmountpt,mountpt,ToUpperStr(fstype),opts);
                 break;
   case ultrx:   //snprintf(fstab,CF_BUFSIZE,"%s@%s:%s:%s:0:0:%s:%s",rmountpt,host,mountpt,mode,fstype,opts);
                 break;
   case hp:      snprintf(fstab,CF_BUFSIZE,"%s:%s %s \t %s \t %s 0 0",host,rmountpt,mountpt,fstype,opts);
                 break;
   case aix:     snprintf(fstab,CF_BUFSIZE,"%s:\n\tdev\t= %s\n\ttype\t= %s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n\toptions\t= %s\n\taccount\t= false\n",mountpt,rmountpt,fstype,fstype,host,opts);
                 break;
   case GnU:
   case linuxx:  snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s \t %s \t %s",host,rmountpt,mountpt,fstype,opts);
                 break;

   case netbsd:
   case openbsd:
   case bsd_i:
   case dragonfly:
   case freebsd: snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s \t %s \t %s 0 0",host,rmountpt,mountpt,fstype,opts);
                 break;

   case unix_sv:
   case solarisx86:
   case solaris: snprintf(fstab,CF_BUFSIZE,"%s:%s - %s %s - yes %s",host,rmountpt,mountpt,fstype,opts);
                 break;

   case cfnt:    snprintf(fstab,CF_BUFSIZE,"/bin/mount %s:%s %s",host,rmountpt,mountpt);
                 break;
   case cfsco:   CfOut(cf_error,"","Don't understand filesystem format on SCO, no data - please fix me");
                 break;

   case unused1:
   case unused2:
   case unused3:
   default:
       free(opts);
       return false;
   }

CfOut(cf_verbose,"","Verifying %s in %s\n",mountpt,VFSTAB[VSYSTEMHARDCLASS]);

if (!MatchFSInFstab(mountpt))
   {
   AppendItem(&FSTABLIST,fstab,NULL);
   FSTAB_EDITS++;
   cfPS(cf_inform,CF_CHG,"",pp,a,"Adding file system %s:%s seems to %s.\n",host,rmountpt,VFSTAB[VSYSTEMHARDCLASS]);   
   }

free(opts);
return 0;
}

/*******************************************************************/

int VerifyNotInFstab(char *name,struct Attributes a,struct Promise *pp)

/* Ensure filesystem is NOT in fstab, and return no of changes */
    
{ char fstab[CF_BUFSIZE],regex[CF_BUFSIZE],aixcomm[CF_BUFSIZE],line[CF_BUFSIZE];
  char *host,*rmountpt,*mountpt,*fstype,*opts;
  FILE *pfp;
  struct Item *ip;

if (!FSTABLIST)
   {
   if (!LoadFileAsItemList(&FSTABLIST,VFSTAB[VSYSTEMHARDCLASS],a,pp))
      {
      CfOut(cf_error,"","Couldn't open %s!\n",VFSTAB[VSYSTEMHARDCLASS]);
      return false;
      }
   else
      {
      FSTAB_EDITS = 0;
      }
   }

if (a.mount.mount_options)
   {
   opts = Rlist2String(a.mount.mount_options,",");
   }
else
   {
   opts = VMOUNTOPTS[VSYSTEMHARDCLASS];
   }

host = a.mount.mount_server;
rmountpt = a.mount.mount_source;
mountpt = name;
fstype = a.mount.mount_type;

if (MatchFSInFstab(mountpt))
   {
   if (a.mount.editfstab)
      {
      switch (VSYSTEMHARDCLASS)
         {
         case aix:
             
             snprintf(aixcomm, CF_BUFSIZE, "/usr/sbin/rmnfsmnt -f %s", mountpt);
             
             if ((pfp = cf_popen(aixcomm,"r")) == NULL)
                {
                cfPS(cf_error,CF_FAIL,"",pp,a,"Failed to invoke /usr/sbin/rmnfsmnt to edit fstab");
                return 0;
                }
             
             while(!feof(pfp))
                {
                CfReadLine(line,CF_BUFSIZE,pfp);
                
                if (line[0] == '#')
                   {
                   continue;
                   }
                
                if (strstr(line,"busy"))
                   {
                   cfPS(cf_inform,CF_INTERPT,"",pp,a,"The device under %s cannot be removed from %s\n",mountpt,VFSTAB[VSYSTEMHARDCLASS]);
                   return 0;
                   }
                }
             
             cf_pclose(pfp);

             return 0; /* ignore internal editing for aix , always returns 0 changes */
             break;
             
         default:

             snprintf(regex,CF_BUFSIZE,".*[\\s]+%s[\\s]+.*",mountpt);
             
             for (ip = FSTABLIST; ip != NULL; ip=ip->next)
                {
                if (FullTextMatch(regex,ip->name))
                   {
                   cfPS(cf_inform,CF_CHG,"",pp,a,"Deleting file system mounted on %s.\n",host,rmountpt,VFSTAB[VSYSTEMHARDCLASS]);
                   // Check host name matches too?
                   DeleteThisItem(&FSTABLIST,ip);
                   FSTAB_EDITS++;
                   }
                }
             break;
         }
      }   
   }

if (a.mount.mount_options)
   {
   free(opts);
   }

return 0;
}

/*******************************************************************/

int VerifyMount(char *name,struct Attributes a,struct Promise *pp)

{ char comm[CF_BUFSIZE],line[CF_BUFSIZE];
  FILE *pfp;
  char *host,*rmountpt,*mountpt,*fstype,*opts;
 
host = a.mount.mount_server;
rmountpt = a.mount.mount_source;
mountpt = name;
fstype = a.mount.mount_type;

if (! DONTDO)
   {
   snprintf(comm,CF_BUFSIZE,"%s %s:%s %s",GetArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]),host,rmountpt,mountpt);
   
   if ((pfp = cf_popen(comm,"r")) == NULL)
      {
      CfOut(cf_error,""," !! Failed to open pipe from %s\n",GetArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]));
      return 0;
      }
   
   CfReadLine(line,CF_BUFSIZE,pfp);
   
   if (strstr(line,"busy") || strstr(line,"Busy"))
      {
      cfPS(cf_inform,CF_INTERPT,"",pp,a," !! The device under %s cannot be mounted\n",mountpt);
      cf_pclose(pfp);
      return 1;
      }
   
   cf_pclose(pfp);
   }

cfPS(cf_inform,CF_CHG,"",pp,a," -> Mounting %s to keep promise\n",mountpt);
      //DeleteItemStarting(&VMOUNTED,ptr->name);
return 0;
}

/*******************************************************************/

int VerifyUnmount(char *name,struct Attributes a,struct Promise *pp)

{ char comm[CF_BUFSIZE],line[CF_BUFSIZE];
  FILE *pfp;
  char *host,*rmountpt,*mountpt,*fstype,*opts;
 
host = a.mount.mount_server;
rmountpt = a.mount.mount_source;
mountpt = name;
fstype = a.mount.mount_type;

if (! DONTDO)
   {
   snprintf(comm,CF_BUFSIZE,"%s %s",VUNMOUNTCOMM[VSYSTEMHARDCLASS],mountpt);
   
   if ((pfp = cf_popen(comm,"r")) == NULL)
      {
      CfOut(cf_error,""," !! Failed to open pipe from %s\n",VUNMOUNTCOMM[VSYSTEMHARDCLASS]);
      return 0;
      }
   
   CfReadLine(line,CF_BUFSIZE,pfp);
   
   if (strstr(line,"busy") || strstr(line,"Busy"))
      {
      cfPS(cf_inform,CF_INTERPT,"",pp,a," !! The device under %s cannot be unmounted\n",mountpt);
      cf_pclose(pfp);
      return 1;
      }
   
   cf_pclose(pfp);
   }

cfPS(cf_inform,CF_CHG,"",pp,a," -> Unmounting %s to keep promise\n",mountpt);
      //DeleteItemStarting(&VMOUNTED,ptr->name);
return 0;
}

/*******************************************************************/

int MatchFSInFstab(char *match)

{ struct Item *ip;
 
for (ip = FSTABLIST; ip != NULL; ip=ip->next)
   {
   if (strstr(ip->name,match))
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

void MountAll()

{ struct stat sb;
  char line[CF_BUFSIZE];
  int fd;
  FILE *pp;
 
if (DONTDO)
   {
   CfOut(cf_verbose,"","Promised to mount filesystem, but not on this trial run\n");
   return;
   }
else
   {
   CfOut(cf_verbose,""," -> Attempting to mount all filesystems.\n");
   }

if (VSYSTEMHARDCLASS == cfnt)
   {
   /* This is a shell script. Make sure it hasn't been compromised. */

   if (cfstat("/etc/fstab",&sb) == -1)
      {
      if ((fd = creat("/etc/fstab",0755)) > 0)
         {
         write(fd,"#!/bin/sh\n\n",10);
         close(fd);
         }
      else
         {
         if (sb.st_mode & (S_IWOTH | S_IWGRP))
            {
            CfOut(cf_error,"","File /etc/fstab was insecure. Cannot mount filesystems.\n");
            return;
            }
         }
      }
   }

SetTimeOut(RPCTIMEOUT);
 
if ((pp = cf_popen(VMOUNTCOMM[VSYSTEMHARDCLASS],"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","Failed to open pipe from %s\n",VMOUNTCOMM[VSYSTEMHARDCLASS]);
   return;
   }
 
while (!feof(pp))
   {
   if (ferror(pp))  /* abortable */
      {
      CfOut(cf_inform,"ferror","Error mounting filesystems\n");
      break;
      }
   
   CfReadLine(line,CF_BUFSIZE,pp);

   if (ferror(pp))  /* abortable */
      {
      CfOut(cf_inform,"ferror","Error mounting filesystems\n");
      break;
      }

   if (strstr(line,"already mounted") || strstr(line,"exceeded") || strstr(line,"determined"))
      {
      continue;
      }

   if (strstr(line,"not supported"))
      {
      continue;
      }

   if (strstr(line,"denied") || strstr(line,"RPC"))
      {
      CfOut(cf_error,"","There was a mount error, trying to mount one of the filesystems on this host.\n");
      break;
      }

   if (strstr(line,"trying") && !strstr(line,"NFS version 2")&& !strstr(line, "vers 3"))
      {
      CfOut(cf_error,"","Attempting abort because mount went into a retry loop.\n");
      break;
      }
   }

alarm(0);
signal(SIGALRM,SIG_DFL);
cf_pclose(pp);
}

/*******************************************************************/
/* Addendum                                                        */
/*******************************************************************/

void DeleteThisItem(struct Item **liststart,struct Item *entry)
 
{ struct Item *ip, *sp;

if (entry != NULL)
   {
   if (entry->name != NULL)
      {
      free(entry->name);
      }

   sp = entry->next;

   if (entry == *liststart)
      {
      *liststart = sp;
      }
   else
      {
      for (ip = *liststart; ip->next != entry; ip=ip->next)
         {
         }

      ip->next = sp;
      }

   free((char *)entry);
   }
}

#endif  /* NOT MINGW */

