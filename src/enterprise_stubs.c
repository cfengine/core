
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

void *Nova_LDAPValue(char *uri,char *basedn,char *filter,char *name,char *scope,char *sec);
void *Nova_LDAPList(char *uri,char *dn,char *filter,char *name,char *scope,char *sec);
void *Nova_LDAPArray(char *array,char *uri,char *dn,char *filter,char *scope,char *sec);
void *Nova_RegLDAP(char *uri,char *dn,char *filter,char *name,char *scope,char *regex,char *sec);

struct Averages SHIFT_VALUE;
char CURRENT_SHIFT[CF_MAXVARSIZE];

/*****************************************************************************/

int IsEnterprise()

{
#if defined HAVE_LIBCFNOVA || defined HAVE_LIBCFCONSTELLATION || defined HAVE_LIBCFGALAXY
return true;
#else
return false;
#endif
}

/*****************************************************************************/

void EnterpriseModuleTrick()

{
#if defined HAVE_LIBMONGOC && defined HAVE_LIBCFNOVA
Nova_EnterpriseModuleTrick();
#endif

}

/*****************************************************************************/

void EnterpriseContext()

{
#ifdef HAVE_LIBCFNOVA
 Nova_EnterpriseContext();
#endif
}

/*****************************************************************************/

void EnterpriseVersion()

{
#ifdef HAVE_LIBCFNOVA
Nova_Version();
#endif 
}

/*****************************************************************************/

int CfSessionKeySize(char c)
{
#ifdef HAVE_LIBCFNOVA
return Nova_CfSessionKeySize(c);
#else
return CF_BLOWFISHSIZE; 
#endif 
}

/*****************************************************************************/

char CfEnterpriseOptions()
{
#ifdef HAVE_LIBCFNOVA
return Nova_CfEnterpriseOptions();
#else
return 'c';
#endif 
}

/*****************************************************************************/

const EVP_CIPHER *CfengineCipher(char type)
{
#ifdef HAVE_LIBCFNOVA
return Nova_CfengineCipher(type);
#else
return EVP_bf_cbc();
#endif 
}

/*****************************************************************************/

int EnterpriseExpiry(char *day,char *month,char *year,char *company)

{
#ifdef HAVE_LIBCFNOVA
 return Nova_EnterpriseExpiry(day,month,year,company);
#else
return false;
#endif
}

/*****************************************************************************/

void CheckLicenses()

{
#ifdef HAVE_LIBCFNOVA
return Nova_CheckLicensePromise();
#else
return;
#endif
}

/*****************************************************************************/

void CheckAutoBootstrap()

{
#ifdef HAVE_LIBCFNOVA
 Nova_CheckAutoBootstrap();
#else
#endif
}

/*****************************************************************************/

char *GetConsolePrefix()
    
{
#ifdef HAVE_LIBCFCONSTELLATION
return "constellation>"; 
#elif defined HAVE_LIBCFNOVA
return "nova>";
#else
return "community>";
#endif
}

/*****************************************************************************/

char *MailSubject()

{ static char buffer[CF_BUFSIZE];
#ifdef HAVE_LIBCFNOVA
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

void SetPolicyServer(char *name)

{
#ifdef HAVE_LIBCFNOVA
Nova_SetPolicyServer(name);
#else
CfOut(cf_verbose,"","Setting policy server requires version Nova or above");
#endif 
}

/*****************************************************************************/

void LogFileChange(char *file,int change,struct Attributes a,struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
Nova_LogFileChange(file,change,a,pp);
#else
CfOut(cf_verbose,"","Logging file differences requires version Nova or above");
#endif 
}

/*****************************************************************************/

void RemoteSyslog(struct Attributes a,struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
Nova_RemoteSyslog(a,pp);
#else
CfOut(cf_verbose,"","Remote logging requires version Nova or above");
#endif 
}

/*****************************************************************************/

void WebCache(char *s,char *t)
{
#if defined HAVE_LIBCFNOVA && defined HAVE_LIBMONGOC
CFDB_PutValue(s,t);
#endif 
}

/*****************************************************************************/
/* Knowledge                                                                 */
/*****************************************************************************/

char *PromiseID(struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
return Nova_PromiseID(pp);
#else
return "";
#endif
}

/*****************************************************************************/

void NotePromiseCompliance(struct Promise *pp,double val,enum cf_status status,char *reason)

{
#ifdef HAVE_LIBCFNOVA
 Nova_NotePromiseCompliance(pp,val,status,reason);
#else
#endif
}

/*****************************************************************************/

void PreSanitizePromise(struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
Nova_PreSanitizePromise(pp);
#else
#endif
}

/*****************************************************************************/

void TrackValue(char *date,double kept,double repaired, double notkept)

{
#ifdef HAVE_LIBCFNOVA
Nova_TrackValue(date,kept,repaired,notkept);
#else
#endif 
}

/*****************************************************************************/

time_t GetPromiseCompliance(struct Promise *pp,double *value,double *average,double *var,time_t *lastseen)
    
{
#ifdef HAVE_LIBCFNOVA
return Nova_GetPromiseCompliance(pp,value,average,var,lastseen);
#else
return time(NULL);
#endif
}

/*****************************************************************************/

void MapPromiseToTopic(FILE *fp,struct Promise *pp,char *version)

{
#ifdef HAVE_LIBCFNOVA
 Nova_MapPromiseToTopic(fp,pp,version); 
#else
#endif
}

/*****************************************************************************/

void ShowTopicRepresentation(FILE *fp)

{
#ifdef HAVE_LIBCFNOVA
Nova_ShowTopicRepresentation(fp);
#else
 CfOut(cf_verbose,"","# Knowledge map reporting feature is only available in version Nova and above\n");
#endif

}

/*****************************************************************************/

void NewPromiser(struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
Nova_NewPromiser(pp);
#else
#endif 
}

/*****************************************************************************/

void AnalyzePromiseConflicts()
{
#ifdef HAVE_LIBCFNOVA
Nova_AnalyzePromiseConflicts();
#else
#endif
}


/*****************************************************************************/

void RegisterBundleDependence(char *name,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
 Nova_RegisterBundleDepedence(name,pp);
#else
#endif
}

/*****************************************************************************/

void SyntaxCompletion(char *s)
{
#ifdef HAVE_LIBCFNOVA
 Nova_SyntaxCompletion(s);
#else
 printf("Syntax completion is available in cfengine Nova,Constellation or Galaxy\n\n");
#endif
}

/*****************************************************************************/

void SyntaxExport()
{
#ifdef HAVE_LIBCFNOVA
Nova_SyntaxTree2JavaScript();
#else
printf("Syntax export is available in cfengine Nova,Constellation or Galaxy\n\n");
#endif
}

/*****************************************************************************/

void VerifyOutputsPromise(struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
 Nova_VerifyOutputsPromise(pp);
#else
 printf(" !! Outputs promises are not available in the community edition of Cfengine\n");
#endif
}

/*****************************************************************************/

void SetPromiseOutputs(struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
Nova_SetPromiseOutputs(pp);
#endif
}

/*****************************************************************************/

void LastSawBundle(char *name)
{
#ifdef HAVE_LIBCFNOVA
Nova_LastSawBundle(name);
#endif
}

/*****************************************************************************/

void SetBundleOutputs(char *name)
{
#ifdef HAVE_LIBCFNOVA
Nova_SetBundleOutputs(name);
#endif
}

/*****************************************************************************/

void ResetBundleOutputs(char *name)
{
#ifdef HAVE_LIBCFNOVA
Nova_ResetBundleOutputs(name);
#endif
}

/*****************************************************************************/

void SpecialQuote(char *topic,char *type)
{
#ifdef HAVE_LIBCFNOVA
Nova_SpecialQuote(topic,type);
#endif
}

/*****************************************************************************/
/* Monitord                                                                  */
/*****************************************************************************/

void HistoryUpdate(struct Averages newvals)

{
#ifdef HAVE_LIBCFNOVA  
  struct Promise *pp = NewPromise("history_db","the long term memory");
  struct Attributes dummyattr = {0};
  struct CfLock thislock;
  time_t now = time(NULL);
  char timekey[CF_MAXVARSIZE];

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

if (!NOHARDCLASSES)
   {
   NewScope("sys");
   NewScope("const");
   NewScope("match");
   NewScope("mon");
   NewScope("control_monitor");
   NewScope("control_common");
   GetNameInfo3();
   CfGetInterfaceInfo(cf_monitor);
   Get3Environment();
   OSClasses();
   SetReferenceTime(true);
   }

LoadPersistentContext();
LoadSystemConstants();

YieldCurrentLock(thislock);
DeletePromise(pp);

snprintf(timekey,CF_MAXVARSIZE-1,"%s_%s_%s_%s",VDAY,VMONTH,VLIFECYCLE,VSHIFT);
Nova_HistoryUpdate(timekey,newvals);

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

void CfGetClassName(int i,char *name)
{
#ifdef HAVE_LIBCFNOVA
 char desc[CF_BUFSIZE];
 Nova_GetClassName(i,name,desc);
#else
strcpy(name,OBS[i][0]);
#endif
}

/*****************************************************************************/

void LookUpClassName(int i,char *name)
{
#ifdef HAVE_LIBCFNOVA
 char desc[CF_BUFSIZE];

Nova_LookupClassName(i,name,desc);
#else
strcpy(name,OBS[i][0]);
#endif
}

/*****************************************************************************/

void LoadSlowlyVaryingObservations()

{
#ifdef HAVE_LIBCFNOVA
Nova_LoadSlowlyVaryingObservations();
#else
CfOut(cf_verbose,"","# Extended system discovery is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/
/* Server                                                                    */
/*****************************************************************************/

void RegisterLiteralServerData(char *handle,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
Nova_RegisterLiteralServerData(handle,pp);
#else
CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

int ReturnLiteralData(char *handle,char *ret)

{
#ifdef HAVE_LIBCFNOVA
return Nova_ReturnLiteralData(handle,ret);
#else
CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
return 0;
#endif 
}

/*****************************************************************************/

char *GetRemoteScalar(char *proto,char *handle,char *server,int encrypted,char *rcv)

{
#ifdef HAVE_LIBCFNOVA
return Nova_GetRemoteScalar(proto,handle,server,encrypted,rcv);
#else
CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
return "";
#endif 
}

/*****************************************************************************/

void CacheUnreliableValue(char *caller,char *handle,char *buffer)

{
#ifdef HAVE_LIBCFNOVA
Nova_CacheUnreliableValue(caller,handle,buffer);
#else
CfOut(cf_verbose,"","# Value fault-tolerance in version Nova and above\n");
return;
#endif 
}

/*****************************************************************************/

int RetrieveUnreliableValue(char *caller,char *handle,char *buffer)

{
#ifdef HAVE_LIBCFNOVA
return Nova_RetrieveUnreliableValue(caller,handle,buffer);
#else
CfOut(cf_verbose,"","# Value fault-tolerance in version Nova and above\n");
return false;
#endif 
}

/*****************************************************************************/

void TranslatePath(char *new,char *old)
{
#ifdef HAVE_LIBCFNOVA
Nova_TranslatePath(new,old);
#else
strncpy(new,old,CF_BUFSIZE-1);
#endif 
}

/*****************************************************************************/

RSA *SelectKeyRing(char *name)
{
#ifdef HAVE_LIBCFNOVA
if (KEYTTL > 0)
   {
   return Nova_SelectKeyRing(name);
   }
else
   {
   return NULL;
   }
#else
return NULL;
#endif 
}

/*****************************************************************************/

void IdempAddToKeyRing(char *name,char *ip,RSA *key)
{
#ifdef HAVE_LIBCFNOVA
Nova_IdempAddToKeyRing(name,ip,key);
#else
return;
#endif 
}

/*****************************************************************************/

void PurgeKeyRing()
{
#ifdef HAVE_LIBCFNOVA
Nova_PurgeKeyRing();
#else
return;
#endif 
}

/*****************************************************************************/
/* Reporting                                                                 */
/*****************************************************************************/

void SummarizeCompliance(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeCompliance(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizeValue(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
Nova_SummarizeValue(xml,html,csv,embed,stylesheet,head,foot,web);
#else
CfOut(cf_verbose,"","# Value reporting feature is only available in version Nova and above - use the state/cf_value.log\n");
#endif
}

/*****************************************************************************/

void SummarizePromiseRepaired(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
Nova_SummarizePromiseRepaired(xml,html,csv,embed,stylesheet,head,foot,web);
#else
CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizePromiseNotKept(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
Nova_SummarizePromiseNotKept(xml,html,csv,embed,stylesheet,head,foot,web);
#else
CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void GrandSummary()

{
#ifdef HAVE_LIBCFNOVA
 Nova_GrandSummary();
#else
 CfOut(cf_verbose,"","# Reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void CSV2XML(struct Rlist *list)

{
#ifdef HAVE_LIBCFNOVA
 Nova_CSV2XML(list);
#else
 CfOut(cf_verbose,"","# Format conversion feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void NoteVarUsage()

{
#ifdef HAVE_LIBCFNOVA
Nova_NoteVarUsage();
#endif 
}

/*****************************************************************************/

void SummarizeVariables(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeVariables(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Variable reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizePerPromiseCompliance(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizePerPromiseCompliance(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizeFileChanges(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeFileChanges(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# File change reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizeSetuid(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeSetuid(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Setuid reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void ReportSoftware(struct CfPackageManager *list)

{ FILE *fout;
  struct CfPackageManager *mp = NULL;
  struct CfPackageItem *pi;
  char name[CF_BUFSIZE],line[CF_BUFSIZE];
  struct Item *ip,*file = NULL;
  char start[32];
  int i = 0;

snprintf(name,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,NOVA_SOFTWARE_INSTALLED);
MapName(name);

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Cannot open the destination file %s",name);
   return;
   }

for (mp = list; mp != NULL; mp = mp->next)
   {
   for (pi = mp->pack_list; pi != NULL; pi=pi->next)
      {
      fprintf(fout,"%s,%s,%s,%s\n",pi->name,pi->version,pi->arch,ReadLastNode(GetArg0(mp->manager)));
      }
   }

fclose(fout);
}

/*****************************************************************************/

void ReportPatches(struct CfPackageManager *list)
{
#ifdef HAVE_LIBCFNOVA
 Nova_ReportPatches(list);
#else
 CfOut(cf_verbose,"","# Patch reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizeSoftware(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeSoftware(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Software summary reporting feature is only available in version Nova and above\n");
#endif
}
    
/*****************************************************************************/

void SummarizeUpdates(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeUpdates(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Software summary reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void VerifyServices(struct Attributes a,struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
 Nova_VerifyServices(a,pp);
#else
 CfOut(cf_verbose,"","# Services promises are only available in Cfengine Nova and above");
#endif
}

/*****************************************************************************/
/* Montoring                                                                 */
/*****************************************************************************/

void InitMeasurements()
{
#ifdef HAVE_LIBCFNOVA
 NovaInitMeasurements();
#endif
}

/*****************************************************************************/

void VerifyMeasurement(double *this,struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
 Nova_VerifyMeasurement(this,a,pp);
#else
 CfOut(cf_verbose,"","# Custom monitoring feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void LongHaul()

{
#ifdef HAVE_LIBCFNOVA
 Nova_LongHaul(VDAY,VMONTH,VLIFECYCLE,VSHIFT);
#else
#endif
}

/*****************************************************************************/

void SetMeasurementPromises(struct Item **classlist)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SetMeasurementPromises(classlist);
#else
#endif
}

/*****************************************************************************/
/* ACLs                                                                      */
/*****************************************************************************/

void VerifyACL(char *file,struct Attributes a, struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
Nova_VerifyACL(file,a,pp);
#else
CfOut(cf_verbose, "", "Verifying ACL promises is only available with Cfengine Nova or above");
#endif
}

/*****************************************************************************/

int CheckACLSyntax(char *file,struct CfACL acl,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
return Nova_CheckACLSyntax(file,acl,pp);
#else
return true;
#endif
}

/*****************************************************************************/
/* Registry                                                                  */
/*****************************************************************************/

void VerifyRegistryPromise(struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
# ifdef NT
Nova_VerifyRegistryPromise(a,pp);
# endif
#else
#endif
}

/*****************************************************************************/

int GetRegistryValue(char *key,char *value,char *buffer)

{
#ifdef HAVE_LIBCFNOVA
# ifdef NT
return Nova_CopyRegistryValue(key,value,buffer);
# endif
return 0;
#else
return 0;
#endif
}

/*****************************************************************************/
/* LDAP                                                                      */
/*****************************************************************************/

void *CfLDAPValue(char *uri,char *dn,char *filter,char *name,char *scope,char *sec)
{
#if defined HAVE_LIBCFNOVA && defined HAVE_LIBLDAP
 return Nova_LDAPValue(uri,dn,filter,name,scope,sec);
#else
 CfOut(cf_error,"","LDAP support available in Nova and above");
return NULL;
#endif
}

/*****************************************************************************/

void *CfLDAPList(char *uri,char *dn,char *filter,char *name,char *scope,char *sec)
{
#if defined HAVE_LIBCFNOVA && defined HAVE_LIBLDAP
 return Nova_LDAPList(uri,dn,filter,name,scope,sec);
#else
 CfOut(cf_error,"","LDAP support available in Nova and above");
return NULL;
#endif
}

/*****************************************************************************/

void *CfLDAPArray(char *array,char *uri,char *dn,char *filter,char *scope,char *sec)
{
#if defined HAVE_LIBCFNOVA && defined HAVE_LIBLDAP
 return Nova_LDAPArray(array,uri,dn,filter,scope,sec);
#else
 CfOut(cf_error,"","LDAP support available in Nova and above");
return NULL;
#endif
}

/*****************************************************************************/

void *CfRegLDAP(char *uri,char *dn,char *filter,char *name,char *scope,char *regex,char *sec)
{
#if defined HAVE_LIBCFNOVA && defined HAVE_LIBLDAP
return Nova_RegLDAP(uri,dn,filter,name,scope,regex,sec);
#else
CfOut(cf_error,"","LDAP support available in Nova and above");
return NULL;
#endif
}

/*****************************************************************************/
/* SQL                                                                       */
/*****************************************************************************/

int CheckDatabaseSanity(struct Attributes a, struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
return Nova_CheckDatabaseSanity(a,pp);
#else
return false;
#endif
}

/*****************************************************************************/

int VerifyDatabasePromise(CfdbConn *cfdb,char *database,struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
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

int CfVerifyTablePromise(CfdbConn *cfdb,char *name,struct Rlist *columns,struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
return Nova_VerifyTablePromise(cfdb,name,columns,a,pp);
#else
CfOut(cf_verbose,"","Verifying SQL table promises is only available with Cfengine Nova or above");
return false;
#endif
}

/*****************************************************************************/
/* Misc                                                                      */
/*****************************************************************************/

void NoteEfficiency(double e)

{
#ifdef HAVE_LIBCFNOVA
 struct Attributes a = {0};
 struct Promise p = {0};
 
NovaNamedEvent("Configuration model efficiency",e,a,&p);
CfOut(cf_verbose,"","Configuration model efficiency for %s = %.4lf%%",VUQNAME,e);
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
 char zone[ZONENAME_MAX];
 static psopts[CF_BUFSIZE];
 
zid = getzoneid();
getzonenamebyid(zid,zone,ZONENAME_MAX);

if (cf_strcmp(zone,"global") == 0)
   {
   if (cf_strcmp(s+strlen(s)-6,"global") == 0)
      {
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

int GetInstalledPkgsRpath(struct CfPackageItem **pkgList, struct Attributes a, struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA

return Nova_GetInstalledPkgsRpath(pkgList, a, pp);

#else

CfOut(cf_error, "", "!! rPath internal package listing only available in Nova or above");
return false;

#endif
}


int ExecPackageCommandRpath(char *command,int verify,int setCmdClasses,struct Attributes a,struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA

return Nova_ExecPackageCommandRpath(command,verify,setCmdClasses,a,pp);

#else

CfOut(cf_error, "", "!! rPath internal package commands only available in Nova or above");
return false;

#endif
}
