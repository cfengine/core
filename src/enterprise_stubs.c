
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

#include "cf3.defs.h"

#include "prototypes3.h"
#include "syntax.h"
#include "cfstream.h"

#if !defined(HAVE_NOVA)

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

/* all agents: generic_agent.c */


const char *GetConsolePrefix(void)
{
    return "cf3";
}

int IsEnterprise(void)
{
    return false;
}


/* all agents: sysinfo.c */

void EnterpriseContext(void)
{
}

void LoadSlowlyVaryingObservations()
{
    CfOut(cf_verbose, "", "# Extended system discovery is only available in version Nova and above\n");
}


/* all agents: generic_agent.c, cf-execd.c, cf-serverd.c */


int EnterpriseExpiry(void)
{
    return false;
}


/* all agents: cfstream.c, expand.c, generic_agent.c */


const char *PromiseID(const Promise *pp)
{
    return "";
}


/* all agents: expand.c */


void RegisterBundleDependence(char *name, const Promise *pp)
{
}


/* all agents: logging.c */


void NotePromiseCompliance(const Promise *pp, double val, PromiseState state, char *reason)
{
}

void TrackValue(char *date, double kept, double repaired, double notkept)
{
}

void LogTotalCompliance(const char *version)
{
    double total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    char string[CF_BUFSIZE] = { 0 };

    snprintf(string, CF_BUFSIZE,
             "Outcome of version %s (" CF_AGENTC "-%d): Promises observed to be kept %.0f%%, Promises repaired %.0f%%, Promises not repaired %.0f\%%",
             version, CFA_BACKGROUND,
             (double) PR_KEPT / total,
             (double) PR_REPAIRED / total,
             (double) PR_NOTKEPT / total);

    CfOut(cf_verbose, "", "Total: %s", string);

    PromiseLog(string);
}


/* all agents: constraints.c */


void PreSanitizePromise(Promise *pp)
{
}

void NewPromiser(Promise *pp)
{
}


/* FIXME: all agents: generic_agent.c */


void ShowTopicRepresentation(const ReportContext *report_context)
{
    CfOut(cf_verbose, "", "# Knowledge map reporting feature is only available in version Nova and above\n");
}

/* cf-execd: cf-execd-runner.c */


const char *MailSubject(void)
{
    return "community";
}


/* network communication: cf-serverd.c, client_protocol.c, client_code.c, crypto.c */


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


/* cf-monitord: env_monitor.c, verify_measurement.c */


void HistoryUpdate(Averages newvals)
{
}

void GetObservable(int i, char *name, char *desc)
{
    strcpy(name, OBS[i][0]);
}

void VerifyMeasurement(double *this, Attributes a, Promise *pp)
{
    CfOut(cf_verbose, "", "# Custom monitoring feature is only available in version Nova and above\n");
}

void SetMeasurementPromises(Item **classlist)
{
}


/* cf-agent: files_operators.c */


void LogFileChange(char *file, int change, Attributes a, Promise *pp, const ReportContext *report_context)
{
    CfOut(cf_verbose, "", "Logging file differences requires version Nova or above");
}


/* cf-agent: cf-agent.c */


void NoteEfficiency(double e)
{
}

void LastSawBundle(const Bundle *bundle, double comp)
{
}


/* cf-agent: verify_packages.c */


void ReportPatches(PackageManager *list)
{
    CfOut(cf_verbose, "", "# Patch reporting feature is only available in version Nova and above\n");
}


/* cf-agent: evalfunction.c */


char *GetRemoteScalar(char *proto, char *handle, char *server, int encrypted, char *rcv)
{
    CfOut(cf_verbose, "", "# Access to server literals is only available in version Nova and above\n");
    return "";
}

void CacheUnreliableValue(char *caller, char *handle, char *buffer)
{
    CfOut(cf_verbose, "", "# Value fault-tolerance in version Nova and above\n");
}

int RetrieveUnreliableValue(char *caller, char *handle, char *buffer)
{
    CfOut(cf_verbose, "", "# Value fault-tolerance in version Nova and above\n");
    return false;
}

#if defined(__MINGW32__)
int GetRegistryValue(char *key, char *name, char *buf, int bufSz)
{
    return 0;
}
#endif

void *CfLDAPValue(char *uri, char *dn, char *filter, char *name, char *scope, char *sec)
{
    CfOut(cf_error, "", "LDAP support is available in Nova and above");
    return NULL;
}

void *CfLDAPList(char *uri, char *dn, char *filter, char *name, char *scope, char *sec)
{
    CfOut(cf_error, "", "LDAP support available in Nova and above");
    return NULL;
}

void *CfLDAPArray(char *array, char *uri, char *dn, char *filter, char *scope, char *sec)
{
    CfOut(cf_error, "", "LDAP support available in Nova and above");
    return NULL;
}

void *CfRegLDAP(char *uri, char *dn, char *filter, char *name, char *scope, char *regex, char *sec)
{
    CfOut(cf_error, "", "LDAP support available in Nova and above");
    return NULL;
}

bool CFDB_HostsWithClass(Rlist **return_list, char *class_name, char *return_format)
{
    CfOut(cf_error, "", "!! Host class counting is only available in CFEngine Nova");
    return false;
}


/* cf-agent: verify_databases.c */


#if defined(__MINGW32__)
void VerifyRegistryPromise(Attributes a, Promise *pp)
{
}
#endif


/* cf-agent: verify_services.c */


void VerifyWindowsService(Attributes a, Promise *pp)
{
    CfOut(cf_error, "", "!! Windows service management is only supported in CFEngine Nova");
}


/* cf-promises: cf-promises.c */


void AnalyzePromiseConflicts(void)
{
}

void AddGoalsToDB(char *goal_patterns)
{
}


/* cf-report: cf-report.c */

void SyntaxExport(void)
{
    Writer *writer = FileWriter(stdout);
    SyntaxPrintAsJson(writer);
    WriterClose(writer);
}

void LookupObservable(int i, char *name, char *desc)
{
    strcpy(name, OBS[i][0]);
}

void SummarizeCompliance(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

void SummarizeValue(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "",
          "# Value reporting feature is only available in version Nova and above - use the state/cf_value.log\n");
}

void SummarizePromiseRepaired(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot,
                              char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

void SummarizePromiseNotKept(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

void GrandSummary()
{
    CfOut(cf_verbose, "", "# Reporting feature is only available in version Nova and above\n");
}

void CSV2XML(Rlist *list)
{
    CfOut(cf_verbose, "", "# Format conversion feature is only available in version Nova and above\n");
}

void SummarizeVariables(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Variable reporting feature is only available in version Nova and above\n");
}

void SummarizePerPromiseCompliance(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot,
                                   char *web)
{
    CfOut(cf_verbose, "", "# Compliance reporting feature is only available in version Nova and above\n");
}

void SummarizeFileChanges(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# File change reporting feature is only available in version Nova and above\n");
}

void SummarizeSetuid(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Setuid reporting feature is only available in version Nova and above\n");
}

void SummarizeSoftware(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Software summary reporting feature is only available in version Nova and above\n");
}

void SummarizeUpdates(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web)
{
    CfOut(cf_verbose, "", "# Software summary reporting feature is only available in version Nova and above\n");
}

void LongHaul(time_t current)
{
}


/* cf-serverd: server_transform.c, cf-serverd.c */


void TranslatePath(char *new, const char *old)
{
    strncpy(new, old, CF_BUFSIZE - 1);
}

void RegisterLiteralServerData(char *handle, Promise *pp)
{
    CfOut(cf_verbose, "", "# Access to server literals is only available in version Nova and above\n");
}

int ReturnLiteralData(char *handle, char *ret)
{
    CfOut(cf_verbose, "", "# Access to server literals is only available in version Nova and above\n");
    return 0;
}

void TryCollectCall(void)
{
    CfOut(cf_verbose, "", " !! Collect calling is only supported in CFEngine Enterprise");
}

int ReceiveCollectCall(struct ServerConnectionState *conn, char *sendbuffer)
{
    CfOut(cf_verbose, "", "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    CfOut(cf_verbose, "", "  Collect Call are only supported in the Enterprise ");
    CfOut(cf_verbose, "", "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"); 
    return false;
}

/* cf-know */

void SyntaxCompletion(char *s)
{
}

#endif
