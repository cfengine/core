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

char *Rlist2String(struct Rlist *list,char *sep)

{ char line[CF_BUFSIZE];
  struct Rlist *rp;

line[0] = '\0';
  
for(rp = list; rp != NULL; rp=rp->next)
   {
   strcat(line,(char *)rp->item);

   if (rp->next)
      {
      strcat(line,sep);
      }
   }
  
return strdup(line);
}

/***************************************************************************/

int Signal2Int(char *s)

{ int i = 0;
  struct Item *ip, *names = SplitString(CF_SIGNALRANGE,',');

for (ip = names; ip != NULL; ip=ip->next)
   {
   i++;
   if (strcmp(s,ip->name) == 0)
      {
      break;
      }
   }
 
DeleteItemList(names);

switch (i)
   {
   case cfa_hup:
       return SIGHUP;
   case cfa_int:
       return SIGINT;
   case cfa_trap:
       return SIGTRAP;
   case cfa_kill:
       return SIGKILL;
   case cfa_pipe:
       return SIGPIPE;
   case cfa_cont:
       return SIGCONT;
   case cfa_abrt:
       return SIGABRT;
   case cfa_stop:
       return SIGSTOP;
   case cfa_quit:
       return SIGQUIT;
   case cfa_term:
       return SIGTERM;
   case cfa_child:
       return SIGCHLD;
   case cfa_usr1:
       return SIGUSR1;
   case cfa_usr2:
       return SIGUSR2;
   case cfa_bus:
       return SIGBUS;
   case cfa_segv:
       return SIGSEGV;
   default:
       return -1;
   }

}

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

enum representations String2Representation(char *s)

{ int i;
 static char *types[] = {"url","web","file","db","literal",NULL};

for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum representations) i;      
      }
   }

return cfk_none;
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
  char c = 'X';

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

sscanf(s,"%ld%c",&a,&c);

if (a == CF_NOINT)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed integer value %s\n",s);
   ReportError(OUTPUT);
   }
else
   {
   switch (c)
      {
      case 'k':
          a = 1000 * a;
          break;
      case 'K':
          a = 1024 * a;
          break;          
      case 'm':
          a = 1000 * 1000 * a;
          break;
      case 'M':
          a = 1024 * 1024 * a;
          break;          
      case 'g':
          a = 1000 * 1000 * 1000 * a;
          break;
      case 'G':
          a = 1024 * 1024 * 1024 * a;
          break;          
      default:          
          break;
      }
   }

return a;
}

/****************************************************************************/

mode_t Str2Mode(char *s)

{ int a = CF_UNDEFINED;

if (s == NULL)
   {
   return 0;
   }

sscanf(s,"%o",&a);

if (a == CF_UNDEFINED)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed octal value %s\n",s);
   ReportError(OUTPUT);
   }

return (mode_t)a;
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

{ struct UidList *uidlist = NULL;
  struct Rlist *rp;
  char username[CF_MAXVARSIZE];
  uid_t uid;

for (rp = uidnames; rp != NULL; rp=rp->next)
   {
   username[0] = '\0';
   uid = Str2Uid(rp->item,username,pp);
   AddSimpleUidItem(&uidlist,uid,username);
   }

if (uidlist == NULL)
   {
   AddSimpleUidItem(&uidlist,CF_SAME_OWNER,NULL);
   }

return (uidlist);
}

/*********************************************************************/

struct GidList *Rlist2GidList(struct Rlist *gidnames,struct Promise *pp)

{ struct GidList *gidlist = NULL;
  struct Rlist *rp;
  char groupname[CF_MAXVARSIZE];
  gid_t gid;
 
for (rp = gidnames; rp != NULL; rp=rp->next)
   {
   groupname[0] = '\0';
   gid = Str2Gid(rp->item,groupname,pp);
   AddSimpleGidItem(&gidlist,gid,groupname);
   }

if (gidlist == NULL)
   {
   AddSimpleGidItem(&gidlist,CF_SAME_GROUP,NULL);
   }

return(gidlist);
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

uid_t Str2Uid(char *uidbuff,char *usercopy,struct Promise *pp)

{ struct Item *ip, *tmplist;
  struct passwd *pw;
  int offset,uid = -1,tmp = -1;
  char *machine,*user,*domain;
 
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
         CfOut(cf_error,"","Unknown user \'%s\'\n",ip->name);

         if (pp != NULL)
            {
            PromiseRef(cf_error,pp);
            }

         uid = CF_UNKNOWN_OWNER; /* signal user not found */
         }
      else
         {
         uid = pw->pw_uid;

         if (usercopy != NULL)
            {
            strcpy(usercopy,ip->name);
            }
         }
      }
   
   DeleteItemList(tmplist);
   return uid;
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
      CfOut(cf_error,"","Unknown user %s\n",uidbuff);
      uid = CF_UNKNOWN_OWNER;  /* signal user not found */

      if (usercopy != NULL)
         {
         strcpy(usercopy,uidbuff);
         }
      }
   else
      {
      uid = pw->pw_uid;
      }
   }

return uid;
}

/*********************************************************************/

gid_t Str2Gid(char *gidbuff,char *groupcopy,struct Promise *pp)

{ struct group *gr;
  int gid, tmp = -1;

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
      CfOut(cf_error,"","Unknown group \'%s\'\n",gidbuff);
      PromiseRef(cf_error,pp);
      gid = CF_UNKNOWN_GROUP;
      }
   else
      {
      gid = gr->gr_gid;
      strcpy(groupcopy,gidbuff);
      }
   }

return gid;
}






