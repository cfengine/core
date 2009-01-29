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
/* File: files_edit.c                                                        */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

struct edit_context *NewEditContext(char *filename,struct Attributes a,struct Promise *pp)

{ struct edit_context *ec;

if ((ec = malloc(sizeof(struct edit_context))) == NULL)
   {
   return NULL;
   }

ec->filename = filename;
ec->file_start = NULL;
ec->file_classes = NULL;
ec->num_edits = 0;

if (!LoadFileAsItemList(&(ec->file_start),filename,a,pp))
   {
   return NULL;
   }

if (a.edits.empty_before_use)
   {
   CfOut(cf_verbose,"","Build file model from a blank slate (emptying)\n");
   DeleteItemList(ec->file_start);
   ec->file_start = NULL;
   }

return ec;
}

/*****************************************************************************/

void FinishEditContext(struct edit_context *ec,struct Attributes a,struct Promise *pp)

{ int retval = false;
  struct Item *ip;

if (DONTDO || a.transaction.action == cfa_warn)
   {
   if (ec && ec->num_edits > 0)
      {
      cfPS(cf_error,CF_NOP,"",pp,a,"Need to edit file %s but only a warning promised",ec->filename);
      }
   return;
   }
else if (ec && ec->num_edits > 0)
   {
   if (CompareToFile(ec->file_start,ec->filename))
      {
      if (ec)
         {
         cfPS(cf_inform,CF_NOP,"",pp,a," -> No edit changes to file %s need saving",ec->filename);
         }      
      }
   else
      {
      cfPS(cf_inform,CF_CHG,"",pp,a," -> Saving edit changes to file %s",ec->filename);
      SaveItemListAsFile(ec->file_start,ec->filename,a,pp);
      }
   }
else
   {
   if (ec)
      {
      cfPS(cf_inform,CF_NOP,"",pp,a," -> No edit changes to file %s need saving",ec->filename);
      }
   }

if (ec != NULL)
   {
   for (ip = ec->file_classes; ip != NULL; ip = ip->next)
      {
      NewClass(ip->name);
      }
   
   DeleteItemList(ec->file_classes);
   DeleteItemList(ec->file_start);
   }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int LoadFileAsItemList(struct Item **liststart,char *file,struct Attributes a,struct Promise *pp)

{ FILE *fp;
  struct stat statbuf;
  char line[CF_BUFSIZE];
  
if (stat(file,&statbuf) == -1)
   {
   cfPS(cf_inform,CF_INTERPT,"stat",pp,a,"File %s could not be loaded",file);
   return false;
   }

if (a.edits.maxfilesize != 0 && statbuf.st_size > a.edits.maxfilesize)
   {
   CfOut(cf_inform,"","File %s is bigger than the limit edit.max_file_size = %d bytes\n",file,a.edits.maxfilesize);
   return(false);
   }

if (! S_ISREG(statbuf.st_mode))
   {
   cfPS(cf_inform,CF_INTERPT,"",pp,a,"%s is not a plain file\n",file);
   return false;
   }
  
if ((fp = fopen(file,"r")) == NULL)
   {
   cfPS(cf_inform,CF_INTERPT,"fopen",pp,a,"Couldn't read file %s for editing\n",file);
   return false;
   }

memset(line,0,CF_BUFSIZE); 

while(!feof(fp))
   {
   ReadLine(line,CF_BUFSIZE-1,fp);

   if (!feof(fp) || (strlen(line) != 0))
      {
      AppendItem(liststart,line,NULL);
      }
   
   line[0] = '\0';
   }

fclose(fp);
return (true); 
}

/*********************************************************************/

int SaveItemListAsFile(struct Item *liststart,char *file,struct Attributes a,struct Promise *pp)

{ struct Item *ip;
  struct stat statbuf;
  char new[CF_BUFSIZE],backup[CF_BUFSIZE];
  FILE *fp;
  mode_t mask;
  char stamp[CF_BUFSIZE]; 
  time_t stamp_now;

#ifdef WITH_SELINUX
  int selinux_enabled=0;
  security_context_t scontext=NULL;

selinux_enabled = (is_selinux_enabled() > 0);
if (selinux_enabled)
   {
   /* get current security context */
   getfilecon(file, &scontext);
   }
#endif

stamp_now = time((time_t *)NULL);
  
if (stat(file,&statbuf) == -1)
   {
   cfPS(cf_error,CF_FAIL,"stat",pp,a,"Can no longer access file %s, which needed editing!\n",file);
   return false;
   }

strcpy(backup,file);

if (a.edits.backup == cfa_timestamp)
   {
   snprintf(stamp,CF_BUFSIZE,"_%d_%s", CFSTARTTIME,CanonifyName(ctime(&stamp_now)));
   strcat(backup,stamp);
   }

strcat(backup,".cf-before-edit");

strcpy(new,file);
strcat(new,".cf-after-edit");
unlink(new); /* Just in case of races */ 
 
if ((fp = fopen(new,"w")) == NULL)
   {
   cfPS(cf_error,CF_FAIL,"fopen",pp,a,"Couldn't write file %s after editing\n",new);
   return false;
   }

for (ip = liststart; ip != NULL; ip=ip->next)
   {
   fprintf(fp,"%s\n",ip->name);
   }

if (fclose(fp) == -1)
   {
   cfPS(cf_error,CF_FAIL,"fclose",pp,a,"Unable to close file while writing","fclose");
   return false;
   }
 
cfPS(cf_inform,CF_CHG,"",pp,a,"Edited file %s \n",file); 

if (rename(file,backup) == -1)
   {
   cfPS(cf_error,CF_FAIL,"rename",pp,a,"Can't rename %s to %s - so promised edits could not be moved into place\n",file,backup);
   return false;
   }

if (a.edits.backup != cfa_nobackup)
   {
   if (ArchiveToRepository(backup,a,pp))
      {
      unlink(backup);
      }
   }

if (rename(new,file) == -1)
   {
   cfPS(cf_error,CF_FAIL,"rename",pp,a,"Can't rename %s to %s - so promised edits could not be moved into place\n",new,file);
   return false;
   }       

mask = umask(0); 
chmod(file,statbuf.st_mode);                    /* Restore file permissions etc */
chown(file,statbuf.st_uid,statbuf.st_gid);
umask(mask); 

#ifdef WITH_SELINUX
if (selinux_enabled)
   {
   /* restore file context */
   setfilecon(file,scontext);
   }
#endif

return true;
}
