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
/* File: evalfunction.c                                                      */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include <libgen.h>

#ifndef MINGW
static struct Rval Unix_FnCallUserExists(struct FnCall *fp,struct Rlist *finalargs);
static struct Rval Unix_FnCallGroupExists(struct FnCall *fp,struct Rlist *finalargs);
#endif  /* NOT MINGW */
static char *StripPatterns(char *file_buffer,char *pattern,char *filename);
static void CloseStringHole(char *s,int start,int end);
static int BuildLineArray(char *array_lval,char *file_buffer,char *split,int maxent,enum cfdatatype type,int intIndex);
static int ExecModule(char *command);
static int CheckID(char *id);

/* assume args are all scalar literals by the time we get here
     and each handler allocates the memory it returns. There is
     a protocol to be followed here:
     Set args,
     Eval Content,
     Set rtype,
     ErrorFlags

     returnval = FnCallXXXResult(fp)
     
  */

/*******************************************************************/
/* FnCall API - OS function mapping                                */
/*******************************************************************/

static struct Rval FnCallUserExists(struct FnCall *fp,struct Rlist *finalargs)

{
#ifdef MINGW
return NovaWin_FnCallUserExists(fp, finalargs);
#else
return Unix_FnCallUserExists(fp, finalargs);
#endif
}

/*********************************************************************/

static struct Rval FnCallGroupExists(struct FnCall *fp,struct Rlist *finalargs)

{
#ifdef MINGW
return NovaWin_FnCallGroupExists(fp, finalargs);
#else
return Unix_FnCallGroupExists(fp, finalargs);
#endif
}

/*******************************************************************/
/* End FnCall API                                                  */
/*******************************************************************/

static struct Rlist *GetHostsFromLastseenDB(struct Item *addresses,
                                            time_t horizon, bool return_address,
                                            bool return_recent)
{
 struct Rlist *recent = NULL, *aged = NULL;
 struct Item *ip;
 time_t now = time(NULL);
 double entrytime;
 char address[CF_MAXVARSIZE];

 for(ip = addresses; ip != NULL; ip = ip->next)
    {
    if(sscanf(ip->classes,"%lf",&entrytime) != 1)
       {
       CfOut(cf_error, "", "!! Could not get host entry age");
       continue;
       }
   
    if (return_address)
       {
       snprintf(address, sizeof(address), "%s", ip->name);
       }
    else
       {
       snprintf(address, sizeof(address), "%s", IPString2Hostname(ip->name));
       }

    if (entrytime < now - horizon)
       {
       Debug("Old entry.\n");

       if (KeyInRlist(recent, address))
          {
          Debug("There is recent entry for this address. Do nothing.\n");
          }
       else
          {
          Debug("Adding to list of aged hosts.\n");
          IdempPrependRScalar(&aged, address, CF_SCALAR);
          }
       }
    else
       {
       struct Rlist *r;
       Debug("Recent entry.\n");

       if ((r = KeyInRlist(aged, address)))
          {
          Debug("Purging from list of aged hosts.\n");
          DeleteRlistEntry(&aged, r);
          }

       Debug ("Adding to list of recent hosts.\n");
       IdempPrependRScalar(&recent, address, CF_SCALAR);
       }

   
    }

 if (return_recent)
    {
    DeleteRlist(aged);
    return recent;
    }
 else
    {
    DeleteRlist(recent);
    return aged;
    }
}

/*********************************************************************/

static struct Rval FnCallAnd(struct FnCall *fp, struct Rlist *finalargs)
{
struct Rval rval;
struct Rlist *arg;
char id[CF_BUFSIZE];
snprintf(id, CF_BUFSIZE, "built-in FnCall and-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
for(arg = finalargs; arg; arg = arg->next)
   {
   CheckConstraintTypeMatch(id, arg->item, arg->type, cf_str, "", 1);
   }

for(arg = finalargs; arg; arg = arg->next)
   {
   if (!IsDefinedClass(arg->item))
      {
      SetFnCallReturnStatus("and", FNCALL_SUCCESS, NULL, NULL);
      rval.item = strdup("!any");
      rval.rtype = CF_SCALAR;
      return rval;
      }
   }

SetFnCallReturnStatus("and", FNCALL_SUCCESS, NULL, NULL);
rval.item = strdup("any");
rval.rtype = CF_SCALAR;
return rval;
}

/*******************************************************************/

static struct Rval FnCallHostsSeen(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rval rval;
 struct Rlist *returnlist = NULL, *rp;
 char *policy,*format;
 CF_DB *dbp;
 CF_DBC *dbcp;
 char name[CF_BUFSIZE];
 int horizon;
 struct Item *addresses = NULL;
 char entrytimeChr[CF_SMALLBUF];
 int ksize, vsize;
 char *key;
 void *value;

/* begin fn specific content */

 horizon = Str2Int((char *)(finalargs->item)) * 3600;
 policy = (char *)(finalargs->next->item);
 format = (char *)(finalargs->next->next->item);

 Debug("Calling hostsseen(%d,%s,%s)\n", horizon, policy, format);

 snprintf(name,CF_BUFSIZE-1,"%s%c%s",CFWORKDIR,FILE_SEPARATOR,CF_LASTDB_FILE);

 // last-seen may be used by cf-serverd when (re-)reading policy
 if (!OpenDB(name,&dbp))
    {
    SetFnCallReturnStatus("hostsseen",FNCALL_FAILURE,NULL,NULL);
    rval.item = NULL;
    rval.rtype = CF_LIST;
    return rval;
    }

 Debug("Database opened succesfully.\n");

/* Acquire a cursor for the database. */

 if (!NewDBCursor(dbp,&dbcp))
    {
    CloseDB(dbp);
    Debug("Failed to obtain cursor for database\n");
    SetFnCallReturnStatus("hostsseen",FNCALL_FAILURE,NULL,NULL);
    CfOut(cf_error,""," !! Error reading from last-seen database: ");
    rval.item = NULL;
    rval.rtype = CF_LIST;
    return rval;
    }

 /* Walk through the database and print out the key/data pairs. */

 while (NextDB(dbp, dbcp, &key, &ksize, &value, &vsize))
    {
    if (value != NULL)
       {
       struct CfKeyHostSeen entry;
      
       if (HostKeyAddressUnknown(value))
          {
          continue;
          }
      
       memcpy(&entry,value,sizeof(entry));
       snprintf(entrytimeChr, sizeof(entrytimeChr), "%.4lf", entry.Q.q);
       PrependItem(&addresses, entry.address, entrytimeChr);
       }
    }

 DeleteDBCursor(dbp,dbcp);
 CloseDB(dbp);
   
 returnlist = GetHostsFromLastseenDB(addresses, horizon,
                                     strcmp(format, "address") == 0,
                                     strcmp(policy, "lastseen") == 0);

 DeleteItemList(addresses);

 Debug(" | Return value:\n");
 for(rp = returnlist; rp; rp = rp->next)
    {
    Debug(" |  %s\n", (char *)rp->item);
    }


/* end fn specific content */

 if (returnlist == NULL)
    {
    SetFnCallReturnStatus("hostsseen",FNCALL_FAILURE,NULL,NULL);
    rval.item = NULL;
    rval.rtype = CF_LIST;
    return rval;
    }
 else
    {
    SetFnCallReturnStatus("hostsseen",FNCALL_SUCCESS,NULL,NULL);
    rval.item = returnlist;
    rval.rtype = CF_LIST;
    return rval;
    }
}


/*********************************************************************/

static struct Rval FnCallRandomInt(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  int tmp,range,result,from=-1,to=-1;
  
buffer[0] = '\0';  

/* begin fn specific content */

from = Str2Int((char *)(finalargs->item));
to = Str2Int((char *)(finalargs->next->item));

if (from == CF_NOINT || to == CF_NOINT)
   {
   SetFnCallReturnStatus("randomint",FNCALL_FAILURE,"unrecognized integer",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

if (from > to)
   {
   tmp = to;
   to = from;
   from = tmp;
   }

range = fabs(to-from);
result = from + (int)(drand48()*(double)range);
snprintf(buffer,CF_BUFSIZE-1,"%d",result);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRandomInt");
   }

/* end fn specific content */

SetFnCallReturnStatus("randomint",FNCALL_SUCCESS,NULL,NULL);
rval.rtype = CF_SCALAR;
return rval;
}


/*********************************************************************/

static struct Rval FnCallGetEnv(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE] = "",ctrlstr[CF_SMALLBUF];
  char *name;
  int limit;

/* begin fn specific content */

name = finalargs->item;
limit = Str2Int(finalargs->next->item);

snprintf(ctrlstr,CF_SMALLBUF,"%%.%ds",limit); // -> %45s

if (getenv(name))
   {
   snprintf(buffer,CF_BUFSIZE-1,ctrlstr,getenv(name));
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallGetUid");
   }

SetFnCallReturnStatus("getenv",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallGetUsers(struct FnCall *fp,struct Rlist *finalargs)
    
#ifndef MINGW
{ struct Rlist *newlist = NULL,*except_names,*except_uids;
  struct Rval rval;
  struct passwd *pw;
  char buffer[CF_BUFSIZE];
  char *except_name,*except_uid;
  
buffer[0] = '\0';  

/* begin fn specific content */

except_name = finalargs->item;
except_uid = finalargs->next->item;

except_names = SplitStringAsRList(except_name,',');
except_uids = SplitStringAsRList(except_uid,',');

setpwent();

while ((pw = getpwent()))
   {
   if (!IsStringIn(except_names,pw->pw_name) && !IsIntIn(except_uids,(int)pw->pw_uid))
      {
      IdempPrependRScalar(&newlist,pw->pw_name,CF_SCALAR);
      }
   }

endpwent();

SetFnCallReturnStatus("getusers",FNCALL_SUCCESS,NULL,NULL);

rval.item = newlist;
rval.rtype = CF_LIST;
return rval;
}
#else
{
struct Rval rval;
CfOut(cf_error,""," -> getusers is not yet implemented on Windows"); 
rval.item = NULL;
rval.rtype = CF_LIST;
return rval;
}
#endif

/*********************************************************************/

static struct Rval FnCallEscape(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *name;
  
buffer[0] = '\0';  

/* begin fn specific content */

name = finalargs->item;

EscapeSpecialChars(name,buffer,CF_BUFSIZE-1,"");

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallEscape");
   }

SetFnCallReturnStatus("escape",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallHost2IP(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *name;
  
buffer[0] = '\0';  

/* begin fn specific content */

name = finalargs->item;

if ((rval.item = strdup(Hostname2IPString(name))) == NULL)
   {
   FatalError("Memory allocation in FnCallHost2IP");
   }

SetFnCallReturnStatus("host2ip",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIP2Host(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *ip;
  
buffer[0] = '\0';  

/* begin fn specific content */

ip = finalargs->item;

if ((rval.item = strdup(IPString2Hostname(ip))) == NULL)
   {
   FatalError("Memory allocation in FnCallIP2Host");
   }

SetFnCallReturnStatus("ip2host",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}


/*********************************************************************/

static struct Rval FnCallGetUid(struct FnCall *fp,struct Rlist *finalargs)

#ifndef MINGW
{
  struct Rval rval;
  struct passwd *pw;
  char buffer[CF_BUFSIZE];
  uid_t uid;
  
buffer[0] = '\0';  

/* begin fn specific content */

if ((pw = getpwnam((char *)finalargs->item)) == NULL)
   {
   uid = CF_UNKNOWN_OWNER; /* signal user not found */
   SetFnCallReturnStatus("getuid",FNCALL_FAILURE,strerror(errno),NULL);
   }
else
   {
   uid = pw->pw_uid;
   SetFnCallReturnStatus("getuid",FNCALL_SUCCESS,NULL,NULL);
   }

snprintf(buffer,CF_BUFSIZE-1,"%d",uid);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallGetUid");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}
#else  /* MINGW */
{
struct Rval rval;
SetFnCallReturnStatus("getuid",FNCALL_FAILURE,"Windows does not have user ids",NULL);
rval.item = strdup("\0");
rval.rtype = CF_SCALAR;
return rval;
}
#endif  /* MINGW */

/*********************************************************************/

static struct Rval FnCallGetGid(struct FnCall *fp,struct Rlist *finalargs)

#ifndef MINGW
{
  struct Rval rval;
  struct group *gr;
  char buffer[CF_BUFSIZE];
  gid_t gid;
  
buffer[0] = '\0';  

/* begin fn specific content */

if ((gr = getgrnam((char *)finalargs->item)) == NULL)
   {
   gid = CF_UNKNOWN_GROUP; /* signal user not found */
   SetFnCallReturnStatus("getgid",FNCALL_FAILURE,strerror(errno),NULL);
   }
else
   {
   gid = gr->gr_gid;
   SetFnCallReturnStatus("getgid",FNCALL_SUCCESS,NULL,NULL);
   }

snprintf(buffer,CF_BUFSIZE-1,"%d",gid);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallGetGid");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}
#else  /* MINGW */
{
struct Rval rval;
SetFnCallReturnStatus("getgid",FNCALL_FAILURE,"Windows does not have group ids",NULL);
rval.item = strdup("\0");
rval.rtype = CF_SCALAR;
return rval;
}
#endif  /* MINGW */

/*********************************************************************/

static struct Rval FnCallHash(struct FnCall *fp,struct Rlist *finalargs)

/* Hash(string,md5|sha1|crypt) */
    
{
  struct Rval rval;
  char buffer[CF_BUFSIZE],*string,*typestring;
  unsigned char digest[EVP_MAX_MD_SIZE+1];
  enum cfhashes type;
  
buffer[0] = '\0';  

/* begin fn specific content */

string = finalargs->item;
typestring = finalargs->next->item;

type = String2HashType(typestring);

if (FIPS_MODE && type == cf_md5)
   {
   CfOut(cf_error,""," !! FIPS mode is enabled, and md5 is not an approved algorithm in call to hash()");
   }

HashString(string,strlen(string),digest,type);

snprintf(buffer,CF_BUFSIZE-1,"%s",HashPrint(type,digest));

/* lopp off prefix */

if ((rval.item = strdup(buffer+4)) == NULL)
   {
   FatalError("Memory allocation in FnCallHash");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallHashMatch(struct FnCall *fp,struct Rlist *finalargs)

/* HashMatch(string,md5|sha1|crypt,"abdxy98edj") */
    
{
  struct Rval rval;
  char buffer[CF_BUFSIZE],ret[CF_BUFSIZE],*string,*typestring,*compare;
  unsigned char digest[EVP_MAX_MD_SIZE+1];
  enum cfhashes type;
  
buffer[0] = '\0';  

/* begin fn specific content */

string = finalargs->item;
typestring = finalargs->next->item;
compare = finalargs->next->next->item;

type = String2HashType(typestring);
HashFile(string,digest,type);
snprintf(buffer,CF_BUFSIZE-1,"%s",HashPrint(type,digest));
CfOut(cf_verbose,""," -> File \"%s\" hashes to \"%s\", compare to \"%s\"\n",string,buffer,compare);

if (strcmp(buffer+4,compare) == 0)
   {
   strcpy(ret,"any");
   }
else
   {
   strcpy(ret,"!any");
   }

if ((rval.item = strdup(ret)) == NULL)
   {
   FatalError("Memory allocation in FnCallHashMatch");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallConcat(struct FnCall *fp, struct Rlist *finalargs)
{
struct Rval rval;
struct Rlist *arg;
char id[CF_BUFSIZE];
snprintf(id, CF_BUFSIZE, "built-in FnCall concat-arg");
char result[CF_BUFSIZE] = "";

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
for(arg = finalargs; arg; arg = arg->next)
   {
   CheckConstraintTypeMatch(id, arg->item, arg->type, cf_str, "", 1);
   }

for(arg = finalargs; arg; arg = arg->next)
   {
   if (strlcat(result, arg->item, CF_BUFSIZE) >= CF_BUFSIZE)
      {
      /* Complain */
      CfOut(cf_error, "", "!! Unable to evaluate concat() function, arguments are too long");
      SetFnCallReturnStatus("concat",FNCALL_FAILURE,"List of arguments is too long",NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;
      }
   }

SetFnCallReturnStatus("concat", FNCALL_SUCCESS, NULL, NULL);
rval.item = strdup(result);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallClassMatch(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
strcpy(buffer,"!any");

/* begin fn specific content */

if (MatchInAlphaList(VHEAP,(char *)finalargs->item))
   {
   SetFnCallReturnStatus("classmatch",FNCALL_SUCCESS,NULL,NULL);
   strcpy(buffer,"any");
   }
else if (MatchInAlphaList(VADDCLASSES,(char *)finalargs->item))
    {
    SetFnCallReturnStatus("classmatch",FNCALL_SUCCESS,NULL,NULL);
    strcpy(buffer,"any");
    }

/*
There is no case in which the function can "fail" to find an answer
SetFnCallReturnStatus("classmatch",FNCALL_FAILURE,strerror(errno),NULL);
*/

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnClassMatch");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallCountClassesMatching(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE], *string = ((char *)finalargs->item);
  struct Item *ip;
  int count = 0;
  int i = (int)*string;
  
/* begin fn specific content */

if (isalnum(i) || *string == '_')
   {
   for (ip = VHEAP.list[i]; ip != NULL; ip=ip->next)
      {
      if (FullTextMatch(string,ip->name))          
         {
         count++;
         }
      }

   for (ip = VHEAP.list[i]; ip != NULL; ip=ip->next)
      {
      if (FullTextMatch(string,ip->name))          
         {
         count++;
         }
      }
   }
else
   {
   for (i = 0; i < CF_ALPHABETSIZE; i++)
      {
      for (ip = VHEAP.list[i]; ip != NULL; ip=ip->next)
         {
         if (FullTextMatch((char *)finalargs->item,ip->name))
            {
            count++;
            }
         }
      
      for (ip = VADDCLASSES.list[i]; ip != NULL; ip=ip->next)
         {
         if (FullTextMatch((char *)finalargs->item,ip->name))
            {
            count++;
            }
         }
      }
   }

SetFnCallReturnStatus("countclassesmatching",FNCALL_SUCCESS,NULL,NULL);

snprintf(buffer,CF_MAXVARSIZE,"%d",count);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnClassMatch");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallCanonify(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  
/* begin fn specific content */

SetFnCallReturnStatus("canonify",FNCALL_SUCCESS,NULL,NULL);

if ((rval.item = strdup(CanonifyName((char *)finalargs->item))) == NULL)
   {
   FatalError("Memory allocation in FnCanonify");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallLastNode(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp,*newlist;
  struct Rval rval;
  char *name,*split;
  
/* begin fn specific content */

name = finalargs->item;
split = finalargs->next->item;

newlist = SplitRegexAsRList(name,split,100,true);

for (rp = newlist; rp != NULL; rp=rp->next)
   {
   if (rp->next == NULL)
      {
      break;
      }
   }

if (rp && rp->item)
   {
   SetFnCallReturnStatus("lastnode",FNCALL_SUCCESS,NULL,NULL);

   if ((rval.item = strdup(rp->item)) == NULL)
      {
      FatalError("Memory allocation in FnLastNode");
      }
   }
else
   {
   SetFnCallReturnStatus("lastnode",FNCALL_FAILURE,NULL,NULL);

   if ((rval.item = strdup("")) == NULL)
      {
      FatalError("Memory allocation in FnLastNode");
      }
   }

/* end fn specific content */

DeleteRlist(newlist);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallClassify(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
/* begin fn specific content */

SetFnCallReturnStatus("classify",FNCALL_SUCCESS,NULL,NULL);

if (IsDefinedClass(CanonifyName(finalargs->item)))
   {
   strcpy(buffer,"any");
   }
else
   {
   strcpy(buffer,"!any");
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnClassify");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/
/* Executions                                                        */
/*********************************************************************/

static struct Rval FnCallReturnsZero(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE],comm[CF_BUFSIZE];
  int useshell = false;
  struct stat statbuf;

buffer[0] = '\0';  

/* begin fn specific content */

if (!IsAbsoluteFileName(finalargs->item))
   {
   CfOut(cf_error,"","execresult \"%s\" does not have an absolute path\n",finalargs->item);
   SetFnCallReturnStatus("execresult",FNCALL_FAILURE,strerror(errno),NULL);
   strcpy(buffer,"!any");   
   }

if (!IsExecutable(GetArg0(finalargs->item)))
   {
   CfOut(cf_error,"","execresult \"%s\" is assumed to be executable but isn't\n",finalargs->item);
   SetFnCallReturnStatus("execresult",FNCALL_FAILURE,strerror(errno),NULL);
   strcpy(buffer,"!any");   
   }
else
   {
   if (strcmp(finalargs->next->item,"useshell") == 0)
      {
      useshell = true;
      snprintf(comm,CF_BUFSIZE,"%s", (char *)finalargs->item);
      }
   else
      {
      useshell = false;
      snprintf(comm,CF_BUFSIZE,"%s", (char *)finalargs->item);
      }
   
   if (cfstat(GetArg0(finalargs->item),&statbuf) == -1)
      {
      SetFnCallReturnStatus("returnszero",FNCALL_FAILURE,strerror(errno),NULL);   
      strcpy(buffer,"!any");   
      }
   else if (ShellCommandReturnsZero(comm,useshell))
      {
      SetFnCallReturnStatus("returnszero",FNCALL_SUCCESS,NULL,NULL);
      strcpy(buffer,"any");
      }
   else
      {
      SetFnCallReturnStatus("returnszero",FNCALL_SUCCESS,strerror(errno),NULL);   
      strcpy(buffer,"!any");
      }
   }
 
if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallReturnsZero");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallExecResult(struct FnCall *fp,struct Rlist *finalargs)

  /* execresult("/programpath",useshell|noshell) */
    
{
  struct Rval rval;
  char buffer[CF_EXPANDSIZE];
  int ret = false;

buffer[0] = '\0';  

/* begin fn specific content */

if (!IsAbsoluteFileName(finalargs->item))
   {
   CfOut(cf_error,"","execresult \"%s\" does not have an absolute path\n",finalargs->item);
   SetFnCallReturnStatus("execresult",FNCALL_FAILURE,strerror(errno),NULL);
   strcpy(buffer,"!any");   
   }

if (!IsExecutable(GetArg0(finalargs->item)))
   {
   CfOut(cf_error,"","execresult \"%s\" is assumed to be executable but isn't\n",finalargs->item);
   SetFnCallReturnStatus("execresult",FNCALL_FAILURE,strerror(errno),NULL);
   strcpy(buffer,"!any");   
   }
else
   {
   if (strcmp(finalargs->next->item,"useshell") == 0)
      {
      ret = GetExecOutput(finalargs->item,buffer,true);
      }
   else
      {
      ret = GetExecOutput(finalargs->item,buffer,false);
      }
   
   if (ret)
      {
      SetFnCallReturnStatus("execresult",FNCALL_SUCCESS,NULL,NULL);
      }
   else
      {
      SetFnCallReturnStatus("execresult",FNCALL_FAILURE,strerror(errno),NULL);
      }
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallExecResult");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallUseModule(struct FnCall *fp,struct Rlist *finalargs)

  /* usemodule("/programpath",varargs) */
    
{
  struct Rval rval;
  char buffer[CF_BUFSIZE],modulecmd[CF_BUFSIZE];
  char *command,*args;
  struct stat statbuf;

buffer[0] = '\0';  

/* begin fn specific content */

command = finalargs->item;
args = finalargs->next->item;
snprintf(modulecmd,CF_BUFSIZE,"%s%cmodules%c%s",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,command);
 
if (cfstat(GetArg0(modulecmd),&statbuf) == -1)
   {
   CfOut(cf_error,"","(Plug-in module %s not found)",modulecmd);
   SetFnCallReturnStatus("usemodule",FNCALL_FAILURE,strerror(errno),NULL);
   strcpy(buffer,"!any");
   }
else if ((statbuf.st_uid != 0) && (statbuf.st_uid != getuid()))
   {
   CfOut(cf_error,"","Module %s was not owned by uid=%d who is executing agent\n",modulecmd,getuid());
   SetFnCallReturnStatus("usemodule",FNCALL_FAILURE,strerror(errno),NULL);
   strcpy(buffer,"!any");
   }
else
   {
   if (!JoinPath(modulecmd,args))
      {
      CfOut(cf_error,"","Culprit: class list for module (shouldn't happen)\n" );
      SetFnCallReturnStatus("usemodule",FNCALL_FAILURE,strerror(errno),NULL);
      strcpy(buffer,"!any");
      }
   else
      {
      snprintf(modulecmd,CF_BUFSIZE,"%s%cmodules%c%s %s",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,command,args);
      CfOut(cf_verbose,"","Executing and using module [%s]\n",modulecmd); 

      if (ExecModule(modulecmd))
         {
         SetFnCallReturnStatus("usemodule",FNCALL_SUCCESS,strerror(errno),NULL);
         strcpy(buffer,"any");
         }
      else
         {
         SetFnCallReturnStatus("usemodule",FNCALL_FAILURE,strerror(errno),NULL);
         strcpy(buffer,"!any");
         }
      }
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallExecResult");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/
/* Misc                                                              */
/*********************************************************************/

static struct Rval FnCallSplayClass(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE],class[CF_MAXVARSIZE],hrs[CF_MAXVARSIZE];
  enum cfinterval policy;
  char *splay;
  int hash,box,hours,minblocks;
  double period;

buffer[0] = '\0';  

/* begin fn specific content */

splay = finalargs->item;
policy = Str2Interval(finalargs->next->item);

switch(policy)
   {
   default:
   case cfa_daily:
       period = 12.0*23.0; // 0-23
       break;

   case cfa_hourly:
       period = 11.0;     // 0-11
       break;
   }

SetFnCallReturnStatus("splayclass",FNCALL_SUCCESS,strerror(errno),NULL);   

hash = (double)GetHash(splay);
box = (int)(0.5 + period*hash/(double)CF_HASHTABLESIZE);

minblocks = box % 12;
hours = box / 12;

if (hours == 0)
   {
   strcpy(hrs,"any");
   }
else
   {
   snprintf(hrs,CF_MAXVARSIZE-1,"Hr%02d",hours);
   }

switch ((minblocks))
   {
   case 0: snprintf(class,CF_MAXVARSIZE,"Min00_05.%s",hrs);
           break;
   case 1: snprintf(class,CF_MAXVARSIZE,"Min05_10.%s",hrs);
           break;
   case 2: snprintf(class,CF_MAXVARSIZE,"Min10_15.%s",hrs);
           break;
   case 3: snprintf(class,CF_MAXVARSIZE,"Min15_20.%s",hrs);
           break;
   case 4: snprintf(class,CF_MAXVARSIZE,"Min20_25.%s",hrs);
           break;
   case 5: snprintf(class,CF_MAXVARSIZE,"Min25_30.%s",hrs);
           break;
   case 6: snprintf(class,CF_MAXVARSIZE,"Min30_35.%s",hrs);
           break;
   case 7: snprintf(class,CF_MAXVARSIZE,"Min35_40.%s",hrs);
           break;
   case 8: snprintf(class,CF_MAXVARSIZE,"Min40_45.%s",hrs);
           break;
   case 9: snprintf(class,CF_MAXVARSIZE,"Min45_50.%s",hrs);
           break;
   case 10: snprintf(class,CF_MAXVARSIZE,"Min50_55.%s",hrs);
            break;
   case 11: snprintf(class,CF_MAXVARSIZE,"Min55_00.%s",hrs);
            break;
   default:
       strcpy(class,"never");
       break;
   }

if (IsDefinedClass(class))
   {
   strcpy(buffer,"any");
   }
else
   {
   strcpy(buffer,"!any");
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in SplayClass");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallReadTcp(struct FnCall *fp,struct Rlist *finalargs)

 /* ReadTCP(localhost,80,'GET index.html',1000) */
    
{ struct cfagent_connection *conn = NULL;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *hostnameip,*maxbytes,*port,*sendstring;
  int val = 0, n_read = 0;
  short portnum;
  struct Attributes attr = {{0}};

memset(buffer, 0, sizeof(buffer));

/* begin fn specific content */

hostnameip = finalargs->item;
port = finalargs->next->item;
sendstring = finalargs->next->next->item;
maxbytes = finalargs->next->next->next->item;

val = Str2Int(maxbytes);
portnum = (short) Str2Int(port);

if (val < 0 || portnum < 0 || THIS_AGENT_TYPE == cf_common)
   {
   SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,"port number or maxbytes out of range",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;         
   }

rval.item = NULL;
rval.rtype = CF_NOPROMISEE;

if (val > CF_BUFSIZE-1)
   {
   CfOut(cf_error,"","Too many bytes to read from TCP port %s@%s",port,hostnameip);
   val = CF_BUFSIZE - CF_BUFFERMARGIN;
   }

Debug("Want to read %d bytes from port %d at %s\n",val,portnum,hostnameip);
    
conn = NewAgentConn();

attr.copy.force_ipv4 = false;
attr.copy.portnumber = portnum;
    
if (!ServerConnect(conn,hostnameip,attr,NULL))
   {
   CfOut(cf_inform,"socket","Couldn't open a tcp socket");
   DeleteAgentConn(conn);
   SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,strerror(errno),NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

if (strlen(sendstring) > 0)
   {
   if (SendSocketStream(conn->sd,sendstring,strlen(sendstring),0) == -1)
      {
      cf_closesocket(conn->sd);
      DeleteAgentConn(conn);
      SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,strerror(errno),NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;   
      }
   }

if ((n_read = recv(conn->sd,buffer,val,0)) == -1)
   {
   }

if (n_read == -1)
   {
   cf_closesocket(conn->sd);
   DeleteAgentConn(conn);
   SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,strerror(errno),NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;         
   }

cf_closesocket(conn->sd);
DeleteAgentConn(conn);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallReadTcp");
   }

SetFnCallReturnStatus("readtcp",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRegList(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp,*list;
  struct Rval rval;
  char buffer[CF_BUFSIZE],naked[CF_MAXVARSIZE],rettype;
  char *regex,*listvar;
  void *retval;

buffer[0] = '\0';  

/* begin fn specific content */

listvar = finalargs->item;
regex = finalargs->next->item;

if (*listvar == '@')
   {
   GetNaked(naked,listvar);
   }
else
   {
   CfOut(cf_error,"","Function reglist was promised a list called \"%s\" but this was not found\n",listvar);
   SetFnCallReturnStatus("reglist",FNCALL_FAILURE,"List was not a list found in scope",NULL);
   rval.item = strdup("!any");
   rval.rtype = CF_SCALAR;
   return rval;            
   }

if (GetVariable(CONTEXTID,naked,&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,"","Function REGLIST was promised a list called \"%s\" but this was not found\n",listvar);
   SetFnCallReturnStatus("reglist",FNCALL_FAILURE,"List was not a list found in scope",NULL);
   rval.item = strdup("!any");
   rval.rtype = CF_SCALAR;
   return rval;         
   }

if (rettype != CF_LIST)
   {
   CfOut(cf_error,"","Function reglist was promised a list called \"%s\" but this variable is not a list\n",listvar);
   SetFnCallReturnStatus("reglist",FNCALL_FAILURE,"Valid list was not found in scope",NULL);
   rval.item = strdup("!any");
   rval.rtype = CF_SCALAR;
   return rval;         
   }

list = (struct Rlist *)retval;

for (rp = list; rp != NULL; rp = rp->next)
   {
   if (FullTextMatch(regex,rp->item))
      {
      strcpy(buffer,"any");
      break;
      }
   else
      {
      strcpy(buffer,"!any");
      }
   }

SetFnCallReturnStatus("reglist",FNCALL_SUCCESS,NULL,NULL);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRegList");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRegArray(struct FnCall *fp,struct Rlist *finalargs)

{ char lval[CF_MAXVARSIZE],scopeid[CF_MAXVARSIZE];
  char *regex,*arrayname,match[CF_MAXVARSIZE],buffer[CF_BUFSIZE];
  struct Scope *ptr;
  struct Rval rval;
  HashIterator i;
  CfAssoc *assoc;

/* begin fn specific content */

arrayname = finalargs->item;
regex = finalargs->next->item;

/* Locate the array */

if (strstr(arrayname,"."))
   {
   scopeid[0] = '\0';
   sscanf(arrayname,"%[^.].%s",scopeid,lval);
   }
else
   {
   strcpy(lval,arrayname);
   strcpy(scopeid,CONTEXTID);
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_error,"","Function regarray was promised an array called \"%s\" but this was not found\n",arrayname);
   SetFnCallReturnStatus("regarray",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = strdup("!any");
   rval.rtype = CF_SCALAR;
   return rval;            
   }

strcpy(buffer,"!any");

i = HashIteratorInit(ptr->hashtable);

while ((assoc = HashIteratorNext(&i)))
   {
   snprintf(match,CF_MAXVARSIZE,"%s[",lval);
   if (strncmp(match,assoc->lval,strlen(match)) == 0)
      {
      if (FullTextMatch(regex,assoc->rval))
         {
         strcpy(buffer,"any");
         break;
         }
      }
   }

SetFnCallReturnStatus("regarray",FNCALL_SUCCESS,NULL,NULL);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRegList");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}


/*********************************************************************/

static struct Rval FnCallGetIndices(struct FnCall *fp,struct Rlist *finalargs)

{ char lval[CF_MAXVARSIZE],scopeid[CF_MAXVARSIZE];
  char *arrayname,index[CF_MAXVARSIZE],match[CF_MAXVARSIZE];
  struct Scope *ptr;
  struct Rval rval;
  struct Rlist *returnlist = NULL;
  HashIterator i;
  CfAssoc *assoc;

/* begin fn specific content */

arrayname = finalargs->item;

/* Locate the array */

if (strstr(arrayname,"."))
   {
   scopeid[0] = '\0';
   sscanf(arrayname,"%127[^.].%127s",scopeid,lval);
   }
else
   {
   strcpy(lval,arrayname);
   strcpy(scopeid,CONTEXTID);
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_verbose,"","Function getindices was promised an array called \"%s\" in scope \"%s\" but this was not found\n",lval,scopeid);
   SetFnCallReturnStatus("getindices",FNCALL_SUCCESS,"Array not found in scope",NULL);
   IdempAppendRScalar(&returnlist,CF_NULL_VALUE,CF_SCALAR);
   rval.item = returnlist;
   rval.rtype = CF_LIST;
   return rval;            
   }

i = HashIteratorInit(ptr->hashtable);

while ((assoc = HashIteratorNext(&i)))
   {
   snprintf(match,CF_MAXVARSIZE-1,"%.127s[",lval);

   if (strncmp(match,assoc->lval,strlen(match)) == 0)
      {
      char *sp;
      index[0] = '\0';
      sscanf(assoc->lval+strlen(match),"%127[^\n]",index);
      if ((sp = strchr(index,']')))
         {
         *sp = '\0';
         }
      else
         {
         index[strlen(index)-1] = '\0';
         }

      if (strlen(index) > 0)
         {
         IdempAppendRScalar(&returnlist,index,CF_SCALAR);
         }
      }
   }

if (returnlist == NULL)
   {
   IdempAppendRScalar(&returnlist,CF_NULL_VALUE,CF_SCALAR);
   }

SetFnCallReturnStatus("getindices",FNCALL_SUCCESS,NULL,NULL);
rval.item = returnlist;

/* end fn specific content */

rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

static struct Rval FnCallGetValues(struct FnCall *fp,struct Rlist *finalargs)

{ char lval[CF_MAXVARSIZE],scopeid[CF_MAXVARSIZE];
  char *arrayname,match[CF_MAXVARSIZE];
  struct Scope *ptr;
  struct Rval rval;
  struct Rlist *rp,*returnlist = NULL;
  HashIterator i;
  CfAssoc *assoc;

/* begin fn specific content */

arrayname = finalargs->item;

/* Locate the array */

if (strstr(arrayname,"."))
   {
   scopeid[0] = '\0';
   sscanf(arrayname,"%127[^.].%127s",scopeid,lval);
   }
else
   {
   strcpy(lval,arrayname);
   strcpy(scopeid,CONTEXTID);
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_verbose,"","Function getvalues was promised an array called \"%s\" in scope \"%s\" but this was not found\n",lval,scopeid);
   SetFnCallReturnStatus("getvalues",FNCALL_SUCCESS,"Array not found in scope",NULL);
   IdempAppendRScalar(&returnlist,CF_NULL_VALUE,CF_SCALAR);
   rval.item = returnlist;
   rval.rtype = CF_LIST;
   return rval;            
   }

i = HashIteratorInit(ptr->hashtable);

while ((assoc = HashIteratorNext(&i)))
   {
   snprintf(match,CF_MAXVARSIZE-1,"%.127s[",lval);

   if (strncmp(match,assoc->lval,strlen(match)) == 0)
      {
      switch(assoc->rtype)
         {
         case CF_SCALAR:
            IdempAppendRScalar(&returnlist,assoc->rval,CF_SCALAR);
            break;

         case CF_LIST:
            for (rp = assoc->rval; rp != NULL; rp = rp->next)
               {
               IdempAppendRScalar(&returnlist,assoc->rval,CF_SCALAR);
               }
            break;
         }
      }
   }

if (returnlist == NULL)
   {
   IdempAppendRScalar(&returnlist,CF_NULL_VALUE,CF_SCALAR);
   }

SetFnCallReturnStatus("getindices",FNCALL_SUCCESS,NULL,NULL);
rval.item = returnlist;

/* end fn specific content */

rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

static struct Rval FnCallGrep(struct FnCall *fp,struct Rlist *finalargs)

{ char lval[CF_MAXVARSIZE];
  char *name,*regex,scopeid[CF_MAXVARSIZE];
  struct Rval rval,rval2;
  struct Rlist *rp,*returnlist = NULL;
  struct Scope *ptr;

/* begin fn specific content */

regex = finalargs->item;
name = finalargs->next->item;

/* Locate the array */

if (strstr(name,"."))
   {
   scopeid[0] = '\0';
   sscanf(name,"%[^127.].%127s",scopeid,lval);
   }
else
   {
   strcpy(lval,name);
   strcpy(scopeid,CONTEXTID);
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_error,"","Function \"grep\" was promised an array in scope \"%s\" but this was not found\n",scopeid);
   SetFnCallReturnStatus("getindices",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_LIST;
   return rval;            
   }

if (GetVariable(scopeid,lval,&rval2.item,&rval2.rtype) == cf_notype)
   {
   CfOut(cf_error,"","Function \"grep\" was promised a list called \"%s\" but this was not found\n",name);
   SetFnCallReturnStatus("getindices",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_LIST;
   return rval;
   }

if (rval2.rtype != CF_LIST)
   {
   CfOut(cf_error,"","Function grep was promised a list called \"%s\" but this was not found\n",name);
   SetFnCallReturnStatus("getindices",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_LIST;
   return rval;
   }

AppendRScalar(&returnlist,CF_NULL_VALUE,CF_SCALAR);

for (rp = (struct Rlist *)rval2.item; rp != NULL; rp=rp->next)
   {
   if (FullTextMatch(regex,rp->item))
      {
      AppendRScalar(&returnlist,rp->item,CF_SCALAR);
      }
   }

SetFnCallReturnStatus("grep",FNCALL_SUCCESS,NULL,NULL);
rval.item = returnlist;

/* end fn specific content */

rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

static struct Rval FnCallSum(struct FnCall *fp,struct Rlist *finalargs)

{ char lval[CF_MAXVARSIZE],buffer[CF_MAXVARSIZE];
  char *name,scopeid[CF_MAXVARSIZE];
  struct Rval rval,rval2;
  struct Rlist *rp;
  struct Scope *ptr;
  double sum = 0;

/* begin fn specific content */

name = finalargs->item;

/* Locate the array */

if (strstr(name,"."))
   {
   scopeid[0] = '\0';
   sscanf(name,"%[^127.].%127s",scopeid,lval);
   }
else
   {
   strcpy(lval,name);
   strcpy(scopeid,CONTEXTID);
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_error,"","Function \"sum\" was promised a list in scope \"%s\" but this was not found\n",scopeid);
   SetFnCallReturnStatus("sum",FNCALL_FAILURE,"List not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;            
   }

if (GetVariable(scopeid,lval,&rval2.item,&rval2.rtype) == cf_notype)
   {
   CfOut(cf_error,"","Function \"sum\" was promised a list called \"%s\" but this was not found\n",name);
   SetFnCallReturnStatus("sum",FNCALL_FAILURE,"List not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

if (rval2.rtype != CF_LIST)
   {
   CfOut(cf_error,"","Function \"sum\" was promised a list called \"%s\" but this was not found\n",name);
   SetFnCallReturnStatus("sum",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

for (rp = (struct Rlist *)rval2.item; rp != NULL; rp=rp->next)
   {
   double x;
   
   if ((x = Str2Double(rp->item)) == CF_NODOUBLE)
      {
      SetFnCallReturnStatus("sum",FNCALL_FAILURE,"Illegal real number",NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;
      }
   else
      {
      sum += x;
      }
   }

snprintf(buffer,CF_MAXVARSIZE,"%lf",sum);

SetFnCallReturnStatus("sum",FNCALL_SUCCESS,NULL,NULL);
rval.item = strdup(buffer);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallProduct(struct FnCall *fp,struct Rlist *finalargs)

{ char lval[CF_MAXVARSIZE],buffer[CF_MAXVARSIZE];
  char *name,scopeid[CF_MAXVARSIZE];
  struct Rval rval,rval2;
  struct Rlist *rp;
  struct Scope *ptr;
  double product = 1.0;

/* begin fn specific content */

name = finalargs->item;

/* Locate the array */

if (strstr(name,"."))
   {
   scopeid[0] = '\0';
   sscanf(name,"%[^127.].%127s",scopeid,lval);
   }
else
   {
   strcpy(lval,name);
   strcpy(scopeid,CONTEXTID);
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_error,"","Function \"product\" was promised a list in scope \"%s\" but this was not found\n",scopeid);
   SetFnCallReturnStatus("product",FNCALL_FAILURE,"List not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;            
   }

if (GetVariable(scopeid,lval,&rval2.item,&rval2.rtype) == cf_notype)
   {
   CfOut(cf_error,"","Function \"product\" was promised a list called \"%s\" but this was not found\n",name);
   SetFnCallReturnStatus("product",FNCALL_FAILURE,"List not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

if (rval2.rtype != CF_LIST)
   {
   CfOut(cf_error,"","Function \"product\" was promised a list called \"%s\" but this was not found\n",name);
   SetFnCallReturnStatus("product",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

for (rp = (struct Rlist *)rval2.item; rp != NULL; rp=rp->next)
   {
   double x;
   
   if ((x = Str2Double(rp->item)) == CF_NODOUBLE)
      {
      SetFnCallReturnStatus("product",FNCALL_FAILURE,"Illegal real number",NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;
      }
   else
      {
      product *= x;
      }
   }

snprintf(buffer,CF_MAXVARSIZE,"%lf",product);

SetFnCallReturnStatus("product",FNCALL_SUCCESS,NULL,NULL);
rval.item = strdup(buffer);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallJoin(struct FnCall *fp,struct Rlist *finalargs)

{ char lval[CF_MAXVARSIZE],*joined;
  char *name,*join,scopeid[CF_MAXVARSIZE];
  struct Rval rval,rval2;
  struct Rlist *rp;
  struct Scope *ptr;
  int size = 0;

/* begin fn specific content */

join = finalargs->item;
name = finalargs->next->item;

/* Locate the array */

if (strstr(name,"."))
   {
   scopeid[0] = '\0';
   sscanf(name,"%[^.].%127s",scopeid,lval);
   }
else
   {
   strcpy(lval,name);
   strcpy(scopeid,"this");
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_error,"","Function \"join\" was promised an array in scope \"%s\" but this was not found\n",scopeid);
   SetFnCallReturnStatus("join",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;            
   }

if (GetVariable(scopeid,lval,&rval2.item,&rval2.rtype) == cf_notype)
   {
   CfOut(cf_verbose,"","Function \"join\" was promised a list called \"%s.%s\" but this was not (yet) found\n",scopeid,name);
   SetFnCallReturnStatus("join",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

if (rval2.rtype != CF_LIST)
   {
   CfOut(cf_verbose,"","Function \"join\" was promised a list called \"%s\" but this was not (yet) found\n",name);
   SetFnCallReturnStatus("join",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }


for (rp = (struct Rlist *)rval2.item; rp != NULL; rp=rp->next)
   {
   if (strcmp(rp->item,CF_NULL_VALUE) == 0)
      {
      continue;
      }

   size += strlen(rp->item) + strlen(join);
   }

if ((joined = malloc(size)) == NULL)
   {
   CfOut(cf_error,"malloc","Function \"join\" was not able to allocate memory\n",name);
   SetFnCallReturnStatus("join",FNCALL_FAILURE,"Memory error",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

size = 0;

for (rp = (struct Rlist *)rval2.item; rp != NULL; rp=rp->next)
   {
   if (strcmp(rp->item,CF_NULL_VALUE) == 0)
      {
      continue;
      }
   
   strcpy(joined+size,rp->item);

   if (rp->next != NULL)
      {
      strcpy(joined+size+strlen(rp->item),join);
      size += strlen(rp->item) + strlen(join);
      }
   }

SetFnCallReturnStatus("grep",FNCALL_SUCCESS,NULL,NULL);
rval.item = joined;

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallGetFields(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rval rval;
  struct Rlist *rp,*newlist;
  char *filename,*regex,*array_lval,*split;
  char name[CF_MAXVARSIZE],line[CF_BUFSIZE],retval[CF_SMALLBUF];
  int lcount = 0,vcount = 0,nopurge = true;
  FILE *fin;
  
/* begin fn specific content */

regex = finalargs->item;
filename = finalargs->next->item;
split = finalargs->next->next->item;
array_lval = finalargs->next->next->next->item;

if ((fin = fopen(filename,"r")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! File \"%s\" could not be read in getfields()",filename);
   SetFnCallReturnStatus("getfields",FNCALL_FAILURE,"File unreadable",NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;               
   }

while (!feof(fin))
   {
   line[0] = '\0';
   fgets(line,CF_BUFSIZE-1,fin);
   Chop(line);
   
   if (feof(fin))
      {
      break;
      }

   if (!FullTextMatch(regex,line))
      {
      continue;
      }

   if (lcount == 0)
      {
      newlist = SplitRegexAsRList(line,split,31,nopurge);
      
      vcount = 1;
      
      for (rp = newlist; rp != NULL; rp=rp->next)
         {
         snprintf(name,CF_MAXVARSIZE-1,"%s[%d]",array_lval,vcount);
         NewScalar(THIS_BUNDLE,name,rp->item,cf_str);
         CfOut(cf_verbose,""," -> getfields: defining %s = %s\n",name,rp->item);
         vcount++;
         }
      }
   
   lcount++;
   }

fclose(fin);

snprintf(retval,CF_SMALLBUF-1,"%d",lcount);

SetFnCallReturnStatus("getfields",FNCALL_SUCCESS,NULL,NULL);
rval.item = strdup(retval);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallCountLinesMatching(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rval rval;
  char *filename,*regex;
  char line[CF_BUFSIZE],retval[CF_SMALLBUF];
  int lcount = 0;
  FILE *fin;
  
/* begin fn specific content */

regex = finalargs->item;
filename = finalargs->next->item;

if ((fin = fopen(filename,"r")) == NULL)
   {
   CfOut(cf_verbose,"fopen"," !! File \"%s\" could not be read in countlinesmatching()",filename);
   snprintf(retval,CF_SMALLBUF-1,"0");
   SetFnCallReturnStatus("countlinesmatching",FNCALL_SUCCESS,NULL,NULL);
   rval.item = strdup(retval);
   rval.rtype = CF_SCALAR;
   return rval;               
   }

while (!feof(fin))
   {
   line[0] = '\0';
   fgets(line,CF_BUFSIZE-1,fin);
   Chop(line);

   if (feof(fin))
      {
      break;
      }

   if (FullTextMatch(regex,line))
      {
      lcount++;
      CfOut(cf_verbose,""," -> countlinesmatching: matched \"%s\"",line);
      continue;
      }
   }

fclose(fin);

snprintf(retval,CF_SMALLBUF-1,"%d",lcount);

SetFnCallReturnStatus("countlinesmatching",FNCALL_SUCCESS,NULL,NULL);
rval.item = strdup(retval);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallSelectServers(struct FnCall *fp,struct Rlist *finalargs)

 /* ReadTCP(localhost,80,'GET index.html',1000) */
    
{ struct cfagent_connection *conn = NULL;
  struct Rlist *rp,*hostnameip;
  struct Rval rval;
  char buffer[CF_BUFSIZE],naked[CF_MAXVARSIZE],rettype;
  char *maxbytes,*port,*sendstring,*regex,*array_lval,*listvar;
  int val = 0, n_read = 0,count = 0;
  short portnum;
  struct Attributes attr = {{0}};
  void *retval;
  struct Promise *pp;

buffer[0] = '\0';  

/* begin fn specific content */

listvar = finalargs->item;
port = finalargs->next->item;
sendstring = finalargs->next->next->item;
regex = finalargs->next->next->next->item;
maxbytes = finalargs->next->next->next->next->item;
array_lval = finalargs->next->next->next->next->next->item;

if (*listvar == '@')
   {
   GetNaked(naked,listvar);
   }
else
   {
   CfOut(cf_error,"","Function selectservers was promised a list called \"%s\" but this was not found\n",listvar);
   SetFnCallReturnStatus("selectservers",FNCALL_FAILURE,"Host list was not a list found in scope",NULL);
   snprintf(buffer,CF_MAXVARSIZE-1,"%d",count);
   rval.item = strdup(buffer);
   rval.rtype = CF_SCALAR;
   return rval;            
   }

if (GetVariable(CONTEXTID,naked,&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,"","Function selectservers was promised a list called \"%s\" but this was not found from context %s.%s\n",listvar,CONTEXTID,naked);
   SetFnCallReturnStatus("selectservers",FNCALL_FAILURE,"Host list was not a list found in scope",NULL);
   snprintf(buffer,CF_MAXVARSIZE-1,"%d",count);
   rval.item = strdup(buffer);
   rval.rtype = CF_SCALAR;
   return rval;         
   }

if (rettype != CF_LIST)
   {
   CfOut(cf_error,"","Function selectservers was promised a list called \"%s\" but this variable is not a list\n",listvar);
   SetFnCallReturnStatus("selectservers",FNCALL_FAILURE,"Valid list was not found in scope",NULL);
   snprintf(buffer,CF_MAXVARSIZE-1,"%d",count);
   rval.item = strdup(buffer);
   rval.rtype = CF_SCALAR;
   return rval;         
   }

hostnameip = (struct Rlist *)retval;
val = Str2Int(maxbytes);
portnum = (short) Str2Int(port);

if (val < 0 || portnum < 0)
   {
   SetFnCallReturnStatus("selectservers",FNCALL_FAILURE,"port number or maxbytes out of range",NULL);
   snprintf(buffer,CF_MAXVARSIZE-1,"%d",count);
   rval.item = strdup(buffer);
   rval.rtype = CF_SCALAR;
   return rval;         
   }

rval.item = NULL;
rval.rtype = CF_NOPROMISEE;

if (val > CF_BUFSIZE-1)
   {
   CfOut(cf_error,"","Too many bytes specificed in selectservers",port);
   val = CF_BUFSIZE - CF_BUFFERMARGIN;
   }

if (THIS_AGENT_TYPE != cf_agent)
   {
   snprintf(buffer,CF_MAXVARSIZE-1,"%d",count);
   rval.item = strdup(buffer);
   rval.rtype = CF_SCALAR;
   return rval;         
   }

pp = NewPromise("select_server","function"); 

for (rp = hostnameip; rp != NULL; rp=rp->next)
   {
   Debug("Want to read %d bytes from port %d at %s\n",val,portnum, (char *)rp->item);
   
   conn = NewAgentConn();
   
   attr.copy.force_ipv4 = false;
   attr.copy.portnumber = portnum;
   
   if (!ServerConnect(conn,rp->item,attr,pp))
      {
      CfOut(cf_inform,"socket","Couldn't open a tcp socket");
      DeleteAgentConn(conn);
      continue;
      }
   
   if (strlen(sendstring) > 0)
      {
      if (SendSocketStream(conn->sd,sendstring,strlen(sendstring),0) == -1)
         {
         cf_closesocket(conn->sd);
         DeleteAgentConn(conn);
         continue;
         }
      
      if ((n_read = recv(conn->sd,buffer,val,0)) == -1)
         {
         }
            
      if (n_read == -1)
         {
         cf_closesocket(conn->sd);
         DeleteAgentConn(conn);
         continue;
         }

      if (strlen(regex) == 0 || FullTextMatch(regex,buffer))
         {
         CfOut(cf_verbose,"","Host %s is alive and responding correctly\n",rp->item);
         snprintf(buffer,CF_MAXVARSIZE-1,"%s[%d]",array_lval,count);
         NewScalar(CONTEXTID,buffer,rp->item,cf_str);
         count++;
         }
      }
   else
      {
      CfOut(cf_verbose,"","Host %s is alive\n",rp->item);
      snprintf(buffer,CF_MAXVARSIZE-1,"%s[%d]",array_lval,count);
      NewScalar(CONTEXTID,buffer,rp->item,cf_str);

      if (IsDefinedClass(CanonifyName(rp->item)))
         {
         CfOut(cf_verbose,"","This host is in the list and has promised to join the class %s - joined\n",array_lval);
         NewClass(array_lval);
         }
      
      count++;
      }
   
   cf_closesocket(conn->sd);
   DeleteAgentConn(conn);
   }

DeletePromise(pp);

/* Return the subset that is alive and responding correctly */

/* Return the number of lines in array */

snprintf(buffer,CF_MAXVARSIZE-1,"%d",count);
rval.item = strdup(buffer);

SetFnCallReturnStatus("selectservers",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIsNewerThan(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat frombuf,tobuf;
  
buffer[0] = '\0';  

/* begin fn specific content */

if (cfstat(finalargs->item,&frombuf) == -1)
   {
   SetFnCallReturnStatus("isnewerthan",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (cfstat(finalargs->next->item,&tobuf) == -1)
   {
   SetFnCallReturnStatus("isnewerthan",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (frombuf.st_mtime > tobuf.st_mtime)
   {
   strcpy(buffer,"any");
   SetFnCallReturnStatus("isnewerthan",FNCALL_SUCCESS,NULL,NULL);   
   }
else
   {
   strcpy(buffer,"!any");
   SetFnCallReturnStatus("isnewerthan",FNCALL_SUCCESS,strerror(errno),NULL);   
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallNewerThan");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIsAccessedBefore(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat frombuf,tobuf;

buffer[0] = '\0';  

/* begin fn specific content */

if (cfstat(finalargs->item,&frombuf) == -1)
   {
   SetFnCallReturnStatus("isaccessedbefore",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (cfstat(finalargs->next->item,&tobuf) == -1)
   {
   SetFnCallReturnStatus("isaccessedbefore",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (frombuf.st_atime < tobuf.st_atime)
   {
   strcpy(buffer,"any");
   SetFnCallReturnStatus("isaccessedbefore",FNCALL_SUCCESS,NULL,NULL);   
   }
else
   {
   strcpy(buffer,"!any");
   SetFnCallReturnStatus("isaccessedbefore",FNCALL_SUCCESS,NULL,NULL);   
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallIsAccessedBefore");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIsChangedBefore(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat frombuf,tobuf;
  
buffer[0] = '\0';  

/* begin fn specific content */

if (cfstat(finalargs->item,&frombuf) == -1)
   {
   SetFnCallReturnStatus("changedbefore",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (cfstat(finalargs->next->item,&tobuf) == -1)
   {
   SetFnCallReturnStatus("changedbefore",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (frombuf.st_ctime > tobuf.st_ctime)
   {
   strcpy(buffer,"any");
   SetFnCallReturnStatus("changedbefore",FNCALL_SUCCESS,NULL,NULL);   
   }
else
   {
   strcpy(buffer,"!any");
   SetFnCallReturnStatus("changedbefore",FNCALL_SUCCESS,NULL,NULL);   
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallFileStat(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat statbuf;
  
buffer[0] = '\0';  

/* begin fn specific content */

if (lstat(finalargs->item,&statbuf) == -1)
   {
   if (!strcmp(fp->name, "filesize"))
      {      
      strcpy(buffer,"-1");
      SetFnCallReturnStatus(fp->name,FNCALL_FAILURE,NULL,NULL);
      }
   else
      {
      strcpy(buffer,"!any");
      SetFnCallReturnStatus(fp->name,FNCALL_SUCCESS,NULL,NULL);
      }
   }
else
   {
   strcpy(buffer,"!any");

   if (!strcmp(fp->name, "isexecutable"))
      {
      if (IsExecutable(finalargs->item))
         {
         strcpy(buffer,"any");
         }
      }
   else if (!strcmp(fp->name, "isdir"))
      {
      if (S_ISDIR(statbuf.st_mode))
         {
         strcpy(buffer,"any");
         }
      }
   else if (!strcmp(fp->name, "islink"))
      {
      if (S_ISLNK(statbuf.st_mode))
         {
         strcpy(buffer,"any");
         }
      }
   else if (!strcmp(fp->name, "isplain"))
      {
      if (S_ISREG(statbuf.st_mode))
         {
         strcpy(buffer,"any");
         }
      }
   else if (!strcmp(fp->name, "fileexists"))
      {
      strcpy(buffer,"any");
      }
   else if (!strcmp(fp->name, "filesize"))
      {
      snprintf(buffer,CF_MAXVARSIZE,"%ld",statbuf.st_size);
      }

   SetFnCallReturnStatus(fp->name,FNCALL_SUCCESS,NULL,NULL);
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallFileStat");
   }

/* end fn specific content */
   
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIPRange(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct Item *ip;
  
buffer[0] = '\0';  

/* begin fn specific content */

strcpy(buffer,"!any");

if (!FuzzyMatchParse(finalargs->item))
   {
   strcpy(buffer,"!any");
   SetFnCallReturnStatus("IPRange",FNCALL_FAILURE,NULL,NULL);   
   }
else
   {
   SetFnCallReturnStatus("IPRange",FNCALL_SUCCESS,NULL,NULL);  

   for (ip = IPADDRESSES; ip != NULL; ip = ip->next)
      {
      Debug("Checking IP Range against RDNS %s\n",VIPADDRESS);
      
      if (FuzzySetMatch(finalargs->item,VIPADDRESS) == 0)
         {
         Debug("IPRange Matched\n");
         strcpy(buffer,"any");
         break;
         }
      else
         {
         Debug("Checking IP Range against iface %s\n",ip->name);
         
         if (FuzzySetMatch(finalargs->item,ip->name) == 0)
            {
            Debug("IPRange Matched\n");
            strcpy(buffer,"any");
            break;
            }
         }
      }
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}


/*********************************************************************/

static struct Rval FnCallHostRange(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
buffer[0] = '\0';  

/* begin fn specific content */

strcpy(buffer,"!any");

if (!FuzzyHostParse(finalargs->item,finalargs->next->item))
   {
   strcpy(buffer,"!any");
   SetFnCallReturnStatus("IPRange",FNCALL_FAILURE,NULL,NULL);   
   }
else if (FuzzyHostMatch(finalargs->item,finalargs->next->item,VUQNAME) == 0)
   {
   strcpy(buffer,"any");
   SetFnCallReturnStatus("IPRange",FNCALL_SUCCESS,NULL,NULL);   
   }
else
   {
   strcpy(buffer,"!any");
   SetFnCallReturnStatus("IPRange",FNCALL_SUCCESS,NULL,NULL);   
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallHostInNetgroup(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *machine, *user, *domain;
   
buffer[0] = '\0';  

/* begin fn specific content */

strcpy(buffer,"!any");

setnetgrent(finalargs->item);

while (getnetgrent(&machine,&user,&domain))
   {
   if (strcmp(machine,VUQNAME) == 0)
      {
      CfOut(cf_verbose,"","Matched %s in netgroup %s\n",machine,finalargs->item);
      strcpy(buffer,"any");
      break;
      }
   
   if (strcmp(machine,VFQNAME) == 0)
      {
      CfOut(cf_verbose,"","Matched %s in netgroup %s\n",machine,finalargs->item);
      strcpy(buffer,"any");
      break;
      }
   }

endnetgrent();

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIsVariable(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
buffer[0] = '\0';  

/* begin fn specific content */

SetFnCallReturnStatus("isvariable",FNCALL_SUCCESS,NULL,NULL);   

if (DefinedVariable(finalargs->item))
   {
   strcpy(buffer,"any");
   }
else
   {
   strcpy(buffer,"!any");
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

SetFnCallReturnStatus("isvariable",FNCALL_SUCCESS,NULL,NULL);   

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallStrCmp(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
buffer[0] = '\0';  

/* begin fn specific content */

SetFnCallReturnStatus("strcmp",FNCALL_SUCCESS,NULL,NULL);   

if (strcmp(finalargs->item,finalargs->next->item) == 0)
   {
   strcpy(buffer,"any");
   }
else
   {
   strcpy(buffer,"!any");
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

SetFnCallReturnStatus("strcmp",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}


/*********************************************************************/

static struct Rval FnCallTranslatePath(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[MAX_FILENAME];
  
buffer[0] = '\0';  

/* begin fn specific content */

snprintf(buffer, sizeof(buffer), "%s", (char *)finalargs->item);
MapName(buffer);


if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

SetFnCallReturnStatus("translatepath",FNCALL_SUCCESS,NULL,NULL);   

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRegistryValue(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
buffer[0] = '\0';  

/* begin fn specific content */

if (GetRegistryValue(finalargs->item,finalargs->next->item,buffer,sizeof(buffer)))
   {
   SetFnCallReturnStatus("registryvalue",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("registryvalue",FNCALL_FAILURE,NULL,NULL);
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRegistrtValue");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRemoteScalar(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *handle,*server;
  int encrypted;
  
buffer[0] = '\0';  

/* begin fn specific content */

handle = finalargs->item;
server = finalargs->next->item;
encrypted = GetBoolean(finalargs->next->next->item);

if (strcmp(server,"localhost") == 0)
   {
   /* The only reason for this is testing...*/
   server = "127.0.0.1";
   }

if (THIS_AGENT_TYPE == cf_common)
   {
   if ((rval.item = strdup("<remote scalar>")) == NULL)
      {
      FatalError("Memory allocation in FnCallRemoteSCalar");
      }
   }
else
   {
   GetRemoteScalar("VAR",handle,server,encrypted,buffer);
   
   if (strncmp(buffer,"BAD:",4) == 0)
      {
      if (RetrieveUnreliableValue("remotescalar",handle,buffer))
         {
         SetFnCallReturnStatus("remotescalar",FNCALL_SUCCESS,NULL,NULL);
         }
      else
         {
         // This function should never fail
         buffer[0] = '\0';
         SetFnCallReturnStatus("remotescalar",FNCALL_SUCCESS,NULL,NULL);
         }
      }
   else
      {
      SetFnCallReturnStatus("remotescalar",FNCALL_SUCCESS,NULL,NULL);
      CacheUnreliableValue("remotescalar",handle,buffer);
      }
   
   if ((rval.item = strdup(buffer)) == NULL)
      {
      FatalError("Memory allocation in FnCallRemoteSCalar");
      }
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallHubKnowledge(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *handle;
  
buffer[0] = '\0';  

/* begin fn specific content */

handle = finalargs->item;

if (THIS_AGENT_TYPE != cf_agent)
   {
   if ((rval.item = strdup("<inaccessible remote scalar>")) == NULL)
      {
      FatalError("Memory allocation in FnCallRemoteSCalar");
      }
   }
else
   {
   CfOut(cf_verbose,""," -> Accessing hub knowledge bank for \"%s\"",handle);
   GetRemoteScalar("VAR",handle,POLICY_SERVER,true,buffer);

   // This should always be successful - and this one doesn't cache
   
   if (strncmp(buffer,"BAD:",4) == 0)
      {
      snprintf(buffer,CF_MAXVARSIZE,"0");
      }
   
   if ((rval.item = strdup(buffer)) == NULL)
      {
      FatalError("Memory allocation in FnCallRemoteSCalar");
      }
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRemoteClassesMatching(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp,*classlist;
  struct Rval rval;  
  char buffer[CF_BUFSIZE],class[CF_MAXVARSIZE];
  char *server,*regex,*prefix;
  int encrypted;
  
buffer[0] = '\0';  

/* begin fn specific content */

regex = finalargs->item;
server = finalargs->next->item;
encrypted = GetBoolean(finalargs->next->next->item);
prefix = finalargs->next->next->next->item;
    
if (strcmp(server,"localhost") == 0)
   {
   /* The only reason for this is testing...*/
   server = "127.0.0.1";
   }

if (THIS_AGENT_TYPE == cf_common)
   {
   if ((rval.item = strdup("<remote classes>")) == NULL)
      {
      FatalError("Memory allocation in FnCallRemoteSCalar");
      }
   }
else
   {
   GetRemoteScalar("CONTEXT",regex,server,encrypted,buffer);
   
   if (strncmp(buffer,"BAD:",4) == 0)
      {
      SetFnCallReturnStatus("remoteclassesmatching",FNCALL_FAILURE,NULL,NULL);

      if ((rval.item = strdup("!any")) == NULL)
         {
         FatalError("Memory allocation in FnCallRemoteClassesMatching");
         }
      rval.rtype = CF_SCALAR;
      return rval;
      }
   else
      {
      SetFnCallReturnStatus("remoteclassesmatching",FNCALL_SUCCESS,NULL,NULL);

      if ((rval.item = strdup("any")) == NULL)
         {
         FatalError("Memory allocation in FnCallRemoteClassesMatching");
         }
      }

   if ((classlist = SplitStringAsRList(buffer,',')))
      {
      for (rp = classlist; rp != NULL; rp=rp->next)
         {
         snprintf(class,CF_MAXVARSIZE-1,"%s_%s",prefix,(char *)rp->item);
         NewBundleClass(class,THIS_BUNDLE);
         }
      DeleteRlist(classlist);
      }
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallPeers(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp,*newlist,*pruned;
  struct Rval rval;
  char *split = "\n",buffer[CF_BUFSIZE];
  char *filename,*comment,*file_buffer = NULL;
  int i,found,groupsize,maxent = 100000,maxsize = 100000;
  
buffer[0] = '\0';  

/* begin fn specific content */

filename = finalargs->item;
comment = (char *)finalargs->next->item;
groupsize = Str2Int(finalargs->next->next->item);

file_buffer = (char *)CfReadFile(filename,maxsize);

if (file_buffer == NULL)
   {
   rval.item = NULL;
   rval.rtype = CF_LIST;
   SetFnCallReturnStatus("peers",FNCALL_FAILURE,NULL,NULL);
   return rval;
   }
else
   {
   file_buffer = StripPatterns(file_buffer,comment,filename);

   if (file_buffer == NULL)
      {
      rval.item = NULL;
      rval.rtype = CF_LIST;
      return rval;
      }
   else
      {
      newlist = SplitRegexAsRList(file_buffer,split,maxent,true);
      }
   }

/* Slice up the list and discard everything except our slice */

i = 0;
found = false;
pruned = NULL;

for (rp = newlist; rp != NULL; rp = rp->next)
   {
   char s[CF_MAXVARSIZE];
   
   if (EmptyString(rp->item))
      {
      continue;
      }

   s[0] = '\0';
   sscanf(rp->item,"%s",s);

   if (strcmp(s,VFQNAME) == 0 || strcmp(s,VUQNAME) == 0)
      {
      found = true;
      }
   else
      {
      PrependRScalar(&pruned,s,CF_SCALAR);
      }

   if (i++ % groupsize == groupsize-1)
      {
      if (found)
         {
         break;
         }
      else
         {
         DeleteRlist(pruned);
         pruned = NULL;
         }
      }
   }

DeleteRlist(newlist);

if (pruned)
   {
   SetFnCallReturnStatus("peers",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("peers",FNCALL_FAILURE,NULL,NULL);
   }


free(file_buffer);
rval.item = pruned;
rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

static struct Rval FnCallPeerLeader(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp,*newlist;
  struct Rval rval;
  char *split = "\n";
  char *filename,*comment,*file_buffer = NULL,buffer[CF_MAXVARSIZE];
  int i,found,groupsize,maxent = 100000,maxsize = 100000;
  
buffer[0] = '\0';  

/* begin fn specific content */

filename = finalargs->item;
comment = (char *)finalargs->next->item;
groupsize = Str2Int(finalargs->next->next->item);

file_buffer = (char *)CfReadFile(filename,maxsize);

if (file_buffer == NULL)
   {
   rval.item = NULL;
   rval.rtype = CF_LIST;
   SetFnCallReturnStatus("peerleader",FNCALL_FAILURE,NULL,NULL);
   return rval;
   }
else
   {
   file_buffer = StripPatterns(file_buffer,comment,filename);

   if (file_buffer == NULL)
      {
      rval.item = NULL;
      rval.rtype = CF_LIST;
      return rval;
      }
   else
      {
      newlist = SplitRegexAsRList(file_buffer,split,maxent,true);
      }
   }

/* Slice up the list and discard everything except our slice */

i = 0;
found = false;
buffer[0] = '\0';

for (rp = newlist; rp != NULL; rp = rp->next)
   {
   char s[CF_MAXVARSIZE];
   
   if (EmptyString(rp->item))
      {
      continue;
      }

   s[0] = '\0';
   sscanf(rp->item,"%s",s);
   
   if (strcmp(s,VFQNAME) == 0 || strcmp(s,VUQNAME) == 0)
      {
      found = true;
      }

   if (i % groupsize == 0)
      {
      if (found)
         {
         if (strcmp(s,VFQNAME) == 0 || strcmp(s,VUQNAME) == 0)
            {
            strncpy(buffer,"localhost",CF_MAXVARSIZE-1);
            }
         else
            {
            strncpy(buffer,s,CF_MAXVARSIZE-1);
            }
         break;
         }
      }

   i++;
   }

DeleteRlist(newlist);

if (strlen(buffer) > 0)
   {
   SetFnCallReturnStatus("peerleader",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("peerleader",FNCALL_FAILURE,NULL,NULL);
   }

free(file_buffer);
rval.item = strdup(buffer);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallPeerLeaders(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp,*newlist,*pruned;
  struct Rval rval;
  char *split = "\n";
  char *filename,*comment,*file_buffer = NULL,buffer[CF_MAXVARSIZE];
  int i,found,groupsize,maxent = 100000,maxsize = 100000;
  
buffer[0] = '\0';  

/* begin fn specific content */

filename = finalargs->item;
comment = (char *)finalargs->next->item;
groupsize = Str2Int(finalargs->next->next->item);

file_buffer = (char *)CfReadFile(filename,maxsize);

if (file_buffer == NULL)
   {
   rval.item = NULL;
   rval.rtype = CF_LIST;
   SetFnCallReturnStatus("peerleaders",FNCALL_FAILURE,NULL,NULL);
   return rval;
   }
else
   {
   file_buffer = StripPatterns(file_buffer,comment,filename);

   if (file_buffer == NULL)
      {
      rval.item = NULL;
      rval.rtype = CF_LIST;
      return rval;
      }
   else
      {
      newlist = SplitRegexAsRList(file_buffer,split,maxent,true);
      }
   }

/* Slice up the list and discard everything except our slice */

i = 0;
found = false;
pruned = NULL;

for (rp = newlist; rp != NULL; rp = rp->next)
   {
   char s[CF_MAXVARSIZE];
   
   if (EmptyString(rp->item))
      {
      continue;
      }
   
   s[0] = '\0';
   sscanf(rp->item,"%s",s);
   
   if (i % groupsize == 0)
      {
      if (strcmp(s,VFQNAME) == 0 || strcmp(s,VUQNAME) == 0)
         {
         PrependRScalar(&pruned,"localhost",CF_SCALAR);
         }
      else
         {
         PrependRScalar(&pruned,s,CF_SCALAR);
         }
      }

   i++;
   }

DeleteRlist(newlist);

if (pruned)
   {
   SetFnCallReturnStatus("peerleaders",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("peerleaders",FNCALL_FAILURE,NULL,NULL);
   }

free(file_buffer);
rval.item = pruned;
rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRegCmp(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *argv0,*argv1;

buffer[0] = '\0';  

/* begin fn specific content */

strcpy(buffer,CF_ANYCLASS);
argv0 = finalargs->item;
argv1 = finalargs->next->item;

if (FullTextMatch(argv0,argv1))
   {
   strcpy(buffer,"any");   
   }
else
   {
   strcpy(buffer,"!any");
   }

SetFnCallReturnStatus("regcmp",FNCALL_SUCCESS,NULL,NULL);   

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRegCmp");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRegExtract(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *regex,*data,*arrayname;
  struct Scope *ptr;

buffer[0] = '\0';  

/* begin fn specific content */

strcpy(buffer,CF_ANYCLASS);
regex = finalargs->item;
data = finalargs->next->item;
arrayname = finalargs->next->next->item;

if (FullTextMatch(regex,data))
   {
   strcpy(buffer,"any");   
   }
else
   {
   strcpy(buffer,"!any");
   }

ptr = GetScope("match");


if (ptr && ptr->hashtable)
   {
   HashIterator i = HashIteratorInit(ptr->hashtable);
   CfAssoc *assoc;

   while ((assoc = HashIteratorNext(&i)))
      {
      char var[CF_MAXVARSIZE];

      if (assoc->rtype != CF_SCALAR)
         {
         CfOut(cf_error,""," !! Software error: pattern match was non-scalar in regextract (shouldn't happen)");
         strcpy(buffer,"!any");
         SetFnCallReturnStatus("regextract",FNCALL_FAILURE,NULL,NULL);
         break;
         }
      else
         {
         snprintf(var,CF_MAXVARSIZE-1,"%s[%s]",arrayname,assoc->lval);
         NewScalar(THIS_BUNDLE,var,assoc->rval,cf_str);
         }
      }
   }
else
   {
   strcpy(buffer,"!any");
   }

SetFnCallReturnStatus("regextract",FNCALL_SUCCESS,NULL,NULL);   

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRegCmp");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRegLine(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE],line[CF_BUFSIZE];
  char *argv0,*argv1;
  FILE *fin;

buffer[0] = '\0';  

/* begin fn specific content */

argv0 = finalargs->item;
argv1 = finalargs->next->item;

strcpy(buffer,"!any");

if ((fin = fopen(argv1,"r")) == NULL)
   {
   strcpy(buffer,"!any");
   }
else
   {
   while (!feof(fin))
      {
      line[0] = '\0';
      fgets(line,CF_BUFSIZE-1,fin);
      Chop(line);

      if (FullTextMatch(argv0,line))
         {
         strcpy(buffer,"any");
         break;
         }
      }

   fclose(fin);
   }
   
SetFnCallReturnStatus("regline",FNCALL_SUCCESS,NULL,NULL);   

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRegLine");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIsLessGreaterThan(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *argv0,*argv1;
  double a = CF_NOVAL,b = CF_NOVAL;
 
buffer[0] = '\0';  

SetFnCallReturnStatus(fp->name,FNCALL_SUCCESS,NULL,NULL);

argv0 = finalargs->item;
argv1 = finalargs->next->item;

if (IsRealNumber(argv0) && IsRealNumber(argv1))
   {
   a = Str2Double(argv0); 
   b = Str2Double(argv1);

   if (a == CF_NODOUBLE || b == CF_NODOUBLE)
      {
      SetFnCallReturnStatus("is*than",FNCALL_FAILURE,NULL,NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;
      }

/* begin fn specific content */
   
   if ((a != CF_NOVAL) && (b != CF_NOVAL)) 
      {
      Debug("%s and %s are numerical\n",argv0,argv1);
      
      if (!strcmp(fp->name, "isgreaterthan"))
         {
         if (a > b)
            {
            strcpy(buffer,"any");
            }
         else
            {
            strcpy(buffer,"!any");
            }
         }
      else
         {
         if (a < b)  
            {
            strcpy(buffer,"any");
            }
         else
            {
            strcpy(buffer,"!any");
            }
         }
      }
   }
else if (strcmp(argv0,argv1) > 0)
   {
   Debug("%s and %s are NOT numerical\n",argv0,argv1);
   
   if (!strcmp(fp->name, "isgreaterthan"))
      {
      strcpy(buffer,"any");
      }
   else
      {
      strcpy(buffer,"!any");
      }
   }
else
   {
   if (!strcmp(fp->name, "isgreaterthan"))
      {
      strcpy(buffer,"!any");
      }
   else
      {
      strcpy(buffer,"any");
      }
   } 

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallChangedBefore");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallIRange(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  long tmp,from=CF_NOINT,to=CF_NOINT;
  
buffer[0] = '\0';  

/* begin fn specific content */

from = Str2Int(finalargs->item);
to = Str2Int(finalargs->next->item);

if (from == CF_NOINT || to == CF_NOINT)
   {
   SetFnCallReturnStatus("irange",FNCALL_FAILURE,NULL,NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

if (from == CF_NOINT || to == CF_NOINT)
   {
   snprintf(buffer,CF_BUFSIZE,"Error reading assumed int values %s=>%d,%s=>%d\n",(char *)(finalargs->item),from,(char *)(finalargs->next->item),to);
   ReportError(buffer);
   }

if (from > to)
   {
   tmp = to;
   to = from;
   from = tmp;
   }

snprintf(buffer,CF_BUFSIZE-1,"%ld,%ld",from,to);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallIRange");
   }

SetFnCallReturnStatus("irange",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRRange(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  int tmp;
  double from=CF_NODOUBLE,to=CF_NODOUBLE;
  
buffer[0] = '\0';  

/* begin fn specific content */

from = Str2Double((char *)(finalargs->item));
to = Str2Double((char *)(finalargs->next->item));

if (from == CF_NODOUBLE || to == CF_NODOUBLE)
   {
   snprintf(buffer,CF_BUFSIZE,"Error reading assumed real values %s=>%lf,%s=>%lf\n",(char *)(finalargs->item),from,(char *)(finalargs->next->item),to);
   ReportError(buffer);
   }

if (from > to)
   {
   tmp = to;
   to = from;
   from = tmp;
   }

snprintf(buffer,CF_BUFSIZE-1,"%lf,%lf",from,to);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallRRange");
   }

SetFnCallReturnStatus("rrange",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallOn(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  long d[6];
  time_t cftime;
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  

/* begin fn specific content */

rp = finalargs;

for (i = 0; i < 6; i++)
   {
   if (rp != NULL)
      {
      d[i] = Str2Int(rp->item);
      rp = rp->next;
      }
   }

/* (year,month,day,hour,minutes,seconds) */

tmv.tm_year = d[cfa_year] - 1900;
tmv.tm_mon  = d[cfa_month] -1;
tmv.tm_mday = d[cfa_day];
tmv.tm_hour = d[cfa_hour];
tmv.tm_min  = d[cfa_min];
tmv.tm_sec  = d[cfa_sec];
tmv.tm_isdst= -1;

if ((cftime=mktime(&tmv))== -1)
   {
   CfOut(cf_inform,"","Illegal time value");
   }

Debug("Time computed from input was: %s\n",cf_ctime(&cftime));

snprintf(buffer,CF_BUFSIZE-1,"%ld",cftime);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallOn");
   }

SetFnCallReturnStatus("on",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallOr(struct FnCall *fp, struct Rlist *finalargs)
{
struct Rval rval;
struct Rlist *arg;
char id[CF_BUFSIZE];
snprintf(id, CF_BUFSIZE, "built-in FnCall or-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
for(arg = finalargs; arg; arg = arg->next)
   {
   CheckConstraintTypeMatch(id, arg->item, arg->type, cf_str, "", 1);
   }

for(arg = finalargs; arg; arg = arg->next)
   {
   if (IsDefinedClass(arg->item))
      {
      SetFnCallReturnStatus("or", FNCALL_SUCCESS, NULL, NULL);
      rval.item = strdup("any");
      rval.rtype = CF_SCALAR;
      return rval;
      }
   }

SetFnCallReturnStatus("or", FNCALL_SUCCESS, NULL, NULL);
rval.item = strdup("!any");
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallLaterThan(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  long d[6];
  time_t cftime,now = time(NULL);
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  

/* begin fn specific content */

rp = finalargs;

for (i = 0; i < 6; i++)
   {
   if (rp != NULL)
      {
      d[i] = Str2Int(rp->item);
      rp = rp->next;
      }
   }

/* (year,month,day,hour,minutes,seconds) */

tmv.tm_year = d[cfa_year] - 1900;
tmv.tm_mon  = d[cfa_month] -1;
tmv.tm_mday = d[cfa_day];
tmv.tm_hour = d[cfa_hour];
tmv.tm_min  = d[cfa_min];
tmv.tm_sec  = d[cfa_sec];
tmv.tm_isdst= -1;

if ((cftime=mktime(&tmv))== -1)
   {
   CfOut(cf_inform,"","Illegal time value");
   }

Debug("Time computed from input was: %s\n",cf_ctime(&cftime));

if (now > cftime)
   {
   strcpy(buffer,CF_ANYCLASS);
   }
else
   {
   strcpy(buffer,"!any");
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallLaterThan");
   }

SetFnCallReturnStatus("laterthan",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallAgoDate(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  time_t cftime;
  long d[6];
  enum cfdatetemplate i;
  
buffer[0] = '\0';  

/* begin fn specific content */

rp = finalargs;

for (i = 0; i < 6; i++)
   {
   if (rp != NULL)
      {
      d[i] = Str2Int(rp->item);
      rp = rp->next;
      }
   }

/* (year,month,day,hour,minutes,seconds) */

cftime = CFSTARTTIME;
cftime -= d[cfa_sec];
cftime -= d[cfa_min] * 60;
cftime -= d[cfa_hour] * 3600;
cftime -= d[cfa_day] * 24 * 3600;
cftime -= Months2Seconds(d[cfa_month]);
cftime -= d[cfa_year] * 365 * 24 * 3600;

Debug("Total negative offset = %.1f minutes\n",(double)(CFSTARTTIME-cftime)/60.0);
Debug("Time computed from input was: %s\n",cf_ctime(&cftime));

snprintf(buffer,CF_BUFSIZE-1,"%ld",cftime);

if (cftime < 0)
   {
   Debug("AGO overflowed, truncating at zero\n");
   snprintf(buffer,CF_BUFSIZE-1,"%ld",0);
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallAgo");
   }

/* end fn specific content */

SetFnCallReturnStatus("ago",FNCALL_SUCCESS,NULL,NULL);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallAccumulatedDate(struct FnCall *fp,struct Rlist *finalargs)

{ struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  long d[6], cftime;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  

/* begin fn specific content */


rp = finalargs;

for (i = 0; i < 6; i++)
   {
   if (rp != NULL)
      {
      d[i] = Str2Int(rp->item);
      rp = rp->next;
      }
   }

/* (year,month,day,hour,minutes,seconds) */

cftime = 0;
cftime += d[cfa_sec];
cftime += d[cfa_min] * 60;
cftime += d[cfa_hour] * 3600;
cftime += d[cfa_day] * 24 * 3600;
cftime += d[cfa_month] * 30 * 24 * 3600;
cftime += d[cfa_year] * 365 * 24 * 3600;

snprintf(buffer,CF_BUFSIZE-1,"%ld",cftime);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallAgo");
   }

/* end fn specific content */

SetFnCallReturnStatus("accumulated",FNCALL_SUCCESS,NULL,NULL);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallNot(struct FnCall *fp, struct Rlist *finalargs)
{
struct Rval rval;
SetFnCallReturnStatus("not", FNCALL_SUCCESS, NULL, NULL);
rval.item = strdup(IsDefinedClass(finalargs->item) ? "!any" : "any");
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallNow(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  time_t cftime;
  
buffer[0] = '\0';  

/* begin fn specific content */

cftime = CFSTARTTIME;

Debug("Time computed from input was: %s\n",cf_ctime(&cftime));

snprintf(buffer,CF_BUFSIZE-1,"%ld",(long)cftime);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallAgo");
   }

/* end fn specific content */

SetFnCallReturnStatus("now",FNCALL_SUCCESS,NULL,NULL);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/
/* Read functions                                                    */
/*********************************************************************/

static struct Rval FnCallReadFile(struct FnCall *fp,struct Rlist *finalargs)
    
{ struct Rval rval;
  char *filename;
  int maxsize;

/* begin fn specific content */

filename = (char *)(finalargs->item);
maxsize = Str2Int(finalargs->next->item);

// Read once to validate structure of file in itemlist

Debug("Read string data from file %s (up to %d)\n",filename,maxsize);

rval.item = CfReadFile(filename,maxsize);

if (rval.item)
   {
   SetFnCallReturnStatus("readfile",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("readfile",FNCALL_FAILURE,NULL,NULL);
   }

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval ReadList(struct FnCall *fp,struct Rlist *finalargs,enum cfdatatype type)
    
{ struct Rlist *rp,*newlist = NULL;
  struct Rval rval;
  char *filename,*comment,*split,fnname[CF_MAXVARSIZE];
  int maxent,maxsize,noerrors = true,blanks = false;
  char *file_buffer = NULL;

/* begin fn specific content */

 /* 5args: filename,comment_regex,split_regex,max number of entries,maxfilesize  */

filename = (char *)(finalargs->item);
comment = (char *)(finalargs->next->item);
split = (char *)(finalargs->next->next->item);
maxent = Str2Int(finalargs->next->next->next->item);
maxsize = Str2Int(finalargs->next->next->next->next->item);

// Read once to validate structure of file in itemlist

Debug("Read string data from file %s\n",filename);
snprintf(fnname,CF_MAXVARSIZE-1,"read%slist",CF_DATATYPES[type]);

file_buffer = (char *)CfReadFile(filename,maxsize);

if (file_buffer == NULL)
   {
   rval.item = NULL;
   rval.rtype = CF_LIST;
   SetFnCallReturnStatus(fnname,FNCALL_FAILURE,NULL,NULL);
   return rval;
   }
else
   {
   file_buffer = StripPatterns(file_buffer,comment,filename);

   if (file_buffer == NULL)
      {
      SetFnCallReturnStatus(fnname,FNCALL_SUCCESS,NULL,NULL);
      rval.item = NULL;
      rval.rtype = CF_LIST;
      return rval;
      }
   else
      {
      newlist = SplitRegexAsRList(file_buffer,split,maxent,blanks);
      }
   }

switch(type)
   {
   case cf_str:
       break;

   case cf_int:
       for (rp = newlist; rp != NULL; rp=rp->next)
          {
          if (Str2Int(rp->item) == CF_NOINT)
             {
             CfOut(cf_error,"","Presumed int value \"%s\" read from file %s has no recognizable value",rp->item,filename);
             noerrors = false;
             }
          }
       break;

   case cf_real:
       for (rp = newlist; rp != NULL; rp=rp->next)
          {
          if (Str2Double(rp->item) == CF_NODOUBLE)
             {
             CfOut(cf_error,"","Presumed real value \"%s\" read from file %s has no recognizable value",rp->item,filename);
             noerrors = false;
             }
          }
       break;

   default:
       FatalError("Software error readstringlist - abused type");       
   }

if (newlist && noerrors)
   {
   SetFnCallReturnStatus(fnname,FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus(fnname,FNCALL_FAILURE,NULL,NULL);
   }

free(file_buffer);
rval.item = newlist;
rval.rtype = CF_LIST;
return rval;
}

static struct Rval FnCallReadStringList(struct FnCall *fp, struct Rlist *args)
{
return ReadList(fp, args, cf_str);
}

static struct Rval FnCallReadIntList(struct FnCall *fp, struct Rlist *args)
{
return ReadList(fp, args, cf_int);
}

static struct Rval FnCallReadRealList(struct FnCall *fp, struct Rlist *args)
{
return ReadList(fp, args, cf_real);
}

/*********************************************************************/

static struct Rval ReadArray(struct FnCall *fp,struct Rlist *finalargs,enum cfdatatype type,int intIndex)

/* lval,filename,separator,comment,Max number of bytes  */

{
  struct Rval rval;
  char *array_lval,*filename,*comment,*split,fnname[CF_MAXVARSIZE];
  int maxent,maxsize,entries = 0;
  char *file_buffer = NULL;

 /* Arg validation */

if (intIndex)
   {
   snprintf(fnname,CF_MAXVARSIZE-1,"read%sarrayidx",CF_DATATYPES[type]);
   }
else
   {
   snprintf(fnname,CF_MAXVARSIZE-1,"read%sarray",CF_DATATYPES[type]);
   }

/* begin fn specific content */

 /* 6 args: array_lval,filename,comment_regex,split_regex,max number of entries,maxfilesize  */

array_lval = (char *)(finalargs->item);
filename = (char *)(finalargs->next->item);
comment = (char *)(finalargs->next->next->item);
split = (char *)(finalargs->next->next->next->item);
maxent = Str2Int(finalargs->next->next->next->next->item);
maxsize = Str2Int(finalargs->next->next->next->next->next->item);

// Read once to validate structure of file in itemlist

Debug("Read string data from file %s - , maxent %d, maxsize %d\n",filename,maxent,maxsize);

file_buffer = (char *)CfReadFile(filename,maxsize);

Debug("FILE: %s\n",file_buffer);

if (file_buffer == NULL)
   {
   entries = 0;
   }
else
   {
   file_buffer = StripPatterns(file_buffer,comment,filename);

   if (file_buffer == NULL)
      {
      entries = 0;
      }
   else
      {
      entries = BuildLineArray(array_lval,file_buffer,split,maxent,type,intIndex);
      }
   }

switch(type)
   {
   case cf_str:
   case cf_int:
   case cf_real:
       break;

   default:
       FatalError("Software error readstringarray - abused type");       
   }

SetFnCallReturnStatus(fnname,FNCALL_SUCCESS,NULL,NULL);

/* Return the number of lines in array */

snprintf(fnname,CF_MAXVARSIZE-1,"%d",entries);
rval.item = strdup(fnname);

free(file_buffer);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallReadStringArray(struct FnCall *fp, struct Rlist *args)
{
return ReadArray(fp, args, cf_str, false);
}


/*********************************************************************/

static struct Rval FnCallReadStringArrayIndex(struct FnCall *fp, struct Rlist *args)
{
return ReadArray(fp, args, cf_str, true);
}

/*********************************************************************/

static struct Rval FnCallReadIntArray(struct FnCall *fp, struct Rlist *args)
{
return ReadArray(fp, args, cf_int, false);
}

/*********************************************************************/

static struct Rval FnCallReadRealArray(struct FnCall *fp, struct Rlist *args)
{
return ReadArray(fp, args, cf_real, false);
}

/*********************************************************************/

static struct Rval ParseArray(struct FnCall *fp,struct Rlist *finalargs,enum cfdatatype type,int intIndex)

/* lval,filename,separator,comment,Max number of bytes  */

{
  struct Rval rval;
  char *array_lval,*instring,*comment,*split,fnname[CF_MAXVARSIZE];
  int maxent,maxsize,entries = 0;

 /* Arg validation */

if (intIndex)
   {
   snprintf(fnname,CF_MAXVARSIZE-1,"read%sarrayidx",CF_DATATYPES[type]);
   }
else
   {
   snprintf(fnname,CF_MAXVARSIZE-1,"read%sarray",CF_DATATYPES[type]);
   }

/* begin fn specific content */

 /* 6 args: array_lval,instring,comment_regex,split_regex,max number of entries,maxfilesize  */

array_lval = (char *)(finalargs->item);
instring = strdup((char *)(finalargs->next->item));
comment = (char *)(finalargs->next->next->item);
split = (char *)(finalargs->next->next->next->item);
maxent = Str2Int(finalargs->next->next->next->next->item);
maxsize = Str2Int(finalargs->next->next->next->next->next->item);

// Read once to validate structure of file in itemlist

Debug("Parse string data from string %s - , maxent %d, maxsize %d\n",instring,maxent,maxsize);

if (instring == NULL)
   {
   entries = 0;
   }
else
   {
   instring = StripPatterns(instring,comment,"string argument 2");

   if (instring == NULL)
      {
      entries = 0;
      }
   else
      {
      entries = BuildLineArray(array_lval,instring,split,maxent,type,intIndex);
      }
   }

switch(type)
   {
   case cf_str:
   case cf_int:
   case cf_real:
       break;

   default:
       FatalError("Software error parsestringarray - abused type");       
   }

SetFnCallReturnStatus(fnname,FNCALL_SUCCESS,NULL,NULL);

/* Return the number of lines in array */

snprintf(fnname,CF_MAXVARSIZE-1,"%d",entries);
rval.item = strdup(fnname);

free(instring);
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallParseStringArray(struct FnCall *fp, struct Rlist *args)
{
return ParseArray(fp, args, cf_str, false);
}

/*********************************************************************/

static struct Rval FnCallParseStringArrayIndex(struct FnCall *fp, struct Rlist *args)
{
return ParseArray(fp, args, cf_str, true);
}

/*********************************************************************/

static struct Rval FnCallParseIntArray(struct FnCall *fp, struct Rlist *args)
{
return ParseArray(fp, args, cf_int, false);
}

/*********************************************************************/

static struct Rval FnCallParseRealArray(struct FnCall *fp, struct Rlist *args)
{
return ParseArray(fp, args, cf_real, false);
}

/*********************************************************************/

static struct Rval FnCallSplitString(struct FnCall *fp,struct Rlist *finalargs)
    
{ struct Rlist *newlist = NULL;
  struct Rval rval;
  char *string,*split;
  int max = 0;
  
/* begin fn specific content */

 /* 2args: string,split_regex,max  */

string = (char *)(finalargs->item);
split = (char *)(finalargs->next->item);
max = Str2Int((char *)(finalargs->next->next->item));
    
// Read once to validate structure of file in itemlist

newlist = SplitRegexAsRList(string,split,max,true);

SetFnCallReturnStatus("splitstring",FNCALL_SUCCESS,NULL,NULL);

rval.item = newlist;
rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

static struct Rval FnCallFileSexist(struct FnCall *fp,struct Rlist *finalargs)

{ char *listvar;
  struct Rlist *rp,*files;
  struct Rval rval;
  char buffer[CF_BUFSIZE],naked[CF_MAXVARSIZE],rettype;
  void *retval;
  struct stat sb;

buffer[0] = '\0';  

/* begin fn specific content */

listvar = finalargs->item;

if (*listvar == '@')
   {
   GetNaked(naked,listvar);
   }
else
   {
   CfOut(cf_error,"","Function filesexist was promised a list called \"%s\" but this was not found\n",listvar);
   SetFnCallReturnStatus("filesexist",FNCALL_FAILURE,"File list was not a list found in scope",NULL);
   rval.item = strdup("!any");
   rval.rtype = CF_SCALAR;
   return rval;            
   }

if (GetVariable(CONTEXTID,naked,&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,"","Function filesexist was promised a list called \"%s\" but this was not found\n",listvar);
   SetFnCallReturnStatus("filesexist",FNCALL_FAILURE,"File list was not a list found in scope",NULL);
   rval.item = strdup("!any");
   rval.rtype = CF_SCALAR;
   return rval;            
   }

if (rettype != CF_LIST)
   {
   CfOut(cf_error,"","Function filesexist was promised a list called \"%s\" but this variable is not a list\n",listvar);
   SetFnCallReturnStatus("filesexist",FNCALL_FAILURE,"File list was not a list found in scope",NULL);
   rval.item = strdup("!any");
   rval.rtype = CF_SCALAR;
   return rval;            
   }

files = (struct Rlist *)retval;

strcpy(buffer,"any");

for (rp = files; rp != NULL; rp=rp->next)
   {
   if (cfstat(rp->item,&sb) == -1)
      {
      strcpy(buffer,"!any");
      break;
      }
   }

rval.item = strdup(buffer);

SetFnCallReturnStatus("filesexist",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/
/* LDAP Nova features                                                */
/*********************************************************************/

static struct Rval FnCallLDAPValue(struct FnCall *fp,struct Rlist *finalargs)
    
{
  struct Rval rval;
  char *uri,*dn,*filter,*name,*scope,*sec;
  char buffer[CF_BUFSIZE],handle[CF_BUFSIZE];
  void *newval = NULL;
  
/* begin fn specific content */
 
   uri = (char *)(finalargs->item);
    dn = (char *)(finalargs->next->item);
filter = (char *)(finalargs->next->next->item);
  name = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->item);

snprintf(handle,CF_BUFSIZE,"%s_%s_%s_%s",dn,filter,name,scope);
   
if ((newval = CfLDAPValue(uri,dn,filter,name,scope,sec)))
   {
   CacheUnreliableValue("ldapvalue",handle,newval);
   }
else
   {
   if (RetrieveUnreliableValue("ldapvalue",handle,buffer))
      {
      newval = strdup(buffer);
      }
   }

if (newval)
   {
   SetFnCallReturnStatus("ldapvalue",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   newval = strdup("no result");
   SetFnCallReturnStatus("ldapvalue",FNCALL_FAILURE,NULL,NULL);
   }

rval.item = newval;
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallLDAPArray(struct FnCall *fp,struct Rlist *finalargs)
    
{
  struct Rval rval;
  char *array,*uri,*dn,*filter,*scope,*sec;
  void *newval;
  
/* begin fn specific content */

 array = (char *)(finalargs->item);
   uri = (char *)(finalargs->next->item);
    dn = (char *)(finalargs->next->next->item);
filter = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->item);
   
if ((newval = CfLDAPArray(array,uri,dn,filter,scope,sec)))
   {
   SetFnCallReturnStatus("ldaparray",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("ldaparray",FNCALL_FAILURE,NULL,NULL);
   }

rval.item = newval;
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallLDAPList(struct FnCall *fp,struct Rlist *finalargs)
    
{
  struct Rval rval;
  char *uri,*dn,*filter,*name,*scope,*sec;
  void *newval;
  
/* begin fn specific content */

   uri = (char *)(finalargs->item);
    dn = (char *)(finalargs->next->item);
filter = (char *)(finalargs->next->next->item);
  name = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->item);

if ((newval = CfLDAPList(uri,dn,filter,name,scope,sec)))
   {
   SetFnCallReturnStatus("ldaplist",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("ldaplist",FNCALL_FAILURE,NULL,NULL);
   }

rval.item = newval;
rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

static struct Rval FnCallRegLDAP(struct FnCall *fp,struct Rlist *finalargs)
    
{
  struct Rval rval;
  char *uri,*dn,*filter,*name,*scope,*regex,*sec;
  void *newval;
  
/* begin fn specific content */

   uri = (char *)(finalargs->item);
    dn = (char *)(finalargs->next->item);
filter = (char *)(finalargs->next->next->item);
  name = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
 regex = (char *)(finalargs->next->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->next->item);

if ((newval = CfRegLDAP(uri,dn,filter,name,scope,regex,sec)))
   {
   SetFnCallReturnStatus("regldap",FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus("regldap",FNCALL_FAILURE,NULL,NULL);
   }

rval.item = newval;
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval FnCallDiskFree(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  off_t df;
  
buffer[0] = '\0';  

df = GetDiskUsage((char *)finalargs->item, cfabs);

if (df == CF_INFINITY)
   {
   df = 0;
   }

snprintf(buffer,CF_BUFSIZE-1,"%lld", df);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallGetGid");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

#ifndef MINGW

/*******************************************************************/
/* Unix implementations                                            */
/*******************************************************************/

static struct Rval Unix_FnCallUserExists(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct passwd *pw;
  uid_t uid = -1;
  char *arg = finalargs->item;
 
buffer[0] = '\0';  

/* begin fn specific content */

strcpy(buffer,CF_ANYCLASS);

if (IsNumber(arg))
   {
   uid = Str2Uid(arg,NULL,NULL);
   
   if (uid < 0)
      {
      SetFnCallReturnStatus("userexists",FNCALL_FAILURE,"Illegal user id",NULL);   
      }
   else
      {
      SetFnCallReturnStatus("userexists",FNCALL_SUCCESS,NULL,NULL);   
      }

   if ((pw = getpwuid(uid)) == NULL)
      {
      strcpy(buffer,"!any");
      }
   }
else if ((pw = getpwnam(arg)) == NULL)
   {
   strcpy(buffer,"!any");
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallUserExists");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

static struct Rval Unix_FnCallGroupExists(struct FnCall *fp,struct Rlist *finalargs)

{
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct group *gr;
  gid_t gid = -1;
  char *arg = finalargs->item;
 
buffer[0] = '\0';  

/* begin fn specific content */

strcpy(buffer,CF_ANYCLASS);

if (isdigit((int)*arg))
   {
   gid = Str2Gid(arg,NULL,NULL);
   
   if (gid < 0)
      {
      SetFnCallReturnStatus("groupexists",FNCALL_FAILURE,"Illegal group id",NULL);   
      }
   else
      {
      SetFnCallReturnStatus("groupexists",FNCALL_SUCCESS,NULL,NULL);   
      }

   if ((gr = getgrgid(gid)) == NULL)
      {
      strcpy(buffer,"!any");
      }
   }
else if ((gr = getgrnam(arg)) == NULL)
   {
   strcpy(buffer,"!any");
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallGroupExists");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}
#endif  /* NOT MINGW */


/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void *CfReadFile(char *filename,int maxsize)

{ struct stat sb;
  char *result = NULL;
  FILE *fp;
  size_t size;
  int i,newlines = 0;

if (cfstat(filename,&sb) == -1)
   {
   if (THIS_AGENT_TYPE == cf_common)
      {
      Debug("Could not examine file %s in readfile on this system",filename);
      }
   else
      {
      if (IsCf3VarString(filename))
         {
         CfOut(cf_verbose,"","Cannot converge/reduce variable \"%s\" yet .. assuming it will resolve later",filename);
         }
      else
         {
         CfOut(cf_inform,"stat"," !! Could not examine file \"%s\" in readfile",filename);
         }
      }
   return NULL;
   }

if (sb.st_size > maxsize)
   {
   CfOut(cf_inform,"","Truncating long file %s in readfile to max limit %d",filename,maxsize);
   size = maxsize;
   }
else
   {
   size = sb.st_size;
   }

if (size == 0)
   {
   CfOut(cf_verbose,"","Aborting read: file %s has zero bytes",filename);
   return NULL;
   }

result = malloc(size+1);
   
if (result == NULL)
   {
   CfOut(cf_error,"malloc","Could not allocate file %s in readfile",filename);
   return NULL;
   }

if ((fp = fopen(filename,"r")) == NULL)
   {
   CfOut(cf_verbose,"fopen","Could not open file \"%s\" in readfile",filename);
   free(result);
   return NULL;
   }

if (fread(result,size,1,fp) != 1)
   {
   CfOut(cf_verbose,"fread","Could not read expected amount from file %s in readfile",filename);
   fclose(fp);
   free(result);
   return NULL;
   }

result[size] = '\0';

for (i = 0; i < size-1; i++)
   {
   if (result[i] == '\n' || result[i] == '\r')
      {
      newlines++;
      }
   }

if (newlines == 0 && (result[size-1] == '\n' || result[size-1] == '\r'))
   {
   result[size-1] = '\0';
   }

fclose(fp);
return (void *)result;
}

/*********************************************************************/

static char *StripPatterns(char *file_buffer,char *pattern,char *filename)

{ int start,end;
  int count = 0;

if(!EMPTY(pattern))
  {
  while(BlockTextMatch(pattern,file_buffer,&start,&end))
     {
     CloseStringHole(file_buffer,start,end);
     
     if (count++ > strlen(file_buffer))
        {
        CfOut(cf_error,""," !! Comment regex \"%s\" was irreconcilable reading input \"%s\" probably because it legally matches nothing",pattern,filename);
        return file_buffer;
        }
     }
  }

return file_buffer;
}

/*********************************************************************/

static void CloseStringHole(char *s,int start,int end)

{ int off = end - start;
  char *sp;

if (off <= 0)
   {
   return;
   }
 
for (sp = s + start; *(sp+off) != '\0'; sp++)
   {
   *sp = *(sp+off);
   }

*sp = '\0';
}

/*********************************************************************/

static int BuildLineArray(char *array_lval,char *file_buffer,char *split,int maxent,enum cfdatatype type,int intIndex)

{ char *sp,linebuf[CF_BUFSIZE],name[CF_MAXVARSIZE],first_one[CF_MAXVARSIZE];
  struct Rlist *rp,*newlist = NULL;
  int allowblanks = true, vcount,hcount,lcount = 0;
  int lineLen;

memset(linebuf,0,CF_BUFSIZE);
hcount = 0;

for (sp = file_buffer; hcount < maxent && *sp != '\0'; sp++)
   {
   linebuf[0] = '\0';
   sscanf(sp,"%1023[^\n]",linebuf);

   lineLen = strlen(linebuf);

   if (lineLen == 0)
      {
      continue;
      }
   else if (lineLen == 1 && linebuf[0] == '\r')
      {
      continue;
      }

   if (linebuf[lineLen - 1] == '\r')
     {
     linebuf[lineLen - 1] = '\0';
     }

   if (lcount++ > CF_HASHTABLESIZE)
      {
      CfOut(cf_error,""," !! Array is too big to be read into Cfengine (max 4000)");
      break;
      }

   newlist = SplitRegexAsRList(linebuf,split,maxent,allowblanks);
   
   vcount = 0;
   first_one[0] = '\0';
   
   for (rp = newlist; rp != NULL; rp=rp->next)
      {
      char this_rval[CF_MAXVARSIZE];
      long ival;
      double rval;

      switch (type)
         {
         case cf_str:
             strncpy(this_rval,rp->item,CF_MAXVARSIZE-1);
             break;
             
         case cf_int:
             ival = Str2Int(rp->item);
             snprintf(this_rval,CF_MAXVARSIZE,"%d",(int)ival);
             break;
             
         case cf_real:
             rval = Str2Int(rp->item);
             sscanf(rp->item,"%255s",this_rval);
             break;
             
         default:
             
             FatalError("Software error readstringarray - abused type");       
         }

      if (strlen(first_one) == 0)
         {
         strncpy(first_one,this_rval,CF_MAXVARSIZE-1);
         }
          
      if (intIndex)
         {
         snprintf(name,CF_MAXVARSIZE,"%s[%d][%d]",array_lval,hcount,vcount);
         }
      else
         {
         snprintf(name,CF_MAXVARSIZE,"%s[%s][%d]",array_lval,first_one,vcount);
         }

      NewScalar(THIS_BUNDLE,name,this_rval,type);
      vcount++;
      }

   hcount++;
   sp += lineLen;

   if (*sp == '\0')  // either \n or \0
      {
      break;
      }
   }

/* Don't free data - goes into vars */

return hcount;
}

/*********************************************************************/

static int ExecModule(char *command)

{ FILE *pp;
  char *sp,line[CF_BUFSIZE];
  int print = false;

if ((pp = cf_popen(command,"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","Couldn't open pipe from %s\n",command);
   return false;
   }
   
while (!feof(pp))
   {
   if (ferror(pp))  /* abortable */
      {
      CfOut(cf_error,"","Shell command pipe %s\n",command);
      break;
      }
   
   CfReadLine(line,CF_BUFSIZE,pp);
   
   if (strlen(line) > CF_BUFSIZE - 80)
      {
      CfOut(cf_error,"","Line from module %s is too long to be sensible\n",command);
      break;
      }
   
   if (ferror(pp))  /* abortable */
      {
      CfOut(cf_error,"","Shell command pipe %s\n",command);
      break;
      }  
   
   print = false;
   
   for (sp = line; *sp != '\0'; sp++)
      {
      if (! isspace((int)*sp))
         {
         print = true;
         break;
         }
      }

   ModuleProtocol(command,line,print);
   }

cf_pclose(pp);
return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void ModuleProtocol(char *command,char *line,int print)

{ char name[CF_BUFSIZE],content[CF_BUFSIZE],context[CF_BUFSIZE];
  char arg0[CF_BUFSIZE];
  char *filename;

/* Infer namespace from script name */

snprintf(arg0, CF_BUFSIZE, "%s", GetArg0(command));
filename = basename(arg0);

/* Canonicalize filename into acceptable namespace name*/

CanonifyNameInPlace(filename);
strcpy(context,filename);
CfOut(cf_verbose, "", "Module context: %s\n", context);

NewScope(context);
name[0] = '\0';
content[0] = '\0';

switch (*line)
   {
   case '+':
       CfOut(cf_verbose,"","Activated classes: %s\n",line+1);
       if (CheckID(line+1))
          {
          NewClass(line+1);
          }
       break;
   case '-':
       CfOut(cf_verbose,"","Deactivated classes: %s\n",line+1);
       if (CheckID(line+1))
          {
          NegateClassesFromString(line+1,&VNEGHEAP);
          }
       break;
   case '=':
       content[0] = '\0';
       sscanf(line+1,"%[^=]=%[^\n]",name,content);
       
       if (CheckID(name))
          {
          CfOut(cf_verbose,"","Defined variable: %s in context %s with value: %s\n",name,context,content);
          NewScalar(context,name,content,cf_str);
          }
       break;
       
   case '@':
       content[0] = '\0';
       sscanf(line+1,"%[^=]=%[^\n]",name,content);
       
       if (CheckID(name))
          {
          CfOut(cf_verbose,"","Defined variable: %s in context %s with value: %s\n",name,context,content);
          struct Rlist *list = ParseShownRlist(content);
          NewList(context,name,list,cf_slist);
          }
       break;
       
   default:
       if (print)
          {
          CfOut(cf_cmdout,"","M \"%s\": %s\n",command,line);
          }
       break;
   }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static int CheckID(char *id)

{ char *sp;

for (sp = id; *sp != '\0'; sp++)
   {
   if (!isalnum((int)*sp) && (*sp != '_'))
      {
      CfOut(cf_error,"","Module protocol contained an illegal character \'%c\' in class/variable identifier \'%s\'.",*sp,id);
      return false;
      }
   }

return true;
}

/*********************************************************************/

struct Rval CallFunction(FnCallType *function, struct FnCall *fp, struct Rlist *expargs)
{
ArgTemplate(fp, function->args, expargs);
return (*function->impl)(fp, expargs);
}

/*********************************************************/
/* Function prototypes                                   */
/*********************************************************/

struct FnCallArg ACCESSEDBEFORE_ARGS[] =
   {
   {CF_ABSPATHRANGE,cf_str,"Newer filename"},
   {CF_ABSPATHRANGE,cf_str,"Older filename"},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg ACCUM_ARGS[] =
   {
   {"0,1000",cf_int,"Years"},
   {"0,1000",cf_int,"Months"},    
   {"0,1000",cf_int,"Days"},      
   {"0,1000",cf_int,"Hours"},     
   {"0,1000",cf_int,"Minutes"},   
   {"0,40000",cf_int,"Seconds"},   
   {NULL,cf_notype,NULL}
   };

struct FnCallArg AND_ARGS[] =
   {
   {NULL,cf_notype,NULL}
   };

struct FnCallArg AGO_ARGS[] =
    {
    {"0,1000",cf_int,"Years"},
    {"0,1000",cf_int,"Months"},    
    {"0,1000",cf_int,"Days"},      
    {"0,1000",cf_int,"Hours"},     
    {"0,1000",cf_int,"Minutes"},   
    {"0,40000",cf_int,"Seconds"},   
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LATERTHAN_ARGS[] =
    {
    {"0,1000",cf_int,"Years"},
    {"0,1000",cf_int,"Months"},    
    {"0,1000",cf_int,"Days"},      
    {"0,1000",cf_int,"Hours"},     
    {"0,1000",cf_int,"Minutes"},   
    {"0,40000",cf_int,"Seconds"},   
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CANONIFY_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"String containing non-identifier characters"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CHANGEDBEFORE_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"Newer filename"},
    {CF_ABSPATHRANGE,cf_str,"Older filename"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CLASSIFY_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input string"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CLASSMATCH_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CONCAT_ARGS[] =
   {
   {NULL,cf_notype,NULL}
   };

struct FnCallArg COUNTCLASSESMATCHING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg COUNTLINESMATCHING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ABSPATHRANGE,cf_str,"Filename"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg DISKFREE_ARGS[] =
   {
   {CF_ABSPATHRANGE,cf_str,"File system directory"},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg ESCAPE_ARGS[] =
   {
   {CF_ANYSTRING,cf_str,"IP address or string to escape"},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg EXECRESULT_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"Fully qualified command path"},
    {"useshell,noshell",cf_opts,"Shell encapsulation option"},
    {NULL,cf_notype,NULL}
    };

// fileexists, isdir,isplain,islink

struct FnCallArg FILESTAT_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"File object name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg FILESEXIST_ARGS[] =
    {
    {CF_NAKEDLRANGE,cf_str,"Array identifier containing list"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETFIELDS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression to match line"},
    {CF_ABSPATHRANGE,cf_str,"Filename to read"},
    {CF_ANYSTRING,cf_str,"Regular expression to split fields"},
    {CF_ANYSTRING,cf_str,"Return array name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETINDICES_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETUSERS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Comma separated list of User names"},
    {CF_ANYSTRING,cf_str,"Comma separated list of UserID numbers"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETENV_ARGS[] =
   {
   {CF_IDRANGE,cf_str,"Name of environment variable"},
   {CF_VALRANGE,cf_int,"Maximum number of characters to read "},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg GETGID_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Group name in text"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETUID_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"User name in text"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GREP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GROUPEXISTS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Group name or identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HASH_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input text"},
    {"md5,sha1,sha256,sha512,sha384,crypt",cf_opts,"Hash or digest algorithm"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HASHMATCH_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"Filename to hash"},
    {"md5,sha1,crypt,cf_sha224,cf_sha256,cf_sha384,cf_sha512",cf_opts,"Hash or digest algorithm"},
    {CF_IDRANGE,cf_str,"ASCII representation of hash for comparison"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HOST2IP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Host name in ascii"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg IP2HOST_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"IP address (IPv4 or IPv6)"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HOSTINNETGROUP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Host name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HOSTRANGE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Hostname prefix"},
    {CF_ANYSTRING,cf_str,"Enumerated range"},
    {NULL,cf_notype,NULL}
    };


struct FnCallArg HOSTSSEEN_ARGS[] =
    {
    {CF_VALRANGE,cf_int,"Horizon since last seen in hours"},
    {"lastseen,notseen",cf_opts,"Complements for selection policy"},
    {"name,address",cf_opts,"Type of return value desired"},
    {NULL,cf_notype,NULL}
    };
    
struct FnCallArg IPRANGE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"IP address range syntax"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg IRANGE_ARGS[] =
    {
    {CF_INTRANGE,cf_int,"Integer"},
    {CF_INTRANGE,cf_int,"Integer"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISGREATERTHAN_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Larger string or value"},
    {CF_ANYSTRING,cf_str,"Smaller string or value"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISLESSTHAN_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Smaller string or value"},
    {CF_ANYSTRING,cf_str,"Larger string or value"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISNEWERTHAN_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"Newer file name"},
    {CF_ABSPATHRANGE,cf_str,"Older file name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISVARIABLE_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Variable identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg JOIN_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Join glue-string"},
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LASTNODE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input string"},
    {CF_ANYSTRING,cf_str,"Link separator, e.g. /,:"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LDAPARRAY_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Array name"},
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LDAPLIST_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LDAPVALUE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg NOT_ARGS[] =
   {
   {CF_ANYSTRING,cf_str,"Class value"},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg NOW_ARGS[] =
    {
    {NULL,cf_notype,NULL}
    };

struct FnCallArg OR_ARGS[] =
   {
   {NULL,cf_notype,NULL}
   };

struct FnCallArg SUM_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"A list of arbitrary real values"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PRODUCT_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"A list of arbitrary real values"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg DATE_ARGS[] =
    {
    {"1970,3000",cf_int,"Year"},
    {"1,12",cf_int,"Month"},    
    {"1,31",cf_int,"Day"},      
    {"0,23",cf_int,"Hour"},     
    {"0,59",cf_int,"Minute"},   
    {"0,59",cf_int,"Second"},   
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PEERS_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"File name of host list"},
    {CF_ANYSTRING,cf_str,"Comment regex pattern"},
    {CF_VALRANGE,cf_int,"Peer group size"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PEERLEADER_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"File name of host list"},
    {CF_ANYSTRING,cf_str,"Comment regex pattern"},
    {CF_VALRANGE,cf_int,"Peer group size"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PEERLEADERS_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"File name of host list"},
    {CF_ANYSTRING,cf_str,"Comment regex pattern"},
    {CF_VALRANGE,cf_int,"Peer group size"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg RANDOMINT_ARGS[] =
    {
    {CF_INTRANGE,cf_int,"Lower inclusive bound"},
    {CF_INTRANGE,cf_int,"Upper inclusive bound"},
    {NULL,cf_notype,NULL}
    };
 
struct FnCallArg READFILE_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"File name"},
    {CF_VALRANGE,cf_int,"Maximum number of bytes to read"},
    {NULL,cf_notype,NULL}
    };


struct FnCallArg READSTRINGARRAY_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Array identifier to populate"},
    {CF_ABSPATHRANGE,cf_str,"File name to read"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PARSESTRINGARRAY_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Array identifier to populate"},
    {CF_ABSPATHRANGE,cf_str,"A string to parse for input data"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg READSTRINGARRAYIDX_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Array identifier to populate"},
    {CF_ABSPATHRANGE,cf_str,"A string to parse for input data"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PARSESTRINGARRAYIDX_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Array identifier to populate"},
    {CF_ABSPATHRANGE,cf_str,"File name to read"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg READSTRINGLIST_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"File name to read"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg READTCP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Host name or IP address of server socket"},
    {CF_VALRANGE,cf_int,"Port number"},
    {CF_ANYSTRING,cf_str,"Protocol query string"},
    {CF_VALRANGE,cf_int,"Maximum number of bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGARRAY_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGCMP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Match string"},
    {NULL,cf_notype,NULL}        
    };

struct FnCallArg REGEXTRACT_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Match string"},
    {CF_IDRANGE,cf_str,"Identifier for back-references"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGISTRYVALUE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Windows registry key"},
    {CF_ANYSTRING,cf_str,"Windows registry value-id"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGLINE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Filename to search"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGLIST_ARGS[] =
    {
    {CF_NAKEDLRANGE,cf_str,"Cfengine list identifier"},
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGLDAP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {CF_ANYSTRING,cf_str,"Regex to match results"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REMOTESCALAR_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Variable identifier"},
    {CF_ANYSTRING,cf_str,"Hostname or IP address of server"},
    {CF_BOOL,cf_opts,"Use enryption"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HUB_KNOWLEDGE_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Variable identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REMOTECLASSESMATCHING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Server name or address"},
    {CF_BOOL,cf_opts,"Use encryption"},
    {CF_IDRANGE,cf_str,"Return class prefix"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg RETURNSZERO_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"Fully qualified command path"},
    {"useshell,noshell",cf_opts,"Shell encapsulation option"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg RRANGE_ARGS[] =
    {
    {CF_REALRANGE,cf_real,"Real number"},
    {CF_REALRANGE,cf_real,"Real number"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg SELECTSERVERS_ARGS[] =
    {
    {CF_NAKEDLRANGE,cf_str,"The identifier of a cfengine list of hosts or addresses to contact"},
    {CF_VALRANGE,cf_int,"The port number"},
    {CF_ANYSTRING,cf_str,"A query string"},
    {CF_ANYSTRING,cf_str,"A regular expression to match success"},     
    {CF_VALRANGE,cf_int,"Maximum number of bytes to read from server"},
    {CF_IDRANGE,cf_str,"Name for array of results"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg SPLAYCLASS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input string for classification"},
    {"daily,hourly",cf_opts,"Splay time policy"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg SPLITSTRING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"A data string"},
    {CF_ANYSTRING,cf_str,"Regex to split on"},     
    {CF_VALRANGE,cf_int,"Maximum number of pieces"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg STRCMP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"String"},
    {CF_ANYSTRING,cf_str,"String"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg TRANSLATEPATH_ARGS[] =
    {
    {CF_ABSPATHRANGE,cf_str,"Unix style path"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg USEMODULE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Name of module command"},
    {CF_ANYSTRING,cf_str,"Argument string for the module"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg USEREXISTS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"User name or identifier"},
    {NULL,cf_notype,NULL}
    };

/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

struct FnCallType CF_FNCALL_TYPES[] = 
   {
   {"accessedbefore",cf_class,2,ACCESSEDBEFORE_ARGS,&FnCallIsAccessedBefore,"True if arg1 was accessed before arg2 (atime)"},
   {"accumulated",cf_int,6,ACCUM_ARGS,&FnCallAccumulatedDate,"Convert an accumulated amount of time into a system representation"},
   {"ago",cf_int,6,AGO_ARGS,&FnCallAgoDate,"Convert a time relative to now to an integer system representation"},
   {"and",cf_str,CF_VARARGS,AND_ARGS,&FnCallAnd,"Calculate whether all arguments evaluate to true"},
   {"canonify",cf_str,1,CANONIFY_ARGS,&FnCallCanonify,"Convert an abitrary string into a legal class name"},
   {"concat",cf_str,CF_VARARGS,CONCAT_ARGS,&FnCallConcat,"Concatenate all arguments into string"},
   {"changedbefore",cf_class,2,CHANGEDBEFORE_ARGS,&FnCallIsChangedBefore,"True if arg1 was changed before arg2 (ctime)"},
   {"classify",cf_class,1,CLASSIFY_ARGS,&FnCallClassify,"True if the canonicalization of the argument is a currently defined class"},
   {"classmatch",cf_class,1,CLASSMATCH_ARGS,&FnCallClassMatch,"True if the regular expression matches any currently defined class"},
   {"countclassesmatching",cf_int,1,COUNTCLASSESMATCHING_ARGS,&FnCallCountClassesMatching,"Count the number of defined classes matching regex arg1"},
   {"countlinesmatching",cf_int,2,COUNTLINESMATCHING_ARGS,&FnCallCountLinesMatching,"Count the number of lines matching regex arg1 in file arg2"},
   {"diskfree",cf_int,1,DISKFREE_ARGS,&FnCallDiskFree,"Return the free space (in KB) available on the directory's current partition (0 if not found)"},
   {"escape",cf_str,1,ESCAPE_ARGS,&FnCallEscape,"Escape regular expression characters in a string"},
   {"execresult",cf_str,2,EXECRESULT_ARGS,&FnCallExecResult,"Execute named command and assign output to variable"},
   {"fileexists",cf_class,1,FILESTAT_ARGS,&FnCallFileStat,"True if the named file can be accessed"},
   {"filesexist",cf_class,1,FILESEXIST_ARGS,&FnCallFileSexist,"True if the named list of files can ALL be accessed"},
   {"filesize",cf_int,1,FILESTAT_ARGS,&FnCallFileStat,"Returns the size in bytes of the file"},
   {"getenv",cf_str,2,GETENV_ARGS,&FnCallGetEnv,"Return the environment variable named arg1, truncated at arg2 characters"},
   {"getfields",cf_int,4,GETFIELDS_ARGS,&FnCallGetFields,"Get an array of fields in the lines matching regex arg1 in file arg2, split on regex arg3 as array name arg4"},
   {"getgid",cf_int,1,GETGID_ARGS,&FnCallGetGid,"Return the integer group id of the named group on this host"},
   {"getindices",cf_slist,1,GETINDICES_ARGS,&FnCallGetIndices,"Get a list of keys to the array whose id is the argument and assign to variable"},
   {"getuid",cf_int,1,GETUID_ARGS,&FnCallGetUid,"Return the integer user id of the named user on this host"},
   {"getusers",cf_slist,2,GETUSERS_ARGS,&FnCallGetUsers,"Get a list of all system users defined, minus those names defined in args 1 and uids in args"},
   {"getvalues",cf_slist,1,GETINDICES_ARGS,&FnCallGetValues,"Get a list of values corresponding to the right hand sides in an array whose id is the argument and assign to variable"},
   {"grep",cf_slist,2,GREP_ARGS,&FnCallGrep,"Extract the sub-list if items matching the regular expression in arg1 of the list named in arg2"},
   {"groupexists",cf_class,1,GROUPEXISTS_ARGS,&FnCallGroupExists,"True if group or numerical id exists on this host"},
   {"hash",cf_str,2,HASH_ARGS,&FnCallHash,"Return the hash of arg1, type arg2 and assign to a variable"},
   {"hashmatch",cf_class,3,HASHMATCH_ARGS,&FnCallHashMatch,"Compute the hash of arg1, of type arg2 and test if it matches the value in arg 3"},
   {"host2ip",cf_str,1,HOST2IP_ARGS,&FnCallHost2IP,"Returns the primary name-service IP address for the named host"},
   {"ip2host",cf_str,1,IP2HOST_ARGS,&FnCallIP2Host,"Returns the primary name-service host name for the IP address"},
   {"hostinnetgroup",cf_class,1,HOSTINNETGROUP_ARGS,&FnCallHostInNetgroup,"True if the current host is in the named netgroup"},
   {"hostrange",cf_class,2,HOSTRANGE_ARGS,&FnCallHostRange,"True if the current host lies in the range of enumerated hostnames specified"},
   {"hostsseen",cf_slist,3,HOSTSSEEN_ARGS,&FnCallHostsSeen,"Extract the list of hosts last seen/not seen within the last arg1 hours"},
   {"hubknowledge",cf_str,1,HUB_KNOWLEDGE_ARGS,&FnCallHubKnowledge,"Read global knowledge from the hub host by id (commercial extension)"},
   {"iprange",cf_class,1,IPRANGE_ARGS,&FnCallIPRange,"True if the current host lies in the range of IP addresses specified"},
   {"irange",cf_irange,2,IRANGE_ARGS,&FnCallIRange,"Define a range of integer values for cfengine internal use"},
   {"isdir",cf_class,1,FILESTAT_ARGS,&FnCallFileStat,"True if the named object is a directory"},
   {"isexecutable",cf_class,1,FILESTAT_ARGS,&FnCallFileStat,"True if the named object has execution rights for the current user"},
   {"isgreaterthan",cf_class,2,ISGREATERTHAN_ARGS,&FnCallIsLessGreaterThan,"True if arg1 is numerically greater than arg2, else compare strings like strcmp"},
   {"islessthan",cf_class,2,ISLESSTHAN_ARGS,&FnCallIsLessGreaterThan,"True if arg1 is numerically less than arg2, else compare strings like NOT strcmp"},
   {"islink",cf_class,1,FILESTAT_ARGS,&FnCallFileStat,"True if the named object is a symbolic link"},
   {"isnewerthan",cf_class,2,ISNEWERTHAN_ARGS,&FnCallIsNewerThan,"True if arg1 is newer (modified later) than arg2 (mtime)"},
   {"isplain",cf_class,1,FILESTAT_ARGS,&FnCallFileStat,"True if the named object is a plain/regular file"},
   {"isvariable",cf_class,1,ISVARIABLE_ARGS,&FnCallIsVariable,"True if the named variable is defined"},
   {"join",cf_str,2,JOIN_ARGS,&FnCallJoin,"Join the items of arg2 into a string, using the conjunction in arg1"},
   {"lastnode",cf_str,2,LASTNODE_ARGS,&FnCallLastNode,"Extract the last of a separated string, e.g. filename from a path"},
   {"laterthan",cf_class,6,LATERTHAN_ARGS,&FnCallLaterThan,"True if the current time is later than the given date"},
   {"ldaparray",cf_class,6,LDAPARRAY_ARGS,&FnCallLDAPArray,"Extract all values from an ldap record"},
   {"ldaplist",cf_slist,6,LDAPLIST_ARGS,&FnCallLDAPList,"Extract all named values from multiple ldap records"},
   {"ldapvalue",cf_str,6,LDAPVALUE_ARGS,&FnCallLDAPValue,"Extract the first matching named value from ldap"},
   {"not",cf_str,1,NOT_ARGS,&FnCallNot,"Calculate whether argument is false"},
   {"now",cf_int,0,NOW_ARGS,&FnCallNow,"Convert the current time into system representation"},
   {"on",cf_int,6,DATE_ARGS,&FnCallOn,"Convert an exact date/time to an integer system representation"},
   {"or",cf_str,CF_VARARGS,OR_ARGS,&FnCallOr,"Calculate whether any argument evaluates to true"},
   {"parseintarray",cf_int,6,PARSESTRINGARRAY_ARGS,&FnCallParseIntArray,"Read an array of integers from a file and assign the dimension to a variable"},
   {"parserealarray",cf_int,6,PARSESTRINGARRAY_ARGS,&FnCallParseRealArray,"Read an array of real numbers from a file and assign the dimension to a variable"},
   {"parsestringarray",cf_int,6,PARSESTRINGARRAY_ARGS,&FnCallParseStringArray,"Read an array of strings from a file and assign the dimension to a variable"},
   {"parsestringarrayidx",cf_int,6,PARSESTRINGARRAYIDX_ARGS,&FnCallParseStringArrayIndex,"Read an array of strings from a file and assign the dimension to a variable with integer indeces"},
   {"peers",cf_slist,3,PEERS_ARGS,&FnCallPeers,"Get a list of peers (not including ourself) from the partition to which we belong"},
   {"peerleader",cf_str,3,PEERLEADER_ARGS,&FnCallPeerLeader,"Get the assigned peer-leader of the partition to which we belong"},
   {"peerleaders",cf_slist,3,PEERLEADERS_ARGS,&FnCallPeerLeaders,"Get a list of peer leaders from the named partitioning"},
   {"product",cf_real,1,PRODUCT_ARGS,&FnCallProduct,"Return the product of a list of reals"},
   {"randomint",cf_int,2,RANDOMINT_ARGS,&FnCallRandomInt,"Generate a random integer between the given limits"},
   {"readfile",cf_str,2,READFILE_ARGS,&FnCallReadFile,"Read max number of bytes from named file and assign to variable"},
   {"readintarray",cf_int,6,READSTRINGARRAY_ARGS,&FnCallReadIntArray,"Read an array of integers from a file and assign the dimension to a variable"},
   {"readintlist",cf_ilist,5,READSTRINGLIST_ARGS,&FnCallReadIntList,"Read and assign a list variable from a file of separated ints"},
   {"readrealarray",cf_int,6,READSTRINGARRAY_ARGS,&FnCallReadRealArray,"Read an array of real numbers from a file and assign the dimension to a variable"},
   {"readreallist",cf_rlist,5,READSTRINGLIST_ARGS,&FnCallReadRealList,"Read and assign a list variable from a file of separated real numbers"},
   {"readstringarray",cf_int,6,READSTRINGARRAY_ARGS,&FnCallReadStringArray,"Read an array of strings from a file and assign the dimension to a variable"},
   {"readstringarrayidx",cf_int,6,READSTRINGARRAYIDX_ARGS,&FnCallReadStringArrayIndex,"Read an array of strings from a file and assign the dimension to a variable with integer indeces"},
   {"readstringlist",cf_slist,5,READSTRINGLIST_ARGS,&FnCallReadStringList,"Read and assign a list variable from a file of separated strings"},
   {"readtcp",cf_str,4,READTCP_ARGS,&FnCallReadTcp,"Connect to tcp port, send string and assign result to variable"},
   {"regarray",cf_class,2,REGARRAY_ARGS,&FnCallRegArray,"True if arg1 matches any item in the associative array with id=arg2"},
   {"regcmp",cf_class,2,REGCMP_ARGS,&FnCallRegCmp,"True if arg1 is a regular expression matching that matches string arg2"},
   {"regextract",cf_class,3,REGEXTRACT_ARGS,&FnCallRegExtract,"True if the regular expression in arg 1 matches the string in arg2 and sets a non-empty array of backreferences named arg3"},
   {"registryvalue",cf_str,2,REGISTRYVALUE_ARGS,&FnCallRegistryValue, "Returns a value for an MS-Win registry key,value pair"},
   {"regline",cf_class,2,REGLINE_ARGS,&FnCallRegLine,"True if the regular expression in arg1 matches a line in file arg2"},
   {"reglist",cf_class,2,REGLIST_ARGS,&FnCallRegList,"True if the regular expression in arg2 matches any item in the list whose id is arg1"},
   {"regldap",cf_class,7,REGLDAP_ARGS,&FnCallRegLDAP,"True if the regular expression in arg6 matches a value item in an ldap search"},
   {"remotescalar",cf_str,3,REMOTESCALAR_ARGS,&FnCallRemoteScalar,"Read a scalar value from a remote cfengine server"},
   {"remoteclassesmatching",cf_class,4,REMOTECLASSESMATCHING_ARGS,&FnCallRemoteClassesMatching,"Read persistent classes matching a regular expression from a remote cfengine server and add them into local context with prefix"},
   {"returnszero",cf_class,2,RETURNSZERO_ARGS,&FnCallReturnsZero,"True if named shell command has exit status zero"},
   {"rrange",cf_rrange,2,RRANGE_ARGS,&FnCallRRange,"Define a range of real numbers for cfengine internal use"},
   {"selectservers",cf_int,6,SELECTSERVERS_ARGS,&FnCallSelectServers,"Select tcp servers which respond correctly to a query and return their number, set array of names"},
   {"splayclass",cf_class,2,SPLAYCLASS_ARGS,&FnCallSplayClass,"True if the first argument's time-slot has arrived, according to a policy in arg2"},
   {"splitstring",cf_slist,3,SPLITSTRING_ARGS,&FnCallSplitString,"Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2"},
   {"strcmp",cf_class,2,STRCMP_ARGS,&FnCallStrCmp,"True if the two strings match exactly"},
   {"sum",cf_real,1,SUM_ARGS,&FnCallSum,"Return the sum of a list of reals"},
   {"translatepath",cf_str,1,TRANSLATEPATH_ARGS,&FnCallTranslatePath,"Translate path separators from Unix style to the host's native"},
   {"usemodule",cf_class,2,USEMODULE_ARGS,&FnCallUseModule,"Execute cfengine module script and set class if successful"},
   {"userexists",cf_class,1,USEREXISTS_ARGS,&FnCallUserExists,"True if user name or numerical id exists on this host"},
   {NULL,cf_notype,0,NULL,NULL}
   };
