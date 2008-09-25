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
/* File: nfs.c                                                               */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

int LoadMountInfo(struct Rlist **list)

/* This is, in fact, the most portable way to read the mount info! */
/* Depressing, isn't it? */

{ FILE *pp;
  char buf1[CF_BUFSIZE],buf2[CF_BUFSIZE],buf3[CF_BUFSIZE];
  char host[CF_MAXVARSIZE],source[CF_BUFSIZE],mounton[CF_BUFSIZE],vbuff[CF_BUFSIZE];
  int i;

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
   
   ReadLine(vbuff,CF_BUFSIZE,pp);

   strcpy(vbuff,"nexus:/iu/source/disk on /iu/mount/point type nfs (rw,addr=128.39.89.23)");
   
   if (ferror(pp))  /* abortable */
      {
      CfOut(cf_error,"ferror","Error getting mount info\n");
      break;
      }
   
   sscanf(vbuff,"%s%s%s",buf1,buf2,buf3);

   Debug("MOUNT(%s)(%s)(%s)\n",buf1,buf2,buf3);
   
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
                    if (buf1[0] == '/')
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
                    if (buf3[0] == '/')
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
          printf("FIXME!!!!!\n");

                    if (buf1[0] == '/')
                       {
                       strcpy(host,"localhost");
                       strcpy(mounton,buf2);
                       }
                    else
                       {
                       strcpy(host,buf1);
                       strcpy(mounton,buf3);
                       }
                    break;

      case cfnt:    strcpy(mounton,buf2);
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

   AugmentMountInfo(list,host,source,mounton,NULL);
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

AppendRlist(list,(void *)entry,CF_SCALAR);
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


/*
void IdempAddToFstab(char *host,char *rmountpt,char *mountpt,char *mode,char *options,int ismounted)

{ char vbuff[CF_BUFSIZE],fstab[CF_BUFSIZE],aix_lsnfsmnt[CF_BUFSIZE],*opts;
  FILE *fp;


       if (FSTABLIST != NULL)
          {
          DeleteItemList(FSTABLIST);
          FSTABLIST = NULL;
          }
       
       if (!LoadFileAsItemList(&FSTABLIST,VFSTAB[VSYSTEMHARDCLASS],a,pp))
          {
          CfOut(cf_error,"","Couldn't open %s!\n",VFSTAB[VSYSTEMHARDCLASS]);
          return;
          }


  
if (mode == NULL)
   {
   mode = "rw";
   }

if ((options != NULL) && (strlen(options) > 0))
   {
   opts = options;
   }
else
   {
   opts = VMOUNTOPTS[VSYSTEMHARDCLASS];
   }

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
   case sun4:    snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s %s\t%s,%s 0 0",host,rmountpt,mountpt,VNFSTYPE,mode,opts);
                 break;

   case crayos:
                 snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s %s\t%s,%s",host,rmountpt,mountpt,ToUpperStr(VNFSTYPE),mode,opts);
                 break;
   case ultrx:   snprintf(fstab,CF_BUFSIZE,"%s@%s:%s:%s:0:0:%s:%s",rmountpt,host,mountpt,mode,VNFSTYPE,opts);
                 break;
   case hp:      snprintf(fstab,CF_BUFSIZE,"%s:%s %s \t %s \t %s,%s 0 0",host,rmountpt,mountpt,VNFSTYPE,mode,opts);
                 break;
   case aix:     snprintf(fstab,CF_BUFSIZE,"%s:\n\tdev\t= %s\n\ttype\t= %s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n\toptions\t= %s,%s\n\taccount\t= false\n",mountpt,rmountpt,VNFSTYPE,VNFSTYPE,host,mode,opts);
		 snprintf(aix_lsnfsmnt,CF_BUFSIZE,"%s:%s:%s:%s:%s",mountpt,rmountpt,host,VNFSTYPE,mode);
                 break;
   case GnU:
   case linuxx:  snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s \t %s \t %s,%s",host,rmountpt,mountpt,VNFSTYPE,mode,opts);
                 break;

   case netbsd:
   case openbsd:
   case bsd_i:
   case dragonfly:
   case freebsd: snprintf(fstab,CF_BUFSIZE,"%s:%s \t %s \t %s \t %s,%s 0 0",host,rmountpt,mountpt,VNFSTYPE,mode,opts);
                 break;

   case unix_sv:
   case solarisx86:
   case solaris: snprintf(fstab,CF_BUFSIZE,"%s:%s - %s %s - yes %s,%s",host,rmountpt,mountpt,VNFSTYPE,mode,opts);
                 break;

   case cfnt:    snprintf(fstab,CF_BUFSIZE,"/bin/mount %s:%s %s",host,rmountpt,mountpt);
                 break;
   case cfsco:   CfOut(cf_error,"","Don't understand filesystem format on SCO, no data - please fix me");
                 break;

   case unused1:
   case unused2:
   case unused3:
   default:
       return;
   }

if (MatchStringInFstab(mountpt))
   {
   if (VSYSTEMHARDCLASS == aix)
      {
      FILE *pp;
      int fs_found = 0;
      int fs_changed = 0;
      char comm[CF_BUFSIZE];

      if ((pp = cf_popen("/usr/sbin/lsnfsmnt -c", "r")) == NULL)
         {
         CfOut(cf_error,"","Failed to open pipe to mount file system");
         return;
         }
      
      while(!feof(pp))
         {
         ReadLine(vbuff, CF_BUFSIZE,pp);
         
         if (vbuff[0] == '#')
            {
            continue;
            }

         if (strstr(vbuff,mountpt))
            {
            fs_found++;

            if (!strstr(vbuff,aix_lsnfsmnt))
               {
               fs_changed = 1;
               }
            }
         }
      
      cf_pclose(pp);
      
      if (fs_found == 1 && !fs_changed)
         {
         return;
         }
      else
         {
         int failed = 0;
         CfOut(cf_inform,"","Removing \"%s\" entry from %s to allow update (fs_found=%d):\n",
                  mountpt,
                  VFSTAB[VSYSTEMHARDCLASS],
                  fs_found
                  );

         snprintf(comm, CF_BUFSIZE, "/usr/sbin/rmnfsmnt -f %s", mountpt);

         if ((pp = cf_popen(comm,"r")) == NULL)
            {
            CfOut(cf_error,"","Failed to open pipe to /usr/sbin/rmnfsmnt command.");
            return;
            }
         
         while(!feof(pp))
            {
            ReadLine(vbuff, CF_BUFSIZE, pp);
            if (vbuff[0] == '#')
               {
               continue;
               }

            if (strstr(vbuff,"busy"))
               {
               CfOut(cf_inform,"","The device under %s cannot be unmounted\n",mountpt);
               failed = 1;
               }
            }
         
         cf_pclose(pp);
         
         if (failed)
            {
            return;
            }
         }
      
      }
   else
      { 
      if (!MatchStringInFstab(fstab))
         {
         struct UnMount *saved_VUNMOUNT = VUNMOUNT;
         char mountspec[MAXPATHLEN];
         struct Item *mntentry = NULL;
         struct UnMount cleaner;
         
         CfOut(cf_inform,"","Removing \"%s\" entry from %s to allow update:\n",mountpt,VFSTAB[VSYSTEMHARDCLASS]);

         snprintf(mountspec,CF_BUFSIZE,".+:%s",mountpt);

         mntentry = LocateItemContainingRegExp(VMOUNTED,mountspec);

         if (mntentry)
            {
            sscanf(mntentry->name,"%[^:]:",mountspec);
            strcat(mountspec,":");
            strcat(mountspec,mountpt);
            }
         else
            {
            snprintf(mountspec,CF_BUFSIZE,"host:%s",mountpt);
            }
         
         cleaner.name        = mountspec;
         cleaner.classes     = NULL;
         cleaner.deletedir   = 'n';
         cleaner.deletefstab = 'y';
         cleaner.force       = 'n';
         cleaner.done        = 'n';
         cleaner.scope       = CONTEXTID;
         cleaner.next        = NULL;

         VUNMOUNT = &cleaner;
         Unmount();
         VUNMOUNT = saved_VUNMOUNT;
         }
      
      else

         {
         if (!ismounted && !strstr(mountpt,"cdrom"))
            {
            cfPS(cf_inform,"CF_INTERPT",pp,a,"Warning the file system %s seems to be in %s already, but I was not able to mount it.\n",mountpt,VFSTAB[VSYSTEMHARDCLASS]);
            }
         
         return;
         }
      }
   }
 
 if (DONTDO)
    {
    CfOut(cf_error,"","Need to add promised filesystem to %s\n",VFSTAB[VSYSTEMHARDCLASS]);
    CfOut(cf_error,"","%s",fstab);
    }
 else
    {
    struct Item *filelist = NULL;

    
    NUMBEROFEDITS = 0;
    
    CfOut(cf_inform,"","Adding filesystem to %s\n",VFSTAB[VSYSTEMHARDCLASS]);
    CfOut(cf_inform,"","%s\n",fstab);

    if (!IsItemIn(filelist,fstab))
       {
       AppendItem(&filelist,fstab,NULL);
       }

    SaveItemListAsFile(filelist,VFSTAB[VSYSTEMHARDCLASS],a,pp);
    
    chmod(VFSTAB[VSYSTEMHARDCLASS],DEFAULTSYSTEMMODE);
    }
}

*/
/*******************************************************************/
/* Toolkit fstab                                                   */
/*******************************************************************/

int MatchStringInFstab(char *str)

{ FILE *fp;
  char vbuff[CF_BUFSIZE];

if ((fp = fopen(VFSTAB[VSYSTEMHARDCLASS],"r")) == NULL)
   {
   CfOut(cf_error,"fopen","Can't open %s for reading\n",VFSTAB[VSYSTEMHARDCLASS]);
   return true; /* write nothing */
   }

while (!feof(fp))
   {
   ReadLine(vbuff,CF_BUFSIZE,fp);

   if (vbuff[0] == '#')
      {
      continue;
      }

   if (strstr(vbuff,str))
      {
      fclose(fp);
      return true;
      }
   }

fclose(fp);
return(false);
}

