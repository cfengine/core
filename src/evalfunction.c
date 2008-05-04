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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
  
  struct Rlist *rp;
  struct Rval rval;
  char buffer[CF_BUFSIZE];
  int ret = false;
  char *sp,*hostnameip,*maxbytes,*port,*sendstring;
  int val = 0, n_read = 0;
  short portnum;

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
   snprintf(OUTPUT,CF_BUFSIZE,"Too many bytes to read from TCP port %s@%s",port,hostnameip);
   CfLog(cferror,OUTPUT,"");
   }

Debug("Want to read %d bytes from port %d at %s\n",val,portnum,hostnameip);

CONN = NewAgentConn();

if (!RemoteConnect(hostnameip,'n',portnum,port))
   {
   CfLog(cferror,"Couldn't open a tcp socket","socket");

   if (CONN->sd != CF_NOT_CONNECTED)
      {
      close(CONN->sd);
      CONN->sd = CF_NOT_CONNECTED;
      }

   DeleteAgentConn(CONN);
   SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,strerror(errno),NULL);
   rval.item = NULL;
   rval.rtype = CF_SCALAR;
   return rval;
   }

if (strlen(sendstring) > 0)
   {
   if (SendSocketStream(CONN->sd,sendstring,strlen(sendstring),0) == -1)
      {
      DeleteAgentConn(CONN);
      SetFnCallReturnStatus("readtcp",FNCALL_FAILURE,strerror(errno),NULL);
      rval.item = NULL;
      rval.rtype = CF_SCALAR;
      return rval;   
      }
   }

if ((n_read = recv(CONN->sd,buffer,val,0)) == -1)
   {
   }

close(CONN->sd);
DeleteAgentConn(CONN);

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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

buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

/* begin fn specific content */

SetFnCallReturnStatus("isvariable",FNCALL_SUCCESS,NULL,NULL);   

CfLog(cferror,"RegCmp() not yet implemented due to design changes","");

strcpy(buffer,"!any");

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
  char *argv0 = finalargs->item;
  char *argv1 = finalargs->next->item;
  double a = CF_NOVAL,b = CF_NOVAL;
 
buffer[0] = '\0';  
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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
ArgTemplate(fp,argtemplate,argtypes); /* Arg validation */

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

