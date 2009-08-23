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

  /* assume args are all scalar literals by the time we get here
     and each handler allocates the memory it returns. There is
     a protocol to be followed here:
     Set args,
     Eval Content,
     Set rtype,
     ErrorFlags

     returnval = FnCallXXXResult(fp)
     
  */

/*********************************************************************/

struct Rval FnCallRandomInt(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_INTRANGE,
     CF_INTRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_int,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  int tmp,range,result,from=-1,to=-1;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallGetUid(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  struct passwd *pw;
  char buffer[CF_BUFSIZE];
  uid_t uid;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

/*********************************************************************/

struct Rval FnCallGetGid(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  struct group *gr;
  char buffer[CF_BUFSIZE];
  gid_t gid;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

/*********************************************************************/

struct Rval FnCallHash(struct FnCall *fp,struct Rlist *finalargs)

/* Hash(string,md5|sha1|crypt) */
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     "md5,sha1,crypt",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE],*string,*typestring;
  unsigned char digest[EVP_MAX_MD_SIZE+1];
  enum cfhashes type;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

string = finalargs->item;
typestring = finalargs->next->item;

type = String2HashType(typestring);
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

struct Rval FnCallHashMatch(struct FnCall *fp,struct Rlist *finalargs)

/* HashMatch(string,md5|sha1|crypt,"abdxy98edj") */
    
{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     "md5,sha1,crypt",
     CF_IDRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_opts,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE],ret[CF_BUFSIZE],*string,*typestring,*compare;
  unsigned char digest[EVP_MAX_MD_SIZE+1];
  enum cfhashes type;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

string = finalargs->item;
typestring = finalargs->next->item;
compare = finalargs->next->next->item;

type = String2HashType(typestring);
HashString(string,strlen(string),digest,type);
snprintf(buffer,CF_BUFSIZE-1,"%s",HashPrint(type,digest));
CfOut(cf_verbose,""," -> File %s hashes to %s, compare to %s\n",string,buffer,compare);

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

struct Rval FnCallClassMatch(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct Item *ip;
  
strcpy(buffer,"!any");
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

for (ip = VHEAP; ip != NULL; ip=ip->next)
   {
   if (FullTextMatch((char *)finalargs->item,ip->name))
      {
      SetFnCallReturnStatus("classmatch",FNCALL_SUCCESS,NULL,NULL);
      strcpy(buffer,"any");
      break;
      }
   }

for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
   {
   if (FullTextMatch((char *)finalargs->item,ip->name))
      {
      SetFnCallReturnStatus("classmatch",FNCALL_SUCCESS,NULL,NULL);
      strcpy(buffer,"any");
      break;
      }
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

struct Rval FnCallCanonify(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  struct Item *ip;
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallLastNode(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp,*newlist;
  struct Rval rval;
  struct Item *ip;
  char *name,*split;
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallClassify(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallReturnsZero(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     "useshell,noshell",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE],comm[CF_BUFSIZE];
  int ret = false, useshell = false;
  struct stat statbuf;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (strcmp(finalargs->next->item,"useshell") == 0)
   {
   useshell = true;
   snprintf(comm,CF_BUFSIZE,"%s",finalargs->item);
   }
else
   {
   useshell = false;
   snprintf(comm,CF_BUFSIZE,"%s",finalargs->item);
   }

if (stat(GetArg0(finalargs->item),&statbuf) == -1)
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
 
if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallReturnsZero");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

struct Rval FnCallExecResult(struct FnCall *fp,struct Rlist *finalargs)

  /* execresult("/programpath",useshell|noshell) */
    
{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     "useshell,noshell",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_EXPANDSIZE];
  int ret = false;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

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

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallExecResult");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

struct Rval FnCallUseModule(struct FnCall *fp,struct Rlist *finalargs)

  /* usemodule("/programpath",varargs) */
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
 
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE],modulecmd[CF_BUFSIZE];
  int ret = false;
  char *command,*args;
  struct stat statbuf;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

command = finalargs->item;
args = finalargs->next->item;
snprintf(modulecmd,CF_BUFSIZE,"%s/modules/%s",CFWORKDIR,command);
 
if (stat(GetArg0(modulecmd),&statbuf) == -1)
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
      snprintf(modulecmd,CF_BUFSIZE,"%s/modules/%s %s",CFWORKDIR,command,args);
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

struct Rval FnCallSplayClass(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     "daily,hourly",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE],class[CF_MAXVARSIZE],hrs[CF_MAXVARSIZE];
  enum cfinterval policy;
  char *splay;
  int hash,box,hours,minblocks;
  double period;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

splay = finalargs->item;
policy = Str2Interval(finalargs->next->item);

switch(policy)
   {
   default:
   case cfa_daily:
       period = 12.0*24.0;
       break;

   case cfa_hourly:
       period = 12.0;
       break;
   }

SetFnCallReturnStatus("splayclass",FNCALL_SUCCESS,strerror(errno),NULL);   

hash = (double)Hash(splay);
box = (int)(0.5 + period*hash/(double)CF_HASHTABLESIZE);

minblocks = box % 12;
hours = box / 12;

if (hours == 0)
   {
   strcpy(hrs,"any");
   }
else if (hours > 0 && hours < 10)
   {
   snprintf(hrs,CF_MAXVARSIZE-1,"Hr0%d",hours);
   }
else
   {
   snprintf(hrs,CF_MAXVARSIZE-1,"Hr%d",hours);
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

struct Rval FnCallReadTcp(struct FnCall *fp,struct Rlist *finalargs)

 /* ReadTCP(localhost,80,'GET index.html',1000) */
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_VALRANGE,
     CF_ANYSTRING,
     CF_VALRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_int,
      cf_str,
      cf_int,
      cf_notype
      };

  struct cfagent_connection *conn = NULL;
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  int ret = false;
  char *sp,*hostnameip,*maxbytes,*port,*sendstring;
  int val = 0, n_read = 0;
  short portnum;
  struct Attributes attr;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
      close(conn->sd);
      DeleteAgentConn(conn);
      SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,strerror(errno),NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;   
      }

   signal(SIGALRM,(void *)TimeOut);
   alarm(CF_TIMEOUT);
   
   if ((n_read = recv(conn->sd,buffer,val,0)) == -1)
      {
      }

   alarm(0);
   signal(SIGALRM,SIG_DFL);

   if (n_read == -1)
      {
      close(conn->sd);
      DeleteAgentConn(conn);
      SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,strerror(errno),NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;         
      }
   }

close(conn->sd);
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

struct Rval FnCallRegList(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_NAKEDLRANGE,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };

  struct Rlist *rp,*list;
  struct Rval rval;
  char buffer[CF_BUFSIZE],naked[CF_MAXVARSIZE],rettype;
  int ret = false;
  char *regex,*listvar;
  void *retval;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallRegArray(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_IDRANGE,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };

  char lval[CF_MAXVARSIZE],scopeid[CF_MAXVARSIZE],rettype;
  char *regex,*arrayname,match[CF_MAXVARSIZE],buffer[CF_BUFSIZE];
  struct Scope *ptr;
  struct Rval rval;
  int i;

ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   if (ptr->hashtable[i] != NULL)
      {
      snprintf(match,CF_MAXVARSIZE,"%s[",lval);
      if (strncmp(match,ptr->hashtable[i]->lval,strlen(match)) == 0)
         {
         if (FullTextMatch(regex,ptr->hashtable[i]->rval))
            {
            strcpy(buffer,"any");
            break;
            }
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

struct Rval FnCallGetIndices(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_IDRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };

  char lval[CF_MAXVARSIZE],scopeid[CF_MAXVARSIZE],rettype;
  char *arrayname,index[CF_MAXVARSIZE],match[CF_MAXVARSIZE];
  struct Scope *ptr;
  struct Rval rval;
  struct Rlist *returnlist = NULL;
  int i;

ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

arrayname = finalargs->item;

/* Locate the array */

if (strstr(arrayname,"."))
   {
   scopeid[0] = '\0';
   sscanf(arrayname,"%[^127.].%127s",scopeid,lval);
   }
else
   {
   strcpy(lval,arrayname);
   strcpy(scopeid,CONTEXTID);
   }

if ((ptr = GetScope(scopeid)) == NULL)
   {
   CfOut(cf_error,"","Function getindices was promised an array in scope \"%s\" but this was not found\n",scopeid);
   SetFnCallReturnStatus("getindices",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_LIST;
   return rval;            
   }

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   snprintf(match,CF_MAXVARSIZE-1,"%.127s[",lval);

   if (ptr->hashtable[i] != NULL)
      {
      if (strncmp(match,ptr->hashtable[i]->lval,strlen(match)) == 0)
         {
         char *sp;
         index[0] = '\0';
         sscanf(ptr->hashtable[i]->lval+strlen(match),"%127s",index);

         if (sp = strchr(index,']'))
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
   }   

if (returnlist == NULL)
   {
   SetFnCallReturnStatus("getindices",FNCALL_FAILURE,"Array not found in scope",NULL);
   rval.item = NULL;
   rval.rtype = CF_LIST;
   return rval;               
   }

SetFnCallReturnStatus("getindices",FNCALL_SUCCESS,NULL,NULL);
rval.item = returnlist;

/* end fn specific content */

rval.rtype = CF_LIST;
return rval;
}

/*********************************************************************/

struct Rval FnCallSelectServers(struct FnCall *fp,struct Rlist *finalargs)

 /* ReadTCP(localhost,80,'GET index.html',1000) */
    
{ static char *argtemplate[] =
     {
     CF_NAKEDLRANGE,
     CF_VALRANGE,
     CF_ANYSTRING,
     CF_ANYSTRING,     
     CF_VALRANGE,
     CF_IDRANGE,     
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_int,
      cf_str,
      cf_str,
      cf_int,
      cf_str,
      cf_notype
      };

  struct cfagent_connection *conn = NULL;
  struct Rlist *rp,*hostnameip;
  struct Rval rval;
  char buffer[CF_BUFSIZE],naked[CF_MAXVARSIZE],rettype;
  int ret = false;
  char *sp,*maxbytes,*port,*sendstring,*regex,*array_lval,*listvar;
  int val = 0, n_read = 0,count = 0;
  short portnum;
  struct Attributes attr;
  void *retval;
  struct Promise *pp;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   CfOut(cf_error,"","Function selectservers was promised a list called \"%s\" but this was not found\n",listvar);
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
   Debug("Want to read %d bytes from port %d at %s\n",val,portnum,rp->item);
   
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
         close(conn->sd);
         DeleteAgentConn(conn);
         continue;
         }
      
      signal(SIGALRM,(void *)TimeOut);
      alarm(CF_TIMEOUT);
      
      if ((n_read = recv(conn->sd,buffer,val,0)) == -1)
         {
         }
      
      alarm(0);
      signal(SIGALRM,SIG_DFL);
      
      if (n_read == -1)
         {
         close(conn->sd);
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
   
   close(conn->sd);
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

struct Rval FnCallIsNewerThan(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_PATHRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat frombuf,tobuf;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (stat(finalargs->item,&frombuf) == -1)
   {
   SetFnCallReturnStatus("isnewerthan",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (stat(finalargs->next->item,&tobuf) == -1)
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
   FatalError("Memory allocation in FnCallReturnsZero");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

struct Rval FnCallIsAccessedBefore(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_PATHRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat frombuf,tobuf;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (stat(finalargs->item,&frombuf) == -1)
   {
   SetFnCallReturnStatus("isaccessedbefore",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (stat(finalargs->next->item,&tobuf) == -1)
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

struct Rval FnCallIsChangedBefore(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_PATHRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat frombuf,tobuf;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (stat(finalargs->item,&frombuf) == -1)
   {
   SetFnCallReturnStatus("ischangedbefore",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (stat(finalargs->next->item,&tobuf) == -1)
   {
   SetFnCallReturnStatus("ischangedbefore",FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else if (frombuf.st_ctime > tobuf.st_ctime)
   {
   strcpy(buffer,"any");
   SetFnCallReturnStatus("ischangedbefore",FNCALL_SUCCESS,NULL,NULL);   
   }
else
   {
   strcpy(buffer,"!any");
   SetFnCallReturnStatus("ischangedbefore",FNCALL_SUCCESS,NULL,NULL);   
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

struct Rval FnCallStatInfo(struct FnCall *fp,struct Rlist *finalargs,enum fncalltype fn)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat statbuf;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (lstat(finalargs->item,&statbuf) == -1)
   {
   strcpy(buffer,"!any");
   }
else
   {
   strcpy(buffer,"!any");

   switch (fn)
      {
      case cfn_isdir:
          if (S_ISDIR(statbuf.st_mode))
             {
             strcpy(buffer,"any");
             }
          break;
      case cfn_islink:
          if (S_ISLNK(statbuf.st_mode))
             {
             strcpy(buffer,"any");
             }
          break;
      case cfn_isplain:
          if (S_ISREG(statbuf.st_mode))
             {
             strcpy(buffer,"any");
             }
          break;
      case cfn_fileexists:
          strcpy(buffer,"any");
          break;
      }

   SetFnCallReturnStatus(CF_FNCALL_TYPES[fn].name,FNCALL_SUCCESS,NULL,NULL);
   }

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallStatInfo");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

struct Rval FnCallIPRange(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct Item *ip;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallHostRange(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct Item *ip;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallHostInNetgroup(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *machine, *user, *domain;
   
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallIsVariable(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_IDRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallStrCmp(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

SetFnCallReturnStatus("isvariable",FNCALL_SUCCESS,NULL,NULL);   

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

struct Rval FnCallRegistryValue(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct stat frombuf,tobuf;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (GetRegistryValue(finalargs->item,finalargs->next->item,buffer))
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

struct Rval FnCallRemoteScalar(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_IDRANGE,
     CF_ANYSTRING,
     CF_BOOL,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *handle,*server;
  int encrypted;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   strncpy(buffer,GetRemoteScalar(handle,server,encrypted),CF_BUFSIZE-1);
   
   if (strncmp(buffer,"BAD:",4) == 0)
      {
      if (RetrieveUnreliableValue("remotescalar",handle,buffer))
         {
         SetFnCallReturnStatus("remotescalar",FNCALL_SUCCESS,NULL,NULL);
         }
      else
         {
         SetFnCallReturnStatus("remotescalar",FNCALL_FAILURE,NULL,NULL);
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

struct Rval FnCallPeers(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_ANYSTRING,
     CF_VALRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp,*newlist,*pruned;
  struct Rval rval;
  char *split = "\n",buffer[CF_BUFSIZE];
  char *filename,*comment,*file_buffer = NULL;
  int i,found,groupsize,maxent = 100000,maxsize = 100000;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   file_buffer = StripPatterns(file_buffer,comment);

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

struct Rval FnCallPeerLeader(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_ANYSTRING,
     CF_VALRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp,*newlist;
  struct Rval rval;
  char *split = "\n";
  char *filename,*comment,*file_buffer = NULL,buffer[CF_MAXVARSIZE];
  int i,found,groupsize,maxent = 100000,maxsize = 100000;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   file_buffer = StripPatterns(file_buffer,comment);

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

struct Rval FnCallPeerLeaders(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_ANYSTRING,
     CF_VALRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp,*newlist,*pruned;
  struct Rval rval;
  char *split = "\n";
  char *filename,*comment,*file_buffer = NULL,buffer[CF_MAXVARSIZE];
  int i,found,groupsize,maxent = 100000,maxsize = 100000;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   file_buffer = StripPatterns(file_buffer,comment);

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

struct Rval FnCallRegCmp(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct Item *list = NULL, *ret; 
  char *argv0,*argv1;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallRegLine(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE],line[CF_BUFSIZE];
  struct Item *list = NULL, *ret; 
  char *argv0,*argv1;
  FILE *fin;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallGreaterThan(struct FnCall *fp,struct Rlist *finalargs,char ch)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };

 static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  char *argv0,*argv1;
  double a = CF_NOVAL,b = CF_NOVAL;
 
buffer[0] = '\0';  

ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

argv0 = finalargs->item;
argv1 = finalargs->next->item;

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

switch (ch)
   {
   case '+':
       SetFnCallReturnStatus("isgreaterthan",FNCALL_SUCCESS,NULL,NULL);
       break;
   case '-':
       SetFnCallReturnStatus("islessthan",FNCALL_SUCCESS,NULL,NULL);
       break;
   }

if ((a != CF_NOVAL) && (b != CF_NOVAL)) 
   {
   Debug("%s and %s are numerical\n",argv0,argv1);
   
   if (ch == '+')
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
else if (strcmp(argv0,argv1) > 0)
   {
   Debug("%s and %s are NOT numerical\n",argv0,argv1);
   
   if (ch == '+')
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
   if (ch == '+')
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

struct Rval FnCallUserExists(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct passwd *pw;
  uid_t uid = -1;
  char *arg = finalargs->item;
 
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

strcpy(buffer,CF_ANYCLASS);

if (isdigit((int)*arg))
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

struct Rval FnCallGroupExists(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  struct group *gr;
  gid_t gid = -1;
  char *arg = finalargs->item;
 
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   FatalError("Memory allocation in FnCallUserExists");
   }

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

struct Rval FnCallIRange(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  long tmp,from=CF_NOINT,to=CF_NOINT;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

if (strcmp((char *)(finalargs->item),"inf") == 0)
   {
   from = CF_INFINITY;
   }

if (strcmp((char *)(finalargs->item),"now") == 0)
   {
   from = CFSTARTTIME;
   }

if (strcmp((char *)(finalargs->next->item),"inf") == 0)
   {
   to = CF_INFINITY;
   }

if (strcmp((char *)(finalargs->next->item),"now") == 0)
   {
   to = CFSTARTTIME;
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

struct Rval FnCallRRange(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_REALRANGE,
     CF_REALRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_real,
      cf_real,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  int tmp,range,result;
  double from=CF_NODOUBLE,to=CF_NODOUBLE;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallOnDate(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     "1970,3000",  /* year*/
     "1,12",       /* month */
     "1,31",       /* day */
     "0,23",       /* hour */
     "0,59",       /* min */
     "0,59",       /* sec */
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  long d[6];
  time_t cftime;
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

Debug("Time computed from input was: %s\n",ctime(&cftime));

snprintf(buffer,CF_BUFSIZE-1,"%ld",cftime);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallOnDate");
   }

SetFnCallReturnStatus("on",FNCALL_SUCCESS,NULL,NULL);

/* end fn specific content */

rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/

struct Rval FnCallAgoDate(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     "0,50",       /* year*/
     "0,11",       /* month */
     "0,31",       /* day */
     "0,23",       /* hour */
     "0,59",       /* min */
     "0,59",       /* sec */
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  time_t cftime;
  long d[6];
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
Debug("Time computed from input was: %s\n",ctime(&cftime));

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

struct Rval FnCallAccumulatedDate(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     "0,1",        /* year*/
     "0,12",       /* month */
     "0,31",       /* day */
     "0,23",       /* hour */
     "0,59",       /* min */
     "0,59",       /* sec */
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  long d[6], cftime;
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallNow(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_notype
      };
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  int d[6];
  time_t cftime;
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

cftime = CFSTARTTIME;

Debug("Time computed from input was: %s\n",ctime(&cftime));

snprintf(buffer,CF_BUFSIZE-1,"%d",time);

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

struct Rval FnCallReadFile(struct FnCall *fp,struct Rlist *finalargs)
    
{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_VALRANGE,
     NULL
     };
 
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_int,
      cf_notype
      };
  
  struct Rval rval;
  char *filename;
  int maxsize;

ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallReadStringList(struct FnCall *fp,struct Rlist *finalargs,enum cfdatatype type)
    
{ static char *argtemplate[] =
     {
     CF_PATHRANGE,
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_VALRANGE,
     CF_VALRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_str,
      cf_int,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp,*newlist = NULL;
  struct Rval rval;
  char *filename,*comment,*split,fnname[CF_MAXVARSIZE];
  int maxent,maxsize,count = 0,noerrors = true,blanks = false;
  char *file_buffer = NULL;

ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   file_buffer = StripPatterns(file_buffer,comment);

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

/*********************************************************************/

struct Rval FnCallReadStringArray(struct FnCall *fp,struct Rlist *finalargs,enum cfdatatype type)

/* lval,filename,separator,comment,Max number of bytes  */

{ static char *argtemplate[] =
     {
     CF_IDRANGE,
     CF_PATHRANGE,
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_VALRANGE,
     CF_VALRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_str,
      cf_str,
      cf_int,
      cf_int,
      cf_notype
      };
  
  struct Rlist *rp,*newlist = NULL;
  struct Rval rval;
  char *array_lval,*filename,*comment,*split,fnname[CF_MAXVARSIZE];
  int maxent,maxsize,count = 0,noerrors = false,entries = 0;
  char *file_buffer = NULL;

ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

snprintf(fnname,CF_MAXVARSIZE-1,"read%sarray",CF_DATATYPES[type]);

 /* 6 args: array_lval,filename,comment_regex,split_regex,max number of entries,maxfilesize  */

array_lval = (char *)(finalargs->item);
filename = (char *)(finalargs->next->item);
comment = (char *)(finalargs->next->next->item);
split = (char *)(finalargs->next->next->next->item);
maxent = Str2Int(finalargs->next->next->next->next->item);
maxsize = Str2Int(finalargs->next->next->next->next->next->item);

// Read once to validate structure of file in itemlist

Debug("Read string data from file %s - , maxent %d\n",filename,maxent);

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
   file_buffer = StripPatterns(file_buffer,comment);

   if (file_buffer == NULL)
      {
      rval.item = NULL;
      rval.rtype = CF_LIST;
      return rval;
      }
   else
      {
      entries = BuildLineArray(array_lval,file_buffer,split,maxent,type);
      }
   }

switch(type)
   {
   case cf_str:
       break;

   case cf_int:
       break;

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

struct Rval FnCallSplitString(struct FnCall *fp,struct Rlist *finalargs)
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_INTRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_int,
      cf_notype
      };
  
  struct Rlist *newlist = NULL;
  struct Rval rval;
  char *string,*split,fnname[CF_MAXVARSIZE];
  int max = 0,noerrors = true,purge = true;
  void *newval;
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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

struct Rval FnCallFileSexist(struct FnCall *fp,struct Rlist *finalargs)

{ static char *argtemplate[] =
     {
     CF_NAKEDLRANGE,
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_notype
      };

  char *listvar;
  struct Rlist *rp,*files;
  struct Rval rval;
  char buffer[CF_BUFSIZE],naked[CF_MAXVARSIZE],rettype;
  void *retval;
  struct stat sb;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

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
   if (stat(rp->item,&sb) == -1)
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

struct Rval FnCallLDAPValue(struct FnCall *fp,struct Rlist *finalargs)
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     "subtree,onelevel,base",
     "none,ssl,sasl",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_str,
      cf_str,
      cf_opts,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *newlist = NULL;
  struct Rval rval;
  char *uri,*dn,*filter,*name,*scope,*sec;
  char buffer[CF_BUFSIZE],handle[CF_BUFSIZE];
  void *newval = NULL;
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */
 
   uri = (char *)(finalargs->item);
    dn = (char *)(finalargs->next->item);
filter = (char *)(finalargs->next->next->item);
  name = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->item);

snprintf(handle,CF_BUFSIZE,"%s_%s_%s_%s",dn,filter,name,scope);
   
if (newval = CfLDAPValue(uri,dn,filter,name,scope,sec))
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

struct Rval FnCallLDAPArray(struct FnCall *fp,struct Rlist *finalargs)
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     "subtree,onelevel,base",
     "none,ssl,sasl",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_str,
      cf_str,
      cf_opts,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *newlist = NULL;
  struct Rval rval;
  char *array,*uri,*dn,*filter,*scope,*sec;
  void *newval;
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

 array = (char *)(finalargs->item);
   uri = (char *)(finalargs->next->item);
    dn = (char *)(finalargs->next->next->item);
filter = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->item);
   
if (newval = CfLDAPArray(array,uri,dn,filter,scope,sec))
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

struct Rval FnCallLDAPList(struct FnCall *fp,struct Rlist *finalargs)
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     "subtree,onelevel,base",
     "none,ssl,sasl",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_str,
      cf_str,
      cf_opts,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *newlist = NULL;
  struct Rval rval;
  char *uri,*dn,*filter,*name,*scope,*sec;
  void *newval;
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

   uri = (char *)(finalargs->item);
    dn = (char *)(finalargs->next->item);
filter = (char *)(finalargs->next->next->item);
  name = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->item);

if (newval = CfLDAPList(uri,dn,filter,name,scope,sec))
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

struct Rval FnCallRegLDAP(struct FnCall *fp,struct Rlist *finalargs)
    
{ static char *argtemplate[] =
     {
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     CF_ANYSTRING,
     "subtree,onelevel,base",
     CF_ANYSTRING,
     "none,ssl,sasl",
     NULL
     };
  static enum cfdatatype argtypes[] =
      {
      cf_str,
      cf_str,
      cf_str,
      cf_str,
      cf_opts,
      cf_str,
      cf_opts,
      cf_notype
      };
  
  struct Rlist *newlist = NULL;
  struct Rval rval;
  char *uri,*dn,*filter,*name,*scope,*regex,*sec;
  void *newval;
  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

   uri = (char *)(finalargs->item);
    dn = (char *)(finalargs->next->item);
filter = (char *)(finalargs->next->next->item);
  name = (char *)(finalargs->next->next->next->item);
 scope = (char *)(finalargs->next->next->next->next->item);
 regex = (char *)(finalargs->next->next->next->next->next->item);
   sec = (char *)(finalargs->next->next->next->next->next->next->item);

if (newval = CfRegLDAP(uri,dn,filter,name,scope,regex,sec))
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
/* Level                                                             */
/*********************************************************************/

void *CfReadFile(char *filename,int maxsize)

{ struct stat sb;
  char *result = NULL;
  FILE *fp;
  size_t size;

if (stat(filename,&sb) == -1)
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
         CfOut(cf_inform,"stat","Could not examine file %s in readfile",filename);
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

result = malloc(size+1);
   
if (result == NULL)
   {
   CfOut(cf_error,"stat","Could not allocate file %s in readfile",filename);
   return NULL;
   }

if ((fp = fopen(filename,"r")) == NULL)
   {
   CfOut(cf_inform,"fopen","Could not open file %s in readfile",filename);
   return NULL;
   }

if (fread(result,size,1,fp) != 1)
   {
   CfOut(cf_inform,"fread","Could not read expected amount from file %s in readfile",filename);
   fclose(fp);
   return NULL;
   }

result[size] = '\0';

fclose(fp);
return (void *)result;
}

/*********************************************************************/

char *StripPatterns(char *file_buffer,char *pattern)

{ int start,end;

while(BlockTextMatch(pattern,file_buffer,&start,&end))
   {
   CloseStringHole(file_buffer,start,end);
   }

return file_buffer;
}

/*********************************************************************/

void CloseStringHole(char *s,int start,int end)

{ int count,off = end - start;
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

int BuildLineArray(char *array_lval,char *file_buffer,char *split,int maxent,enum cfdatatype type)

{ char *sp,linebuf[CF_BUFSIZE],name[CF_MAXVARSIZE];
  struct Rlist *rp,*newlist = NULL;
  int nopurge = false, vcount,hcount;

memset(linebuf,0,CF_BUFSIZE);
hcount = 0;

for (sp = file_buffer; hcount < maxent && *sp != '\0'; sp++)
   {
   linebuf[0] = '\0';
   sscanf(sp,"%1023[^\n]",linebuf);

   if (strlen(linebuf) == 0)
      {
      continue;
      }

   newlist = SplitRegexAsRList(linebuf,split,maxent,nopurge);
   
   vcount = 0;
   
   for (rp = newlist; rp != NULL; rp=rp->next)
      {
      snprintf(name,CF_MAXVARSIZE,"%s[%s][%d]",array_lval,newlist->item,vcount);
      NewScalar(THIS_BUNDLE,name,rp->item,type);
      vcount++;
      }

   hcount++;
   sp += strlen(linebuf);
   }

/* Don't free data - goes into vars */
return hcount;
}

/*********************************************************************/

int ExecModule(char *command)

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
  char *sp;

for (sp = command+strlen(command)-1; sp >= command && *sp != FILE_SEPARATOR; sp--)
   {
   strncpy(context,sp,CF_MAXVARSIZE);
   }

NewScope(context);

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
       
   default:
       if (print)
          {
          CfOut(cf_error,"","M \"%s\": %s\n",command,line);
          }
       break;
   }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int CheckID(char *id)

{ char *sp;

for (sp = id; *sp != '\0'; sp++)
   {
   if (!isalnum((int)*sp) && (*sp != '_'))
      {
      CfOut(cf_error,"","Module protocol contained an illegal character (%c) in class/variable identifier %s.",*sp,id);
      return false;
      }
   }

return true;
}

