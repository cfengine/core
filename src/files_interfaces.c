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
/* File: files_interfaces.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

/* File copying is a special case, particularly complex - cannot be integrated */

void SourceSearchAndCopy(char *from,char *to,int maxrecurse,struct FileAttr attr,struct Promise *pp)

{ struct stat sb, dsb;
  char newfrom[CF_BUFSIZE];
  char newto[CF_BUFSIZE];
  struct Item *namecache = NULL;
  struct cfdirent *dirp;
  CFDIR *dirh;

if (maxrecurse == 0)  /* reached depth limit */
   {
   Debug("MAXRECURSE ran out, quitting at level %s\n",from);
   return;
   }

Debug("RecursiveCopy(%s,lev=%d)\n",from,maxrecurse);

if (strlen(from) == 0)     /* Check for root dir */
   {
   from = "/";
   }

  /* Check that dest dir exists before starting */

strncpy(newto,to,CF_BUFSIZE-2);
AddSlash(newto);
strcat(newto,".");

if (attr.transaction.action != cfa_warn)
   {
   if (!MakeParentDirectory(newto,attr.move_obstructions))
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Unable to make directory for %s in file-copy %s to %s\n",newto,attr.copy.source,attr.copy.destination);
      CfLog(cferror,OUTPUT,"");
      return;
      }
   }

if ((dirh = cf_opendir(from,attr,pp)) == NULL)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"copy can't open directory [%s]\n",from);
   CfLog(cfverbose,OUTPUT,"");
   return;
   }

for (dirp = cf_readdir(dirh,attr,pp); dirp != NULL; dirp = cf_readdir(dirh,attr,pp))
   {
   if (!ConsiderFile(dirp->d_name,from,attr,pp))
      {
      continue;
      }

   if (attr.copy.purge) /* Do not purge this file */
      {
      AppendItem(&namecache,dirp->d_name,NULL);
      }

   strncpy(newfrom,from,CF_BUFSIZE-2);                             /* Assemble pathname */
   AddSlash(newfrom);
   strncpy(newto,to,CF_BUFSIZE-2);
   AddSlash(newto);

   if (!JoinPath(newfrom,dirp->d_name))
      {
      cf_closedir(dirh);
      return;
      }

   if (!JoinPath(newto,dirp->d_name))
      {
      cf_closedir(dirh);
      return;
      }

   if (attr.recursion.travlinks || attr.copy.link_type == cfa_notlinked)
      {
      /* No point in checking if there are untrusted symlinks here,
         since this is from a trusted source, by defintion */
      
      if (cf_stat(newfrom,&sb,attr,pp) == -1)
         {
         snprintf(OUTPUT,CF_BUFSIZE," !! (Can't stat %s)\n",newfrom);
         CfLog(cfverbose,OUTPUT,"cf_stat");
         continue;
         }
      }
   else
      {
      if (cf_lstat(newfrom,&sb,attr,pp) == -1)
         {
         snprintf(OUTPUT,CF_BUFSIZE," !! (Can't stat %s)\n",newfrom);
         CfLog(cfverbose,OUTPUT,"cf_stat");
         continue;
         }
      }

   if (attr.recursion.xdev && DeviceBoundary(&sb,pp))
      {
      Verbose(" !! Skipping %s on different device\n",newfrom);
      continue;
      }

   if (S_ISDIR(sb.st_mode))
      {
      if (attr.recursion.travlinks)
         {
         CfLog(cfverbose,"Traversing directory links during copy is too dangerous, pruned","");
         continue;
         }
      
      if (SkipDirLinks(newfrom,dirp->d_name,attr.recursion))
         {
         continue;
         }

      memset(&dsb,0,sizeof(struct stat));
      
      if (stat(newto,&dsb) == -1)
         {
         if (stat(newto,&dsb) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2," !! Can't stat local copy %s - failed to establish directory\n",newto);
            CfLog(cferror,OUTPUT,"stat");
            continue;
            }
         }
      
      VerifyCopiedFileAttributes(newto,&dsb,&sb,attr,pp);
      SourceSearchAndCopy(newfrom,newto,maxrecurse-1,attr,pp);
      }
   else
      {
      VerifyCopy(newfrom,newto,attr,pp);
      }
   }

if (attr.copy.purge)
   {
   PurgeLocalFiles(namecache,to,attr,pp); 
   DeleteItemList(namecache);
   }
 
DeleteCompressedArray(pp->inode_cache);
pp->inode_cache = NULL;
cf_closedir(dirh);
}

/*********************************************************************/

void VerifyCopy(char *source,char *destination,struct FileAttr attr,struct Promise *pp)

{ CFDIR *dirh;
  char sourcefile[CF_BUFSIZE];
  char sourcedir[CF_BUFSIZE];
  char destdir[CF_BUFSIZE];
  char destfile[CF_BUFSIZE];
  struct stat ssb, dsb;
  struct cfdirent *dirp;
  int save_uid, save_gid, found;
  
Debug("VerifyCopy (source=%s destination=%s)\n",source,destination);

if (attr.copy.link_type == cfa_notlinked)
   {
   Debug("Treating links as files for %s\n",source);
   found = cf_stat(source,&ssb,attr,pp);
   }
else
   {
   found = cf_lstat(source,&ssb,attr,pp);
   }

if (found == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't stat %s\n",source);
   CfLog(cferror,OUTPUT,"");
   DeleteClientCache(attr,pp);
   ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
   return;
   }

if (ssb.st_nlink > 1)    /* Preserve hard link structure when copying */
   {
   RegisterAHardLink(ssb.st_ino,destination,attr,pp);
   }

if (S_ISDIR(ssb.st_mode))
   {
   strcpy(sourcedir,source);
   AddSlash(sourcedir);
   strcpy(destdir,destination);
   AddSlash(destdir);

   if ((dirh = cf_opendir(sourcedir,attr,pp)) == NULL)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Can't open directory %s\n",sourcedir);
      CfLog(cfverbose,OUTPUT,"opendir");
      DeleteClientCache(attr,pp);
      return;
      }
   
   /* Now check any overrides */
   
   if (stat(destdir,&dsb) == -1)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Can't stat directory %s\n",destdir);
      CfLog(cferror,OUTPUT,"stat");
      }
   else
      {
      VerifyCopiedFileAttributes(destdir,&dsb,&ssb,attr,pp);
      }
   
   for (dirp = cf_readdir(dirh,attr,pp); dirp != NULL; dirp = cf_readdir(dirh,attr,pp))
      {
      if (!ConsiderFile(dirp->d_name,sourcedir,attr,pp))
         {
         continue;
         }
      
      strcpy(sourcefile, sourcedir);
      
      if (!JoinPath(sourcefile,dirp->d_name))
         {
         FatalError("VerifyCopy");
         }
      
      strcpy(destfile, destdir);
      
      if (!JoinPath(destfile,dirp->d_name))
         {
         FatalError("VerifyCopy");
         }
            
      if (attr.copy.link_type == cfa_notlinked)
         {
         if (cf_stat(sourcefile,&ssb,attr,pp) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Can't stat %s\n",sourcefile);
            CfLog(cfinform,OUTPUT,"stat");
            DeleteClientCache(attr,pp);       
            return;
            }
         }
      else
         {
         if (cf_lstat(sourcefile,&ssb,attr,pp) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Can't stat %s\n",sourcefile);
            CfLog(cfinform,OUTPUT,"lstat");
            DeleteClientCache(attr,pp);       
            return;
            }
         }
      
      CopyFile(sourcefile,destfile,ssb,attr,pp);
      }
   
   cfclosedir(dirh);
   DeleteClientCache(attr,pp);
   return;
   }

strcpy(sourcefile,source);
strcpy(destfile,destination);

CopyFile(sourcefile,destfile,ssb,attr,pp);
DeleteClientCache(attr,pp);
}

/*********************************************************************/

void PurgeLocalFiles(struct Item *filelist,char *localdir,struct FileAttr attr,struct Promise *pp)

{ DIR *dirh;
  struct stat sb; 
  struct dirent *dirp;
  char filename[CF_BUFSIZE];

Debug("PurgeFiles(%s)\n",localdir);

 /* If we purge with no authentication we wipe out EVERYTHING ! */ 

 /* We should be chdir inside the directory here */

if (strlen(localdir) < 2)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Purge of %s denied -- too dangerous!",localdir);
   CfLog(cferror,OUTPUT,"");
   return;
   }
 
if (pp->conn && !pp->conn->authenticated)
   {
   Verbose(" !! Not purging local copy %s - no authenticated contact with a source\n",localdir);
   return;
   }

if (!attr.havedepthsearch)
   {
   Verbose(" !! No depth search when copying %s so no refrece from which to purge\n",localdir);
   return;
   }

/* chdir to minimize the risk of race exploits during copy (which is inherently dangerous) */

if (chdir(localdir) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't chdir to local directory %s\n",localdir);
   CfLog(cfverbose,OUTPUT,"chdir");
   return;
   }

if ((dirh = opendir(".")) == NULL)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't open local directory %s\n",localdir);
   CfLog(cfverbose,OUTPUT,"opendir");
   return;
   }

for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
   {
   if (!ConsiderFile(dirp->d_name,localdir,attr,pp))
      {
      continue;
      }
   
   if (!IsItemIn(filelist,dirp->d_name))
      {
      strncpy(filename,localdir,CF_BUFSIZE-2);
      AddSlash(filename);
      strncat(filename,dirp->d_name,CF_BUFSIZE-2);
      
      if (DONTDO)
         {
         printf(" !! Need to purge %s from copy dest directory\n",filename);
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE*2," !! Purging %s in copy dest directory\n",filename);
         CfLog(cfinform,OUTPUT,"");
         
         if (lstat(filename,&sb) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2," !! Couldn't stat %s while purging\n",filename);
            CfLog(cfverbose,OUTPUT,"lstat");
            ClassAuditLog(pp,attr,OUTPUT,CF_INTERPT);
            }
         else if (S_ISDIR(sb.st_mode))
            {
            struct FileAttr purgeattr;
            memset(&purgeattr,0,sizeof(purgeattr));

            purgeattr.havedepthsearch = true;
            purgeattr.havedelete = true;
            purgeattr.recursion.depth = CF_INFINITY;
            purgeattr.recursion.travlinks = false;

            SetSearchDevice(&sb,pp);

            if (!DepthSearch(filename,&sb,0,purgeattr,pp))
               {
               snprintf(OUTPUT,CF_BUFSIZE*2," !! Couldn't empty directory %s while purging\n",filename);
               CfLog(cfverbose,OUTPUT,"rmdir");
               ClassAuditLog(pp,attr,OUTPUT,CF_INTERPT);
               }
            
            if (rmdir(filename) == -1)
               {
               snprintf(OUTPUT,CF_BUFSIZE*2," !! Couldn't remove directory %s while purging\n",filename);
               CfLog(cfverbose,OUTPUT,"rmdir");
               ClassAuditLog(pp,attr,OUTPUT,CF_INTERPT);
               }
            }
         else if (unlink(filename) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2," !! Couldn't delete %s while purging\n",filename);
            CfLog(cfverbose,OUTPUT,"");
            ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
            }
         }
      }
   }
 
closedir(dirh);
}


/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

void CopyFile(char *sourcefile,char *destfile,struct stat ssb,struct FileAttr attr, struct Promise *pp)

{ char *lastnode,*server;
  struct stat dsb;
  struct Link empty;
  struct Item *ptr, *ptr1;
  int found,succeed = false;
  mode_t srcmode = ssb.st_mode;

Debug2("CopyFile(%s,%s)\n",sourcefile,destfile);

if (attr.copy.servers)
   {
   server = (char *)attr.copy.servers->item;
   }
else
   {
   server = NULL;
   }

if ((strcmp(sourcefile,destfile) == 0) && (strcmp(server,"localhost") == 0))
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Image loop: file/dir %s copies to itself",sourcefile);
   CfLog(cfinform,OUTPUT,"");
   return;
   }

memset(&empty,0,sizeof(struct Link));
empty.nofile = true;

if (!SelectLeaf(sourcefile,&ssb,attr,pp))
   {
   Debug("Skipping non-selected file %s\n",sourcefile);
   return;
   }

/*
if (IsItemIn(VEXCLUDECACHE,destfile))
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Skipping single-copied file %s class %s\n",destfile,ip->classes);
   CfLog(cfverbose,OUTPUT,"");
   return;
   }
*/

if (attr.copy.link_type != cfa_notlinked)
   {
   lastnode=ReadLastNode(sourcefile);

   if (MatchRlistItem(attr.copy.link_instead,lastnode))
      {
      Verbose("cfengine: copy item %s marked for linking instead\n",sourcefile);
      LinkCopy(sourcefile,destfile,&ssb,attr,pp);
      return;
      }
   }
 
found = lstat(destfile,&dsb);

if (found != -1)
   {
   if ((S_ISLNK(dsb.st_mode) && (attr.copy.link_type == cfa_notlinked)) || (S_ISLNK(dsb.st_mode) && ! S_ISLNK(ssb.st_mode)))
      {
      if (!S_ISLNK(ssb.st_mode) && (attr.copy.type_check && (attr.copy.link_type != cfa_notlinked)))
         {
         snprintf(OUTPUT,CF_BUFSIZE,"file image exists but destination type is silly (file/dir/link doesn't match)\n");
         CfLog(cferror,OUTPUT,"");
         PromiseRef(cferror,pp);
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         return;
         }
      
      if (DONTDO)
         {
         Verbose("Need to remove old symbolic link %s to make way for copy\n",destfile);
         }
      else 
         {
         if (unlink(destfile) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't remove link %s",destfile);
            CfLog(cferror,OUTPUT,"unlink");
            ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
            return;
            }
         
         Verbose("Removing old symbolic link %s to make way for copy\n",destfile);
         found = -1;
         }
      } 
   }
else
   {
   MakeParentDirectory(destfile,true);
   }

if (attr.copy.min_size == 0 && attr.copy.max_size == 0)
   {
   if (ssb.st_size < attr.copy.min_size || ssb.st_size > attr.copy.max_size)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Source file %s size is not in the permitted range\n",sourcefile);
      CfLog(cfinform,OUTPUT,"");
      ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
      return;
      }
   }
   
if (found == -1)
   {
   if (attr.transaction.action == cfa_warn)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Image file %s is non-existent\n",destfile);
      CfLog(cferror,OUTPUT,"");
      snprintf(OUTPUT,CF_BUFSIZE*2,"(should be copy of %s)\n",sourcefile);
      CfLog(cferror,OUTPUT,"");
      return;
      }
   
   if (S_ISREG(srcmode) || (S_ISLNK(srcmode) && attr.copy.link_type == cfa_notlinked))
      {
      if (DONTDO)
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"%s wasn't at destination (needs copying)",destfile);
         CfLog(cfverbose,OUTPUT,"");
         return;
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"%s wasn't at destination (copying)",destfile);
         CfLog(cfverbose,OUTPUT,"");
         snprintf(OUTPUT,CF_BUFSIZE*2,"Copying from %s:%s\n",server,sourcefile);
         CfLog(cfinform,OUTPUT,"");
         }
      
      if (CopyRegularFile(sourcefile,destfile,ssb,dsb,attr,pp))
         {
         if (stat(destfile,&dsb) == -1)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Can't stat %s\n",destfile);
            CfLog(cferror,OUTPUT,"stat");
            }
         else
            {
            VerifyCopiedFileAttributes(destfile,&dsb,&ssb,attr,pp);
            }

         snprintf(OUTPUT,CF_BUFSIZE*2,"Copied from %s:%s\n",server,sourcefile);
         CfLog(cfverbose,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_CHG);

         /*
         if (controlVAL SINGLECOPY LIST)
            {
            if (!IsItemIn(VEXCLUDECACHE,destfile))
               {
               Debug("Appending image %s class %s to singlecopy list\n",destfile,ip->classes);
               PrependItem(&VEXCLUDECACHE,destfile,NULL);
               }
            }         

         for (ptr = VAUTODEFINE; ptr != NULL; ptr=ptr->next)
            {
            if (WildMatch(ptr->name,destfile))
               {
               snprintf(OUTPUT,CF_BUFSIZE*2,"Image %s was set to autodefine class %s\n",ptr->name,ptr->classes);
               CfLog(cfinform,OUTPUT,"");
               AddMultipleClasses(ptr->classes);
               }
            }

         */
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Copy from %s:%s failed\n",server,sourcefile);
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         }
      
      Debug2("Leaving CopyFile\n");
      return;
      }
   
   if (S_ISFIFO (srcmode))
      {
#ifdef HAVE_MKFIFO
      if (DONTDO)
         {
         Silent("%s: Make FIFO %s\n",VPREFIX,destfile);
         }
      else if (mkfifo (destfile,srcmode))
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Cannot create fifo `%s'", destfile);
         CfLog(cferror,OUTPUT,"mkfifo");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         return;
         }

      snprintf(OUTPUT,CF_BUFSIZE*2,"Created fifo %s", destfile);
      ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
#endif
      }
   else
      {
      if (S_ISBLK (srcmode) || S_ISCHR (srcmode) || S_ISSOCK (srcmode))
         {
         if (DONTDO)
            {
            Silent("%s: Make BLK/CHR/SOCK %s\n",VPREFIX,destfile);
            }
         else if (mknod (destfile, srcmode, ssb.st_rdev))
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Cannot create special file `%s'",destfile);
            CfLog(cferror,OUTPUT,"mknod");
            ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
            return;
            }

         snprintf(OUTPUT,CF_BUFSIZE*2,"Created socket/device %s", destfile);
         ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
         }
      }
   
   if (S_ISLNK(srcmode) && attr.copy.link_type != cfa_notlinked)
      {
      LinkCopy(sourcefile,destfile,&ssb,attr,pp);
      }
   }
else
   {
   int ok_to_copy = false;
   
   Debug("Destination file %s already exists\n",destfile);
   
   if (!attr.copy.force_update)
      {
      ok_to_copy = CompareForFileCopy(sourcefile,destfile,&ssb,&dsb,attr,pp);
      }
   
   if (attr.copy.type_check && attr.copy.link_type != cfa_notlinked)
      {
      if ((S_ISDIR(dsb.st_mode)  && ! S_ISDIR(ssb.st_mode))  ||
          (S_ISREG(dsb.st_mode)  && ! S_ISREG(ssb.st_mode))  ||
          (S_ISBLK(dsb.st_mode)  && ! S_ISBLK(ssb.st_mode))  ||
          (S_ISCHR(dsb.st_mode)  && ! S_ISCHR(ssb.st_mode))  ||
          (S_ISSOCK(dsb.st_mode) && ! S_ISSOCK(ssb.st_mode)) ||
          (S_ISFIFO(dsb.st_mode) && ! S_ISFIFO(ssb.st_mode)) ||
          (S_ISLNK(dsb.st_mode)  && ! S_ISLNK(ssb.st_mode)))
          
         {
         snprintf(OUTPUT,CF_BUFSIZE,"Promised file copy %s exists but type mismatch with source=%s\n",destfile,sourcefile);
         CfLog(cfinform,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
         return;
         }
      }
   
   if (attr.copy.force_update || ok_to_copy || S_ISLNK(ssb.st_mode))  /* Always check links */
      {
      if (S_ISREG(srcmode) || attr.copy.link_type == cfa_notlinked)
         {
         if (DONTDO)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Should update file %s from source %s on %s",destfile,sourcefile,server);
            CfLog(cferror,OUTPUT,"");
            return;
            }
         else
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Updated %s from source %s on %s",destfile,sourcefile,server);
            CfLog(cfinform,OUTPUT,"");
            ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
            }

         /*
         for (ptr = VAUTODEFINE; ptr != NULL; ptr=ptr->next)
            {
            if (WildMatch(ptr->name,destfile))
               {
               snprintf(OUTPUT,CF_BUFSIZE*2,"Image %s was set to autodefine class %s\n",ptr->name,ptr->classes);
               CfLog(cfinform,OUTPUT,"");
               AddMultipleClasses(ptr->classes);
               }
            } 
         */
         
         if (CopyRegularFile(sourcefile,destfile,ssb,dsb,attr,pp))
            {
            if (stat(destfile,&dsb) == -1)
               {
               snprintf(OUTPUT,CF_BUFSIZE*2,"Can't stat %s\n",destfile);
               CfLog(cferror,OUTPUT,"stat");
               ClassAuditLog(pp,attr,OUTPUT,CF_INTERPT);
               }
            else
               {
               VerifyCopiedFileAttributes(destfile,&dsb,&ssb,attr,pp);
               }
            
/*            if (ALL_SINGLECOPY || IsWildItemIn(VSINGLECOPY,destfile))
               {
               if (!IsItemIn(VEXCLUDECACHE,destfile))
                  {
                  Debug("Appending image %s class %s to singlecopy list\n",destfile,ip->classes);
                  PrependItem(&VEXCLUDECACHE,destfile,NULL);
                  }
               }
*/
            } 
         else
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Can't stat %s\n",destfile);
            CfLog(cferror,OUTPUT,"stat");
            ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
            }
         
         return;
         }
      
      if (S_ISLNK(ssb.st_mode))
         {
         LinkCopy(sourcefile,destfile,&ssb,attr,pp);
         }
      }
   else
      {
      VerifyCopiedFileAttributes(destfile,&dsb,&ssb,attr,pp);
      
      /* Now we have to check for single copy, even though nothing was copied
         otherwise we can get oscillations between multipe versions if type is based on a checksum */
/*      
      if (ALL_SINGLECOPY || IsWildItemIn(VSINGLECOPY,destfile))
         {
         if (!IsItemIn(VEXCLUDECACHE,destfile))
            {
            Debug("Appending image %s class %s to singlecopy list\n",destfile,ip->classes);
            PrependItem(&VEXCLUDECACHE,destfile,NULL);
            }
          }
*/    
//      snprintf(OUTPUT,CF_BUFSIZE," -> File %s is an up to date copy of source\n",destfile);
//      CfLog(cfinform,OUTPUT,"");
//      ClassAuditLog(pp,attr,OUTPUT,CF_NOP);
      CfOut(cfinform,CF_NOP,"",pp,attr," -> File %s is an up to date copy of source\n",destfile);
      }
   }
}

/*********************************************************************/

int cf_stat(char *file,struct stat *buf,struct FileAttr attr,struct Promise *pp)

{ int res;

if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item,"localhost") == 0)
   {
   res = stat(file,buf);
   CheckForFileHoles(buf,attr,pp);
   return res;
   }
else
   {
   return cf_remote_stat(file,buf,"file",attr,pp);
   }
}

/*********************************************************************/

int cf_lstat(char *file,struct stat *buf,struct FileAttr attr,struct Promise *pp)

{ int res;

if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item,"localhost") == 0)
   {
   res = lstat(file,buf);
   CheckForFileHoles(buf,attr,pp);
   return res;
   }
else
   {
   return cf_remote_stat(file,buf,"link",attr,pp);
   }
}

/*********************************************************************/

int cf_readlink(char *sourcefile,char *linkbuf,int buffsize,struct FileAttr attr,struct Promise *pp)

 /* wrapper for network access */

{ struct cfstat *sp;

memset(linkbuf,0,buffsize);
 
if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item,"localhost") == 0)
   {
   return readlink(sourcefile,linkbuf,buffsize-1);
   }

for (sp = pp->cache; sp != NULL; sp=sp->next)
   {
   if ((strcmp(attr.copy.servers->item,sp->cf_server) == 0) && (strcmp(sourcefile,sp->cf_filename) == 0))
      {
      if (sp->cf_readlink != NULL)
         {
         if (strlen(sp->cf_readlink)+1 > buffsize)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"readlink value is too large in cfreadlink\n");
            CfLog(cferror,OUTPUT,"");
            ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
            snprintf(OUTPUT,CF_BUFSIZE,"%s: [%s]]n",VPREFIX,sp->cf_readlink);
            CfLog(cferror,OUTPUT,"");
            return -1;
            }
         else
            {
            memset(linkbuf,0,buffsize);
            strcpy(linkbuf,sp->cf_readlink);
            return 0;
            }
         }
      }
   }

return -1;
}

/*********************************************************************/

CFDIR *cf_opendir(char *name,struct FileAttr attr,struct Promise *pp)

{ CFDIR *returnval;

if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item,"localhost") == 0)
   {
   if ((returnval = (CFDIR *)malloc(sizeof(CFDIR))) == NULL)
      {
      FatalError("Can't allocate memory in cfopendir()\n");
      }
   
   returnval->cf_list = NULL;
   returnval->cf_listpos = NULL;
   returnval->cf_dirh = opendir(name);

   if (returnval->cf_dirh != NULL)
      {
      return returnval;
      }
   else
      {
      free ((char *)returnval);
      return NULL;
      }
   }
else
   {
   return cf_remote_opendir(name,attr,pp);
   }
}

/*********************************************************************/

struct cfdirent *cf_readdir(CFDIR *cfdirh,struct FileAttr attr,struct Promise *pp)

  /* We need this cfdirent type to handle the weird hack */
  /* used in SVR4/solaris dirent structures              */

{ static struct cfdirent dir;
  struct dirent *dirp;

memset(dir.d_name,0,CF_BUFSIZE);

if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item,"localhost") == 0)
   {
   dirp = readdir(cfdirh->cf_dirh);

   if (dirp == NULL)
      {
      return NULL;
      }

   strncpy(dir.d_name,dirp->d_name,CF_BUFSIZE-1);
   return &dir;
   }
else
   {
   if (cfdirh->cf_listpos != NULL)
      {
      strncpy(dir.d_name,(cfdirh->cf_listpos)->name,CF_BUFSIZE);
      cfdirh->cf_listpos = cfdirh->cf_listpos->next;
      return &dir;
      }
   else
      {
      return NULL;
      }
   }
}
 
/*********************************************************************/

void cf_closedir(CFDIR *dirh)

{
if ((dirh != NULL) && (dirh->cf_dirh != NULL))
   {
   closedir(dirh->cf_dirh);
   }

Debug("cfclosedir()\n");
DeleteItemList(dirh->cf_list);
free((char *)dirh); 
}

/*********************************************************************/
/* Level 4                                                           */
/*********************************************************************/

int CompareForFileCopy(char *sourcefile,char *destfile,struct stat *ssb, struct stat *dsb,struct FileAttr attr,struct Promise *pp)

{ int ok_to_copy;
 
switch (attr.copy.compare)
   {
   case cfa_checksum:
       
       if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
          {
          ok_to_copy = CompareFileHashes(sourcefile,destfile,ssb,dsb,attr,pp);
          }      
       else
          {
          CfLog(cfinform,"Checksum comparison replaced by ctime: files not regular\n","");
          snprintf(OUTPUT,CF_BUFSIZE*2,"%s -> %s\n",sourcefile,destfile);
          CfLog(cfinform,OUTPUT,"");
          ok_to_copy = (dsb->st_ctime < ssb->st_ctime)||(dsb->st_mtime < ssb->st_mtime);
          }
       
       if (ok_to_copy && (attr.transaction.action == cfa_warn))
          { 
          snprintf(OUTPUT,CF_BUFSIZE*2,"Image file %s has a wrong MD5 checksum (should be copy of %s)\n",destfile,sourcefile);
          CfLog(cfinform,OUTPUT,"");
          return ok_to_copy;
          }
       break;
       
   case cfa_binary:
       
       if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
          {
          ok_to_copy = CompareBinaryFiles(sourcefile,destfile,ssb,dsb,attr,pp);
          }      
       else
          {
          CfLog(cfinform,"Byte comparison replaced by ctime: files not regular\n","");
          snprintf(OUTPUT,CF_BUFSIZE*2,"%s -> %s\n",sourcefile,destfile);
          CfLog(cfinform,OUTPUT,"");
          ok_to_copy = (dsb->st_ctime < ssb->st_ctime)||(dsb->st_mtime < ssb->st_mtime);
          }
       
       if (ok_to_copy && (attr.transaction.action == cfa_warn))
          { 
          snprintf(OUTPUT,CF_BUFSIZE*2,"Image file %s has a wrong binary checksum (should be copy of %s)\n",destfile,sourcefile);
          CfLog(cferror,OUTPUT,"");
          return ok_to_copy;
          }
       break;
       
   case cfa_mtime:
       
       ok_to_copy = (dsb->st_mtime < ssb->st_mtime);
       
       if (ok_to_copy && (attr.transaction.action == cfa_warn))
          { 
          snprintf(OUTPUT,CF_BUFSIZE*2,"Image file %s out of date (should be copy of %s)\n",destfile,sourcefile);
          CfLog(cferror,OUTPUT,"");
          return ok_to_copy;
          }
       break;
       
   case cfa_atime:
       
       ok_to_copy = (dsb->st_ctime < ssb->st_ctime)||
           (dsb->st_mtime < ssb->st_mtime)||
           CompareBinaryFiles(sourcefile,destfile,ssb,dsb,attr,pp);
       
       if (ok_to_copy && (attr.transaction.action == cfa_warn))
          { 
          snprintf(OUTPUT,CF_BUFSIZE*2,"Image file %s is updated somehow (should be copy of %s)\n",destfile,sourcefile);
          CfLog(cferror,OUTPUT,"");
          return ok_to_copy;
          }
       break;
       
   default:
       ok_to_copy = (dsb->st_ctime < ssb->st_ctime)||(dsb->st_mtime < ssb->st_mtime);
       
       if (ok_to_copy && (attr.transaction.action == cfa_warn))
          { 
          snprintf(OUTPUT,CF_BUFSIZE*2,"Image file %s out of date (should be copy of %s)\n",destfile,sourcefile);
          CfLog(cferror,OUTPUT,"");
          return ok_to_copy;
          }
       break;
   }

return false;
}

/*************************************************************************************/
      
void LinkCopy(char *sourcefile,char *destfile,struct stat *sb,struct FileAttr attr, struct Promise *pp)

{ char linkbuf[CF_BUFSIZE];
  int succeed = false;
  struct stat dsb;
  
if (cf_readlink(sourcefile,linkbuf,CF_BUFSIZE,attr,pp) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't readlink %s\n",sourcefile);
   CfLog(cferror,OUTPUT,"");
   ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
   return;
   }

snprintf(OUTPUT,CF_BUFSIZE*2,"Checking link from %s to %s\n",destfile,linkbuf);
CfLog(cfverbose,OUTPUT,"");

if (attr.copy.link_type == cfa_absolute && linkbuf[0] != '/')      /* Not absolute path - must fix */
   {
   char vbuff[CF_BUFSIZE];
   strcpy(vbuff,sourcefile);
   ChopLastNode(vbuff);
   AddSlash(vbuff);
   strncat(vbuff,linkbuf,CF_BUFSIZE-1);
   strncpy(linkbuf,vbuff,CF_BUFSIZE-1);
   }

switch (attr.copy.link_type)
   {
   case cfa_symlink:
       
       if (*linkbuf == '.')
          {
          succeed = VerifyRelativeLink(destfile,linkbuf,attr,pp);
          }
       else
          {
          succeed = VerifyLink(destfile,linkbuf,attr,pp);
          }
       break;
       
   case cfa_relative:
       succeed = VerifyRelativeLink(destfile,linkbuf,attr,pp);
       break;
       
   case cfa_absolute:
       succeed = VerifyAbsoluteLink(destfile,linkbuf,attr,pp);
       break;
       
   default:
       FatalError("LinkCopy software error");
       return;
   }

if (succeed)
   {
   if (lstat(destfile,&dsb) == -1)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Can't lstat %s\n",destfile);
      CfLog(cferror,OUTPUT,"lstat");
      }
   else
      {
      VerifyCopiedFileAttributes(destfile,&dsb,sb,attr,pp);
      }
   
   snprintf(OUTPUT,CF_BUFSIZE*2,"Created link %s", destfile);
   ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
   }
}

/*************************************************************************************/

int CopyRegularFile(char *source,char *dest,struct stat sstat,struct stat dstat,struct FileAttr attr,struct Promise *pp)

{ char backup[CF_BUFSIZE];
  char new[CF_BUFSIZE], *linkable;
  struct cfagent_connection *conn = pp->conn;
  int remote = false, silent, backupisdir=false, backupok=false,discardbackup;
  struct stat s;
#ifdef HAVE_UTIME_H
  struct utimbuf timebuf;
#endif  

#ifdef DARWIN
/* For later copy from new to dest */
  char *rsrcbuf;
  int rsrcbytesr; /* read */
  int rsrcbytesw; /* written */
  int rsrcbytesl; /* to read */
  int rsrcrd;
  int rsrcwd;
  
/* Keep track of if a resrouce fork */
  char * tmpstr;
  char * forkpointer;
  int rsrcfork = 0;
#endif
  
#ifdef WITH_SELINUX
  int selinux_enabled=0;
/* need to keep track of security context of destination file (if any) */
  security_context_t scontext=NULL;
  struct stat cur_dest;
  int dest_exists;
  selinux_enabled = (is_selinux_enabled()>0);
#endif

Debug2("CopyRegularFile(%s,%s)\n",source,dest);

discardbackup = (attr.copy.backup == cfa_nobackup || attr.copy.backup == cfa_repos_store);
    
if (DONTDO)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Promise requires copy from %s to %s\n",source,dest);
   CfLog(cferror,OUTPUT,"");
   return false;
   }

#ifdef WITH_SELINUX
if (selinux_enabled)
   {
   dest_exists = stat(dest,&cur_dest);
   
   if(dest_exists == 0)
      {
      /* get current security context of destination file */
      getfilecon(dest,&scontext);
      }
   else
      {
      /* use default security context when creating destination file */
      matchpathcon(dest,0,&scontext);
      setfscreatecon(scontext);
      }
   }
#endif

 /* Make an assoc array of inodes used to preserve hard links */

linkable = CompressedArrayValue(pp->inode_cache,sstat.st_ino);

if (sstat.st_nlink > 1)  /* Preserve hard links, if possible */
   {
   if (CompressedArrayElementExists(pp->inode_cache,sstat.st_ino) && (strcmp(dest,linkable) != 0))
      {
      unlink(dest);
      DoHardLink(dest,linkable,NULL);
      return true;
      }
   }

if (attr.copy.servers != NULL && strcmp(attr.copy.servers->item,"localhost") != 0)
   {
   Debug("This is a remote copy from server: %s\n",attr.copy.servers->item);
   remote = true;
   }

#ifdef DARWIN
if (strstr(dest, _PATH_RSRCFORKSPEC))
   { /* Need to munge the "new" name */
   rsrcfork = 1;
   
   tmpstr = malloc(CF_BUFSIZE);
   
   /* Drop _PATH_RSRCFORKSPEC */
   strncpy(tmpstr, dest, CF_BUFSIZE);
   forkpointer = strstr(tmpstr, _PATH_RSRCFORKSPEC);
   *forkpointer = '\0';
   
   strncpy(new, tmpstr, CF_BUFSIZE);
   
   free(tmpstr);
   }
else
   {
#endif

if (BufferOverflow(dest,CF_NEW))
   {
   printf(" culprit: CopyReg\n");
   return false;
   }
strcpy(new,dest);

#ifdef DARWIN
   }
#endif

strcat(new,CF_NEW);

if (remote)
   {
   if (conn->error)
      {
      return false;
      }
   
   if (!CopyRegularFileNet(source,new,sstat.st_size,attr,pp))
      {
      return false;
      }
   }
else
   {
   if (!CopyRegularFileDisk(source,new,attr,pp))
      {
      return false;
      }

   if (attr.copy.stealth)
      {
#ifdef HAVE_UTIME_H
      timebuf.actime = sstat.st_atime;
      timebuf.modtime = sstat.st_mtime;
      utime(source,&timebuf);
#endif      
      }
   }

Debug("CopyRegular succeeded in copying to %s to %s\n",source,new);

if (!discardbackup)
   {
   char stamp[CF_BUFSIZE];
   time_t stampnow;

   Debug("Backup file %s\n",source);

   stampnow = time((time_t *)NULL);
   
   sprintf(stamp, "_%d_%s", CFSTARTTIME, CanonifyName(ctime(&stampnow)));

   if (BufferOverflow(dest,stamp))
      {
      printf(" culprit: CopyReg\n");
      return false;
      }

   strcpy(backup,dest);

   if (attr.copy.backup == cfa_timestamp)
      {
      strcat(backup,stamp);
      }

   /* rely on prior BufferOverflow() and on strlen(CF_SAVED) < CF_BUFFERMARGIN */

   strcat(backup,CF_SAVED);

   /* Now in case of multiple copies of same object, try to avoid overwriting original backup */
   
   if (lstat(backup,&s) != -1)
      {
      if (S_ISDIR(s.st_mode))      /* if there is a dir in the way */
         {
         backupisdir = true;
         PurgeFiles(NULL,backup,NULL);
         rmdir(backup);
         }
      
      unlink(backup);
      }
   
   if (rename(dest,backup) == -1)
      {
      /* ignore */
      }
   
   backupok = (lstat(backup,&s) != -1); /* Did the rename() succeed? NFS-safe */
   }
else
   {
   /* Mainly important if there is a dir in the way */
   
   if (stat(dest,&s) != -1)
      {
      if (S_ISDIR(s.st_mode))
         {
         PurgeFiles(NULL,dest,NULL);
         rmdir(dest);
         }
      }
   }

if (stat(new,&dstat) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't stat new file %s\n",new);
   CfLog(cferror,OUTPUT,"stat");
   return false;
   }

if (dstat.st_size != sstat.st_size)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"WARNING: new file %s seems to have been corrupted in transit (sizes %d and %d), aborting!\n",new, (int) dstat.st_size, (int) sstat.st_size);
   CfLog(cfverbose,OUTPUT,"");
   if (backupok)
      {
      rename(backup,dest); /* ignore failure */
      }
   return false;
   }

if (attr.copy.verify)
   {
   Verbose("Final verification of transmission: %s -> %s\n",source,new);

   if (CompareFileHashes(source,new,&sstat,&dstat,attr,pp))
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"WARNING: new file %s seems to have been corrupted in transit, aborting!\n",new);
      CfLog(cfverbose,OUTPUT,"");
      if (backupok)
         {
         rename(backup,dest); /* ignore failure */
         }
      return false;
      }
   }
 
#ifdef DARWIN
if (rsrcfork)
   { /* Can't just "mv" the resource fork, unfortunately */   
   rsrcrd = open(new, O_RDONLY|O_BINARY);
   rsrcwd = open(dest, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0600);
   
   if (rsrcrd == -1 || rsrcwd == -1)
      {
      snprintf(OUTPUT, CF_BUFSIZE, "Open of rsrcrd/rsrcwd failed\n");
      CfLog(cfinform,OUTPUT,"open");
      close(rsrcrd);
      close(rsrcwd);
      return(false);
      }
   
   rsrcbuf = malloc(CF_BUFSIZE);
   
   rsrcbytesr = 0;
   
   while(1)
      {
      rsrcbytesr = read(rsrcrd, rsrcbuf, CF_BUFSIZE);
      
      if (rsrcbytesr == -1)
         { /* Ck error */
         if (errno == EINTR)
            {
            continue;
            }
         else
            {
            snprintf(OUTPUT, CF_BUFSIZE, "Read of rsrcrd failed\n");
            CfLog(cfinform,OUTPUT,"read");
            close(rsrcrd);
            close(rsrcwd);
            free(rsrcbuf);
            return(false);
            }
         }
      
      else if (rsrcbytesr == 0)
         { /* Reached EOF */
         close(rsrcrd);
         close(rsrcwd);
         free(rsrcbuf);
         
         unlink(new); /* Go ahead and unlink .cfnew */
         
         break;
         }
      
      rsrcbytesl = rsrcbytesr;
      rsrcbytesw = 0;
      
      while (rsrcbytesl > 0)
         {
         
         rsrcbytesw += write(rsrcwd, rsrcbuf, rsrcbytesl);
         
         if (rsrcbytesw == -1)
            { /* Ck error */
            if (errno == EINTR)
                {
                continue;
                }
            else
               {
               snprintf(OUTPUT, CF_BUFSIZE, "Write of rsrcwd failed\n");
               CfLog(cfinform,OUTPUT,"write");
               
               close(rsrcrd);
               close(rsrcwd);
               free(rsrcbuf);
               return(false);
               }
            }  
         rsrcbytesl = rsrcbytesr - rsrcbytesw;  
         }
      }
   
   }
else
   {
#endif   
   
   if (rename(new,dest) == -1)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Problem: could not install copy file as %s, directory in the way?\n",dest);
      CfLog(cferror,OUTPUT,"rename");
      if (backupok)
         {
         rename(backup,dest); /* ignore failure */
         }
      return false;
      }
   
#ifdef DARWIN
   }
#endif

if (!discardbackup && backupisdir)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Cannot move a directory to repository, leaving at %s",backup);
   CfLog(cfinform,OUTPUT,"");
   }
else if (!discardbackup && ArchiveToRepository(backup,attr,pp))
   {
   unlink(backup);
   }

if (attr.copy.preserve)
   {
#ifdef HAVE_UTIME_H
   timebuf.actime = sstat.st_atime;
   timebuf.modtime = sstat.st_mtime;
   utime(dest,&timebuf);
#endif
   }

#ifdef WITH_SELINUX
if (selinux_enabled)
   {
   if (dest_exists == 0)
      {
      /* set dest context to whatever it was before copy */
      setfilecon(dest,scontext);
      }
   else
      {
      /* set create context back to default */
      setfscreatecon(NULL);
      }
   freecon(scontext);
   }
#endif

return true;
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

void RegisterAHardLink(int i,char *value,struct FileAttr attr,struct Promise *pp)

{
if (!FixCompressedArrayValue(i,value,&(pp->inode_cache)))
   {
    /* Not root hard link, remove to preserve consistency */
   if (DONTDO)
      {
      Verbose("Need to remove old hard link %s to preserve structure..\n",value);
      }
   else
      {
      if (attr.transaction.action = cfa_warn)
         {
         Verbose("Need to remove old hard link %s to preserve structure..\n",value);
         }
      else
         {
         Verbose("Removing old hard link %s to preserve structure..\n",value);
         unlink(value);
         }
      }
   }
}
