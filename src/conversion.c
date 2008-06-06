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
/* File: conversion.c                                                        */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


/***************************************************************************/

enum cfreport String2ReportLevel(char *s)

{ int i;
  static char *types[] = { "inform","verbose","error","log",NULL };

for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum cfreport) i;      
      }
   }

return cf_noreport;
}

/***************************************************************************/

enum cfhashes String2HashType(char *typestr)

{ int i;

for (i = 0; CF_DIGEST_TYPES[i][0] != NULL; i++)
   {
   if (typestr && strcmp(typestr,CF_DIGEST_TYPES[i][0]) == 0)
      {
      return (enum cfhashes)i;
      }
   }

return cf_nohash;
}

/****************************************************************************/

enum cflinktype String2LinkType(char *s)

{ int i;
  static char *types[] = { "symlink","hardlink","relative","absolute",NULL };

for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum cflinktype) i;      
      }
   }

return cfa_notlinked;
}

/****************************************************************************/

enum cfcomparison String2Comparison(char *s)

{ int i;
  static char *types[] = {"atime","mtime","ctime","checksum","binary",NULL};

for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum cfcomparison) i;      
      }
   }

return cfa_nocomparison;
}

/****************************************************************************/

enum cfsbundle Type2Cfs(char *name)

{ int i;
 
for (i = 0; i < (int)cfs_nobtype; i++)
   {
   if (name && strcmp(CF_REMACCESS_SUBTYPES[i].subtype,name)==0)
      {
      break;
      }
   }

return (enum cfsbundle)i;
}

/****************************************************************************/

enum cfdatatype Typename2Datatype(char *name)

/* convert abstract data type names: int, ilist etc */
    
{ int i;

Debug("typename2type(%s)\n",name);
 
for (i = 0; i < (int)cf_notype; i++)
   {
   if (name && strcmp(CF_DATATYPES[i],name)==0)
      {
      break;
      }
   }

return (enum cfdatatype)i;
}

/****************************************************************************/

enum cfagenttype Agent2Type(char *name)

/* convert abstract data type names: int, ilist etc */
    
{ int i;

Debug("Agent2Type(%s)\n",name);
 
for (i = 0; i < (int)cf_notype; i++)
   {
   if (name && strcmp(CF_AGENTTYPES[i],name)==0)
      {
      break;
      }
   }

return (enum cfagenttype)i;
}

/****************************************************************************/

enum cfdatatype GetControlDatatype(char *varname,struct BodySyntax *bp)

{ int i = 0;

for (i = 0; bp[i].range != NULL; i++)
   {
   if (varname && strcmp(bp[i].lval,varname) == 0)
      {
      return bp[i].dtype;
      }
   }

return cf_notype;
}

/****************************************************************************/

int GetBoolean(char *s)

{ struct Item *list = SplitString(CF_BOOL,','), *ip;
  int count = 0;

for (ip = list; ip != NULL; ip=ip->next)
   {
   if (strcmp(s,ip->name) == 0)
      {
      break;
      }

   count++;
   }

DeleteItemList(list);

if (count % 2)
   {
   return false;
   }
else
   {
   return true;
   }
}

/****************************************************************************/

long Str2Int(char *s)

{ long a = CF_NOINT;

if (s == NULL)
   {
   return CF_NOINT;
   }

if (strcmp(s,"inf") == 0)
   {
   return (long)CF_INFINITY;
   }

if (strcmp(s,"now") == 0)
   {
   return (long)CFSTARTTIME;
   }   

sscanf(s,"%ld",&a);

if (a == CF_NOINT)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed integer value %s\n",s);
   ReportError(OUTPUT);
   }

return a;
}

/****************************************************************************/

int Str2Double(char *s)

{ double a = CF_NODOUBLE;
 
if (s == NULL)
   {
   return CF_NODOUBLE;
   }

sscanf(s,"%d",&a);
 
if (a == CF_NODOUBLE)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed real value %s\n",s);
   ReportError(OUTPUT);
   }

return a;
}

/****************************************************************************/

void IntRange2Int(char *intrange,long *min,long *max,struct Promise *pp)

{ struct Item *split;
  long lmax = CF_LOWINIT, lmin = CF_HIGHINIT;
 
/* Numeric types are registered by range separated by comma str "min,max" */

if (intrange == NULL)
   {
   *min = CF_NOINT;
   *max = CF_NOINT;
   return;
   }

split = SplitString(intrange,',');

sscanf(split->name,"%ld",&lmin);

if (strcmp(split->next->name,"inf") == 0)
   {
   lmax = (long)CF_INFINITY;
   }
else
   {
   sscanf(split->next->name,"%ld",&lmax);
   }

DeleteItemList(split);

if (lmin == CF_HIGHINIT || lmax == CF_LOWINIT)
   {
   PromiseRef(cf_error,pp);
   snprintf(OUTPUT,CF_BUFSIZE,"Could not make sense of integer range [%s]",intrange);
   FatalError(OUTPUT);
   }

*min = lmin;
*max = lmax;
}

/****************************************************************************/
/* Rlist to Uid/Gid lists                                                   */
/****************************************************************************/

struct UidList *Rlist2UidList(struct Rlist *uidnames,struct Promise *pp)

{ struct UidList *uidlist;
  struct Item *ip, *tmplist;
  struct passwd *pw;
  struct Rlist *rp;
  char *uidbuff;
  int offset,uid,tmp = -1;
  char *machine, *user, *domain,*usercopy=NULL;

uidlist = NULL;

for (rp = uidnames; rp != NULL; rp=rp->next)
   {
   uidbuff = (char *)rp->item;
   
   if (uidbuff[0] == '+')        /* NIS group - have to do this in a roundabout     */
      {                          /* way because calling getpwnam spoils getnetgrent */
      offset = 1;
      if (uidbuff[1] == '@')
         {
         offset++;
         }
      
      setnetgrent(uidbuff+offset);
      tmplist = NULL;
      
      while (getnetgrent(&machine,&user,&domain))
         {
         if (user != NULL)
            {
            AppendItem(&tmplist,user,NULL);
            }
         }
      
      endnetgrent();
      
      for (ip = tmplist; ip != NULL; ip=ip->next)
         {
         if ((pw = getpwnam(ip->name)) == NULL)
            {
            CfOut(cf_error,"","Unknown user [%s]\n",ip->name);
            PromiseRef(cf_error,pp);
            uid = CF_UNKNOWN_OWNER; /* signal user not found */
            usercopy = ip->name;
            }
         else
            {
            uid = pw->pw_uid;
            }
         AddSimpleUidItem(&uidlist,uid,usercopy); 
         }
      
      DeleteItemList(tmplist);
      continue;
      }
   
   if (isdigit((int)uidbuff[0]))
      {
      sscanf(uidbuff,"%d",&tmp);
      uid = (uid_t)tmp;
      }
   else
      {
      if (strcmp(uidbuff,"*") == 0)
         {
         uid = CF_SAME_OWNER;                     /* signals wildcard */
         }
      else if ((pw = getpwnam(uidbuff)) == NULL)
         {
         if (!PARSING)
            {
            CfOut(cf_error,"","Unknown user %s\n",uidbuff);
            }
         uid = CF_UNKNOWN_OWNER;  /* signal user not found */
         usercopy = uidbuff;
         }
      else
         {
         uid = pw->pw_uid;
         }
      }
   AddSimpleUidItem(&uidlist,uid,usercopy);
   }

if (uidlist == NULL)
   {
   AddSimpleUidItem(&uidlist,CF_SAME_OWNER,(char *) NULL);
   }

return (uidlist);
}

/*********************************************************************/

struct GidList *Rlist2GidList(struct Rlist *gidnames,struct Promise *pp)

{ struct GidList *gidlist;
  struct group *gr;
  struct Rlist *rp;
  char *gidbuff,*groupcopy=NULL;
  int gid, tmp = -1;

gidlist = NULL;
 
for (rp = gidnames; rp != NULL; rp=rp->next)
   {
   gidbuff = (char *)rp->item;

   if (isdigit((int)gidbuff[0]))
      {
      sscanf(gidbuff,"%d",&tmp);
      gid = (gid_t)tmp;
      }
   else
      {
      if (strcmp(gidbuff,"*") == 0)
         {
         gid = CF_SAME_GROUP;                     /* signals wildcard */
         }
      else if ((gr = getgrnam(gidbuff)) == NULL)
         {
         CfOut(cf_error,"","Unknown group %s\n",gidbuff);
         PromiseRef(cf_error,pp);
         gid = CF_UNKNOWN_GROUP;
         groupcopy = gidbuff;
         }
      else
         {
         gid = gr->gr_gid;
         }
      }
   
   AddSimpleGidItem(&gidlist,gid,groupcopy);
   }

if (gidlist == NULL)
   {
   AddSimpleGidItem(&gidlist,CF_SAME_GROUP,NULL);
   }

return(gidlist);
}

