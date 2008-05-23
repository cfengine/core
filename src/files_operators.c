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
/* File: files_operators.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern int CFA_MAXTHREADS;
extern struct cfagent_connection *COMS;

/*****************************************************************************/

int VerifyFileLeaf(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp)

{
/* Here we can assume that we are in the parent directory of the leaf */

if (!SelectLeaf(path,sb,attr,pp))
   {
   Debug("Skipping non-selected file %s\n",path);
   return false;
   }

Verbose(" -> Handling file existence constraints on %s\n",path);

/* We still need to augment the scope of context "this" for commands */

NewScalar("this","promiser",path,cf_str); // Parameters may only be scalars

//   AugmentScope("this",bp->args,params);


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

if (attr.haveperms)
   {
   VerifyFileAttributes(path,sb,attr,pp);
   }

DeleteScalar("this","promiser");   
return true;
}

/*****************************************************************************/

int CreateFile(char *file,struct Promise *pp,struct FileAttr attr)

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
      MakeParentDirectory(file,attr.move_obstructions);
      
      if ((fd = creat(file,attr.perms.plus)) == -1)
         { 
         snprintf(OUTPUT,CF_BUFSIZE*2,"Error creating file %s, mode = %o\n",file,attr.perms.plus);
         CfLog(cfinform,OUTPUT,"creat");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         return false;
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Creating file %s, mode = %o\n",file,attr.perms.plus);
         CfLog(cfinform,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
         close(fd);
         }
      }
   }

return true;
}

/*****************************************************************************/

int ScheduleCopyOperation(char *destination,struct FileAttr attr,struct Promise *pp)

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
      snprintf(OUTPUT,CF_BUFSIZE,"No suitable server responded to hail\n");
      CfLog(cfinform,OUTPUT,"");
      PromiseRef(cfinform,pp);
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

int ScheduleLinkOperation(char *destination,struct FileAttr attr,struct Promise *pp)

{
Verbose(" -> Handling linking\n");
return true;
}

/*****************************************************************************/

int ScheduleEditOperation(char *destination,struct FileAttr attr,struct Promise *pp)

{
Verbose(" -> Handling file edits (not yet implemented)\n");
return true;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void VerifyFileAttributes(char *file,struct stat *dstat,struct FileAttr attr,struct Promise *pp)

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
   if (attr.perms.rxdirs != 'n')
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
if (CheckFinderType(file,action,cf_findertype,dstat))
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
   snprintf(OUTPUT,CF_BUFSIZE*2,"  -> File permissions on %s as promised\n",file);
   CfLog(cfverbose,OUTPUT,"");
   ClassAuditLog(pp,attr,OUTPUT,CF_NOP);
   }
else
   {
   Debug("Trying to fix mode...newperm = %o, stat = %o\n",(newperm & 07777),(dstat->st_mode & 07777));
   
   switch (attr.transaction.action)
      {
      case cfa_warn:
          
          snprintf(OUTPUT,CF_BUFSIZE*2,"%s has permission %o\n",file,dstat->st_mode & 07777);
          CfLog(cferror,OUTPUT,"");
          ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
          snprintf(OUTPUT,CF_BUFSIZE*2,"[should be %o]\n",newperm & 07777);
          CfLog(cferror,OUTPUT,"");
          break;
          
      case cfa_fix:
          
          if (!DONTDO)
             {
             if (chmod(file,newperm & 07777) == -1)
                {
                snprintf(OUTPUT,CF_BUFSIZE*2,"chmod failed on %s\n",file);
                CfLog(cferror,OUTPUT,"chmod");
                break;
                }
             }
          
          snprintf(OUTPUT,CF_BUFSIZE*2,"Object %s had permission %o, changed it to %o\n",
                   file,dstat->st_mode & 07777,newperm & 07777);
          CfLog(cfinform,OUTPUT,"");
          ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
          break;
          
      default:
          FatalError("cfengine: internal error VerifyFileAttributes(): illegal file action\n");
      }
   }
 
#if defined HAVE_CHFLAGS  /* BSD special flags */
newflags = (dstat->st_flags & CHFLAGS_MASK) ;
newflags |= ptr->plus_flags;
newflags &= ~(ptr->minus_flags);
   
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

          snprintf(OUTPUT,CF_BUFSIZE*2,"%s has flags %o\n",file,dstat->st_mode & CHFLAGS_MASK);
          CfLog(cferror,OUTPUT,"");
          ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
          snprintf(OUTPUT,CF_BUFSIZE*2,"[should be %o]\n",newflags & CHFLAGS_MASK);
          CfLog(cferror,OUTPUT,"");
          break;
          
      case cfa_fix:

          if (! DONTDO)
             {
             if (chflags (file,newflags & CHFLAGS_MASK) == -1)
                {
                snprintf(OUTPUT,CF_BUFSIZE*2,"chflags failed on %s\n",file);
                CfLog(cferror,OUTPUT,"chflags");
                ClassAuditLog(pp,attr,OUTPUT,CF_DENIED);
                break;
                }
             else
                {
                snprintf(OUTPUT,CF_BUFSIZE*2,"%s had flags %o, changed it to %o\n",
                         file,dstat->st_flags & CHFLAGS_MASK,newflags & CHFLAGS_MASK);
                CfLog(cfinform,OUTPUT,"");
                ClassAuditLog(pp,attr,OUTPUT,CF_CHG);                
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
      snprintf(OUTPUT,CF_BUFSIZE,"Touching file %s",file);
      CfLog(cfinform,OUTPUT,"utime");
      ClassAuditLog(pp,attr,OUTPUT,CF_DENIED);
      }
   else
      {
      snprintf(OUTPUT,CF_BUFSIZE,"Touching file %s",file);
      CfLog(cfinform,OUTPUT,"");
      ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
      }
   }

umask(maskvalue);  
Debug("CheckExistingFile(Done)\n"); 
}

/*********************************************************************/

void VerifyCopiedFileAttributes(char *file,struct stat *dstat,struct stat *sstat,struct FileAttr attr,struct Promise *pp)

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

void VerifySetUidGid(char *file,struct stat *dstat,mode_t newperm,struct Promise *pp,struct FileAttr attr)

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
            snprintf(OUTPUT,CF_BUFSIZE*2,"NEW SETUID root PROGRAM %s\n",file);
            CfLog(cfinform,OUTPUT,"");
            ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
            }
         
         PrependItem(&VSETUIDLIST,file,NULL);
         }
      }
   else
      {
      switch (attr.transaction.action)
         {
         case cfa_fix:

             snprintf(OUTPUT,CF_BUFSIZE*2,"Removing setuid (root) flag from %s...\n\n",file);
             CfLog(cfinform,OUTPUT,"");
             ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
             break;

         case cfa_warn:

             if (amroot)
                {
                snprintf(OUTPUT,CF_BUFSIZE*2,"WARNING setuid (root) flag on %s...\n\n",file);
                CfLog(cferror,OUTPUT,"");
                ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
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
               snprintf(OUTPUT,CF_BUFSIZE*2,"NEW SETGID root PROGRAM %s\n",file);
               CfLog(cferror,OUTPUT,"");
               ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
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

             snprintf(OUTPUT,CF_BUFSIZE*2,"Removing setgid (root) flag from %s...\n\n",file);
             CfLog(cfinform,OUTPUT,"");
             ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
             break;

         case cfa_warn:

             snprintf(OUTPUT,CF_BUFSIZE*2,"WARNING setgid (root) flag on %s...\n\n",file);
             ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
             CfLog(cferror,OUTPUT,"");
             break;
             
         default:
             break;
         }
      }
   } 
}

/*****************************************************************************/

int MoveObstruction(char *from,struct FileAttr attr,struct Promise *pp)

{ struct stat sb;
  char stamp[CF_BUFSIZE],saved[CF_BUFSIZE];
  time_t now_stamp = time((time_t *)NULL);

if (lstat(from,&sb) == 0)
   {
   if (!attr.move_obstructions)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Object %s exists and is obstructing our promise\n",from);
      CfLog(cfverbose,OUTPUT,"");
      ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
      return false;
      }
   
   if (S_ISREG(sb.st_mode))
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Moving plain file %s to %s%s\n",from,from,CF_SAVED);
      CfLog(cfsilent,OUTPUT,"");

      if (DONTDO)
         {
         return false;
         }

      saved[0] = '\0';
      strcpy(saved,from);

      sprintf(stamp, "_%d_%s",CFSTARTTIME,CanonifyName(ctime(&now_stamp)));
      strcat(saved,stamp);      
      strcat(saved,CF_SAVED);

      if (rename(from,saved) == -1)
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Can't rename %s to %s\n",from,saved);
         CfLog(cferror,OUTPUT,"rename");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
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
      snprintf(OUTPUT,CF_BUFSIZE*2,"Moving directory %s to %s%s.dir\n",from,from,CF_SAVED);
      CfLog(cfsilent,OUTPUT,"");
      
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
         snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't save directory %s, since %s exists already\n",from,saved);
         CfLog(cferror,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         snprintf(OUTPUT,CF_BUFSIZE*2,"Unable to force link to existing directory %s\n",from);
         CfLog(cferror,OUTPUT,"");
         return false;
         }
      
      if (rename(from,saved) == -1)
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Can't rename %s to %s\n",from,saved);
         CfLog(cferror,OUTPUT,"rename");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         return false;
         }
      }
   }

return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void VerifyName(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp)

{ mode_t newperm;
  struct stat dsb;
 
if (lstat(path,&dsb) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"File object named %s is not there (promise kept)",path);
   CfLog(cfinform,OUTPUT,"");
   ClassAuditLog(pp,attr,OUTPUT,CF_NOP);
   return;
   }
else
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Warning - file object %s exists, contrary to promise\n",path);
   CfLog(cfinform,OUTPUT,"");
   }

if (S_ISLNK(dsb.st_mode))
   {
   if (attr.rename.disable)
      {
      if (!DONTDO)
         {
         if (unlink(path) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2," !! Unable to unlink %s\n",path);
            CfLog(cferror,OUTPUT,"unlink");
            ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
            }
         else
            {
            snprintf(OUTPUT,CF_BUFSIZE," -> Disabling symbolic link %s -> %s by deleting it\n",path,VBUFF);
            CfLog(cfinform,OUTPUT,"");
            ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
            }
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE," * Need to disable link %s -> %s to keep promise\n",path,VBUFF);
         CfLog(cfinform,OUTPUT,"");
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
      snprintf(OUTPUT,CF_BUFSIZE*2," -> File %s should be renamed to %s to keep promise\n",path,newname);
      CfLog(cfinform,OUTPUT,"");
      return;
      }
   else
      {
      chmod(path,newperm);

      snprintf(OUTPUT,CF_BUFSIZE*2," -> Disabling/renaming file %s to %s with mode %o\n",path,newname,newperm);
      CfLog(cfinform,OUTPUT,"");
      ClassAuditLog(pp,attr,OUTPUT,CF_CHG);

      if (!IsItemIn(VREPOSLIST,newname))
         {
         if (rename(path,newname) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Error occurred while renaming %s\n",path);
            CfLog(cferror,OUTPUT,"rename");
            ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
            return;
            }
         
         if (ArchiveToRepository(newname,attr,pp))
            {
            unlink(newname);
            }
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE," !! Disable required twice? Would overwrite saved copy - changing permissions only",path);
         CfLog(cferror,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
         }        
      }

   return;
   }

if (attr.rename.rotate == 0)
   {   
   if (! DONTDO)
      {
      TruncateFile(path);
      snprintf(OUTPUT,CF_BUFSIZE*2," -> Truncating (emptying) %s\n",path);
      CfLog(cfinform,OUTPUT,"");
      ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
      }
   else
      {
      snprintf(OUTPUT,CF_BUFSIZE," * File %s needs emptying",path);
      CfLog(cferror,OUTPUT,"");
      }
   return;
   }


if (attr.rename.rotate > 0)
   {
   if (!DONTDO)
      {
      RotateFiles(path,attr.rename.rotate);
      snprintf(OUTPUT,CF_BUFSIZE*2," -> Rotating files %s in %d fifo\n",path,attr.rename.rotate);
      CfLog(cfinform,OUTPUT,"");
      ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
      }
   else        
      {
      snprintf(OUTPUT,CF_BUFSIZE," * File %s needs rotating",path);
      CfLog(cferror,OUTPUT,"");
      }

   return;
   }
}

/*********************************************************************/

void VerifyDelete(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp)

{ char *lastnode = ReadLastNode(path);

Verbose(" -> Verifying file deletions for %s\n",path);
 
if (DONTDO)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Promise requires deletion of file object %s\n",path);
   CfLog(cfinform,OUTPUT,"");
   }
else
   {
   if (!S_ISDIR(sb->st_mode))
      {
      if (unlink(lastnode) == -1)
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't unlink %s tidying\n",path);
         CfLog(cfverbose,OUTPUT,"unlink");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE," -> Deleted file %s\n",path);
         CfLog(cfinform,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
         }      
      }
   else
      {
      if (!attr.delete.rmdirs)
         {
         snprintf(OUTPUT,CF_BUFSIZE,"Keeping directory %s\n",path);
         CfLog(cfinform,OUTPUT,"unlink");
         return;
         }
      
      if (rmdir(lastnode) == -1)
         {
         snprintf(OUTPUT,CF_BUFSIZE,"Delete directory %s failed\n",path);
         CfLog(cfinform,OUTPUT,"unlink");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         }            
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Deleted directory %s\n",path);
         CfLog(cfinform,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
         }
      }
   }   
}

/*********************************************************************/

void TouchFile(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp)
{
}

/*********************************************************************/

void VerifyFileIntegrity(char *file,struct Promise *pp,struct FileAttr attr)

{ unsigned char digest1[EVP_MAX_MD_SIZE+1];
  unsigned char digest2[EVP_MAX_MD_SIZE+1];
  
Debug("Checking checksum/hash integrity of %s\n",file);

memset(digest1,0,EVP_MAX_MD_SIZE+1);
memset(digest2,0,EVP_MAX_MD_SIZE+1);

if (attr.change.hash == cf_besthash)
   {
   if (!DONTDO)
      {
      HashFile(file,digest1,cf_md5);
      HashFile(file,digest2,cf_sha1);
      FileHashChanged(file,digest1,cferror,cf_md5,attr,pp);
      FileHashChanged(file,digest2,cferror,cf_sha1,attr,pp);
      }
   }
else
   {
   HashFile(file,digest1,attr.change.hash);
   
   if (!DONTDO)
      {
      if (FileHashChanged(file,digest1,cferror,attr.change.hash,attr,pp))
         {
         }
      }
   }
}

/*********************************************************************/

int VerifyOwner(char *file,struct Promise *pp,struct FileAttr attr,struct stat *sb)

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
                snprintf(OUTPUT,CF_BUFSIZE*2,"Cannot set ownership on link %s!\n",file);
                CfLog(cflogonly,OUTPUT,"lchown");
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
                snprintf(OUTPUT,CF_BUFSIZE,"Owner of %s was %d, setting to %d",file,sb->st_uid,uid);
                CfLog(cfinform,OUTPUT,"");
                ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
                }
             
             if (!gidmatch)
                {
                snprintf(OUTPUT,CF_BUFSIZE,"Group of %s was %d, setting to %d",file,sb->st_gid,gid);
                CfLog(cfinform,OUTPUT,"");
                ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
                }
             
             if (!S_ISLNK(sb->st_mode))
                {
                if (chown(file,uid,gid) == -1)
                   {
                   snprintf(OUTPUT,CF_BUFSIZE*2,"Cannot set ownership on file %s!\n",file);
                   CfLog(cflogonly,OUTPUT,"chown");
                   ClassAuditLog(pp,attr,OUTPUT,CF_DENIED);
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
             snprintf(OUTPUT,CF_BUFSIZE*2,"File %s is not owned by anybody in the passwd database\n",file);
             CfLog(cferror,OUTPUT,"");
             snprintf(OUTPUT,CF_BUFSIZE*2,"(uid = %d,gid = %d)\n",sb->st_uid,sb->st_gid);
             CfLog(cferror,OUTPUT,"");
             break;
             }
          
          if ((gp = getgrgid(sb->st_gid)) == NULL)
             {
             snprintf(OUTPUT,CF_BUFSIZE*2,"File %s is not owned by any group in group database\n",file);
             CfLog(cferror,OUTPUT,"");
             ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
             break;
             }
          
          snprintf(OUTPUT,CF_BUFSIZE*2,"File %s is owned by [%s], group [%s]\n",file,pw->pw_name,gp->gr_name);
          CfLog(cferror,OUTPUT,"");
          ClassAuditLog(pp,attr,OUTPUT,CF_WARN);
          break;
      }
   }

return false; 
}


/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int TransformFile(char *file,struct FileAttr attr,struct Promise *pp)

{ char comm[CF_EXPANDSIZE];
  FILE *pop;

if (attr.transformer == NULL)
   {
   return false;
   }

ExpandScalar(attr.transformer,comm);

snprintf(OUTPUT,CF_BUFSIZE*2,"Transforming: %s ",comm); 
CfLog(cfinform,OUTPUT,"");

if ((pop = cfpopen(comm,"r")) == NULL)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Transformer %s %s failed",attr.transformer,file);
   CfLog(cfinform,OUTPUT,"");
   ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
   return false;
   }

while (!feof(pop))
   {
   ReadLine(VBUFF,CF_BUFSIZE,pop);
   CfLog(cfinform,VBUFF,"");
   }

cfpclose(pop);

snprintf(OUTPUT,CF_BUFSIZE,"Transformer %s => %s seemed ok",file,comm);
CfLog(cfinform,OUTPUT,"");
ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
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
