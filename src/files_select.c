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
/* File: files_select.c                                                      */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int SelectLeaf(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp)

{ struct Item *leaf_attr = NULL;
  int result = true, tmpres;
  char *criteria = NULL;
  struct Rlist *rp;

#ifdef MINGW
if(attr.select.issymlinkto != NULL)
{
CfOut(cf_verbose, "", "files_select.issymlinkto is ignored on Windows (symbolic links are not supported by Windows)");
}

if(attr.select.groups != NULL)
{
CfOut(cf_verbose, "", "files_select.search_groups is ignored on Windows (file groups are not supported by Windows)");
}

if(attr.select.bsdflags != NULL)
{
CfOut(cf_verbose, "", "files_select.search_bsdflags is ignored on Windows");
}
#endif  /* MINGW */
  
if (!attr.haveselect)
   {
   return true;
   }

if (attr.select.name == NULL)
   {
   PrependItem(&leaf_attr,"leaf_name","");
   }

for (rp = attr.select.name; rp != NULL; rp = rp->next)
   {
   if (SelectNameRegexMatch(path,rp->item))
      {
      PrependItem(&leaf_attr,"leaf_name","");
      break;
      }
   }

if (attr.select.path == NULL)
   {
   PrependItem(&leaf_attr,"leaf_path","");
   }

for (rp = attr.select.path; rp != NULL; rp = rp->next)
   {
   if (SelectPathRegexMatch(path,rp->item))
      {
      PrependItem(&leaf_attr,"path_name","");
      break;         
      }
   }

if (SelectTypeMatch(sb,attr.select.filetypes))
   {
   PrependItem(&leaf_attr,"file_types","");
   }

if (attr.select.owners && SelectOwnerMatch(path,sb,attr.select.owners))
   {
   PrependItem(&leaf_attr,"owner","");
   }

if (attr.select.owners == NULL)
   {
   PrependItem(&leaf_attr,"owner","");
   }

#ifdef MINGW
PrependItem(&leaf_attr,"group","");

#else  /* NOT MINGW */
if (attr.select.groups && SelectGroupMatch(sb,attr.select.groups))
   {
   PrependItem(&leaf_attr,"group","");
   }
   
if (attr.select.groups == NULL)
   {
   PrependItem(&leaf_attr,"group","");
   }
#endif  /* NOT MINGW */

if (SelectModeMatch(sb,attr.select.perms))
   {
   PrependItem(&leaf_attr,"mode","");
   }

#if defined HAVE_CHFLAGS 
if (SelectBSDMatch(sb,attr.select.bsdflags,pp))
   {
   PrependItem(&leaf_attr,"bsdflags","");
   }
#endif

if (SelectTimeMatch(sb->st_atime,attr.select.min_atime,attr.select.max_atime))
   { 
   PrependItem(&leaf_attr,"atime","");
   }

if (SelectTimeMatch(sb->st_ctime,attr.select.min_ctime,attr.select.max_ctime))
   { 
   PrependItem(&leaf_attr,"ctime","");
   }

if (SelectSizeMatch(sb->st_size,attr.select.min_size,attr.select.max_size))
   { 
   PrependItem(&leaf_attr,"size","");
   }

if (SelectTimeMatch(sb->st_mtime,attr.select.min_mtime,attr.select.max_mtime))
   { 
   PrependItem(&leaf_attr,"mtime","");
   }

if (attr.select.issymlinkto && SelectIsSymLinkTo(path,attr.select.issymlinkto))
   {
   PrependItem(&leaf_attr,"issymlinkto","");
   }

if (attr.select.exec_regex && SelectExecRegexMatch(path,attr.select.exec_regex,attr.select.exec_program))
   {
   PrependItem(&leaf_attr,"exec_regex","");
   }

if (attr.select.exec_program && SelectExecProgram(path,attr.select.exec_program))
   {
   PrependItem(&leaf_attr,"exec_program","");
   }

if (result = EvaluateORString(attr.select.result,leaf_attr,0))
   {
   //NewClassesFromString(fp->defines);
   }
 
Debug("Select result \"%s\"on %s was %d\n",attr.select.result,path,result);

DeleteItemList(leaf_attr);

return result; 
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

int SelectSizeMatch(size_t size,size_t min,size_t max)

{ struct Item *leafattrib = NULL;
  struct Rlist *rp;

if (size <= max && size >= min)
   {
   return true;
   }
  
return false;
}

/*******************************************************************/

int SelectTypeMatch(struct stat *lstatptr,struct Rlist *crit)

{ struct Item *leafattrib = NULL;
  struct Rlist *rp;
 
if (S_ISREG(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"reg","");
   PrependItem(&leafattrib,"plain","");
   }

if (S_ISDIR(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"dir","");
   }

#ifndef MINGW   
if (S_ISLNK(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"symlink","");
   }   
 
if (S_ISFIFO(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"fifo","");
   }

if (S_ISSOCK(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"socket","");
   }

if (S_ISCHR(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"char","");
   }

if (S_ISBLK(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"block","");
   }
#endif  /* NOT MINGW */

#ifdef HAVE_DOOR_CREATE
if (S_ISDOOR(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"door","");
   }
#endif

for (rp = crit; rp != NULL; rp=rp->next)
   {
   if (EvaluateORString((char *)rp->item,leafattrib,0))
      {
      DeleteItemList(leafattrib);
      return true;
      }
   }

DeleteItemList(leafattrib);
return false;
}

/*******************************************************************/

/* Writes the owner of file 'path', with stat 'lstatptr' into buffer 'owner' of
 * size 'ownerSz'. Returns true on success, false otherwise                      */
int GetOwnerName(char *path, struct stat *lstatptr, char *owner, int ownerSz)

{
#ifdef MINGW
return NovaWin_GetOwnerName(path, owner, ownerSz);
#else  /* NOT MINGW */
return Unix_GetOwnerName(lstatptr, owner, ownerSz);
#endif  /* NOT MINGW */
}

/*******************************************************************/

int SelectOwnerMatch(char *path,struct stat *lstatptr,struct Rlist *crit)

{ struct Item *leafattrib = NULL;
  struct Rlist *rp;
  char ownerName[CF_BUFSIZE];
  int gotOwner;

#ifndef MINGW  // no uids on Windows
char buffer[CF_SMALLBUF];
sprintf(buffer,"%d",lstatptr->st_uid);
PrependItem(&leafattrib,buffer,""); 
#endif  /* MINGW */

gotOwner = GetOwnerName(path, lstatptr, ownerName, sizeof(ownerName));

if (gotOwner)
   {
   PrependItem(&leafattrib,ownerName,""); 
   }
else
   {
   PrependItem(&leafattrib,"none",""); 
   }

for (rp = crit; rp != NULL; rp = rp->next)
   {
   if (EvaluateORString((char *)rp->item,leafattrib,0))
      {
      Debug(" - ? Select owner match\n");
      DeleteItemList(leafattrib);
      return true;
      }

   if (gotOwner && FullTextMatch((char *)rp->item,ownerName))
      {
      Debug(" - ? Select owner match\n");
      DeleteItemList(leafattrib);
      return true;
      }

#ifndef MINGW	  
   if (FullTextMatch((char *)rp->item,buffer))
      {
      Debug(" - ? Select owner match\n");
      DeleteItemList(leafattrib);
      return true;
      }
#endif  /* NOT MINGW */
   }

DeleteItemList(leafattrib);
return false;
} 

/*******************************************************************/

int SelectModeMatch(struct stat *lstatptr,struct Rlist *list)

{ mode_t newperm,plus,minus;
  struct Rlist *rp;

for  (rp = list; rp != NULL; rp=rp->next)
   {
   plus = 0;
   minus = 0;

   if (!ParseModeString(rp->item,&plus,&minus))
      {
      CfOut(cf_error,""," !! Problem validating a mode string \"%s\" in search filter",rp->item);
      continue;
      }

   newperm = (lstatptr->st_mode & 07777);
   newperm |= plus;
   newperm &= ~minus;
   
   if ((newperm & 07777) == (lstatptr->st_mode & 07777))
      {
      return true;
      }   
   }

return false;
} 

/*******************************************************************/

int SelectBSDMatch(struct stat *lstatptr,struct Rlist *bsdflags,struct Promise *pp)

{
#if defined HAVE_CHFLAGS
  u_long newflags,plus,minus;
  struct Rlist *rp;

if (!ParseFlagString(bsdflags,&plus,&minus))
   {
   CfOut(cf_error,""," !! Problem validating a BSD flag string");
   PromiseRef(cf_error,pp);
   }

newflags = (lstatptr->st_flags & CHFLAGS_MASK) ;
newflags |= plus;
newflags &= ~minus;

if ((newflags & CHFLAGS_MASK) == (lstatptr->st_flags & CHFLAGS_MASK))    /* file okay */
   {
   return true;
   }
#endif
  
return false;
} 

/*******************************************************************/

int SelectTimeMatch(time_t stattime,time_t fromtime,time_t totime)

{
return ((fromtime < stattime) && (stattime < totime));
} 

/*******************************************************************/

int SelectNameRegexMatch(char *filename,char *crit)

{
if (FullTextMatch(crit,ReadLastNode(filename)))
   {
   return true;
   }            

return false;      
}

/*******************************************************************/

int SelectPathRegexMatch(char *filename,char *crit)

{
if (FullTextMatch(crit,filename))
   {
   return true;
   }            

return false;      
}

/*******************************************************************/

int SelectExecRegexMatch(char *filename,char *crit,char *prog)

{ char line[CF_BUFSIZE];
  int s,e;
  FILE *pp;

if ((pp = cf_popen(prog,"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","Couldn't open pipe to command %s\n",crit);
   return false;
   }

while (!feof(pp))
   {
   line[0] = '\0';
   CfReadLine(line,CF_BUFSIZE,pp);  /* One buffer only */

   if (FullTextMatch(crit,line))
      {
      cf_pclose(pp); 
      return true;
      }
   }

cf_pclose(pp); 


return false;      
}

/*******************************************************************/

int SelectIsSymLinkTo(char *filename,struct Rlist *crit)

{
#ifndef MINGW
  char buffer[CF_BUFSIZE];
  struct Rlist *rp;

for (rp = crit; rp != NULL; rp = rp->next)
   {
   memset(buffer,0,CF_BUFSIZE);
   
   if (readlink(filename,buffer,CF_BUFSIZE-1) == -1)
      {
      CfOut(cf_error,"readlink","Unable to read link %s in filter",filename);
      return false;      
      }

   if (FullTextMatch(rp->item,buffer))
      {
      return true;
      }
   }
#endif  /* NOT MINGW */
return false;      
}

/*******************************************************************/

int SelectExecProgram(char *filename,char *crit)

  /* command can include $(this) for the name of the file */

{
if (ShellCommandReturnsZero(filename,false))
   {
   Debug(" - ? Select ExecProgram match for %s\n",crit);
   return true;
   }
else
   {
   return false;
   }
}


#ifndef MINGW

/*******************************************************************/
/* Unix implementations                                            */
/*******************************************************************/

int Unix_GetOwnerName(struct stat *lstatptr, char *owner, int ownerSz)

{
struct passwd *pw;

memset(owner, 0, ownerSz);
pw = getpwuid(lstatptr->st_uid);

if(pw == NULL)
  {
  CfOut(cf_error, "getpwuid", "!! Could not get owner name of user with uid=%d", lstatptr->st_uid);
  return false;
  }
  
strncpy(owner, pw->pw_name, ownerSz - 1);

return true;
}

/*******************************************************************/

int SelectGroupMatch(struct stat *lstatptr,struct Rlist *crit)

{ struct Item *leafattrib = NULL;
  char buffer[CF_SMALLBUF];
  struct group *gr;
  struct Rlist *rp;
  
sprintf(buffer,"%d",lstatptr->st_gid);
PrependItem(&leafattrib,buffer,""); 

if ((gr = getgrgid(lstatptr->st_gid)) != NULL)
   {
   PrependItem(&leafattrib,gr->gr_name,""); 
   }
else
   {
   PrependItem(&leafattrib,"none",""); 
   }

for (rp = crit; rp != NULL; rp = rp->next)
   {
   if (EvaluateORString((char *)rp->item,leafattrib,0))
      {
      Debug(" - ? Select group match\n");
      DeleteItemList(leafattrib);
      return true;
      }

   if (gr && FullTextMatch((char *)rp->item,gr->gr_name))
      {
      Debug(" - ? Select owner match\n");
      DeleteItemList(leafattrib);
      return true;
      }

   if (FullTextMatch((char *)rp->item,buffer))
      {
      Debug(" - ? Select owner match\n");
      DeleteItemList(leafattrib);
      return true;
      }
   }

DeleteItemList(leafattrib);
return false;
} 

#endif  /* NOT MINGW */
