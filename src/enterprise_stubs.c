
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
/* File: enterprise_stubs.c                                                  */
/*                                                                           */
/*****************************************************************************/

/*

This file is a stub for generating cfengine's commerical enterprise level
versions. We appreciate your respect of our commercial offerings, which go
to accelerate future developments of both free and commercial versions. If
you have a good reason why a particular feature of the commercial version
should be free, please let us know and we will consider this carefully.

*/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef HAVE_CFLIBNOVA
# include <cf.nova.h>
#endif

#ifdef HAVE_ZONE_H
# include <zone.h>
#endif

static struct Averages SHIFT_VALUE;
static char CURRENT_SHIFT[CF_MAXVARSIZE];

/*****************************************************************************/

void EnterpriseModuleTrick()

{
#if defined HAVE_NOVA
Nova_EnterpriseModuleTrick();
#endif

#if defined HAVE_CONSTELLATION
Constellation_EnterpriseModuleTrick();
#endif
}

/*****************************************************************************/

#ifdef HAVE_NOVA

void CheckLicenses()

{
Nova_CheckLicensePromise();
}

#else

void CheckLicenses()

{
struct stat sb;
char name[CF_BUFSIZE];

snprintf(name,sizeof(name),"%s/state/am_policy_hub",CFWORKDIR);
MapName(name);

if (stat(name,&sb) != -1)
   {
   NewClass("am_policy_hub");
   CfOut(cf_verbose,""," -> Additional class defined: am_policy_hub");
   }

return;
}

#endif

/*****************************************************************************/

char *GetConsolePrefix()
    
{
#ifdef HAVE_CONSTELLATION
return "constellation";
#elif defined HAVE_NOVA
return "nova";
#else
return "cf3";
#endif
}

/*****************************************************************************/

char *MailSubject()

{ static char buffer[CF_BUFSIZE];
#ifdef HAVE_NOVA
if (LICENSES)
   {
   strcpy(buffer,"Nova");
   }
else
   {
   strcpy(buffer,"NO LICENSE");
   }
#else
strcpy(buffer,"community");
#endif
return buffer;
}

/*****************************************************************************/

void SyntaxExport()
{
#ifdef HAVE_NOVA
Nova_SyntaxTree2JavaScript();
#else
SyntaxPrintAsJson(stdout);
#endif
}

/*****************************************************************************/
/* Monitord                                                                  */
/*****************************************************************************/

void HistoryUpdate(struct Averages newvals)

{
#ifdef HAVE_NOVA  
  struct Promise *pp = NewPromise("history_db","the long term memory");
  struct Attributes dummyattr = {{0}};
  struct CfLock thislock;
  time_t now = time(NULL);

/* We do this only once per hour - this should not be changed */

Banner("Update long-term history");

if (strlen(CURRENT_SHIFT) == 0)
   {
   // initialize
   Nova_ResetShiftAverage(&SHIFT_VALUE);
   }

memset(&dummyattr,0,sizeof(dummyattr));
dummyattr.transaction.ifelapsed = 59;

thislock = AcquireLock(pp->promiser,VUQNAME,now,dummyattr,pp,false);

if (thislock.lock == NULL)
   {
   Nova_UpdateShiftAverage(&SHIFT_VALUE,&newvals);
   DeletePromise(pp);
   return;
   }

/* Refresh the class context of the agent */
DeletePrivateClassContext();
DeleteEntireHeap();
DeleteAllScope();

NewScope("sys");
NewScope("const");
NewScope("match");
NewScope("mon");
NewScope("control_monitor");
NewScope("control_common");
GetNameInfo3();
CfGetInterfaceInfo(cf_monitor);
Get3Environment();
BuiltinClasses();
OSClasses();
SetReferenceTime(true);

LoadPersistentContext();
LoadSystemConstants();

YieldCurrentLock(thislock);
DeletePromise(pp);

Nova_HistoryUpdate(CFSTARTTIME, &newvals);

if (strcmp(CURRENT_SHIFT,VSHIFT) != 0)
   {
   strcpy(CURRENT_SHIFT,VSHIFT);
   Nova_ResetShiftAverage(&SHIFT_VALUE);
   }

Nova_DumpSlowlyVaryingObservations();

#else
#endif
}

/*****************************************************************************/

int VerifyDatabasePromise(CfdbConn *cfdb,char *database,struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_NOVA
  char query[CF_BUFSIZE],name[CF_MAXVARSIZE];
  int found = false;
 
CfOut(cf_verbose,""," -> Verifying promised database");

if (!cfdb->connected)
   {
   CfOut(cf_inform,"","Database %s is not connected\n",database);
   return false;
   }

Nova_CreateDBQuery(cfdb->type,query);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns < 1)
   {
   CfOut(cf_error,""," !! The schema did not promise the expected number of fields - got %d expected >= %d\n",cfdb->maxcolumns,1);
   return false;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);

   CfOut(cf_verbose,"","      ? ... discovered a database called \"%s\"",name);
   
   if (strcmp(name,database) == 0)
      {
      found = true;
      }
   }

if (found)
   {
   CfOut(cf_verbose,""," -> Database \"%s\" exists on this connection",database);
   return true;
   }
else
   {
   CfOut(cf_verbose,""," !! Database \"%s\" does not seem to exist on this connection",database);
   }

if (a.database.operation && strcmp(a.database.operation,"drop") == 0)
   {
   if (a.transaction.action != cfa_warn && !DONTDO)
      {
      CfOut(cf_verbose,""," -> Attempting to delete the database %s",database);
      snprintf(query,CF_MAXVARSIZE-1,"drop database %s",database); 
      CfVoidQueryDB(cfdb,query);
      return cfdb->result;
      }
   else
      {
      CfOut(cf_error,""," !! Need to delete the database %s but only a warning was promised\n",database);
      return false;
      }   
   }

if (a.database.operation && strcmp(a.database.operation,"create") == 0)
   {
   if (a.transaction.action != cfa_warn && !DONTDO)
      {
      CfOut(cf_verbose,""," -> Attempting to create the database %s",database);
      snprintf(query,CF_MAXVARSIZE-1,"create database %s",database); 
      CfVoidQueryDB(cfdb,query);
      return cfdb->result;
      }
   else
      {
      CfOut(cf_error,""," !! Need to create the database %s but only a warning was promised\n",database);
      return false;
      }
   }

return false;
#else
CfOut(cf_verbose,"","Verifying SQL database promises is only available with Cfengine Nova or above");
return false;
#endif
}

/*****************************************************************************/

void NoteEfficiency(double e)

{
#ifdef HAVE_NOVA
 struct Attributes a = {{0}};
 struct Promise p = {0};
 
NovaNamedEvent("Configuration model efficiency",e,a,&p);
CfOut(cf_verbose,""," -> Configuration model efficiency for %s = %.2lf%%",VUQNAME,e);
#endif 
}

/*****************************************************************************/

char *GetProcessOptions()
{
#ifdef HAVE_GETZONEID
 zoneid_t zid;
 char zone[ZONENAME_MAX];
 static char psopts[CF_BUFSIZE];
 
zid = getzoneid();
getzonenamebyid(zid,zone,ZONENAME_MAX);

if (cf_strcmp(zone,"global") == 0)
   {
   snprintf(psopts,CF_BUFSIZE,"%s,zone",VPSOPTS[VSYSTEMHARDCLASS]);
   return psopts;
   }
#endif

#ifdef LINUX
if (strncmp(VSYSNAME.release,"2.4",3) == 0)
   {
   // No threads on 2.4 kernels
   return "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,stime,time,args";
   }

#endif

return VPSOPTS[VSYSTEMHARDCLASS];
}

/*****************************************************************************/

int ForeignZone(char *s)
{
// We want to keep the banner

if (strstr(s,"%CPU"))
   {
   return false;
   }

#ifdef HAVE_GETZONEID
 zoneid_t zid;
 char *sp,zone[ZONENAME_MAX];
 static psopts[CF_BUFSIZE];
 
zid = getzoneid();
getzonenamebyid(zid,zone,ZONENAME_MAX);

if (cf_strcmp(zone,"global") == 0)
   {
   if (cf_strcmp(s+strlen(s)-6,"global") == 0)
      {
      *(s+strlen(s)-6) = '\0';

      for (sp = s+strlen(s)-1; isspace(*sp); sp--)
         {
         *sp = '\0';
         }

      return false;
      }
   else
      {
      return true;
      }
   }
#endif
return false;
}

/*****************************************************************************/

#if !defined(HAVE_NOVA)

int IsEnterprise(void)
{
return false;
}

void EnterpriseContext(void)
{
}

int CfSessionKeySize(char type)
{
return CF_BLOWFISHSIZE;
}

char CfEnterpriseOptions(void)
{
return 'c';
}

const EVP_CIPHER *CfengineCipher(char type)
{
return EVP_bf_cbc();
}

int EnterpriseExpiry(void)
{
return false;
}

void LogFileChange(char *file, int change, struct Attributes a, struct Promise *pp)
{
CfOut(cf_verbose, "", "Logging file differences requires version Nova or above");
}

void RemoteSysLog(int log_priority, const char *log_string)
{
CfOut(cf_verbose,"","Remote logging requires version Nova or above");
}

void WebCache(char *s,char *t)
{
}

const char *PromiseID(struct Promise *pp)
{
return "";
}

void NotePromiseCompliance(struct Promise *pp,double val,enum cf_status status,char *reason)
{
}

void PreSanitizePromise(struct Promise *pp)
{
}

void TrackValue(char *date,double kept,double repaired, double notkept)
{
}

time_t GetPromiseCompliance(struct Promise *pp,double *value,double *average,double *var,time_t *lastseen)
{
return time(NULL);
}

void ShowTopicRepresentation(FILE *fp)
{
CfOut(cf_verbose,"","# Knowledge map reporting feature is only available in version Nova and above\n");
}

void NewPromiser(struct Promise *pp)
{
}

void AnalyzePromiseConflicts(void)
{
}

void RegisterBundleDependence(char *name,struct Promise *pp)
{
}

void SyntaxCompletion(char *s)
{
printf("Syntax completion is available in cfengine Nova,Constellation or Galaxy\n\n");
}

void VerifyOutputsPromise(struct Promise *pp)
{
printf(" !! Outputs promises are not available in the community edition of Cfengine\n");
}

void SetPromiseOutputs(struct Promise *pp)
{
}

void LastSawBundle(char *name)
{
}

void SetBundleOutputs(char *name)
{
}

void ResetBundleOutputs(char *name)
{
}

void SpecialQuote(char *topic,char *type)
{
}

void GetClassName(int i,char *name, char *desc)
{
strcpy(name,OBS[i][0]);
}

void LookupClassName(int i, char *name, char *desc)
{
strcpy(name, OBS[i][0]);
}

void LoadSlowlyVaryingObservations()
{
CfOut(cf_verbose,"","# Extended system discovery is only available in version Nova and above\n");
}

void RegisterLiteralServerData(char *handle,struct Promise *pp)
{
CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
}

int ReturnLiteralData(char *handle,char *ret)
{
CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
return 0;
}

char *GetRemoteScalar(char *proto,char *handle,char *server,int encrypted,char *rcv)
{
CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
return "";
}

void CacheUnreliableValue(char *caller,char *handle,char *buffer)
{
CfOut(cf_verbose,"","# Value fault-tolerance in version Nova and above\n");
}

int RetrieveUnreliableValue(char *caller,char *handle,char *buffer)
{
CfOut(cf_verbose,"","# Value fault-tolerance in version Nova and above\n");
return false;
}

void TranslatePath(char *new, const char *old)
{
strncpy(new,old,CF_BUFSIZE-1);
}

void SummarizeCompliance(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
}

void SummarizeValue(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Value reporting feature is only available in version Nova and above - use the state/cf_value.log\n");
}

void SummarizePromiseRepaired(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
}

void SummarizePromiseNotKept(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
}

void GrandSummary()
{
CfOut(cf_verbose,"","# Reporting feature is only available in version Nova and above\n");
}

void CSV2XML(struct Rlist *list)
{
CfOut(cf_verbose,"","# Format conversion feature is only available in version Nova and above\n");
}

void SummarizeVariables(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Variable reporting feature is only available in version Nova and above\n");
}

void SummarizePerPromiseCompliance(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
}

void SummarizeFileChanges(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# File change reporting feature is only available in version Nova and above\n");
}

void SummarizeSetuid(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Setuid reporting feature is only available in version Nova and above\n");
}

void ReportPatches(struct CfPackageManager *list)
{
CfOut(cf_verbose,"","# Patch reporting feature is only available in version Nova and above\n");
}

void SummarizeSoftware(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Software summary reporting feature is only available in version Nova and above\n");
}

void SummarizeUpdates(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)
{
CfOut(cf_verbose,"","# Software summary reporting feature is only available in version Nova and above\n");
}
void VerifyMeasurement(double *this,struct Attributes a,struct Promise *pp)
{
CfOut(cf_verbose,"","# Custom monitoring feature is only available in version Nova and above\n");
}

void LongHaul(time_t current)
{
}

void SetMeasurementPromises(struct Item **classlist)
{
}

void VerifyACL(char *file,struct Attributes a, struct Promise *pp)
{
CfOut(cf_verbose, "", "Verifying ACL promises is only available with Cfengine Nova or above");
}

int CheckACLSyntax(char *file,struct CfACL acl,struct Promise *pp)
{
return true;
}

void VerifyRegistryPromise(struct Attributes a,struct Promise *pp)
{
}

int GetRegistryValue(char *key,char *name,char *buf, int bufSz)
{
return 0;
}

void *CfLDAPValue(char *uri,char *dn,char *filter,char *name,char *scope,char *sec)
{
CfOut(cf_error, "", "LDAP support is available in Nova and above");
return NULL;
}

void *CfLDAPList(char *uri,char *dn,char *filter,char *name,char *scope,char *sec)
{
CfOut(cf_error,"","LDAP support available in Nova and above");
return NULL;
}

void *CfLDAPArray(char *array,char *uri,char *dn,char *filter,char *scope,char *sec)
{
CfOut(cf_error,"","LDAP support available in Nova and above");
return NULL;
}

void *CfRegLDAP(char *uri,char *dn,char *filter,char *name,char *scope,char *regex,char *sec)
{
CfOut(cf_error,"","LDAP support available in Nova and above");
return NULL;
}

int CheckDatabaseSanity(struct Attributes a, struct Promise *pp)
{
return false;
}

int VerifyTablePromise(CfdbConn *cfdb,char *name,struct Rlist *columns,struct Attributes a,struct Promise *pp)
{
CfOut(cf_verbose,"","Verifying SQL table promises is only available with Cfengine Nova or above");
return false;
}

int GetInstalledPkgsRpath(struct CfPackageItem **pkgList, struct Attributes a, struct Promise *pp)
{
CfOut(cf_error, "", "!! rPath internal package listing only available in Nova or above");
return false;
}

int ExecPackageCommandRpath(char *command,int verify,int setCmdClasses,struct Attributes a,struct Promise *pp)
{
CfOut(cf_error, "", "!! rPath internal package commands only available in Nova or above");
return false;
}

void AddGoalsToDB(char *goal_patterns, char *goal_categories)
{
}

void SetSyslogHost(const char *host)
{
CfOut(cf_error, "", "!! Remote syslog functionality is only available in Nova");
}

void SetSyslogPort(uint16_t port)
{
CfOut(cf_error, "", "!! Remote syslog functionality is only available in Nova");
}

#endif
