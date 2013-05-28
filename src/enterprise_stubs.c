
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

/*****************************************************************************/

#if !defined(HAVE_NOVA)

void EnterpriseModuleTrick()
{
}

/*****************************************************************************/

const char *GetConsolePrefix(void)
{
    return "cf3";
}

/*****************************************************************************/

const char *MailSubject(void)
{
    return "community";
}

/*****************************************************************************/

int IsEnterprise(void)
{
    return false;
}

/*****************************************************************************/

void EnterpriseContext(void)
{
}

/*****************************************************************************/

int CfSessionKeySize(char type)
{
    return CF_BLOWFISHSIZE;
}

/*****************************************************************************/

char CfEnterpriseOptions(void)
{
    return 'c';
}

/*****************************************************************************/

const EVP_CIPHER *CfengineCipher(char type)
{
    return EVP_bf_cbc();
}

/*****************************************************************************/

int EnterpriseExpiry(void)
{
    return false;
}

/*****************************************************************************/

void HistoryUpdate(Averages newvals)
{
}

/*****************************************************************************/

void LogFileChange(char *file, int change, Attributes a, Promise *pp)
{
    CfOut(cf_verbose, "", "Logging file differences requires version Nova or above");
}

/*****************************************************************************/

void RemoteSysLog(int log_priority, const char *log_string)
{
    CfOut(cf_verbose, "", "Remote logging requires version Nova or above");
}

/*****************************************************************************/

void WebCache(char *s, char *t)
{
}

/*****************************************************************************/

const char *PromiseID(Promise *pp)
{
    return "";
}

/*****************************************************************************/

void NotePromiseCompliance(Promise *pp, double val, PromiseState state, char *reason)
{
}

/*****************************************************************************/

void NoteEfficiency(double e)
{
}

/*****************************************************************************/

void PreSanitizePromise(Promise *pp)
{
}

/*****************************************************************************/

void TrackValue(char *date, double kept, double repaired, double notkept)
{
}

/*****************************************************************************/

void ShowTopicRepresentation(FILE *fp)
{
    CfOut(cf_verbose, "", "# Knowledge map reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void NewPromiser(Promise *pp)
{
}

/*****************************************************************************/

void AnalyzePromiseConflicts(void)
{
}

/*****************************************************************************/

void RegisterBundleDependence(char *name, Promise *pp)
{
}

/*****************************************************************************/

void SyntaxCompletion(char *s)
{
    printf("Syntax completion is available in cfengine Nova,Constellation or Galaxy\n\n");
}

/*****************************************************************************/

void SyntaxExport()
{
# ifdef HAVE_NOVA
    Nova_SyntaxTree2JavaScript();
# else
    Writer *writer = FileWriter(stdout);

    SyntaxPrintAsJson(writer);
    WriterClose(writer);
# endif
}

/*****************************************************************************/

void VerifyOutputsPromise(Promise *pp)
{
    printf(" !! Outputs promises are not available in the community edition of Cfengine\n");
}

/*****************************************************************************/

void SetPromiseOutputs(Promise *pp)
{
}

/*****************************************************************************/

void LastSawBundle(char *name, double comp)
{
}

/*****************************************************************************/

void SetBundleOutputs(char *name)
{
}

/*****************************************************************************/

void ResetBundleOutputs(char *name)
{
}

/*****************************************************************************/

void SpecialQuote(char *topic, char *type)
{
}

/*****************************************************************************/

void GetObservable(int i, char *name, char *desc)
{
    strcpy(name, OBS[i][0]);
}

/*****************************************************************************/

void LookupObservable(int i, char *name, char *desc)
{
    strcpy(name, OBS[i][0]);
}

/*****************************************************************************/

void LoadSlowlyVaryingObservations()
{
    CfOut(cf_verbose, "", "# Extended system discovery is only available in version Nova and above\n");
}

/*****************************************************************************/

void RegisterLiteralServerData(char *handle, Promise *pp)
{
    CfOut(cf_verbose, "", "# Access to server literals is only available in version Nova and above\n");
}

/*****************************************************************************/

int ReturnLiteralData(char *handle, char *ret)
{
    CfOut(cf_verbose, "", "# Access to server literals is only available in version Nova and above\n");
    return 0;
}

/*****************************************************************************/

char *GetRemoteScalar(char *proto, char *handle, char *server, int encrypted, char *rcv)
{
    CfOut(cf_verbose, "", "# Access to server literals is only available in version Nova and above\n");
    return "";
}

/*****************************************************************************/

void CacheUnreliableValue(char *caller, char *handle, char *buffer)
{
    CfOut(cf_verbose, "", "# Value fault-tolerance in version Nova and above\n");
}

/*****************************************************************************/

int RetrieveUnreliableValue(char *caller, char *handle, char *buffer)
{
    CfOut(cf_verbose, "", "# Value fault-tolerance in version Nova and above\n");
    return false;
}

/*****************************************************************************/

void TranslatePath(char *new, const char *old)
{
    strncpy(new, old, CF_BUFSIZE - 1);
}

/*****************************************************************************/

void SummarizeCompliance(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizeValue(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "",
          "# Value reporting feature is only available in version Nova and above - use the state/cf_value.log\n");
}

/*****************************************************************************/

void SummarizePromiseRepaired(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot,
                              char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizePromiseNotKept(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void GrandSummary()
{
    CfOut(cf_verbose, "", "# Reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void CSV2XML(Rlist *list)
{
    CfOut(cf_verbose, "", "# Format conversion feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizeVariables(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Variable reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizePerPromiseCompliance(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot,
                                   char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizeFileChanges(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# File change reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizeSetuid(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Setuid reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void ReportPatches(PackageManager *list)
{
    CfOut(cf_verbose, "", "# Patch reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizeSoftware(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Software summary reporting feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void SummarizeUpdates(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Software summary reporting feature is only available in version Nova and above\n");
}

void VerifyMeasurement(double *this, Attributes a, Promise *pp)
{
    CfOut(cf_verbose, "", "# Custom monitoring feature is only available in version Nova and above\n");
}

/*****************************************************************************/

void LongHaul(time_t current)
{
}

/*****************************************************************************/

void SetMeasurementPromises(Item **classlist)
{
}

/*****************************************************************************/

int VerifyDatabasePromise(CfdbConn *cfdb, char *database, Attributes a, Promise *pp)
{
    CfOut(cf_verbose, "", "Verifying SQL database promises is only available with Cfengine Nova or above");
    return false;
}

/*****************************************************************************/

void VerifyACL(char *file, Attributes a, Promise *pp)
{
    CfOut(cf_verbose, "", "Verifying ACL promises is only available with Cfengine Nova or above");
}

/*****************************************************************************/

int CheckACLSyntax(char *file, Acl acl, Promise *pp)
{
    return true;
}

/*****************************************************************************/

void VerifyRegistryPromise(Attributes a, Promise *pp)
{
}

/*****************************************************************************/

int GetRegistryValue(char *key, char *name, char *buf, int bufSz)
{
    return 0;
}

/*****************************************************************************/

void *CfLDAPValue(char *uri, char *dn, char *filter, char *name, char *scope, char *sec)
{
    CfOut(cf_error, "", "LDAP support is available in Nova and above");
    return NULL;
}

/*****************************************************************************/

void *CfLDAPList(char *uri, char *dn, char *filter, char *name, char *scope, char *sec)
{
    CfOut(cf_error, "", "LDAP support available in Nova and above");
    return NULL;
}

/*****************************************************************************/

void *CfLDAPArray(char *array, char *uri, char *dn, char *filter, char *scope, char *sec)
{
    CfOut(cf_error, "", "LDAP support available in Nova and above");
    return NULL;
}

/*****************************************************************************/

void *CfRegLDAP(char *uri, char *dn, char *filter, char *name, char *scope, char *regex, char *sec)
{
    CfOut(cf_error, "", "LDAP support available in Nova and above");
    return NULL;
}

/*****************************************************************************/

int GetInstalledPkgsRpath(PackageItem ** pkgList, Attributes a, Promise *pp)
{
    CfOut(cf_error, "", "!! rPath internal package listing only available in Nova or above");
    return false;
}

/*****************************************************************************/

int ExecPackageCommandRpath(char *command, int verify, int setCmdClasses, Attributes a, Promise *pp)
{
    CfOut(cf_error, "", "!! rPath internal package commands only available in Nova or above");
    return false;
}

/*****************************************************************************/

void AddGoalsToDB(char *goal_patterns)
{
}

/*****************************************************************************/

void SetSyslogHost(const char *host)
{
    CfOut(cf_error, "", "!! Remote syslog functionality is only available in Nova");
}

/*****************************************************************************/

void SetSyslogPort(uint16_t port)
{
    CfOut(cf_error, "", "!! Remote syslog functionality is only available in Nova");
}

/*****************************************************************************/

void VerifyWindowsService(Attributes a, Promise *pp)
{
    CfOut(cf_error, "", "!! Windows service management is only supported in CFEngine Nova");
}

/*****************************************************************************/

bool CFDB_HostsWithClass(Rlist **return_list, char *class_name, char *return_format)
{
    CfOut(cf_error, "", "!! Host class counting is only available in CFEngine Nova");
    
    return false;
}

int CopyACLs(const char *src, const char *dst)
{
    return true;
}

#endif
