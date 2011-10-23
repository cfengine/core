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
/* File: conversion.c                                                        */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static int IsSpace(char *remainder);

/*****************************************************************************/

void IPString2KeyDigest(char *ipv4,char *result)

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  char name[CF_BUFSIZE];
  void *value;
  struct CfKeyHostSeen entry;
  int ksize,vsize;
  unsigned char digest[EVP_MAX_MD_SIZE+1];

result[0] = '\0';

if (strcmp(ipv4,"127.0.0.1") == 0 || strcmp(ipv4,"::1") == 0 || strcmp(ipv4,VIPADDRESS) == 0)
   {
   if (PUBKEY)
      {
      HashPubKey(PUBKEY,digest,CF_DEFAULT_DIGEST);
      snprintf(result,CF_MAXVARSIZE,"%s",HashPrint(CF_DEFAULT_DIGEST,digest));
      }
   return;
   }

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan last-seen database");
   CloseDB(dbp);
   return;
   }

 /* Initialize the key/data return pair. */

memset(&entry, 0, sizeof(entry));

 /* Walk through the database and print out the key/data pairs. */

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   if (value != NULL)
      {
      memcpy(&entry,value,sizeof(entry));

      // Warning this is not 1:1
      
      if (strcmp(ipv4,MapAddress((char *)entry.address)) == 0)
         {
         CfOut(cf_verbose,""," -> Matched IP %s to key %s",ipv4,key+1);
         strncpy(result,key+1,CF_MAXVARSIZE-1);
         break;
         }
      }
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);

if(EMPTY(result))
   {
   CfOut(cf_verbose, "", "!! Unable to find a key for ip %s", ipv4);
   }
}

/***************************************************************/

char *MapAddress(char *unspec_address)

{ /* Is the address a mapped ipv4 over ipv6 address */

if (strncmp(unspec_address,"::ffff:",7) == 0)
   {
   return (char *)(unspec_address+7);
   }
else
   {
   return unspec_address;
   }
}

/***************************************************************************/

char *EscapeQuotes(char *s, char *out, int outSz)

{ char *spt,*spf;
  int i = 0;

memset(out,0,outSz);
 
 for (spf = s, spt = out; (i < outSz - 2) && (*spf != '\0'); spf++,spt++,i++)
   {
   switch (*spf)
      {
      case '\'':
      case '\"':
          *spt++ = '\\';
          *spt = *spf;
	  i+=3;
          break;

      default:
          *spt = *spf;
	  i++;
          break;
      }
   }

return out;
}

/***************************************************************************/

char *EscapeJson(char *s, char *out, int outSz)

{ char *spt,*spf;
  int i = 0;

memset(out,0,outSz);
 
 for (spf = s, spt = out; (i < outSz - 2) && (*spf != '\0'); spf++,spt++,i++)
   {
   switch (*spf)
      {
      case '\"':
      case '\\':
      case '/':
          *spt++ = '\\';
          *spt = *spf;
          i+=2;
          break;
      case '\n':
          *spt++ = '\\';
          *spt = 'n';
          i+=2;
          break;
      case '\t':
          *spt++ = '\\';
          *spt = 't';
          i+=2;
          break;          
      case '\r':
          *spt++ = '\\';
          *spt = 'r';
          i+=2;
          break;
      case '\b':
          *spt++ = '\\';
          *spt = 'b';
          i+=2;
          break;
      case '\f':
          *spt++ = '\\';
          *spt = 'f';
          i+=2;
          break;
      default:
          *spt = *spf;
	  i++;
          break;
      }
   }

return out;
}

/***************************************************************************/

char *EscapeRegex(char *s, char *out, int outSz)

{ char *spt,*spf;
  int i = 0;

memset(out,0,outSz);
 
 for (spf = s, spt = out; (i < outSz - 2) && (*spf != '\0'); spf++,spt++,i++)
   {
   switch (*spf)
      {
      case '\\': 
      case '.': 
      case '|':
      case '*':
      case '?':
      case '+':
      case '(':
      case ')':
      case '{':
      case '}':
      case '[':
      case ']':
      case '^':
      case '$': 
          *spt++ = '\\';
          *spt = *spf;
	  i+=2;
          break;

      default:
          *spt = *spf;
	  i++;
          break;
      }
   }
return out;
}

/***************************************************************************/

enum cfhypervisors Str2Hypervisors(char *s)

{ static char *names[] = { "xen", "kvm", "esx", "test",
                           "xen_net", "kvm_net", "esx_net", "test_net",
                           "zone", "ec2", "eucalyptus", NULL };
  int i;

if (s == NULL)
   {
   return cfv_virt_test;
   }
  
for (i = 0; names[i] != NULL; i++)
   {
   if (s && strcmp(s,names[i]) == 0)
      {
      return (enum cfhypervisors) i;
      }
   }

return (enum cfhypervisors) i;
}

/***************************************************************************/

enum cfenvironment_state Str2EnvState(char *s)

{ static char *names[] = { "create", "delete", "running", "suspended", "down", NULL };
  int i;
 
if (s == NULL)
   {
   return cfvs_create;
   }

for (i = 0; names[i] != NULL; i++)
   {
   if (s && strcmp(s,names[i]) == 0)
      {
      return (enum cfenvironment_state) i;
      }
   }

return (enum cfenvironment_state) i;
}

/***************************************************************************/

enum insert_match String2InsertMatch(char *s)

{ static char *names[] = { "ignore_leading","ignore_trailing","ignore_embedded",
                           "exact_match", NULL };
 int i;

for (i = 0; names[i] != NULL; i++)
   {
   if (s && strcmp(s,names[i]) == 0)
      {
      return i;
      }
   }

return cf_exact_match;
}
    
/***************************************************************************/

int SyslogPriority2Int(char *s)

{ int i;
  static char *types[] = { "emergency","alert","critical","error",
                           "warning","notice","info","debug", NULL };
    
for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return i;
      }
   }

return 3;
}

/***************************************************************************/

enum cfdbtype Str2dbType(char *s)

{ int i;
  static char *types[] = { "mysql","postgres", NULL };
    
for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum cfdbtype) i;
      }
   }

return cfd_notype;
}

/***************************************************************************/

enum package_actions Str2PackageAction(char *s)

{ int i;
  static char *types[] = { "add","delete","reinstall","update","addupdate","patch","verify", NULL };
    
for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum package_actions) i;      
      }
   }

return cfa_pa_none;
}

/***************************************************************************/

enum version_cmp Str2PackageSelect(char *s)

{ int i;
  static char *types[] = { "==","!=",">","<",">=","<=", NULL};
  
for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum version_cmp) i;      
      }
   }

return cfa_cmp_none;
}

/***************************************************************************/

enum action_policy Str2ActionPolicy(char *s)

{ int i;
 static char *types[] = { "individual","bulk",NULL};
  
for (i = 0; types[i] != NULL; i++)
   {
   if (s && strcmp(s,types[i]) == 0)
      {
      return (enum version_cmp) i;      
      }
   }

return cfa_no_ppolicy;
}

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
   if (strcmp(s,ip->name) == 0)
      {
      break;
      }
   i++;
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

return cfa_symlink;
}

/****************************************************************************/

enum cfcomparison String2Comparison(char *s)

{ int i;
  static char *types[] = {"atime","mtime","ctime","digest","hash","binary","exists",NULL};

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
 static char *types[] = {"url","web","file","db","literal","image","portal",NULL};

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
  char remainder[CF_BUFSIZE];
  char output[CF_BUFSIZE];

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

remainder[0] = '\0';

sscanf(s,"%ld%c%s",&a,&c,remainder);

// Test whether remainder is space only

if (a == CF_NOINT || !IsSpace(remainder))
   {
   if (THIS_AGENT_TYPE == cf_common)
      {
      CfOut(cf_inform,""," !! Error reading assumed integer value \"%s\" => \"%s\" (found remainder \"%s\")\n",s,"non-value",remainder);
      if (strchr(s,'$'))
         {
         CfOut(cf_inform,""," !! The variable might not yet be expandable - not necessarily an error");
         }
      }
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
      case '%':
          if (a < 0 || a > 100)
             {
             CfOut(cf_error,"","Percentage out of range (%d)",a);
             return CF_NOINT;
             }
          else
             {
             /* Represent percentages internally as negative numbers */
             a = -a;
             }
          break;

      case ' ':
          break;
          
      default:          
          break;
      }
   }

return a;
}

/****************************************************************************/

long TimeCounter2Int(const char *s)

{
long d = 0, h = 0, m = 0;
char output[CF_BUFSIZE];

if (s == NULL)
   {
   return CF_NOINT;
   }

if (strchr(s, '-'))
   {
   if (sscanf(s, "%ld-%ld:%ld", &d, &h, &m) != 3)
      {
      snprintf(output, CF_BUFSIZE, "Unable to parse TIME 'ps' field, expected dd-hh:mm, got '%s'", s);
      ReportError(output);
      }
   }
else
   {
   if (sscanf(s, "%ld:%ld", &h, &m) != 2)
      {
      snprintf(output, CF_BUFSIZE, "Unable to parse TIME 'ps' field, expected hH:mm, got '%s'", s);
      ReportError(output);
      }
   }

return 60 * (m + 60 * (h + 24 * d));
}

/****************************************************************************/

long TimeAbs2Int(char *s)

{ time_t cftime;
  int i;
  char mon[4],h[3],m[3];
  long month = 0,day = 0,hour = 0,min = 0, year = 0;
  static long days[] = {31,28,31,30,31,30,31,31,30,31,30,31};

if (s == NULL)
   {
   return CF_NOINT;
   }

year = Str2Int(VYEAR);

if (year % 4 == 0) /* leap years */
   {
   days[1] = 29;
   }

if (strstr(s,":")) /* Hr:Min */
   {
   sscanf(s,"%2[^:]:%2[^:]:",h,m);
   month = Month2Int(VMONTH);
   day = Str2Int(VDAY);
   hour = Str2Int(h);
   min = Str2Int(m);
   }
else               /* date Month */
   {
   sscanf(s,"%3[a-zA-Z] %ld",mon,&day);

   month = Month2Int(mon);
   
   if (Month2Int(VMONTH) < month)
      {
      /* Wrapped around */
      year--;
      }
   }

Debug("(%s)\n%ld=%s,%ld=%s,%ld,%ld,%ld\n",s,year,VYEAR,month,VMONTH,day,hour,min);

cftime = 0;
cftime += min * 60;
cftime += hour * 3600;
cftime += (day - 1) * 24 * 3600;
cftime += 24 * 3600 * ((year-1970)/4); /* Leap years */

for (i = 0; i < month - 1; i++)
   {
   cftime += days[i] * 24 * 3600;
   }

cftime += (year - 1970) * 365 * 24 * 3600;

Debug("Time %s CORRESPONDS %s\n",s,cf_ctime(&cftime));
return (long) cftime;
}

/****************************************************************************/

long Months2Seconds(int m)

{
  static long days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  long tot_days = 0;
  int this_month,i,month,year;

if (m == 0)
   {
   return 0;
   }
  
this_month = Month2Int(VMONTH);
year = Str2Int(VYEAR);

for (i = 0; i < m; i++)
   {
   month = (this_month - i) % 12;

   while (month < 0)
      {
      month += 12;
      year--;
      }

   if ((year % 4) && (month == 1))
      {
      tot_days += 29;
      }
   else
      {
      tot_days += days[month];
      }
   }

return (long) tot_days * 3600 * 24;
}

/*********************************************************************/

enum cfinterval Str2Interval(char *string)

{ static char *text[3] = { "hourly", "daily", NULL };
  int i;
 
for (i = 0; text[i] != NULL; i++)
   {
   if (string && (strcmp(text[i],string) == 0))
      {
      return i;
      }
   }

return cfa_nointerval;
}

/*********************************************************************/

int Day2Number(char *datestring)

{ int i = 0;

for (i = 0; i < 7; i++)
   {
   if (strncmp(datestring,DAY_TEXT[i],3) == 0)
      {
      return i;
      }
   }

return -1;
}

/****************************************************************************/

void UtcShiftInterval(time_t t, char *out, int outSz)
/* 00 - 06, 
   06 - 12, 
   12 - 18, 
   18 - 24*/
{
  char buf[CF_MAXVARSIZE];
  int hr = 0, fromHr = 0, toHr = 0;

  cf_strtimestamp_utc(t,buf);
  
  sscanf(buf+11,"%d", &hr);
  buf[11] = '\0';

  if(hr < 6)
    {
      fromHr = 0;
      toHr = 6;
    }
  else if(hr < 12)
    {
      fromHr = 6;
      toHr = 12;
    }
  else if(hr < 18)
    {
      fromHr = 12;
      toHr = 18;
    }
  else
    {
      fromHr = 18;
      toHr = 24;
    }

  snprintf(out, outSz, "%s %02d-%02d", buf, fromHr, toHr);
}

/****************************************************************************/

mode_t Str2Mode(char *s)

{ int a = CF_UNDEFINED;
  char output[CF_BUFSIZE];
  
if (s == NULL)
   {
   return 0;
   }

sscanf(s,"%o",&a);

if (a == CF_UNDEFINED)
   {
   snprintf(output,CF_BUFSIZE,"Error reading assumed octal value %s\n",s);
   ReportError(output);
   }

return (mode_t)a;
}

/****************************************************************************/

double Str2Double(char *s)

{ double a = CF_NODOUBLE;
  char remainder[CF_BUFSIZE];
  char output[CF_BUFSIZE];
  char c = 'X';
  
if (s == NULL)
   {
   return CF_NODOUBLE;
   }

remainder[0] = '\0';

sscanf(s,"%lf%c%s",&a,&c,remainder);

if (a == CF_NODOUBLE || !IsSpace(remainder))
   {
   snprintf(output,CF_BUFSIZE,"Error reading assumed real value %s (anomalous remainder %s)\n",s,remainder);
   ReportError(output);
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
      case '%':
          if (a < 0 || a > 100)
             {
             CfOut(cf_error,"","Percentage out of range (%d)",a);
             return CF_NOINT;
             }
          else
             {
             /* Represent percentages internally as negative numbers */
             a = -a;
             }
          break;

      case ' ':
          break;
          
      default:          
          break;
      }
   }

return a;
}

/****************************************************************************/

void IntRange2Int(char *intrange,long *min,long *max,struct Promise *pp)

{ struct Item *split;
  long lmax = CF_LOWINIT, lmin = CF_HIGHINIT;
  char output[CF_BUFSIZE];
  
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
   snprintf(output,CF_BUFSIZE,"Could not make sense of integer range [%s]",intrange);
   FatalError(output);
   }

*min = lmin;
*max = lmax;
}

/*********************************************************************/

enum cf_acl_method Str2AclMethod(char *string)

{ static char *text[3] = { "append", "overwrite", NULL };
  int i;

for (i = 0; i < 2; i++)
   {
   if (string && (strcmp(text[i],string) == 0))
      {
      return i;
      }
   }

return cfacl_nomethod;
}

/*********************************************************************/

enum cf_acl_type Str2AclType(char *string)

{ static char *text[4] = { "generic","posix", "ntfs", NULL };
  int i;
 
for (i = 0; i < 3; i++)
   {
   if (string && (strcmp(text[i],string) == 0))
      {
      return i;
      }
   }

return cfacl_notype;
}

/*********************************************************************/

enum cf_acl_inherit Str2AclInherit(char *string)

{ static char *text[5] = { "nochange", "specify", "parent", "clear", NULL };
  int i;
 
for (i = 0; i < 4; i++)
   {
   if (string && (strcmp(text[i],string) == 0))
      {
      return i;
      }
   }

return cfacl_noinherit;
}

/*********************************************************************/

enum cf_acl_inherit Str2ServicePolicy(char *string)

{ static char *text[4] = { "start", "stop", "disable", NULL };
  int i;
 
for (i = 0; i < 3; i++)
   {
   if (string && (strcmp(text[i],string) == 0))
      {
      return i;
      }
   }

return cfsrv_nostatus;
}

/*********************************************************************/

char *Dtype2Str(enum cfdatatype dtype)
{
  switch(dtype)
    {
    case cf_str:
      return "s";
    case cf_slist:
      return "sl";      
    case cf_int:
      return "i";
    case cf_ilist:
      return "il";
    case cf_real:
      return "r";
    case cf_rlist:
      return "rl";
    case cf_opts:
      return "m";
    case cf_olist:
      return "ml";
    default:
      return "D?";
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int Month2Int(char *string)

{
return MonthLen2Int(string,10);  // no month names longer than 10 chars
}

/*************************************************************/

int MonthLen2Int(char *string, int len)

{ int i;

if (string == NULL)
   {
   return -1;
   }
 
for (i = 0; i < 12; i++)
   {
   if (strncmp(MONTH_TEXT[i],string,strlen(string))==0)
      {
      return i+1;
      break;
      }
   }

return -1;
}

/*********************************************************************/

void TimeToDateStr(time_t t, char *outStr, int outStrSz)
/**
 * Formats a time as "30 Sep 2010".
 */
{
char month[CF_SMALLBUF],day[CF_SMALLBUF],year[CF_SMALLBUF];
char tmp[CF_SMALLBUF];

snprintf(tmp,sizeof(tmp),"%s",cf_ctime(&t));
sscanf(tmp,"%*s %5s %3s %*s %5s",month,day,year);
snprintf(outStr,outStrSz,"%s %s %s",day,month,year);
}

/*********************************************************************/

const char *GetArg0(const char *execstr)

{ const char *sp;
  static char arg[CF_BUFSIZE];
  int i = 0;

for (sp = execstr; *sp != ' ' && *sp != '\0'; sp++)
   {
   i++;

   if (*sp == '\"')
      {
      DeEscapeQuotedString(sp,arg);
      return arg;
      }
   }

memset(arg,0,CF_MAXVARSIZE);
strncpy(arg,execstr,i);
arg[i] = '\0';
return arg;
}

/*************************************************************/

void CommPrefix(char *execstr,char *comm)

{ char *sp;

for (sp = execstr; *sp != ' ' && *sp != '\0'; sp++)
   {
   }

if (sp - 10 >= execstr)
   {
   sp -= 10;   /* copy 15 most relevant characters of command */
   }
else
   {
   sp = execstr;
   }

memset(comm,0,20);
strncpy(comm,sp,15);
}

/*************************************************************/

int NonEmptyLine(char *line)

{ char *sp;
            
for (sp = line; *sp != '\0'; sp++)
   {
   if (!isspace((int)*sp))
      {
      return true;
      }
   }

return false;
}

/*************************************************************/

char *Item2String(struct Item *ip)
{
  struct Item *currItem;
  int stringSz = 0;
  char *buf;
  
  // compute required buffer size
  for(currItem = ip; currItem != NULL; currItem = currItem->next)
    {
      stringSz += strlen(currItem->name);
      stringSz++; // newline space
    }
  
  // we automatically get \0-termination because we are not appending a \n after the last item

  buf = calloc(1, stringSz);

  if(buf == NULL)
    {
      FatalError("Memory allocation in ItemToString()");
    }
  
  // do the copy
  for(currItem = ip; currItem != NULL; currItem = currItem->next)
    {
      strcat(buf, currItem->name);
	  
	  if(currItem->next != NULL)  // no newline after last item
	  {
        strcat(buf, "\n");
	  }
    }
  
  return buf;
}

/*******************************************************************/

static int IsSpace(char *remainder)

{ char *sp;

for (sp = remainder; *sp != '\0'; sp++)
    {
    if (!isspace(*sp))
       {
       return false;
       }    
    }

return true;
}

/*******************************************************************/

int IsNumber(char *s)

{ char *sp;

for (sp = s; *sp != '\0'; sp++)
    {
    if (!isdigit(*sp))
       {
       return false;
       }    
    }

return true;
}

/*******************************************************************/

int IsRealNumber(char *s)

{ double a = CF_NODOUBLE;

sscanf(s,"%lf",&a);

if (a == CF_NODOUBLE)
   {
   return false;
   }

return true;
}

/********************************************************************/

enum cfd_menu String2Menu(char *s)

{ static char *menus[] = { "delta", "full", "relay", NULL };
  int i;
 
for (i = 0; menus[i] != NULL; i++)
   {
   if (strcmp(s,menus[i]) == 0)
      {
      return i;
      }
   }

return cfd_menu_error;
}

/*******************************************************************/
/* Unix-only functions                                             */
/*******************************************************************/

#ifndef MINGW

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

uid_t Str2Uid(char *uidbuff,char *usercopy,struct Promise *pp)

{ struct Item *ip, *tmplist;
  struct passwd *pw;
  int offset,uid = -2,tmp = -2;
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
         CfOut(cf_inform,""," !! Unknown user in promise \'%s\'\n",ip->name);

         if (pp != NULL)
            {
            PromiseRef(cf_inform,pp);
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
      CfOut(cf_inform,""," !! Unknown user %s in promise\n",uidbuff);
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
  int gid = -2, tmp = -2;

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
      CfOut(cf_inform,""," !! Unknown group \'%s\' in promise\n",gidbuff);

      if (pp)
         {         
         PromiseRef(cf_inform,pp);
         }

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
#endif  /* NOT MINGW */
