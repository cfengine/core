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
  
if (!attr.haveselect)
   {
   return true;
   }

if (S_ISDIR(sb->st_mode) || attr.select.name == NULL)
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

if (attr.select.owners && SelectOwnerMatch(sb,attr.select.owners))
   {
   PrependItem(&leaf_attr,"owner","");
   }

if (attr.select.owners == NULL)
   {
   PrependItem(&leaf_attr,"owner","");
   }

if (attr.select.groups && SelectGroupMatch(sb,attr.select.groups))
   {
   PrependItem(&leaf_attr,"group","");
   }

if (attr.select.groups == NULL)
   {
   PrependItem(&leaf_attr,"group","");
   }

if (SelectModeMatch(sb,attr.select.plus,attr.select.minus))
   {
   PrependItem(&leaf_attr,"mode","");
   }

if (SelectTimeMatch(sb->st_atime,attr.select.min_atime,attr.select.max_atime))
   { 
   PrependItem(&leaf_attr,"atime","");
   }

if (SelectTimeMatch(sb->st_ctime,attr.select.min_ctime,attr.select.max_ctime))
   { 
   PrependItem(&leaf_attr,"ctime","");
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

int SelectTypeMatch(struct stat *lstatptr,struct Rlist *crit)

{ struct Item *leafattrib = NULL;
  struct Rlist *rp;

if (S_ISLNK(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"symlink","");
   }
 
if (S_ISREG(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"reg","");
   PrependItem(&leafattrib,"plain","");
   }

if (S_ISDIR(lstatptr->st_mode))
   {
   PrependItem(&leafattrib,"dir","");
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

int SelectOwnerMatch(struct stat *lstatptr,struct Rlist *crit)

{ struct Item *leafattrib = NULL;
  char buffer[CF_SMALLBUF];
  struct passwd *pw;
  struct Rlist *rp;

sprintf(buffer,"%d",lstatptr->st_uid);
PrependItem(&leafattrib,buffer,""); 

if ((pw = getpwuid(lstatptr->st_uid)) != NULL)
   {
   PrependItem(&leafattrib,pw->pw_name,""); 
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
   }

DeleteItemList(leafattrib);
return false;
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
   }

DeleteItemList(leafattrib);
return false;
} 

/*******************************************************************/

int SelectModeMatch(struct stat *lstatptr,mode_t plus,mode_t minus)

{ mode_t newperm;

newperm = (lstatptr->st_mode & 07777);
newperm |= plus;
newperm &= ~minus;

Debug(" - ? Select mode match?\n");
return ((newperm & 07777) == (lstatptr->st_mode & 07777));
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
   ReadLine(line,CF_BUFSIZE,pp);  /* One buffer only */

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

{ char buffer[CF_BUFSIZE];
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

