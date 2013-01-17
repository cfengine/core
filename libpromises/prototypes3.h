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

#ifndef CFENGINE_PROTOTYPES3_H
#define CFENGINE_PROTOTYPES3_H

#include "compiler.h"

/* Versions */

const char *Version(void);
const char *NameVersion(void);

/* cfparse.y */

void yyerror(const char *s);

/* agent.c */

int ScheduleAgentOperations(Bundle *bp, const ReportContext *report_context);

/* agentdiagnostic.c */

void AgentDiagnostic(void);

/* Mark connection as free */
void ServerNotBusy(AgentConnection *conn);

/* Only for agent.c */

void ConnectionsInit(void);
void ConnectionsCleanup(void);

/* client_protocol.c */

void SetSkipIdentify(bool enabled);

/* dtypes.c */

int IsSocketType(char *s);
int IsTCPType(char *s);

/* enterprise_stubs.c */

void SyntaxExport(void);
#if defined(__MINGW32__)
void VerifyRegistryPromise(Attributes a, Promise *pp);
#endif
int CfSessionKeySize(char c);
char CfEnterpriseOptions(void);
const EVP_CIPHER *CfengineCipher(char type);
void Aggregate(char *stylesheet, char *banner, char *footer, char *webdriver);
int IsEnterprise(void);
void EnterpriseContext(void);
int EnterpriseExpiry(void);
const char *GetConsolePrefix(void);
const char *MailSubject(void);
void RegisterBundleDependence(char *absscope, const Promise *pp);
void ShowTopicRepresentation(const ReportContext *report_context);
void PreSanitizePromise(Promise *pp);
void Nova_ShowTopicRepresentation(FILE *fp);
void NoteEfficiency(double e);
void HistoryUpdate(Averages newvals);
void GetObservable(int i, char *name, char *desc);
void LookupObservable(int i, char *name, char *desc);
void SummarizePromiseRepaired(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot,
                              char *web);
void SummarizePromiseNotKept(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot,
                             char *web);
void SummarizeCompliance(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web);
void SummarizePerPromiseCompliance(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot,
                                   char *web);
void SummarizeSetuid(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web);
void SummarizeFileChanges(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web);
void SummarizeValue(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web);
void VerifyMeasurement(double *this, Attributes a, Promise *pp);
void SetMeasurementPromises(Item **classlist);
void LongHaul(time_t current);
void ReportPatches(PackageManager *list);
void SummarizeSoftware(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web);
void SummarizeUpdates(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web);
void VerifyServices(Attributes a, Promise *pp, const ReportContext *report_context);
void LoadSlowlyVaryingObservations(void);
void MonOtherInit(void);
void MonOtherGatherData(double *cf_this);
void RegisterLiteralServerData(char *handle, Promise *pp);
int ReturnLiteralData(char *handle, char *ret);
char *GetRemoteScalar(char *proto, char *handle, char *server, int encrypted, char *rcv);
const char *PromiseID(const Promise *pp);     /* Not thread-safe */
void NotePromiseCompliance(const Promise *pp, double val, PromiseState state, char *reasoin);
void LogTotalCompliance(const char *version);
#if defined(__MINGW32__)
int GetRegistryValue(char *key, char *name, char *buf, int bufSz);
#endif
void NoteVarUsage(void);
void NoteVarUsageDB(void);
void SummarizeVariables(int xml, int html, int csv, int embed, char *stylesheet, char *head, char *foot, char *web);
void CSV2XML(Rlist *list);
void *CfLDAPValue(char *uri, char *dn, char *filter, char *name, char *scope, char *sec);
void *CfLDAPList(char *uri, char *dn, char *filter, char *name, char *scope, char *sec);
void *CfLDAPArray(char *array, char *uri, char *dn, char *filter, char *scope, char *sec);
void *CfRegLDAP(char *uri, char *dn, char *filter, char *name, char *scope, char *regex, char *sec);
void CacheUnreliableValue(char *caller, char *handle, char *buffer);
int RetrieveUnreliableValue(char *caller, char *handle, char *buffer);
void TranslatePath(char *new, const char *old);
void GrandSummary(void);
void TrackValue(char *date, double kept, double repaired, double notkept);
void LastSawBundle(const Bundle *bundle, double compliance);
void NewPromiser(Promise *pp);
void AnalyzePromiseConflicts(void);
void AddGoalsToDB(char *goal_patterns);
void VerifyWindowsService(Attributes a, Promise *pp);
bool CFDB_HostsWithClass(Rlist **return_list, char *class_name, char *return_format);

void SyntaxCompletion(char *s);
void TryCollectCall(void);
int SetServerListenState(size_t queue_size);

struct ServerConnectionState;

int ReceiveCollectCall(struct ServerConnectionState *conn, char *sendbuffer);

/* files_editline.c */

int ScheduleEditLineOperations(char *filename, Bundle *bp, Attributes a, Promise *pp, const ReportContext *report_context);
Bundle *MakeTemporaryBundleFromTemplate(Attributes a,Promise *pp);

/* files_editxml.c */

int ScheduleEditXmlOperations(char *filename, Bundle *bp, Attributes a, Promise *parentp,
                              const ReportContext *report_context);
#ifdef HAVE_LIBXML2
int XmlCompareToFile(xmlDocPtr doc, char *file, Attributes a, Promise *pp);
#endif

/* files_select.c */

int SelectLeaf(char *path, struct stat *sb, Attributes attr, Promise *pp);

/* full_write.c */

int FullWrite(int desc, const char *ptr, size_t len);

/* html.c */

void CfHtmlHeader(Writer *writer, char *title, char *css, char *webdriver, char *banner);
void CfHtmlFooter(Writer *writer, char *footer);

/* interfaces.c */

void VerifyInterfacePromise(char *vifdev, char *vaddress, char *vnetmask, char *vbroadcast);

/* manual.c */

void TexinfoManual(const char *source_dir, const char *output_file);

/* modes.c */

int ParseModeString(const char *modestring, mode_t *plusmask, mode_t *minusmask);

/* patches.c */

int IsPrivileged(void);
char *MapName(char *s);
char *MapNameCopy(const char *s);
char *MapNameForward(char *s);
char *cf_ctime(const time_t *timep);
char *cf_strtimestamp_local(const time_t time, char *buf);
char *cf_strtimestamp_utc(const time_t time, char *buf);
int cf_closesocket(int sd);
int cf_mkdir(const char *path, mode_t mode);
int cf_chmod(const char *path, mode_t mode);
int cf_rename(const char *oldpath, const char *newpath);

#if !defined(__MINGW32__)
#define OpenNetwork() /* noop */
#define CloseNetwork() /* noop */
#else
void OpenNetwork(void);
void CloseNetwork(void);
#endif

int LinkOrCopy(const char *from, const char *to, int sym);
int ExclusiveLockFile(int fd);
int ExclusiveUnlockFile(int fd);

/* selfdiagnostic.c */

void SelfDiagnostic(void);
void TestVariableScan(void);
void TestExpandPromise(const ReportContext *report_context);
void TestExpandVariables(const ReportContext *report_context);

/* server_transform.c */

void KeepControlPromises(Policy *policy);
Auth *GetAuthPath(char *path, Auth *list);
void Summarize(void);

/* sockaddr.c */

/* Not thread-safe */
char *sockaddr_ntop(struct sockaddr *sa);

/* Thread-safe. Returns boolean success.
   It's up to caller to provide large enough addr. */
bool sockaddr_pton(int af, const void *src, void *addr);

/* storage_tools.c */

off_t GetDiskUsage(char *file, enum cfsizes type);

/* timeout.c */

void SetTimeOut(int timeout);
void TimeOut(void);
void SetReferenceTime(int setclasses);
void SetStartTime(void);
bool IsReadReady(int fd, int timeout_sec);

/* verify_files.c */

void VerifyFilePromise(char *path, Promise *pp, const ReportContext *report_context);

void LocateFilePromiserGroup(char *wildpath, Promise *pp, void (*fnptr) (char *path, Promise *ptr, const ReportContext *report_context),
                             const ReportContext *report_context);
void *FindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context);

/* verify_interfaces.c */

void VerifyInterface(Attributes a, Promise *pp);
void VerifyInterfacesPromise(Promise *pp);

/* verify_measurements.c */

void VerifyMeasurementPromise(double *this, Promise *pp);

/* verify_methods.c */

void VerifyMethodsPromise(Promise *pp, const ReportContext *report_context);
int VerifyMethod(char *attrname, Attributes a, Promise *pp, const ReportContext *report_context);

/* verify_reports.c */

void VerifyReportPromise(Promise *pp);

/* misc */

int GracefulTerminate(pid_t pid);

#endif
