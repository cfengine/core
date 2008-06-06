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

from = atoi((char *)(finalargs->item));
to = atoi((char *)(finalargs->next->item));
 
if (from > to)
   {
   tmp = to;
   to = from;
   from = tmp;
   }

range = abs(to-from);
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
   SetFnCallReturnStatus("getgid",FNCALL_FAILURE,NULL,NULL);
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
  char buffer[CF_BUFSIZE];
  int ret = false;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (strcmp(finalargs->next->item,"useshell"))
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

val = atoi(maxbytes);
portnum = (short) atoi(port);

rval.item = NULL;
rval.rtype = CF_NOPROMISEE;

if (val > CF_BUFSIZE-1)
   {
   CfOut(cf_error,"","Too many bytes to read from TCP port %s@%s",port,hostnameip);
   }

Debug("Want to read %d bytes from port %d at %s\n",val,portnum,hostnameip);
    
conn = NewAgentConn();

attr.copy.force_ipv4 = false;
attr.copy.portnumber = portnum;
    
if (!ServerConnect(conn,hostnameip,attr,NULL))
   {
   CfOut(cf_error,"socket","Couldn't open a tcp socket");
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
  char buffer[CF_BUFSIZE];
  int ret = false, useshell = false;
  struct stat statbuf;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

if (strcmp(finalargs->next->item,"useshell"))
   {
   useshell = true;
   }

if (stat(finalargs->item,&statbuf) == -1)
   {
   SetFnCallReturnStatus("returnszero",FNCALL_FAILURE,strerror(errno),NULL);   
   strcmp(buffer,"!any");   
   }
else if (useshell && ShellCommandReturnsZero(finalargs->item,useshell))
   {
   SetFnCallReturnStatus("returnszero",FNCALL_SUCCESS,NULL,NULL);
   strcmp(buffer,"any");
   }
else if (!useshell && ShellCommandReturnsZero(finalargs->item,useshell))
   {
   SetFnCallReturnStatus("returnszero",FNCALL_SUCCESS,NULL,NULL);   
   strcmp(buffer,"any");
   }
else
   {
   SetFnCallReturnStatus("returnszero",FNCALL_FAILURE,strerror(errno),NULL);   
   strcmp(buffer,"!any");
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
else if (frombuf.st_mtime < tobuf.st_mtime)
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
else if (frombuf.st_ctime < tobuf.st_ctime)
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

if (stat(finalargs->item,&statbuf) == -1)
   {
   SetFnCallReturnStatus(CF_FNCALL_TYPES[fn].name,FNCALL_FAILURE,strerror(errno),NULL);   
   strcpy(buffer,"!any");
   }
else
   {
   strcpy(buffer,"!any");
   
   switch (fn)
      {
      case fn_isdir:
          if (S_ISDIR(statbuf.st_mode))
             {
             strcpy(buffer,"any");
             }
          break;
      case fn_islink:
          if (S_ISLNK(statbuf.st_mode))
             {
             strcpy(buffer,"any");
             }
          break;
      case fn_isplain:
          if (S_ISREG(statbuf.st_mode))
             {
             strcpy(buffer,"any");
             }
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
         Debug("Checking IP Range against iface %s\n",VIPADDRESS);
         
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

/* end fn specific content */

rval.rtype = CF_SCALAR;
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
SetFnCallReturnStatus("regcmp",FNCALL_SUCCESS,NULL,NULL);   
argv0 = finalargs->item;
argv1 = finalargs->next->item;

if (FullTextMatch(argv0,argv1))
   {
   strcpy(buffer,"any");   
   }
else
   {
   SetFnCallReturnStatus("regcmp",FNCALL_FAILURE,NULL,NULL);
   strcpy(buffer,"!any");
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

sscanf(argv0,"%lf",&a);
sscanf(argv1,"%lf",&b);

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
SetFnCallReturnStatus("userexists",FNCALL_SUCCESS,NULL,NULL);   

if (isdigit((int)*arg))
   {
   sscanf(arg,"%d",&uid);

   if (uid < 0)
      {
      SetFnCallReturnStatus("userexists",FNCALL_FAILURE,"Illegal user id",NULL);   
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
SetFnCallReturnStatus("groupexists",FNCALL_SUCCESS,NULL,NULL);   

if (isdigit((int)*arg))
   {
   sscanf(arg,"%d",&gid);

   if (gid < 0)
      {
      SetFnCallReturnStatus("groupexists",FNCALL_FAILURE,"Illegal group id",NULL);   
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
  int tmp,from=CF_NOINT,to=CF_NOINT;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

sscanf((char *)(finalargs->item),"%d",&from);
sscanf((char *)(finalargs->next->item),"%d",&to);
   
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
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed int values %s=>%d,%s=>%d\n",(char *)(finalargs->item),from,(char *)(finalargs->next->item),to);
   ReportError(OUTPUT);
   }

if (from > to)
   {
   tmp = to;
   to = from;
   from = tmp;
   }

snprintf(buffer,CF_BUFSIZE-1,"%d,%d",from,to);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallIRange");
   }

/* end fn specific content */

SetFnCallReturnStatus("irange",FNCALL_SUCCESS,NULL,NULL);
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

from = atof((char *)(finalargs->item));
to = atof((char *)(finalargs->next->item));

sscanf((char *)(finalargs->item),"%lf",&from);
sscanf((char *)(finalargs->next->item),"%lf",&to);
   
if (from == CF_NODOUBLE || to == CF_NODOUBLE)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed real values %s=>%lf,%s=>%lf\n",(char *)(finalargs->item),from,(char *)(finalargs->next->item),to);
   ReportError(OUTPUT);
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

/* end fn specific content */

SetFnCallReturnStatus("rrange",FNCALL_SUCCESS,NULL,NULL);
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
  int d[6];
  time_t cftime;
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

rp = finalargs;

for (i = 1; i < 6; i++)
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
   error("Illegal time value");
   }

Debug("Time computed from input was: %s\n",ctime(&cftime));

snprintf(buffer,CF_BUFSIZE-1,"%d",time);

if ((rval.item = strdup(buffer)) == NULL)
   {
   FatalError("Memory allocation in FnCallOnDate");
   }

/* end fn specific content */

SetFnCallReturnStatus("on",FNCALL_SUCCESS,NULL,NULL);
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
  int d[6];
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */


rp = finalargs;

for (i = 1; i < 6; i++)
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
cftime -= d[cfa_month] * 30 * 24 * 3600;
cftime -= d[cfa_year] * 365 * 24 * 3600;

Debug("Total negative offset = %.1f minutes\n",(double)(CFSTARTTIME-cftime)/60.0);
Debug("Time computed from input was: %s\n",ctime(&cftime));

snprintf(buffer,CF_BUFSIZE-1,"%d",time);

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
  int d[6],cftime;
  struct tm tmv;
  enum cfdatetemplate i;
  
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */


rp = finalargs;

for (i = 1; i < 6; i++)
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

snprintf(buffer,CF_BUFSIZE-1,"%d",cftime);

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

rval.item = ReadFile(filename,maxsize);

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
  int maxent,maxsize,count = 0,noerrors = false,purge = true;
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

file_buffer = (char *)ReadFile(filename,maxsize);

if (file_buffer == NULL)
   {
   rval.item = NULL;
   rval.rtype = CF_LIST;
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
      newlist = SplitRegexAsRList(file_buffer,split,maxent,purge);
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

snprintf(fnname,CF_MAXVARSIZE-1,"read%slist",CF_DATATYPES[type]);
       
if (newlist && noerrors)
   {
   SetFnCallReturnStatus(fnname,FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus(fnname,FNCALL_FAILURE,NULL,NULL);
   }

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
  int maxent,maxsize,count = 0,noerrors = false;
  char *file_buffer = NULL;

ArgTemplate(fp,argtemplate,argtypes,finalargs); /* Arg validation */

/* begin fn specific content */

 /* 6 args: array_lval,filename,comment_regex,split_regex,max number of entries,maxfilesize  */

array_lval = (char *)(finalargs->item);
filename = (char *)(finalargs->next->item);
comment = (char *)(finalargs->next->next->item);
split = (char *)(finalargs->next->next->next->item);
maxent = Str2Int(finalargs->next->next->next->next->item);
maxsize = Str2Int(finalargs->next->next->next->next->next->item);

// Read once to validate structure of file in itemlist

Debug("Read string data from file %s\n",filename);

file_buffer = (char *)ReadFile(filename,maxsize);

if (file_buffer == NULL)
   {
   rval.item = NULL;
   rval.rtype = CF_LIST;
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
      BuildLineArray(array_lval,file_buffer,split,maxent,type);
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

snprintf(fnname,CF_MAXVARSIZE-1,"read%slist",CF_DATATYPES[type]);
       
if (newlist && noerrors)
   {
   SetFnCallReturnStatus(fnname,FNCALL_SUCCESS,NULL,NULL);
   }
else
   {
   SetFnCallReturnStatus(fnname,FNCALL_FAILURE,NULL,NULL);
   }

rval.item = strdup("any");
rval.rtype = CF_SCALAR;
return rval;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void *ReadFile(char *filename,int maxsize)

{ struct stat sb;
  char *result = NULL;
  FILE *fp;
  size_t size;

if (stat(filename,&sb) == -1)
   {
   CfOut(cf_error,"stat","Could not examine file %s in readfile",filename);
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
   CfOut(cf_error,"stat","Could not examine file %s in readfile",filename);
   return NULL;
   }

if ((fp = fopen(filename,"r")) == NULL)
   {
   CfOut(cf_error,"fopen","Could not open file %s in readfile",filename);
   return NULL;
   }

if (fread(result,size,1,fp) != 1)
   {
   CfOut(cf_error,"fread","Could not read expected amount from file %s in readfile",filename);
   }

result[size] = '\0';

fclose(fp);
return (void *)result;
}

/*********************************************************************/

char *StripPatterns(char *file_buffer,char *pattern)

{ regmatch_t pm;

while(BlockTextMatch(pattern,file_buffer,&pm))
   {
   CloseStringHole(file_buffer,pm.rm_so,pm.rm_eo);
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

void BuildLineArray(char *array_lval,char *file_buffer,char *split,int maxent,enum cfdatatype type)

{ char *sp,linebuf[CF_BUFSIZE],name[CF_MAXVARSIZE];
  struct Rlist *rp,*newlist = NULL;
  int nopurge = false, vcount,hcount;

memset(linebuf,0,CF_BUFSIZE);
hcount = 0;

for (sp = file_buffer; hcount < maxent && *sp != '\0'; sp++)
   {
   sscanf(sp,"%1023[^\n]",linebuf);

   newlist = SplitRegexAsRList(linebuf,split,maxent,nopurge);
   
   vcount = 0;
   
   for (rp = newlist; rp != NULL; rp=rp->next)
      {
      snprintf(name,CF_MAXVARSIZE,"%s[%s][%d]",array_lval,newlist->item,vcount);
      NewScalar(CONTEXTID,name,rp->item,type);
      vcount++;
      }
   
   hcount++;
   sp += strlen(linebuf);
   }

/* Don't free data - goes into vars*/

//ShowScopedVariables(stdout);
}
