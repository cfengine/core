
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

int EnterpriseExpiry(char *day,char *month,char *year)

{
#ifdef HAVE_LIBCFNOVA
return Nova_EnterpriseExpiry(day,month,year);
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
#ifdef HAVE_LIBCFNOVA
 return "nova>";
#else
 return "cf3";
#endif
}

/*****************************************************************************/

pid_t StartTwin(int argc,char **argv)

/* Self-monitor in case of crash or binary change */
    
{
#ifdef HAVE_LIBCFNOVA
return Nova_StartTwin(argc,argv);
#else
return 0;
#endif
}

/*****************************************************************************/

void SignalTwin()

/* Self-monitor in case of crash or binary change */
    
{
#ifdef HAVE_LIBCFNOVA
 Nova_SignalTwin();
#else
#endif
}

/*****************************************************************************/

void ReviveOther(int argc,char **argv)

/* Self-monitor in case of crash or binary change */
    
{
#ifdef HAVE_LIBCFNOVA
 Nova_ReviveOther(argc,argv);
#else
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
   strcpy(buffer,"EXPIRED");
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
/* Knowledge                                                                 */
/*****************************************************************************/

void BundleNode(FILE *fp,char *bundle)

{
#ifdef HAVE_LIBCFNOVA
Nova_BundleNode(fp,bundle);
#else
#endif
}

/*****************************************************************************/

void BodyNode(FILE *fp,char *bundle,int calltype)

{
#ifdef HAVE_LIBCFNOVA
 Nova_BodyNode(fp,bundle,calltype);
#else
#endif
}

/*****************************************************************************/

void TypeNode(FILE *fp,char *type)

{
#ifdef HAVE_LIBCFNOVA
 Nova_TypeNode(fp,type);
#else
#endif
}

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

void NotePromiseCompliance(struct Promise *pp,double val,enum cf_status status)

{
#ifdef HAVE_LIBCFNOVA
Nova_NotePromiseCompliance(pp,val,status);
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

void PromiseNode(FILE *fp,struct Promise *pp,int type)

{
#ifdef HAVE_LIBCFNOVA
 Nova_PromiseNode(fp,pp,type);
#else
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
/* Monitord                                                                  */
/*****************************************************************************/

void HistoryUpdate(struct Averages newvals)

{ struct Promise *pp = NewPromise("history_db","the long term memory");
  struct Attributes dummyattr;
  struct CfLock thislock;
  time_t now = time(NULL);
  char timekey[CF_MAXVARSIZE];

#ifdef HAVE_LIBCFNOVA  
/* We do this only once per hour - this should not be changed */

Banner("Update long-term history");

if (strlen(CURRENT_SHIFT) == 0)
   {
   // initialize
   Nova_ResetShiftAverage(&SHIFT_VALUE);
   }

memset(&dummyattr,0,sizeof(dummyattr));
dummyattr.transaction.ifelapsed = 59;

thislock = AcquireLock(pp->promiser,VUQNAME,now,dummyattr,pp);

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

void Aggregate(char *stylesheet,char *banner,char *footer,char *webdriver)

{
#ifdef HAVE_LIBCFNOVA
Nova_Aggregate(stylesheet,banner,footer,webdriver);
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
{
#ifdef HAVE_LIBCFNOVA
 Nova_ReportSoftware(list);
#else
 CfOut(cf_verbose,"","# Software reporting feature is only available in version Nova and above\n");
#endif
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
 struct Attributes a;
 struct Promise p;
 
NovaNamedEvent("Configuration model efficiency",e,a,&p);
CfOut(cf_verbose,"","Configuration model efficiency for %s = %.2lf%%",VUQNAME,e);
#endif 
}

/*****************************************************************************/

char *GetProcessOptions()
{
#ifdef HAVE_LIBCFNOVA
 return Nova_GetProcessOptions();
#else
CfOut(cf_verbose,"","Verifying SQL table promises is only available with Cfengine Nova or above");
return VPSOPTS[VSYSTEMHARDCLASS];
#endif
}

/*****************************************************************************/
/* Linker troubles require this code to be here in the main body             */
/*****************************************************************************/

#ifdef HAVE_LIBCFNOVA

int Nova_VerifyTablePromise(CfdbConn *cfdb,char *table_path,struct Rlist *columns,struct Attributes a,struct Promise *pp)

{ char name[CF_MAXVARSIZE],type[CF_MAXVARSIZE],query[CF_MAXVARSIZE],table[CF_MAXVARSIZE],db[CF_MAXVARSIZE];
  int i,count,size,no_of_cols,*size_table,*done,identified,retval = true;
  char **name_table,**type_table;
  struct Rlist *rp, *cols;

CfOut(cf_verbose,""," -> Verifying promised table structure for \"%s\"",table_path);

if (!Nova_ValidateSQLTableName(table_path,db,table))
   {
   CfOut(cf_error,""," !! The structure of the promiser did not match that for an SQL table, i.e. \"database.table\"\n",table_path);
   return false;
   }
else
   {
   CfOut(cf_verbose,""," -> Assuming database \"%s\" with table \"%s\"",db,table);
   }

/* Verify the existence of the tables within the database */

if (!Nova_TableExists(cfdb,table))
   {
   CfOut(cf_error,""," !! The database did not contain the promised table \"%s\"\n",table_path);

   if (a.database.operation && strcmp(a.database.operation,"create") == 0)
      {
      if (!DONTDO && a.transaction.action != cfa_warn)
         {
         cfPS(cf_error,CF_CHG,"",pp,a," -> Database.table %s doesn't seem to exist, creating\n",table_path);
         return Nova_CreateTableColumns(cfdb,table,columns,a,pp);
         }
      else
         {
         CfOut(cf_error,""," -> Database.table %s doesn't seem to exist, but only a warning was promised\n",table_path);
         }
      }
   
   return false;
   }

/* Get a list of the columns in the table */

Nova_QueryTableColumns(query,db,table);
CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"Could not make sense of the columns");
   CfDeleteQuery(cfdb);
   return false;
   }

/* Assume that the Rlist has been validated and consists of a,b,c */

count = 0;
no_of_cols = RlistLen(columns);

if (!Nova_NewSQLColumns(table,columns,&name_table,&type_table,&size_table,&done))
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"Could not make sense of the columns");
   return false;
   }

/* Obtain columns from the named table - if any */

while(CfFetchRow(cfdb))
   {
   name[0] = '\0';
   type[0] = '\0';
   size = CF_NOINT;
   
   strncpy(name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(type,ToLowerStr(CfFetchColumn(cfdb,1)),CF_MAXVARSIZE-1);
   size = Str2Int(CfFetchColumn(cfdb,2));

   CfOut(cf_verbose,"","    ... discovered column (%s,%s,%d)",name,type,size);
   
   if (size == CF_NOINT)
      {
      cfPS(cf_error,CF_NOP,"",pp,a," !! Integer size of datatype could not be determined - invalid promise.");
      Nova_DeleteSQLColumns(name_table,type_table,size_table,done,no_of_cols);
      free(done);
      CfDeleteQuery(cfdb);
      return false;
      }

   identified = false;
   
   for (i = 0; i < no_of_cols; i++)
      {
      if (done[i])
         {
         continue;
         }

      if (strcmp(name,name_table[i]) == 0)
         {
         NovaCheckSQLDataType(type,type_table[i],pp);

         if (size != size_table[i])
            {
            cfPS(cf_error,CF_FAIL,"",pp,a," !! Promised column \"%s\" in database.table \"%s\" has a non-matching array size (%d != %d)",name,table_path,size,size_table[i]);
            }
         else
            {
            CfOut(cf_verbose,""," -> Promised column \"%s\" in database.table \"%s\" is as promised",name,table_path);
            }
         
         count++;
         done[i] = true;
         identified = true;
         break;
         }
      }

   if (!identified)
      {
      cfPS(cf_error,CF_FAIL,"",pp,a,"Column \"%s\" found in database.table \"%s\" is not part of its promise.",name,table_path);

      if (a.database.operation && strcmp(a.database.operation,"drop") == 0)
         {
         cfPS(cf_error,CF_FAIL,"",pp,a,"Cfengine will not promise to repair this, as the operation is potentially too destructive.");
         // Future allow deletion?
         }
      
      retval = false;
      }
   }

CfDeleteQuery(cfdb);

/* Now look for deviations - only if we have promised to create missing */

if (a.database.operation && strcmp(a.database.operation,"drop") == 0)
   {
   return retval;
   }

if (count != no_of_cols)
   {
   for (i = 0; i < no_of_cols; i++)
      {
      if (!done[i])
         {
         CfOut(cf_error,""," !! Promised column \"%s\" missing from database table %s",name_table[i],pp->promiser);
         
         if (!DONTDO && a.transaction.action != cfa_warn)
            {
            if (size_table[i] > 0)
               {
               snprintf(query,CF_MAXVARSIZE-1,"ALTER TABLE %s ADD %s %s(%d)",table,name_table[i],type_table[i],size_table[i]);
               }
            else
               {
               snprintf(query,CF_MAXVARSIZE-1,"ALTER TABLE %s ADD %s %s",table,name_table[i],type_table[i]);
               }
            
            CfVoidQueryDB(cfdb,query);
            cfPS(cf_error,CF_CHG,"",pp,a," !! Adding promised column \"%s\" to database table %s",name_table[i],table);
            retval = true;
            }
         else
            {
            cfPS(cf_error,CF_WARN,"",pp,a," !! Promised column \"%s\" missing from database table %s but only a warning was promised",name_table[i],table);
            retval = false;
            }
         }
      }
   }

Nova_DeleteSQLColumns(name_table,type_table,size_table,done,no_of_cols);

return retval;
}

/*****************************************************************************/

int Nova_TableExists(CfdbConn *cfdb,char *name)

{ struct Rlist *rp,*list = NULL;
  int match = false;

list = Nova_GetSQLTables(cfdb);
 
for (rp = list; rp != NULL; rp=rp->next)
   {
   if (strcmp(name,rp->item) == 0)
      {
      match = true;
      }
   }

DeleteRlist(list);

return match;
}

/*****************************************************************************/

int Nova_CreateTableColumns(CfdbConn *cfdb,char *table,struct Rlist *columns,struct Attributes a,struct Promise *pp)

{ char entry[CF_MAXVARSIZE],query[CF_BUFSIZE];
  int i,count,size,*size_table,*done,identified,retval = true;
  char **name_table,**type_table;
  struct Rlist *rp, *cols;
  int no_of_cols = RlistLen(columns);

CfOut(cf_error,""," -> Trying to create table %s\n",table);
  
if (!Nova_NewSQLColumns(table,columns,&name_table,&type_table,&size_table,&done))
   {
   return false;
   }

if (no_of_cols > 0)
   {
   snprintf(query,CF_BUFSIZE-1,"create table %s(",table);
   
   for (i = 0; i < no_of_cols; i++)
      {
      CfOut(cf_verbose,""," -> Forming column template %s %s %d\n",name_table[i],type_table[i],size_table[i]);;
      
      if (size_table[i] > 0)
         {
         snprintf(entry,CF_MAXVARSIZE-1,"%s %s(%d)",name_table[i],type_table[i],size_table[i]);
         }
      else
         {
         snprintf(entry,CF_MAXVARSIZE-1,"%s %s",name_table[i],type_table[i]);
         }

      strcat(query,entry);

      if (i < no_of_cols -1)
         {
         strcat(query,",");
         }
      }

   strcat(query,")");
   }

CfVoidQueryDB(cfdb,query);
Nova_DeleteSQLColumns(name_table,type_table,size_table,done,no_of_cols);
return true;
}


/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

struct Rlist *Nova_GetSQLTables(CfdbConn *cfdb)

{ struct Rlist *list = NULL;
  char query[CF_MAXVARSIZE];

Nova_ListTables(cfdb->type,query);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 1)
   {
   CfOut(cf_error,"","Could not make sense of the columns");
   CfDeleteQuery(cfdb);
   return NULL;
   }

while(CfFetchRow(cfdb))
   {
   PrependRScalar(&list,CfFetchColumn(cfdb,0),CF_SCALAR);
   }

CfDeleteQuery(cfdb);

return list;
}

#endif
