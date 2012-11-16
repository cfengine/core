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

/* cflex.l */

int yylex(void);

/* cfparse.y */

void yyerror(const char *s);
int yyparse(void);

/* agent.c */

int ScheduleAgentOperations(Bundle *bp, const ReportContext *report_context);

/* agentdiagnostic.c */

void AgentDiagnostic(void);

/* args.c */

int MapBodyArgs(const char *scopeid, Rlist *give, const Rlist *take);
Rlist *NewExpArgs(const FnCall *fp, const Promise *pp);
void ArgTemplate(FnCall *fp, const FnCallArg *argtemplate, Rlist *finalargs);
void DeleteExpArgs(Rlist *args);

/* bootstrap.c */

void CheckAutoBootstrap(void);
void SetPolicyServer(char *name);
void CreateFailSafe(char *name);

/* cf_sql.c */

int CfConnectDB(CfdbConn *cfdb, enum cfdbtype dbtype, char *remotehost, char *dbuser, char *passwd, char *db);
void CfCloseDB(CfdbConn *cfdb);
void CfVoidQueryDB(CfdbConn *cfdb, char *query);
void CfNewQueryDB(CfdbConn *cfdb, char *query);
char **CfFetchRow(CfdbConn *cfdb);
char *CfFetchColumn(CfdbConn *cfdb, int col);
void CfDeleteQuery(CfdbConn *cfdb);

/* client_code.c */

void DetermineCfenginePort(void);
AgentConnection *NewServerConnection(Attributes attr, Promise *pp);
AgentConnection *ServerConnection(char *server, Attributes attr, Promise *pp);
void DisconnectServer(AgentConnection *conn);
int cf_remote_stat(char *file, struct stat *buf, char *stattype, Attributes attr, Promise *pp);
void DeleteClientCache(Attributes attr, Promise *pp);
int CompareHashNet(char *file1, char *file2, Attributes attr, Promise *pp);
int CopyRegularFileNet(char *source, char *new, off_t size, Attributes attr, Promise *pp);
int EncryptCopyRegularFileNet(char *source, char *new, off_t size, Attributes attr, Promise *pp);
int ServerConnect(AgentConnection *conn, char *host, Attributes attr, Promise *pp);
void DestroyServerConnection(AgentConnection *conn);

/* Mark connection as free */
void ServerNotBusy(AgentConnection *conn);

/* Only for agent.c */

void ConnectionsInit(void);
void ConnectionsCleanup(void);

/* client_protocol.c */

void SetSkipIdentify(bool enabled);

/* chflags.c */

int ParseFlagString(Rlist *flags, u_long *plusmask, u_long *minusmask);

/* communication.c */

AgentConnection *NewAgentConn(void);
void DeleteAgentConn(AgentConnection *ap);
void DePort(char *address);
int IsIPV6Address(char *name);
int IsIPV4Address(char *name);
const char *Hostname2IPString(const char *hostname);
char *IPString2Hostname(const char *ipaddress);
int GetMyHostInfo(char nameBuf[MAXHOSTNAMELEN], char ipBuf[MAXIP4CHARLEN]);
unsigned short SocketFamily(int sd);

/* comparray.c */

int FixCompressedArrayValue(int i, char *value, CompressedArray **start);
void DeleteCompressedArray(CompressedArray *start);
int CompressedArrayElementExists(CompressedArray *start, int key);
char *CompressedArrayValue(CompressedArray *start, int key);

#ifndef MINGW
UidList *Rlist2UidList(Rlist *uidnames, const Promise *pp);
GidList *Rlist2GidList(Rlist *gidnames, const Promise *pp);
uid_t Str2Uid(char *uidbuff, char *copy, const Promise *pp);
gid_t Str2Gid(char *gidbuff, char *copy, const Promise *pp);
#endif /* NOT MINGW */

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
void SetPolicyServer(char *name);
int IsEnterprise(void);
void EnterpriseContext(void);
int EnterpriseExpiry(void);
const char *GetConsolePrefix(void);
const char *MailSubject(void);
void CheckAutoBootstrap(void);
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
void LogFileChange(char *file, int change, Attributes a, Promise *pp, const ReportContext *report_context);
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

/* evalfunction.c */

FnCallResult CallFunction(const FnCallType *function, FnCall *fp, Rlist *finalargs);
int FnNumArgs(const FnCallType *call_type);

void ModuleProtocol(char *command, char *line, int print, const char *namespace);

/* exec_tool.c */

int IsExecutable(const char *file);
int ShellCommandReturnsZero(const char *comm, int useshell);
int GetExecOutput(char *command, char *buffer, int useshell);
void ActAsDaemon(int preserve);
char *ShEscapeCommand(char *s);
char **ArgSplitCommand(const char *comm);
void ArgFree(char **args);

/* files_copy.c */

void *CopyFileSources(char *destination, Attributes attr, Promise *pp, const ReportContext *report_context);
bool CopyRegularFileDiskReport(char *source, char *destination, Attributes attr, Promise *pp);
bool CopyRegularFileDisk(char *source, char *destination, bool make_holes);
void CheckForFileHoles(struct stat *sstat, Promise *pp);
int FSWrite(char *new, int dd, char *buf, int towrite, int *last_write_made_hole, int n_read, Attributes attr,
            Promise *pp);

/* files_edit.c */

EditContext *NewEditContext(char *filename, Attributes a, Promise *pp);
void FinishEditContext(EditContext *ec, Attributes a, Promise *pp, const ReportContext *report_context);
int SaveItemListAsFile(Item *liststart, const char *file, Attributes a, Promise *pp, const ReportContext *report_context);
#ifdef HAVE_LIBXML2
int LoadFileAsXmlDoc(xmlDocPtr *doc, const char *file, Attributes a, Promise *pp);
int SaveXmlDocAsFile(xmlDocPtr doc, const char *file, Attributes a, Promise *pp, const ReportContext *report_context);
#endif
int BeginSaveAsFile(const char *file, char *new, char *backup, struct stat *statbuf, Attributes a, Promise *pp);
int FinishSaveAsFile(const char *file, char *new, char *backup, struct stat *statbuf, Attributes a, Promise *pp, const ReportContext *report_context);
int AppendIfNoSuchLine(char *filename, char *line);

/* files_editline.c */

int ScheduleEditLineOperations(char *filename, Bundle *bp, Attributes a, Promise *pp, const ReportContext *report_context);
Bundle *MakeTemporaryBundleFromTemplate(Attributes a,Promise *pp);

/* files_editxml.c */

int ScheduleEditXmlOperations(char *filename, Bundle *bp, Attributes a, Promise *parentp,
                              const ReportContext *report_context);
#ifdef HAVE_LIBXML2
int XmlCompareToFile(xmlDocPtr doc, char *file, Attributes a, Promise *pp);
#endif

/* files_links.c */

char VerifyLink(char *destination, char *source, Attributes attr, Promise *pp, const ReportContext *report_context);
char VerifyAbsoluteLink(char *destination, char *source, Attributes attr, Promise *pp, const ReportContext *report_context);
char VerifyRelativeLink(char *destination, char *source, Attributes attr, Promise *pp, const ReportContext *report_context);
char VerifyHardLink(char *destination, char *source, Attributes attr, Promise *pp, const ReportContext *report_context);
int KillGhostLink(char *name, Attributes attr, Promise *pp);
int MakeHardLink(char *from, char *to, Attributes attr, Promise *pp);
int ExpandLinks(char *dest, char *from, int level);

/* files_hashes.c */

int FileHashChanged(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], int warnlevel, enum cfhashes type,
                    Attributes attr, Promise *pp);
void PurgeHashes(char *file, Attributes attr, Promise *pp);
void DeleteHash(CF_DB *dbp, enum cfhashes type, char *name);
ChecksumValue *NewHashValue(unsigned char digest[EVP_MAX_MD_SIZE + 1]);
int CompareFileHashes(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp);
int CompareBinaryFiles(char *file1, char *file2, struct stat *sstat, struct stat *dstat, Attributes attr, Promise *pp);
void HashFile(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type);
void HashString(const char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type);
int HashesMatch(unsigned char digest1[EVP_MAX_MD_SIZE + 1], unsigned char digest2[EVP_MAX_MD_SIZE + 1],
                enum cfhashes type);
char *HashPrint(enum cfhashes type, unsigned char digest[EVP_MAX_MD_SIZE + 1]);
char *HashPrintSafe(enum cfhashes type, unsigned char digest[EVP_MAX_MD_SIZE + 1], char buffer[EVP_MAX_MD_SIZE * 4]);
char *SkipHashType(char *hash);
const char *FileHashName(enum cfhashes id);
void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type);

/* files_properties.c */

void AddFilenameToListOfSuspicious(const char *filename);
int ConsiderFile(const char *nodename, char *path, Attributes attr, Promise *pp);
void SetSearchDevice(struct stat *sb, Promise *pp);
int DeviceBoundary(struct stat *sb, Promise *pp);

/* files_repository.c */

#include "files_repository.h"

/* files_select.c */

int SelectLeaf(char *path, struct stat *sb, Attributes attr, Promise *pp);
int GetOwnerName(char *path, struct stat *lstatptr, char *owner, int ownerSz);

#include "fncall.h"

/* full_write.c */

int FullWrite(int desc, const char *ptr, size_t len);

#include "generic_agent.h"

/* html.c */

void CfHtmlHeader(Writer *writer, char *title, char *css, char *webdriver, char *banner);
void CfHtmlFooter(Writer *writer, char *footer);
void CfHtmlTitle(FILE *fp, char *title);
int IsHtmlHeader(char *s);

/* iteration.c */

Rlist *NewIterationContext(const char *scopeid, Rlist *listvars);
void DeleteIterationContext(Rlist *lol);
int IncrementIterationContext(Rlist *iterators);
int EndOfIteration(Rlist *iterator);
int NullIterators(Rlist *iterator);

/* install.c */
int RelevantBundle(const char *agent, const char *blocktype);
Bundle *AppendBundle(Policy *policy, const char *name, const char *type, Rlist *args, const char *source_path);
Body *AppendBody(Policy *policy, const char *name, const char *type, Rlist *args, const char *source_path);
SubType *AppendSubType(Bundle *bundle, char *typename);
Promise *AppendPromise(SubType *type, char *promiser, Rval promisee, char *classes, char *bundle, char *bundletype, char *namespace);
void DeleteBundles(Bundle *bp);
void DeleteBodies(Body *bp);

/* interfaces.c */

void VerifyInterfacePromise(char *vifdev, char *vaddress, char *vnetmask, char *vbroadcast);

/* logging.c */

void BeginAudit(void);
void ClassAuditLog(const Promise *pp, Attributes attr, char *str, char status, char *error);
void PromiseLog(char *s);
void FatalError(char *s, ...) FUNC_ATTR_NORETURN FUNC_ATTR_PRINTF(1, 2);

void AuditStatusMessage(Writer *writer, char status);

/* manual.c */

void TexinfoManual(const char *source_dir, const char *output_file);

/* modes.c */

int ParseModeString(const char *modestring, mode_t *plusmask, mode_t *minusmask);

/* net.c */

int SendTransaction(int sd, char *buffer, int len, char status);
int ReceiveTransaction(int sd, char *buffer, int *more);
int RecvSocketStream(int sd, char *buffer, int toget, int nothing);
int SendSocketStream(int sd, char *buffer, int toget, int flags);

int SetReceiveTimeout(int sd, const struct timeval *timeout);

/* nfs.c */

#ifndef MINGW
int LoadMountInfo(Rlist **list);
void DeleteMountInfo(Rlist *list);
int VerifyNotInFstab(char *name, Attributes a, Promise *pp);
int VerifyInFstab(char *name, Attributes a, Promise *pp);
int VerifyMount(char *name, Attributes a, Promise *pp);
int VerifyUnmount(char *name, Attributes a, Promise *pp);
void MountAll(void);
#endif /* NOT MINGW */

/* ontology.c */

#include "ontology.h"

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

/* pipes.c */

FILE *cf_popen(const char *command, char *type);
FILE *cf_popensetuid(const char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background);
FILE *cf_popen_sh(const char *command, char *type);
FILE *cf_popen_shsetuid(const char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background);
int cf_pclose(FILE *pp);
int cf_pclose_def(FILE *pfp, Attributes a, Promise *pp);
int VerifyCommandRetcode(int retcode, int fallback, Attributes a, Promise *pp);

#ifndef MINGW
int cf_pwait(pid_t pid);
#endif /* NOT MINGW */

/* processes_select.c */

int SelectProcess(char *procentry, char **names, int *start, int *end, Attributes a, Promise *pp);
bool IsProcessNameRunning(char *procNameRegex);

/* recursion.c */

int DepthSearch(char *name, struct stat *sb, int rlevel, Attributes attr, Promise *pp, const ReportContext *report_context);
int SkipDirLinks(char *path, const char *lastnode, Recursion r);

/* rlist.c */
#include "rlist.h"

/* selfdiagnostic.c */

void SelfDiagnostic(void);
void TestVariableScan(void);
void TestExpandPromise(const ReportContext *report_context);
void TestExpandVariables(const ReportContext *report_context);

/* server_transform.c */

void KeepControlPromises(Policy *policy);
Auth *GetAuthPath(char *path, Auth *list);
void Summarize(void);

/* signals.c */

void HandleSignals(int signum);

/* string_lib.c */

#include "string_lib.h"

/* sockaddr.c */

/* Not thread-safe */
char *sockaddr_ntop(struct sockaddr *sa);

/* Thread-safe. Returns boolean success.
   It's up to caller to provide large enough addr. */
bool sockaddr_pton(int af, void *src, void *addr);

/* storage_tools.c */

off_t GetDiskUsage(char *file, enum cfsizes type);

/* transaction.c */

void SummarizeTransaction(Attributes attr, const Promise *pp, const char *logname);
CfLock AcquireLock(char *operand, char *host, time_t now, Attributes attr, Promise *pp, int ignoreProcesses);
void YieldCurrentLock(CfLock this);
void GetLockName(char *lockname, char *locktype, char *base, Rlist *params);

#if defined(HAVE_PTHREAD)
int ThreadLock(pthread_mutex_t *name);
int ThreadUnlock(pthread_mutex_t *name);
#else
# define ThreadLock(name) (1)
# define ThreadUnlock(name) (1)
#endif

void PurgeLocks(void);
int ShiftChange(void);

int WriteLock(char *lock);
CF_DB *OpenLock(void);
void CloseLock(CF_DB *dbp);

/* timeout.c */

void SetTimeOut(int timeout);
void TimeOut(void);
void SetReferenceTime(int setclasses);
void SetStartTime(void);

/* verify_databases.c */

void VerifyDatabasePromises(Promise *pp);

/* verify_exec.c */

void VerifyExecPromise(Promise *pp);

/* verify_files.c */

void VerifyFilePromise(char *path, Promise *pp, const ReportContext *report_context);

void LocateFilePromiserGroup(char *wildpath, Promise *pp, void (*fnptr) (char *path, Promise *ptr, const ReportContext *report_context),
                             const ReportContext *report_context);
void *FindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context);
int FileSanityChecks(char *path, Attributes a, Promise *pp);

/* verify_interfaces.c */

void VerifyInterface(Attributes a, Promise *pp);
void VerifyInterfacesPromise(Promise *pp);

/* verify_measurements.c */

void VerifyMeasurementPromise(double *this, Promise *pp);

/* verify_methods.c */

void VerifyMethodsPromise(Promise *pp, const ReportContext *report_context);
int VerifyMethod(char *attrname, Attributes a, Promise *pp, const ReportContext *report_context);

/* verify_outputs.c */

void VerifyOutputsPromise(Promise *pp);
void SetPromiseOutputs(Promise *pp);
void SetBundleOutputs(char *name);
void ResetBundleOutputs(char *name);

/* verify_packages.c */

void VerifyPackagesPromise(Promise *pp);
void ExecuteScheduledPackages(void);
void CleanScheduledPackages(void);
int PrependPackageItem(PackageItem ** list, const char *name, const char *version, const char *arch, Attributes a, Promise *pp);

/* verify_processes.c */

void VerifyProcessesPromise(Promise *pp);
void VerifyProcesses(Attributes a, Promise *pp);
int LoadProcessTable(Item **procdata);
int DoAllSignals(Item *siglist, Attributes a, Promise *pp);
int GracefulTerminate(pid_t pid);
void GetProcessColumnNames(char *proc, char **names, int *start, int *end);

/* verify_services.c */

void VerifyServicesPromise(Promise *pp, const ReportContext *report_context);

/* verify_storage.c */

void *FindAndVerifyStoragePromises(Promise *pp, const ReportContext *report_context);
void VerifyStoragePromise(char *path, Promise *pp, const ReportContext *report_context);

/* verify_reports.c */

void VerifyReportPromise(Promise *pp);

#endif
