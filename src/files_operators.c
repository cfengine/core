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
/* File: files_operators.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern int CFA_MAXTHREADS;
extern struct cfagent_connection *COMS;

/*****************************************************************************/

int VerifyFileLeaf(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp)

{
/* Here we can assume that we are in the parent directory of the leaf */

if (!SelectLeaf(path,sb,attr,pp))
   {
   Debug("Skipping non-selected file %s\n",path);
   return false;
   }

Debug(" -> Handling file existence constraints on %s\n",path);

/* We still need to augment the scope of context "this" for commands */

NewScalar("this","promiser",path,cf_str); // Parameters may only be scalars

if (attr.transformer != NULL)
   {
   if (!TransformFile(path,attr,pp))
      {
      /* NOP? */
      }
   }
else
   {
   if (attr.haverename)
      {
      VerifyName(path,sb,attr,pp);
      }
   
   if (attr.havedelete)
      {
      VerifyDelete(path,sb,attr,pp);
      }
   
   if (attr.touch)
      {
      TouchFile(path,sb,attr,pp); // intrinsically non-convergent op
      }
   }

if (attr.haveperms || attr.havechange)
   {
   VerifyFileAttributes(path,sb,attr,pp);
   }

DeleteScalar("this","promiser");
return true;
}

/*****************************************************************************/

int CreateFile(char *file,struct Promise *pp,struct Attributes attr)

{ int fd;
 
 /* If name ends in /. then this is a directory */

// attr.move_obstructions for MakeParentDirectory
 
if (strcmp("/.",file+strlen(file)-2) == 0)
   {
   if (!DONTDO)
      {
      if (!MakeParentDirectory(file,attr.move_obstructions))
         {
         return false;
         }
      }
   }
else
   {
   if (!DONTDO)
      {
      mode_t filemode = 0600;  /* Decide the mode for filecreation */

      if (GetConstraint("mode",pp->conlist,CF_SCALAR) == NULL)
         {
         /* Relying on umask is risky */
         filemode = 0600;
         Verbose("No mode was set, choose plain file default %o\n",filemode);
         }
      else
         {
         filemode = attr.perms.plus & ~(attr.perms.minus);
         }

      MakeParentDirectory(file,attr.move_obstructions);
      
      if ((fd = creat(file,filemode)) == -1)
         { 
         cfPS(cf_inform,CF_FAIL,"creat",pp,attr,"Error creating file %s, mode = %o\n",file,filemode);
         return false;
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,attr," -> Created file %s, mode = %o\n",file,filemode);
         close(fd);
         }
      }
   }

return true;
}

/*****************************************************************************/

int ScheduleCopyOperation(char *destination,struct Attributes attr,struct Promise *pp)

{ struct cfagent_connection *conn;

 Verbose(" -> Copy file %s from %s check\n",destination,pp->promiser);  
  
if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item,"localhost") == 0)
   {
   conn = NULL;
   pp->this_server = strdup("localhost");
   }
else
   {
   conn = NewServerConnection(attr,pp);

   if (conn == NULL)
      {
      CfOut(cf_inform,"","No suitable server responded to hail\n");
      PromiseRef(cf_inform,pp);
      ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
      return false;
      }
   }

pp->conn = conn; /* for ease of access */
pp->cache = NULL;

CopyFileSources(destination,attr,pp);

return true;
}

/*****************************************************************************/

int ScheduleLinkChildrenOperation(char *destination,struct Attributes attr,struct Promise *pp)

{ DIR *dirh;
  struct dirent *dirp;
  char promiserpath[CF_BUFSIZE],sourcepath[CF_BUFSIZE];
  struct stat lsb;

if (lstat(destination,&lsb) != -1)
   {
   if (attr.move_obstructions && S_ISLNK(lsb.st_mode))
      {
      unlink(destination);
      }
   else if (!S_ISDIR(lsb.st_mode))
      {
      CfOut(cf_error,"","Cannot promise to link multiple files to children of %s as it is not a directory!",destination);
      return false;
      }
   }

snprintf(promiserpath,CF_BUFSIZE,"%s/.",destination);

if (!CreateFile(promiserpath,pp,attr))
   {
   CfOut(cf_error,"","Cannot promise to link multiple files to children of %s as it is not a directory!",destination);
   return false;
   }
  
if ((dirh = opendir(attr.link.source)) == NULL)
   {
   cfPS(cf_error,CF_FAIL,"opendir",pp,attr,"Can't open source of children to link %s\n",attr.link.source);
   return false;
   }

for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
   {
   if (!SensibleFile(dirp->d_name,attr.link.source,NULL))
      {
      continue;
      }

   /* Assemble pathnames */

   strncpy(promiserpath,destination,CF_BUFSIZE-1);
   AddSlash(promiserpath);

   if (!JoinPath(promiserpath,dirp->d_name))
      {
      cfPS(cf_error,CF_INTERPT,"",pp,attr,"Can't construct filename which verifying child links\n");
      closedir(dirh);
      return false;
      }
   
   strncpy(sourcepath,attr.link.source,CF_BUFSIZE-1);
   AddSlash(sourcepath);
   
   if (!JoinPath(sourcepath,dirp->d_name))
      {
      cfPS(cf_error,CF_INTERPT,"",pp,attr,"Can't construct filename while verifying child links\n");
      closedir(dirh);
      return false;
      }

   if ((lstat(promiserpath,&lsb) != -1) && !S_ISLNK(lsb.st_mode))
      {
      if (attr.link.when_linking_children == cfa_override)
         {
         attr.move_obstructions = true;
         }
      else
         {
         CfOut(cf_verbose,"","Have promised not to disturb %s\'s existing content",promiserpath);
         continue;
         }
      }
   
   ScheduleLinkOperation(promiserpath,sourcepath,attr,pp);
   }

closedir(dirh);
return true;
}

/*****************************************************************************/

int ScheduleLinkOperation(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char *lastnode;
 
lastnode = ReadLastNode(destination);

if (MatchRlistItem(attr.link.copy_patterns,lastnode))
   {
   CfOut(cf_verbose,"","Link %s matches copy_patterns\n",destination);
   VerifyCopy(attr.link.source,destination,attr,pp);
   return true;
   }

switch (attr.link.link_type)
   {
   case cfa_symlink:
       VerifyLink(destination,source,attr,pp);
       break;
   case cfa_hardlink:
       VerifyHardLink(destination,source,attr,pp);
       break;
   case cfa_relative:
       VerifyRelativeLink(destination,source,attr,pp);
       break;
   case cfa_absolute:
       VerifyAbsoluteLink(destination,source,attr,pp);
       break;
   default:
       CfOut(cf_error,"","Unknown link type - should not happen.\n");
       break;
   }

return true;
}

/*****************************************************************************/

int ScheduleEditOperation(char *filename,struct Attributes a,struct Promise *pp)

{ struct Bundle *bp;
  void *vp;
  struct FnCall *fp;
  char *edit_bundle_name = NULL;
  struct Rlist *params;
  int retval = false;
  struct CfLock thislock;
  char lockname[CF_BUFSIZE],context_save[CF_MAXVARSIZE];

snprintf(lockname,CF_BUFSIZE-1,"edit-%s",filename);
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return false;
   }
 
pp->edcontext = NewEditContext(filename,a,pp);

if (pp->edcontext == NULL)
   {
   CfOut(cf_error,"","File %s was marked for editing but could not be opened\n",filename);
   FinishEditContext(pp->edcontext,a,pp);
   YieldCurrentLock(thislock);
   return false;
   }

if (a.haveeditline)
   {
   if (vp = GetConstraint("edit_line",pp->conlist,CF_FNCALL))
      {
      fp = (struct FnCall *)vp;
      edit_bundle_name = fp->name;
      params = fp->args;;
      }
   else if (vp = GetConstraint("edit_line",pp->conlist,CF_SCALAR))
      {
      edit_bundle_name = (char *)vp;
      params = NULL;
      }
   else
      {
      FinishEditContext(pp->edcontext,a,pp);
      YieldCurrentLock(thislock);
      return false;
      }
   
   Verbose(" -> Handling file edits in edit_line bundle %s\n",edit_bundle_name);

   // add current filename to context - already there?

   if (bp = GetBundle(edit_bundle_name,"edit_line"))
      {
      BannerSubBundle(bp,params);
      NewScope(bp->name);
      AugmentScope(bp->name,bp->args,params);
      retval = ScheduleEditLineOperations(filename,bp,a,pp);
      DeleteFromScope(bp->name,bp->args);
      }
   }

FinishEditContext(pp->edcontext,a,pp);
YieldCurrentLock(thislock);
return retval;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void VerifyFileAttributes(char *file,struct stat *dstat,struct Attributes attr,struct Promise *pp)

{ mode_t newperm = dstat->st_mode, maskvalue;

#if defined HAVE_CHFLAGS
  u_long newflags;
#endif

Debug("VerifyFileAttributes(%s)\n",file);
  
maskvalue = umask(0);                 /* This makes the DEFAULT modes absolute */
 
newperm = (dstat->st_mode & 07777);
newperm |= attr.perms.plus;
newperm &= ~(attr.perms.minus);

 /* directories must have x set if r set, regardless  */

if (S_ISDIR(dstat->st_mode))  
   {
   if (!attr.perms.rxdirs)
      {
      Debug("Directory...fixing x bits\n");
      
      if (newperm & S_IRUSR)
         {
         newperm  |= S_IXUSR;
         }
      
      if (newperm & S_IRGRP)
         {
         newperm |= S_IXGRP;
         }
      
      if (newperm & S_IROTH)
         {
         newperm |= S_IXOTH;
         }
      }
   else
      {
      Verbose("NB: rxdirs is set to false - x for r bits not checked\n");
      }
   }

VerifySetUidGid(file,dstat,newperm,pp,attr);

#ifdef DARWIN
if (VerifyFinderType(file,dstat,attr,pp))
   {
   /* nop */
   }
#endif

if (VerifyOwner(file,pp,attr,dstat))
   {
   /* nop */
   }

if (attr.havechange && S_ISREG(dstat->st_mode))
   {
   VerifyFileIntegrity(file,pp,attr);
   }

if (S_ISLNK(dstat->st_mode))             /* No point in checking permission on a link */
   {
   KillGhostLink(file,attr,pp);
   umask(maskvalue);
   return;
   }

/*
  if (CheckACLs(file,action,ptr->acl_aliases))
   {
   if (ptr != NULL)
      {
      AddMultipleClasses(ptr->defines);
      }
   }
*/

if ((newperm & 07777) == (dstat->st_mode & 07777))            /* file okay */
   {
   Debug("File okay, newperm = %o, stat = %o\n",(newperm & 07777),(dstat->st_mode & 07777));
   cfPS(cf_verbose,CF_NOP,"",pp,attr," -> File permissions on %s as promised\n",file);
   }
else
   {
   Debug("Trying to fix mode...newperm = %o, stat = %o\n",(newperm & 07777),(dstat->st_mode & 07777));
   
   switch (attr.transaction.action)
      {
      case cfa_warn:
          
          cfPS(cf_error,CF_WARN,"",pp,attr,"%s has permission %o - [should be %o]\n",file,dstat->st_mode & 07777,newperm & 07777);
          break;
          
      case cfa_fix:
          
          if (!DONTDO)
             {
             if (chmod(file,newperm & 07777) == -1)
                {
                CfOut(cf_error,"chmod","chmod failed on %s\n",file);
                break;
                }
             }
          
          cfPS(cf_inform,CF_CHG,"",pp,attr,"Object %s had permission %o, changed it to %o\n",file,dstat->st_mode & 07777,newperm & 07777);
          break;
          
      default:
          FatalError("cfengine: internal error VerifyFileAttributes(): illegal file action\n");
      }
   }
 
#if defined HAVE_CHFLAGS  /* BSD special flags */
newflags = (dstat->st_flags & CHFLAGS_MASK) ;
newperm |= attr.perms.plus_flags;
newperm &= ~(attr.perms.minus_flags);

if ((newflags & CHFLAGS_MASK) == (dstat->st_flags & CHFLAGS_MASK))    /* file okay */
   {
   Debug("BSD File okay, flags = %o, current = %o\n",(newflags & CHFLAGS_MASK),(dstat->st_flags & CHFLAGS_MASK));
   }
else
   {
   Debug("BSD Fixing %s, newflags = %o, flags = %o\n",file,(newflags & CHFLAGS_MASK),(dstat->st_flags & CHFLAGS_MASK));
   
   switch (attr.transaction.action)
      {
      case cfa_warn:

          cfPS(cf_error,CF_WARN,"",pp,attr,"%s has flags %o - [should be %o]\n",file,dstat->st_mode & CHFLAGS_MASK,newflags & CHFLAGS_MASK);
          break;
          
      case cfa_fix:

          if (! DONTDO)
             {
             if (chflags (file,newflags & CHFLAGS_MASK) == -1)
                {
                cfPS(cf_error,CF_DENIED,"chflags",pp,attr,"chflags failed on %s\n",file);
                break;
                }
             else
                {
                cfPS(cf_inform,CF_CHG,"",pp,attr,"%s had flags %o, changed it to %o\n",file,dstat->st_flags & CHFLAGS_MASK,newflags & CHFLAGS_MASK);
                }
             }
          
          break;
          
      default:
          FatalError("cfengine: internal error VerifyFileAttributes() illegal file action\n");
      }
   }
#endif

if (attr.touch)
   {
   if (utime(file,NULL) == -1)
      {
      cfPS(cf_inform,CF_DENIED,"utime",pp,attr,"Touching file %s failed",file);
      }
   else
      {
      cfPS(cf_inform,CF_CHG,"",pp,attr,"Touching file %s",file);
      }
   }

umask(maskvalue);  
Debug("CheckExistingFile(Done)\n"); 
}

/*********************************************************************/

void VerifyCopiedFileAttributes(char *file,struct stat *dstat,struct stat *sstat,struct Attributes attr,struct Promise *pp)

{ mode_t newplus,newminus;
  uid_t save_uid;
  gid_t save_gid;

// How do we get the default attr?
  
Debug("VerifyCopiedFile(%s,+%o,-%o)\n",file,attr.perms.plus,attr.perms.minus); 

save_uid = (attr.perms.owners)->uid;
save_gid = (attr.perms.groups)->gid;

if ((attr.perms.owners)->uid == CF_SAME_OWNER)          /* Preserve uid and gid  */
   {
   (attr.perms.owners)->uid = sstat->st_uid;
   }

if ((attr.perms.groups)->gid == CF_SAME_GROUP)
   {
   (attr.perms.groups)->gid = sstat->st_gid;
   }

// Will this preserve if no mode set?

newplus = (sstat->st_mode & 07777) | attr.perms.plus;
newminus = ~(newplus & ~(attr.perms.minus)) & 07777;

attr.perms.plus = newplus;
attr.perms.minus = newminus;

VerifyFileAttributes(file,dstat,attr,pp);

(attr.perms.owners)->uid = save_uid;
(attr.perms.groups)->gid = save_gid;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void VerifySetUidGid(char *file,struct stat *dstat,mode_t newperm,struct Promise *pp,struct Attributes attr)

{ int amroot = true;

if (!IsPrivileged())                            
   {
   amroot = false;
   }

if (dstat->st_uid == 0 && (dstat->st_mode & S_ISUID))
   {
   if (newperm & S_ISUID)
      {
      if (!IsItemIn(VSETUIDLIST,file))
         {
         if (amroot)
            {
            cfPS(cf_inform,CF_WARN,"",pp,attr,"NEW SETUID root PROGRAM %s\n",file);
            }
         
         PrependItem(&VSETUIDLIST,file,NULL);
         }
      }
   else
      {
      switch (attr.transaction.action)
         {
         case cfa_fix:

             cfPS(cf_inform,CF_CHG,"",pp,attr,"Removing setuid (root) flag from %s...\n\n",file);
             break;

         case cfa_warn:

             if (amroot)
                {
                cfPS(cf_error,CF_WARN,"",pp,attr,"WARNING setuid (root) flag on %s...\n\n",file);
                }
             break;             
         }
      }
   }
 
if (dstat->st_uid == 0 && (dstat->st_mode & S_ISGID))
   {
   if (newperm & S_ISGID)
      {
      if (!IsItemIn(VSETUIDLIST,file))
         {
         if (S_ISDIR(dstat->st_mode))
            {
            /* setgid directory */
            }
         else
            {
            if (amroot)
               {
               cfPS(cf_error,CF_WARN,"",pp,attr,"NEW SETGID root PROGRAM %s\n",file);
               }

            PrependItem(&VSETUIDLIST,file,NULL);
            }
         }
      }
   else
      {
      switch (attr.transaction.action)
         {
         case cfa_fix:

             cfPS(cf_inform,CF_CHG,"",pp,attr,"Removing setgid (root) flag from %s...\n\n",file);
             break;

         case cfa_warn:

             cfPS(cf_inform,CF_WARN,"",pp,attr,"WARNING setgid (root) flag on %s...\n\n",file);
             break;
             
         default:
             break;
         }
      }
   } 
}

/*****************************************************************************/

int MoveObstruction(char *from,struct Attributes attr,struct Promise *pp)

{ struct stat sb;
  char stamp[CF_BUFSIZE],saved[CF_BUFSIZE];
  time_t now_stamp = time((time_t *)NULL);

if (lstat(from,&sb) == 0)
   {
   if (!attr.move_obstructions)
      {
      cfPS(cf_verbose,CF_FAIL,"",pp,attr,"Object %s exists and is obstructing our promise\n",from);
      return false;
      }
   
   if (!S_ISDIR(sb.st_mode))
      {
      if (DONTDO)
         {
         return false;
         }

      saved[0] = '\0';
      strcpy(saved,from);

      if (attr.copy.backup == cfa_timestamp || attr.edits.backup == cfa_timestamp)
         {
         sprintf(stamp, "_%d_%s",CFSTARTTIME,CanonifyName(ctime(&now_stamp)));
         strcat(saved,stamp);
         }
      
      strcat(saved,CF_SAVED);

      cfPS(cf_verbose,CF_CHG,"",pp,attr,"Moving file object %s to %s\n",from,saved);

      if (rename(from,saved) == -1)
         {
         cfPS(cf_error,CF_FAIL,"rename",pp,attr,"Can't rename %s to %s\n",from,saved);
         return false;
         }

      if (ArchiveToRepository(saved,attr,pp))
         {
         unlink(saved);
         }

      return true;
      }
   
   if (S_ISDIR(sb.st_mode) && attr.link.when_no_file == cfa_force)
      {
      cfPS(cf_verbose,CF_CHG,"",pp,attr,"Moving directory %s to %s%s\n",from,from,CF_SAVED);
      
      if (DONTDO)
         {
         return false;
         }
      
      saved[0] = '\0';
      strcpy(saved,from);
      
      sprintf(stamp, "_%d_%s", CFSTARTTIME, CanonifyName(ctime(&now_stamp)));
      strcat(saved,stamp);
      strcat(saved,CF_SAVED);
      strcat(saved,".dir");
      
      if (stat(saved,&sb) != -1)
         {
         cfPS(cf_error,CF_FAIL,"",pp,attr,"Couldn't save directory %s, since %s exists already\n",from,saved);
         CfOut(cf_error,"","Unable to force link to existing directory %s\n",from);
         return false;
         }
      
      if (rename(from,saved) == -1)
         {
         cfPS(cf_error,CF_FAIL,"rename",pp,attr,"Can't rename %s to %s\n",from,saved);
         return false;
         }
      }
   }

return true;
}

/*********************************************************************/

#ifdef DARWIN

int VerifyFinderType(char *file,struct stat *statbuf,struct Attributes a,struct Promise *pp)

{ /* Code modeled after hfstar's extract.c */
 typedef struct t_fndrinfo
    {
    long fdType;
    long fdCreator;
    short fdFlags;
    short fdLocationV;
    short fdLocationH;
    short fdFldr;
    short fdIconID;
    short fdUnused[3];
    char fdScript;
    char fdXFlags;
    short fdComment;
    long fdPutAway;
    }
 FInfo;
 
 struct attrlist attrs;
 struct
    {
    long ssize;
    struct timespec created;
    struct timespec modified;
    struct timespec changed;
    struct timespec backup;
    FInfo fi;
    }
 fndrInfo; 
 int retval;
 
Debug("VerifyFinderType of %s for %s\n", file,a.perms.findertype);
 
if (strncmp(a.perms.findertype,"*",CF_BUFSIZE) == 0 || strncmp(a.perms.findertype,"",CF_BUFSIZE) == 0)
   {
   return 0;
   }

attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
attrs.reserved = 0;
attrs.commonattr = ATTR_CMN_CRTIME | ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | ATTR_CMN_BKUPTIME | ATTR_CMN_FNDRINFO;
attrs.volattr = 0;
attrs.dirattr = 0;
attrs.fileattr = 0;
attrs.forkattr = 0;

memset(&fndrInfo, 0, sizeof(fndrInfo));

getattrlist(file, &attrs, &fndrInfo, sizeof(fndrInfo),0);

if (fndrInfo.fi.fdType != *(long *)a.perms.findertype)
   {
   fndrInfo.fi.fdType = *(long *)a.perms.findertype;
   
   switch (a.transaction.action)
      {       
      case cfa_fix:
          
          if (DONTDO)
             {
             CfOut(cf_inform,"","Promised to set Finder Type code of %s to %s\n",file,a.perms.findertype);
             return 0;
             }
          
          /* setattrlist does not take back in the long ssize */        
          retval = setattrlist(file, &attrs, &fndrInfo.created, 4*sizeof(struct timespec) + sizeof(FInfo), 0);
          
          Debug("CheckFinderType setattrlist returned %d\n", retval);
          
          if (retval >= 0)
             {
             cfPS(cf_inform,CF_CHG,"",pp,a,"Setting Finder Type code of %s to %s\n",file,a.perms.findertype);
             }
          else
             {
             cfPS(cf_error,CF_FAIL,"",pp,a,"Setting Finder Type code of %s to %s failed!!\n",file,a.perms.findertype);
             }
          
          return retval;
          
      case cfa_warn:
          CfOut(cf_error,"","Darwin FinderType does not match -- not fixing.\n");
          return 0;

      default:
          return 0;
      }
   }
else
   {
   cfPS(cf_verbose,CF_NOP,"",pp,a,"Finder Type code of %s to %s is as promised\n",file,a.perms.findertype);
   return 0;
   }
}

#endif

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void VerifyName(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp)

{ mode_t newperm;
  struct stat dsb;
 
if (lstat(path,&dsb) == -1)
   {
   cfPS(cf_inform,CF_NOP,"",pp,attr,"File object named %s is not there (promise kept)",path);
   return;
   }
else
   {
   CfOut(cf_inform,""," !! Warning - file object %s exists, contrary to promise\n",path);
   }

if (S_ISLNK(dsb.st_mode))
   {
   if (attr.rename.disable)
      {
      if (!DONTDO)
         {
         if (unlink(path) == -1)
            {
            cfPS(cf_error,CF_FAIL,"unlink",pp,attr," !! Unable to unlink %s\n",path);
            }
         else
            {
            cfPS(cf_inform,CF_CHG,"",pp,attr," -> Disabling symbolic link %s by deleting it\n",path);
            }
         }
      else
         {
         CfOut(cf_inform,""," * Need to disable link %s to keep promise\n",path);
         }
      
      return;
      }
   }

/* Normal disable - has priority */

if (attr.rename.disable)
   {
   char newname[CF_BUFSIZE];
   
   if (attr.rename.newname && strlen(attr.rename.newname) > 0)
      {
      if (IsAbsPath(attr.rename.newname))
         {
         strncpy(path,attr.rename.newname,CF_BUFSIZE-1);
         }
      else
         {
         strcpy(newname,path);
         ChopLastNode(newname);
         if (!JoinPath(newname,attr.rename.newname))
            {
            ReleaseCurrentLock();
            return;
            }
         }
      }
   else
      {
      strcpy(newname,path);

      if (attr.rename.disable_suffix)
         {
         if (!JoinSuffix(newname,attr.rename.disable_suffix))
            {
            return;
            }
         }
      else
         {
         if (!JoinSuffix(newname,".cfdisabled"))
            {
            return;
            }
         }
      }
   
   if ((attr.rename.plus != CF_SAMEMODE) && (attr.rename.minus != CF_SAMEMODE))
      {
      newperm = (sb->st_mode & 07777);
      newperm |= attr.perms.plus;
      newperm &= ~(attr.perms.minus);
      }
   else
      {
      newperm = (mode_t)0600;
      }
   
   if (DONTDO)
      {
      CfOut(cf_inform,""," -> File %s should be renamed to %s to keep promise\n",path,newname);
      return;
      }
   else
      {
      chmod(path,newperm);
      cfPS(cf_inform,CF_CHG,"",pp,attr," -> Disabling/renaming file %s to %s with mode %o\n",path,newname,newperm);

      if (!IsItemIn(VREPOSLIST,newname))
         {
         if (rename(path,newname) == -1)
            {
            cfPS(cf_error,CF_FAIL,"rename",pp,attr,"Error occurred while renaming %s\n",path);
            return;
            }
         
         if (ArchiveToRepository(newname,attr,pp))
            {
            unlink(newname);
            }
         }
      else
         {
         cfPS(cf_error,CF_WARN,"",pp,attr," !! Disable required twice? Would overwrite saved copy - changing permissions only",path);
         }        
      }

   return;
   }

if (attr.rename.rotate == 0)
   {   
   if (! DONTDO)
      {
      TruncateFile(path);
      cfPS(cf_inform,CF_CHG,"",pp,attr," -> Truncating (emptying) %s\n",path);
      }
   else
      {
      CfOut(cf_error,""," * File %s needs emptying",path);
      }
   return;
   }


if (attr.rename.rotate > 0)
   {
   if (!DONTDO)
      {
      RotateFiles(path,attr.rename.rotate);
      cfPS(cf_inform,CF_CHG,"",pp,attr," -> Rotating files %s in %d fifo\n",path,attr.rename.rotate);
      }
   else        
      {
      CfOut(cf_error,""," * File %s needs rotating",path);
      }

   return;
   }
}

/*********************************************************************/

void VerifyDelete(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp)

{ char *lastnode = ReadLastNode(path);

Debug(" -> Verifying file deletions for %s\n",path);
 
if (DONTDO)
   {
   CfOut(cf_inform,"","Promise requires deletion of file object %s\n",path);
   }
else
   {
   if (!S_ISDIR(sb->st_mode))
      {
      if (unlink(lastnode) == -1)
         {
         cfPS(cf_verbose,CF_FAIL,"unlink",pp,attr,"Couldn't unlink %s tidying\n",path);
         }
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,attr," -> Deleted file %s\n",path);
         }      
      }
   else
      {
      if (!attr.delete.rmdirs)
         {
         CfOut(cf_inform,"unlink","Keeping directory %s\n",path);
         return;
         }
      
      if (rmdir(lastnode) == -1)
         {
         cfPS(cf_inform,CF_FAIL,"unlink",pp,attr,"Delete directory %s failed\n",path);
         }            
      else
         {
         cfPS(cf_inform,CF_CHG,"",pp,attr,"Deleted directory %s\n",path);
         }
      }
   }   
}

/*********************************************************************/

void TouchFile(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp)
{
if (! DONTDO)
   {
   if (utime(path,NULL) != -1)
      {
      cfPS(cf_inform,CF_CHG,"",pp,attr,"Touched (updated time stamps) %s\n",path);
      }
   else
      {
      cfPS(cf_inform,CF_FAIL,"utime",pp,attr,"Touch %s failed to update timestamps\n",path);
      }
   }
else
   {
   CfOut(cf_error,"","Need to touch (update timestamps) %s\n",path);
   }
}

/*********************************************************************/

void VerifyFileIntegrity(char *file,struct Promise *pp,struct Attributes attr)

{ unsigned char digest1[EVP_MAX_MD_SIZE+1];
  unsigned char digest2[EVP_MAX_MD_SIZE+1];
  int changed = false;
  
Debug("Checking checksum/hash integrity of %s\n",file);

memset(digest1,0,EVP_MAX_MD_SIZE+1);
memset(digest2,0,EVP_MAX_MD_SIZE+1);

if (attr.change.hash == cf_besthash)
   {
   if (!DONTDO)
      {
      HashFile(file,digest1,cf_md5);
      HashFile(file,digest2,cf_sha1);
      
      if (FileHashChanged(file,digest1,cf_error,cf_md5,attr,pp)
          || FileHashChanged(file,digest2,cf_error,cf_sha1,attr,pp))
         {
         changed = true;
         }
      }
   }
else
   {
   if (!DONTDO)
      {
      HashFile(file,digest1,attr.change.hash);
   
      if (FileHashChanged(file,digest1,cf_error,attr.change.hash,attr,pp))
         {
         changed = true;
         }
      }
   }

if (changed)
   {
   NewPersistentContext("checksum_alerts",CF_PERSISTENCE,cfpreserve);
   LogHashChange(file);
   }
}

/*********************************************************************/

int VerifyOwner(char *file,struct Promise *pp,struct Attributes attr,struct stat *sb)

{ struct passwd *pw;
  struct group *gp;
  struct UidList *ulp, *unknownulp;
  struct GidList *glp, *unknownglp;
  short uidmatch = false, gidmatch = false;
  uid_t uid = CF_SAME_OWNER; 
  gid_t gid = CF_SAME_GROUP;

Debug("VerifyOwner: %d\n",sb->st_uid);
  
for (ulp = attr.perms.owners; ulp != NULL; ulp=ulp->next)
   {
   if (ulp->uid == CF_UNKNOWN_OWNER)
      {
      unknownulp = MakeUidList(ulp->uidname); /* Will only match one */
      
      if (unknownulp != NULL && sb->st_uid == unknownulp->uid)
         {
         uid = unknownulp->uid;
         uidmatch = true;
         break;
         }
      }
   
   if (ulp->uid == CF_SAME_OWNER || sb->st_uid == ulp->uid)   /* "same" matches anything */
      {
      uid = ulp->uid;
      uidmatch = true;
      break;
      }
   }
 
for (glp = attr.perms.groups; glp != NULL; glp=glp->next)
   {
   if (glp->gid == CF_UNKNOWN_GROUP) /* means not found while parsing */
      {
      unknownglp = MakeGidList(glp->gidname); /* Will only match one */
      
      if (unknownglp != NULL && sb->st_gid == unknownglp->gid)
         {
         gid = unknownglp->gid;
         gidmatch = true;
         break;
         }
      }

   if (glp->gid == CF_SAME_GROUP || sb->st_gid == glp->gid)  /* "same" matches anything */
      {
      gid = glp->gid;
      gidmatch = true;
      break;
      }
   }
 
if (uidmatch && gidmatch)
   {
   return false;
   }
else
   {
   if (! uidmatch)
      {
      for (ulp = attr.perms.owners; ulp != NULL; ulp=ulp->next)
         {
         if (attr.perms.owners->uid != CF_UNKNOWN_OWNER)
            {
            uid = attr.perms.owners->uid;    /* default is first (not unknown) item in list */
            break;
            }
         }
      }
   
   if (! gidmatch)
      {
      for (glp = attr.perms.groups; glp != NULL; glp=glp->next)
         {
         if (attr.perms.groups->gid != CF_UNKNOWN_GROUP)
            {
            gid = attr.perms.groups->gid;    /* default is first (not unknown) item in list */
            break;
            }
         }
      }
   
   switch (attr.transaction.action)
      {
      case cfa_fix:

          if (uid == CF_SAME_OWNER && gid == CF_SAME_GROUP)
             {
             Verbose("%s:   touching %s\n",VPREFIX,file);
             }
          else
             {
             if (uid != CF_SAME_OWNER)
                {
                Debug("(Change owner to uid %d if possible)\n",uid);
                }
             
             if (gid != CF_SAME_GROUP)
                {
                Debug("Change group to gid %d if possible)\n",gid);
                }
             }
          
          if (!DONTDO && S_ISLNK(sb->st_mode))
             {
#ifdef HAVE_LCHOWN
             Debug("Using LCHOWN function\n");
             if (lchown(file,uid,gid) == -1)
                {
                CfOut(cf_inform,"lchown","Cannot set ownership on link %s!\n",file);
                }
             else
                {
                return true;
                }
#endif
             }
          else if (!DONTDO)
             {
             if (!uidmatch)
                {
                cfPS(cf_inform,CF_CHG,"",pp,attr,"Owner of %s was %d, setting to %d",file,sb->st_uid,uid);
                }
             
             if (!gidmatch)
                {
                cfPS(cf_inform,CF_CHG,"",pp,attr,"Group of %s was %d, setting to %d",file,sb->st_gid,gid);
                }
             
             if (!S_ISLNK(sb->st_mode))
                {
                if (chown(file,uid,gid) == -1)
                   {
                   cfPS(cf_inform,CF_DENIED,"chown",pp,attr,"Cannot set ownership on file %s!\n",file);
                   }
                else
                   {
                   return true;
                   }
                }
             }
          break;
          
      case cfa_warn:
          
          if ((pw = getpwuid(sb->st_uid)) == NULL)
             {
             CfOut(cf_error,"","File %s is not owned by anybody in the passwd database\n",file);
             CfOut(cf_error,"","(uid = %d,gid = %d)\n",sb->st_uid,sb->st_gid);
             break;
             }
          
          if ((gp = getgrgid(sb->st_gid)) == NULL)
             {
             cfPS(cf_error,CF_WARN,"",pp,attr,"File %s is not owned by any group in group database\n",file);
             break;
             }
          
          cfPS(cf_error,CF_WARN,"",pp,attr,"File %s is owned by [%s], group [%s]\n",file,pw->pw_name,gp->gr_name);
          break;
      }
   }

return false; 
}


/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int TransformFile(char *file,struct Attributes attr,struct Promise *pp)

{ char comm[CF_EXPANDSIZE],line[CF_BUFSIZE];
  FILE *pop;

if (attr.transformer == NULL)
   {
   return false;
   }

ExpandScalar(attr.transformer,comm);
CfOut(cf_inform,"","Transforming: %s ",comm); 

if ((pop = cfpopen(comm,"r")) == NULL)
   {
   cfPS(cf_inform,CF_FAIL,"",pp,attr,"Transformer %s %s failed",attr.transformer,file);
   return false;
   }

while (!feof(pop))
   {
   ReadLine(line,CF_BUFSIZE,pop);
   CfOut(cf_inform,"",line);
   }

cfpclose(pop);

cfPS(cf_inform,CF_CHG,"",pp,attr,"Transformer %s => %s seemed ok",file,comm);
return true;
}

/*******************************************************************/

int MakeParentDirectory(char *parentandchild,int force)

{
/* For now, just transform to cf2 code */
 
if (force)
   {
   return MakeDirectoriesFor(parentandchild,'y');
   }
else
   {
   return MakeDirectoriesFor(parentandchild,'n');
   }
}


/*********************************************************************/

void LogHashChange(char *file)

{ FILE *fp;
 char fname[CF_BUFSIZE],timebuf[CF_MAXVARSIZE];
  time_t now = time(NULL);

/* This is inefficient but we don't want to lose any data */
  
snprintf(fname,CF_BUFSIZE,"%s/state/file_hash_event_history",CFWORKDIR);

if ((fp = fopen(fname,"a")) == NULL)
   {
   CfLog(cferror,"Could not write to the change log","");
   return;
   }

snprintf(timebuf,CF_MAXVARSIZE-1,"%s",ctime(&now));
Chop(timebuf);
fprintf(fp,"%s,%s\n",timebuf,file);

fclose(fp);
}
