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
/* File: prototypes3.h                                                       */
/*                                                                           */
/* Created: Sun Aug  5 13:43:56 2007                                         */
/*                                                                           */
/*****************************************************************************/

#ifndef CFENGINE_PROTOTYPES3_H
#define CFENGINE_PROTOTYPES3_H

#include "compiler.h"

/* Versions */

const char *Version(void);
const char *NameVersion(void);

/* full-write.c */

int cf_full_write (int desc, char *ptr, size_t len);

/* cflex.l */

int yylex (void);

/* cfparse.y */

void yyerror (const char *s);
int yyparse (void);

/* Generic stubs for the agents */

void ThisAgentInit(void);
void KeepPromises(void);

/* alloc.c */

void *xcalloc(size_t nmemb, size_t size);
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);
char *xstrndup(const char *str, size_t n);

/* agent.c */

int ScheduleAgentOperations(struct Bundle *bp);

/* agentdiagnostic.c */

void AgentDiagnostic(char *inputfile);

/* alphalist.c */

void InitAlphaList(struct AlphaList *al);
int InAlphaList(struct AlphaList al,const char *string);
int MatchInAlphaList(struct AlphaList al,char *string);
void PrependAlphaList(struct AlphaList *al, const char *string);
void IdempPrependAlphaList(struct AlphaList *al, const char *string);
void ListAlphaList(FILE *fp,struct AlphaList al,char sep);
void DeleteAlphaList(struct AlphaList *al);
struct AlphaList *CopyAlphaListPointers(struct AlphaList *al,struct AlphaList *ap);

/* args.c */

int MapBodyArgs(char *scopeid,struct Rlist *give,struct Rlist *take);
struct Rlist *NewExpArgs(struct FnCall *fp, struct Promise *pp);
void ArgTemplate(struct FnCall *fp,struct FnCallArg *argtemplate,struct Rlist *finalargs);
void DeleteExpArgs(struct Rlist *args);

/* assoc.c */

struct CfAssoc *NewAssoc(const char *lval,void *rval,char rtype,enum cfdatatype dt);
void DeleteAssoc(struct CfAssoc *ap);
struct CfAssoc *CopyAssoc(struct CfAssoc *old);

/* attributes.c */

struct Attributes GetEnvironmentsAttributes(struct Promise *pp);
struct CfEnvironments GetEnvironmentsConstraints(struct Promise *pp);
struct Attributes GetOutputsAttributes(struct Promise *pp);
struct Attributes GetServicesAttributes(struct Promise *pp);
struct CfServices GetServicesConstraints(struct Promise *pp);
struct Attributes GetFilesAttributes(struct Promise *pp);
struct Attributes GetReportsAttributes(struct Promise *pp);
struct Attributes GetExecAttributes(struct Promise *pp);
struct Attributes GetProcessAttributes(struct Promise *pp);
struct Attributes GetStorageAttributes(struct Promise *pp);
struct Attributes GetClassContextAttributes(struct Promise *pp);
struct Attributes GetTopicsAttributes(struct Promise *pp);
struct Attributes GetThingsAttributes(struct Promise *pp);
struct Attributes GetInferencesAttributes(struct Promise *pp);
struct Attributes GetOccurrenceAttributes(struct Promise *pp);
struct Attributes GetPackageAttributes(struct Promise *pp);
struct Attributes GetMeasurementAttributes(struct Promise *pp);
struct Attributes GetDatabaseAttributes(struct Promise *pp);

struct Packages GetPackageConstraints(struct Promise *pp);
struct ExecContain GetExecContainConstraints(struct Promise *pp);
struct Recursion GetRecursionConstraints(struct Promise *pp);
struct FileSelect GetSelectConstraints(struct Promise *pp);
struct FilePerms GetPermissionConstraints(struct Promise *pp);
struct TransactionContext GetTransactionConstraints(struct Promise *pp);
struct DefineClasses GetClassDefinitionConstraints(struct Promise *pp);
struct FileRename GetRenameConstraints(struct Promise *pp);
struct FileDelete GetDeleteConstraints(struct Promise *pp);
struct FileChange GetChangeMgtConstraints(struct Promise *pp);
struct FileCopy GetCopyConstraints(struct Promise *pp);
struct FileLink GetLinkConstraints(struct Promise *pp);
struct Context GetContextConstraints(struct Promise *pp);
struct ProcessSelect GetProcessFilterConstraints(struct Promise *pp);
struct ProcessCount GetMatchesConstraints(struct Promise *pp);
struct EditDefaults GetEditDefaults(struct Promise *pp);
struct Attributes GetMethodAttributes(struct Promise *pp);
struct Attributes GetInterfacesAttributes(struct Promise *pp);
struct Attributes GetInsertionAttributes(struct Promise *pp);
struct EditLocation GetLocationAttributes(struct Promise *pp);
struct Attributes GetDeletionAttributes(struct Promise *pp);
struct Attributes GetColumnAttributes(struct Promise *pp);
struct Attributes GetReplaceAttributes(struct Promise *pp);
struct EditRegion GetRegionConstraints(struct Promise *pp);
struct EditReplace GetReplaceConstraints(struct Promise *pp);
struct EditColumn GetColumnConstraints(struct Promise *pp);
struct TopicAssociation GetAssociationConstraints(struct Promise *pp);
struct StorageMount GetMountConstraints(struct Promise *pp);
struct StorageVolume GetVolumeConstraints(struct Promise *pp);
struct CfTcpIp GetTCPIPAttributes(struct Promise *pp);
struct Report GetReportConstraints(struct Promise *pp);
struct LineSelect GetInsertSelectConstraints(struct Promise *pp);
struct LineSelect GetDeleteSelectConstraints(struct Promise *pp);
struct Measurement GetMeasurementConstraint(struct Promise *pp);
struct CfACL GetAclConstraints(struct Promise *pp);
struct CfDatabase GetDatabaseConstraints(struct Promise *pp);

/* bootstrap.c */

void CheckAutoBootstrap(void);
void SetPolicyServer(char *name);
void CreateFailSafe(char *name);
void SetDocRoot(char *name);

/* cfstream.c */

void CfFOut(char *filename,enum cfreport level,char *errstr,char *fmt, ...);
void CfOut(enum cfreport level, const char *errstr, const char *fmt, ...);
void cfPS(enum cfreport level,char status,char *errstr,struct Promise *pp,struct Attributes attr,char *fmt, ...);
void CfFile(FILE *fp,char *fmt, ...);
char *GetErrorStr(void);

/* cf_sql.c */

int CfConnectDB(CfdbConn *cfdb,enum cfdbtype dbtype,char *remotehost,char *dbuser, char *passwd, char *db);
void CfCloseDB(CfdbConn *cfdb);
void CfVoidQueryDB(CfdbConn *cfdb,char *query);
void CfNewQueryDB(CfdbConn *cfdb,char *query);
char **CfFetchRow(CfdbConn *cfdb);
char *CfFetchColumn(CfdbConn *cfdb,int col);
void CfDeleteQuery(CfdbConn *cfdb);

/* client_code.c */

void DetermineCfenginePort(void);
struct cfagent_connection *NewServerConnection(struct Attributes attr,struct Promise *pp);
struct cfagent_connection *ServerConnection(char *server,struct Attributes attr,struct Promise *pp);
void ServerDisconnection(struct cfagent_connection *conn);
int cf_remote_stat(char *file,struct stat *buf,char *stattype,struct Attributes attr,struct Promise *pp);
void DeleteClientCache(struct Attributes attr,struct Promise *pp);
int CompareHashNet(char *file1,char *file2,struct Attributes attr,struct Promise *pp);
int CopyRegularFileNet(char *source,char *new,off_t size,struct Attributes attr,struct Promise *pp);
int EncryptCopyRegularFileNet(char *source,char *new,off_t size,struct Attributes attr,struct Promise *pp);
int ServerConnect(struct cfagent_connection *conn,char *host,struct Attributes attr, struct Promise *pp);
void DestroyServerConnection(struct cfagent_connection *conn);

/* Only for OpenDirForPromise implementation */
CFDIR *OpenDirRemote(const char *dirname,struct Attributes attr,struct Promise *pp);

/* Mark connection as free */
void ServerNotBusy(struct cfagent_connection *conn);

/* client_protocols.c */

int IdentifyAgent(int sd,char *localip,int family);
int AuthenticateAgent(struct cfagent_connection *conn,struct Attributes attr,struct Promise *pp);
int BadProtoReply(char *buf);
int OKProtoReply(char *buf);
int FailedProtoReply(char *buf);


/* chflags.c */

int ParseFlagString (struct Rlist *flags, u_long *plusmask, u_long *minusmask);

/* communication.c */

struct cfagent_connection *NewAgentConn(void);
void DeleteAgentConn(struct cfagent_connection *ap);
void DePort(char *address);
int IsIPV6Address(char *name);
int IsIPV4Address(char *name);
const char *Hostname2IPString(const char *hostname);
char *IPString2Hostname(char *ipaddress);
int GetMyHostInfo(char nameBuf[MAXHOSTNAMELEN], char ipBuf[MAXIP4CHARLEN]);

/* comparray.c */

int FixCompressedArrayValue (int i, char *value, struct CompressedArray **start);
void DeleteCompressedArray (struct CompressedArray *start);
int CompressedArrayElementExists (struct CompressedArray *start, int key);
char *CompressedArrayValue (struct CompressedArray *start, int key);

/* constraints.c */

struct Constraint *AppendConstraint(struct Constraint **conlist,char *lval, void *rval, char type,char *classes,int body);
void DeleteConstraintList(struct Constraint *conlist);
void EditScalarConstraint(struct Constraint *conlist,char *lval,char *rval);
void *GetConstraint(char *lval,struct Promise *list,char type);
int GetBooleanConstraint(char *lval,struct Promise *list);
int GetRawBooleanConstraint(char *lval,struct Constraint *list);
int GetIntConstraint(char *lval,struct Promise *list);
double GetRealConstraint(char *lval,struct Promise *list);
mode_t GetOctalConstraint(char *lval,struct Promise *list);
uid_t GetUidConstraint(char *lval,struct Promise *pp);
gid_t GetGidConstraint(char *lval,struct Promise *pp);
struct Rlist *GetListConstraint(char *lval,struct Promise *list);
void ReCheckAllConstraints(struct Promise *pp);
int GetBundleConstraint(char *lval,struct Promise *list);
struct PromiseIdent *NewPromiseId(char *handle,struct Promise *pp);
void DeleteAllPromiseIds(void);

/* conversion.c */

char *EscapeJson(char *s, char *out, int outSz);
char *EscapeRegex(char *s, char *out, int outSz);
char *EscapeQuotes(char *s, char *out, int outSz);
char *MapAddress (char *addr);
void IPString2KeyDigest(char *ipv4,char *result);
enum cfhypervisors Str2Hypervisors(char *s);
enum cfenvironment_state Str2EnvState(char *s);
enum insert_match String2InsertMatch(char *s);
long Months2Seconds(int m);
enum cfinterval Str2Interval(char *s);
int SyslogPriority2Int(char *s);
enum cfdbtype Str2dbType(char *s);
char *Rlist2String(struct Rlist *list,char *sep);
int Signal2Int(char *s);
enum cfreport String2ReportLevel(char *typestr);
enum cfhashes String2HashType(char *typestr);
enum cfcomparison String2Comparison(char *s);
enum cflinktype String2LinkType(char *s);
enum cfdatatype Typename2Datatype(char *name);
enum cfdatatype GetControlDatatype(char *varname,struct BodySyntax *bp);
enum cfagenttype Agent2Type(char *name);
enum cfsbundle Type2Cfs(char *name);
enum representations String2Representation(char *s);
int GetBoolean(char *val);
long Str2Int(char *s);
long TimeCounter2Int(const char *s);
long TimeAbs2Int(char *s);
mode_t Str2Mode(char *s);
double Str2Double(char *s);
void IntRange2Int(char *intrange,long *min,long *max,struct Promise *pp);
int Month2Int(char *string);
int MonthLen2Int(char *string, int len);
void TimeToDateStr(time_t t, char *outStr, int outStrSz);
const char *GetArg0(const char *execstr);
void CommPrefix(char *execstr,char *comm);
int NonEmptyLine(char *s);
int Day2Number(char *datestring);
void UtcShiftInterval(time_t t, char *out, int outSz);
enum action_policy Str2ActionPolicy(char *s);
enum version_cmp Str2PackageSelect(char *s);
enum package_actions Str2PackageAction(char *s);
enum cf_acl_method Str2AclMethod(char *string);
enum cf_acl_type Str2AclType(char *string);
enum cf_acl_inherit Str2AclInherit(char *string);
enum cf_acl_inherit Str2ServicePolicy(char *string);
char *Dtype2Str(enum cfdatatype dtype);
char *Item2String(struct Item *ip);
int IsNumber(char *s);
int IsRealNumber(char *s);
enum cfd_menu String2Menu(char *s);

#ifndef MINGW
struct UidList *Rlist2UidList(struct Rlist *uidnames, struct Promise *pp);
struct GidList *Rlist2GidList(struct Rlist *gidnames, struct Promise *pp);
uid_t Str2Uid(char *uidbuff,char *copy,struct Promise *pp);
gid_t Str2Gid(char *gidbuff,char *copy,struct Promise *pp);
#endif  /* NOT MINGW */

/* crypto.c */

void KeepKeyPromises(void);
void DebugBinOut(char *buffer,int len,char *com);
void RandomSeed (void);
void LoadSecretKeys (void);
int EncryptString (char type,char *in, char *out, unsigned char *key, int len);
int DecryptString (char type,char *in, char *out, unsigned char *key, int len);
RSA *HavePublicKey (char *username,char *ipaddress,char *digest);
RSA *HavePublicKeyByIP(char *username,char *ipaddress);
void SavePublicKey (char *username,char *ipaddress,char *digest,RSA *key);
int RemovePublicKeys(const char *hostname);

/* dbm_api.c */

int OpenDB(char *filename, CF_DB **dbp);
int CloseDB(CF_DB *dbp);
int ValueSizeDB(CF_DB *dbp, char *key);
int ReadComplexKeyDB(CF_DB *dbp, char *key, int keySz,void *dest, int destSz);
int RevealDB(CF_DB *dbp, char *key, void **result, int *rsize);
int WriteComplexKeyDB(CF_DB *dbp, char *key,int keySz, const void *src, int srcSz);
int DeleteComplexKeyDB(CF_DB *dbp, char *key, int size);
int NewDBCursor(CF_DB *dbp,CF_DBC **dbcp);
int NextDB(CF_DB *dbp,CF_DBC *dbcp,char **key,int *ksize,void **value,int *vsize);
int DeleteDBCursor(CF_DB *dbp,CF_DBC *dbcp);
int ReadDB(CF_DB *dbp, char *key, void *dest, int destSz);
int WriteDB(CF_DB *dbp, char *key, const void *src, int srcSz);
int DeleteDB(CF_DB *dbp, char *key);
void OpenDBTransaction(CF_DB *dbp);
void CommitDBTransaction(CF_DB *dbp);
void CloseAllDB(void);

/* dbm_berkely.c */

#ifdef BDB  // FIXME
int BDB_OpenDB(char *filename,DB **dbp);
int BDB_CloseDB(DB *dbp);
int BDB_ValueSizeDB(DB *dbp, char *key);
int BDB_ReadComplexKeyDB(DB *dbp,char *name,int keysize,void *ptr,int size);
int BDB_RevealDB(DB *dbp,char *name,void **result,int *rsize);
int BDB_WriteComplexKeyDB(DB *dbp,char *name,int keysize, const void *ptr,int size);
int BDB_DeleteComplexKeyDB(DB *dbp,char *name,int size);
int BDB_NewDBCursor(CF_DB *dbp,CF_DBC **dbcp);
int BDB_NextDB(CF_DB *dbp,CF_DBC *dbcp,char **key,int *ksize,void **value,int *vsize);
int BDB_DeleteDBCursor(CF_DB *dbp,CF_DBC *dbcp);
#endif

/* dbm_quick.c */

#ifdef QDB  // FIXME
int QDB_OpenDB(char *filename, CF_QDB **qdbp);
int QDB_CloseDB(CF_QDB *qdbp);
int QDB_ValueSizeDB(CF_QDB *qdbp, char *key);
int QDB_ReadComplexKeyDB(CF_QDB *qdbp, char *key, int keySz,void *dest, int destSz);
int QDB_RevealDB(CF_QDB *qdbp, char *key, void **result, int *rsize);
int QDB_WriteComplexKeyDB(CF_QDB *qdbp, char *key, int keySz, const void *src, int srcSz);
int QDB_DeleteComplexKeyDB(CF_QDB *qdbp, char *key, int size);
int QDB_NewDBCursor(CF_QDB *qdbp,CF_QDBC **qdbcp);
int QDB_NextDB(CF_QDB *qdbp,CF_QDBC *qdbcp,char **key,int *ksize,void **value,int *vsize);
int QDB_DeleteDBCursor(CF_QDB *qdbp,CF_QDBC *qdbcp);
#endif

/* dbm_tokyocab.c */

#ifdef TCDB
int TCDB_OpenDB(char *filename, CF_TCDB **hdbp);
int TCDB_CloseDB(CF_TCDB *hdbp);
int TCDB_ValueSizeDB(CF_TCDB *hdbp, char *key);
int TCDB_ReadComplexKeyDB(CF_TCDB *hdbp, char *key, int keySz,void *dest, int destSz);
int TCDB_RevealDB(CF_TCDB *hdbp, char *key, void **result, int *rsize);
int TCDB_WriteComplexKeyDB(CF_TCDB *hdbp, char *key, int keySz, const void *src, int srcSz);
int TCDB_DeleteComplexKeyDB(CF_TCDB *hdbp, char *key, int size);
int TCDB_NewDBCursor(CF_TCDB *hdbp,CF_TCDBC **hdbcp);
int TCDB_NextDB(CF_TCDB *hdbp,CF_TCDBC *hdbcp,char **key,int *ksize,void **value,int *vsize);
int TCDB_DeleteDBCursor(CF_TCDB *hdbp,CF_TCDBC *hdbcp);
#endif

/* dir.c */

CFDIR *OpenDirForPromise(const char *dirname, struct Attributes attr, struct Promise *pp);
CFDIR *OpenDirLocal(const char *dirname);
const struct dirent *ReadDir(CFDIR *dir);
void CloseDir(CFDIR *dir);

/* Only for OpenDirRemote implementation */
struct dirent *AllocateDirentForFilename(const char *filename);

/* dtypes.c */

int IsSocketType(char *s);
int IsTCPType(char *s);


/* enterprise_stubs.c */

void WebCache(char *s,char *t);
void EnterpriseModuleTrick(void);
int CheckDatabaseSanity(struct Attributes a, struct Promise *pp);
void VerifyRegistryPromise(struct Attributes a,struct Promise *pp);
int CfSessionKeySize(char c);
char CfEnterpriseOptions(void);
const EVP_CIPHER *CfengineCipher(char type);
void Aggregate(char *stylesheet,char *banner,char *footer,char *webdriver);
void SetPolicyServer(char *name);
int IsEnterprise(void);
void EnterpriseContext(void);
char *GetProcessOptions(void);
int EnterpriseExpiry(void);
char *GetConsolePrefix(void);
char *MailSubject(void);
void CheckAutoBootstrap(void);
void CheckLicenses(void);
void RegisterBundleDependence(char *absscope,struct Promise *pp);
void MapPromiseToTopic(FILE *fp,struct Promise *pp,const char *version);
void ShowTopicRepresentation(FILE *fp);
void PreSanitizePromise(struct Promise *pp);
void Nova_ShowTopicRepresentation(FILE *fp);
void NoteEfficiency(double e);
void HistoryUpdate(struct Averages newvals);
void CfGetClassName(int i,char *name);
void LookUpClassName(int i,char *name);
void SummarizePromiseRepaired(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void SummarizePromiseNotKept(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void SummarizeCompliance(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void SummarizePerPromiseCompliance(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void SummarizeSetuid(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void SummarizeFileChanges(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void SummarizeValue(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void VerifyMeasurement(double *this,struct Attributes a,struct Promise *pp);
void SetMeasurementPromises(struct Item **classlist);
void LongHaul(void);
void VerifyACL(char *file,struct Attributes a, struct Promise *pp);
int CheckACLSyntax(char *file,struct CfACL acl,struct Promise *pp);
int CfVerifyTablePromise(CfdbConn *cfdb,char *name,struct Rlist *columns,struct Attributes a,struct Promise *pp);
void LogFileChange(char *file,int change,struct Attributes a,struct Promise *pp);
void RemoteSyslog(struct Attributes a,struct Promise *pp);
int VerifyDatabasePromise(CfdbConn *cfdb,char *database,struct Attributes a,struct Promise *pp);
int VerifyTablePromise(CfdbConn *cfdb,char *table,struct Rlist *columns,struct Attributes a,struct Promise *pp);
void ReportPatches(struct CfPackageManager *list);
void SummarizeSoftware(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void SummarizeUpdates(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void VerifyServices(struct Attributes a,struct Promise *pp);
void LoadSlowlyVaryingObservations(void);
void MonOtherInit(void);
void MonOtherGatherData(double *cf_this);
void RegisterLiteralServerData(char *handle,struct Promise *pp);
int ReturnLiteralData(char *handle,char *ret);
char *GetRemoteScalar(char *proto,char *handle,char *server,int encrypted,char *rcv);
char *PromiseID(struct Promise *pp);
void NotePromiseCompliance(struct Promise *pp,double val,enum cf_status status,char *reasoin);
time_t GetPromiseCompliance(struct Promise *pp,double *value,double *average,double *var,time_t *lastseen);
void SyntaxCompletion(char *s);
void SyntaxExport(void);
int GetRegistryValue(char *key,char *name,char *buf, int bufSz);
void NoteVarUsage(void);
void NoteVarUsageDB(void);
void SummarizeVariables(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web);
void CSV2XML(struct Rlist *list);
void *CfLDAPValue(char *uri,char *dn,char *filter,char *name,char *scope,char *sec);
void *CfLDAPList(char *uri,char *dn,char *filter,char *name,char *scope,char *sec);
void *CfLDAPArray(char *array,char *uri,char *dn,char *filter,char *scope,char *sec);
void *CfRegLDAP(char *uri,char *dn,char *filter,char *name,char *scope,char *regex,char *sec);
void CacheUnreliableValue(char *caller,char *handle,char *buffer);
int RetrieveUnreliableValue(char *caller,char *handle,char *buffer);
void TranslatePath(char *new,const char *old);
void GrandSummary(void);
void TrackValue(char *date,double kept,double repaired, double notkept);
void SetBundleOutputs(char *name);
void ResetBundleOutputs(char *name);
void SetPromiseOutputs(struct Promise *pp);
void VerifyOutputsPromise(struct Promise *pp);
void SpecialQuote(char *topic,char *type);
void LastSawBundle(char *name);
int GetInstalledPkgsRpath(struct CfPackageItem **pkgList, struct Attributes a, struct Promise *pp);
int ExecPackageCommandRpath(char *command,int verify,int setCmdClasses,struct Attributes a,struct Promise *pp);
int ForeignZone(char *s);
void NewPromiser(struct Promise *pp);
void AnalyzePromiseConflicts(void);
void AddGoalsToDB(char *goal_patterns, char *goal_categories);
/* env_context.c */

/* - Parsing/evaluating expressions - */
void ValidateClassSyntax(const char *str);
bool IsDefinedClass(const char *class);
bool IsExcluded(const char *exception);

bool EvalProcessResult(const char *process_result, struct AlphaList *proc_attr);
bool EvalFileResult(const char *file_result, struct AlphaList *leaf_attr);

/* - Rest - */
int Abort(void);
void KeepClassContextPromise(struct Promise *pp);
void PushPrivateClassContext(void);
void PopPrivateClassContext(void);
void DeletePrivateClassContext(void);
void DeleteEntireHeap(void);
void NewPersistentContext(char *name,unsigned int ttl_minutes,enum statepolicy policy);
void DeletePersistentContext(char *name);
void LoadPersistentContext(void);
void AddEphemeralClasses(struct Rlist *classlist);
void NewClass(const char *oclass); /* Copies oclass */
void NewBundleClass(char *class,char *bundle);
struct Rlist *SplitContextExpression(char *context,struct Promise *pp);
void DeleteClass(char *class);
int VarClassExcluded(struct Promise *pp,char **classes);
void NewClassesFromString(char *classlist);
void NegateClassesFromString(char *class,struct Item **heap);
void AddPrefixedClasses(char *name,char *classlist);
int IsHardClass (char *sp);
void SaveClassEnvironment(void);

/* env_monitor.c */

void MonInitialize(void);
void StartServer (int argc, char **argv);

/* evalfunction.c */

struct Rval CallFunction(FnCallType *function, struct FnCall *fp, struct Rlist *finalargs);

void *CfReadFile(char *filename,int maxsize);
void ModuleProtocol(char *command,char *line,int print);

/* expand.c */

void ExpandPromise(enum cfagenttype ag,char *scopeid,struct Promise *pp,void *fnptr);
void ExpandPromiseAndDo(enum cfagenttype ag,char *scope,struct Promise *p,struct Rlist *scalarvars,struct Rlist *listvars,void (*fnptr)());
struct Rval ExpandDanglers(char *scope,struct Rval rval,struct Promise *pp);
void ScanRval(char *scope,struct Rlist **los,struct Rlist **lol,void *string,char type,struct Promise *pp);

int IsExpandable(const char *str);
int ExpandScalar(const char *string, char buffer[CF_EXPANDSIZE]);
int ExpandThis(enum cfreport level,char *string,char buffer[CF_EXPANDSIZE]);
struct Rval ExpandBundleReference(char *scopeid,void *rval,char type);
struct FnCall *ExpandFnCall(char *contextid,struct FnCall *f,int expandnaked);
struct Rval ExpandPrivateRval(char *contextid,void *rval,char type);
struct Rlist *ExpandList(char *scopeid,struct Rlist *list,int expandnaked);
struct Rval EvaluateFinalRval(char *scopeid,void *rval,char rtype,int forcelist,struct Promise *pp);
int IsNakedVar(char *str,char vtype);
void GetNaked(char *s1, char *s2);
void ConvergeVarHashPromise(char *scope,struct Promise *pp,int checkdup);

/* exec_tool.c */

int IsExecutable(const char *file);
int ShellCommandReturnsZero(char *comm,int useshell);
int GetExecOutput(char *command,char *buffer,int useshell);
void ActAsDaemon(int preserve);
char *ShEscapeCommand(char *s);
int ArgSplitCommand(char *comm,char arg[CF_MAXSHELLARGS][CF_BUFSIZE]);

/* files_copy.c */

void *CopyFileSources(char *destination,struct Attributes attr,struct Promise *pp);
int CopyRegularFileDisk(char *source,char *new,struct Attributes attr,struct Promise *pp);
void CheckForFileHoles(struct stat *sstat,struct Promise *pp);
int FSWrite(char *new,int dd,char *buf,int towrite,int *last_write_made_hole,int n_read,struct Attributes attr,struct Promise *pp);

/* files_edit.c */

struct edit_context *NewEditContext(char *filename,struct Attributes a,struct Promise *pp);
void FinishEditContext(struct edit_context *ec,struct Attributes a,struct Promise *pp);
int LoadFileAsItemList(struct Item **liststart,char *file,struct Attributes a,struct Promise *pp);
int SaveItemListAsFile(struct Item *liststart,char *file,struct Attributes a,struct Promise *pp);
int AppendIfNoSuchLine(char *filename, char *line);

/* files_editline.c */

int ScheduleEditLineOperations(char *filename,struct Bundle *bp,struct Attributes a,struct Promise *pp);

/* files_links.c */

char VerifyLink(char *destination,char *source,struct Attributes attr,struct Promise *pp);
char VerifyAbsoluteLink(char *destination,char *source,struct Attributes attr,struct Promise *pp);
char VerifyRelativeLink(char *destination,char *source,struct Attributes attr,struct Promise *pp);
char VerifyHardLink(char *destination,char *source,struct Attributes attr,struct Promise *pp);
int KillGhostLink(char *name,struct Attributes attr,struct Promise *pp);
int MakeHardLink (char *from,char *to,struct Attributes attr,struct Promise *pp);
int ExpandLinks(char *dest,char *from,int level);

/* files_hashes.c */

int FileHashChanged(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1],int warnlevel,enum cfhashes type,struct Attributes attr,struct Promise *pp);
void PurgeHashes(char *file,struct Attributes attr,struct Promise *pp);
void DeleteHash(CF_DB *dbp,enum cfhashes type,char *name);
struct Checksum_Value *NewHashValue(unsigned char digest[EVP_MAX_MD_SIZE+1]);
int CompareFileHashes(char *file1,char *file2,struct stat *sstat,struct stat *dstat,struct Attributes attr,struct Promise *pp);
int CompareBinaryFiles(char *file1,char *file2,struct stat *sstat,struct stat *dstat,struct Attributes attr,struct Promise *pp);
void HashFile(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type);
void HashString(char *buffer,int len,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type);
int HashesMatch(unsigned char digest1[EVP_MAX_MD_SIZE+1],unsigned char digest2[EVP_MAX_MD_SIZE+1],enum cfhashes type);
char *HashPrint(enum cfhashes type,unsigned char digest[EVP_MAX_MD_SIZE+1]);
char *HashPrintSafe(enum cfhashes type,unsigned char digest[EVP_MAX_MD_SIZE+1], char buffer[EVP_MAX_MD_SIZE*4]);
char *FileHashName(enum cfhashes id);
void HashPubKey(RSA *key,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type);

/* files_interfaces.c */

void SourceSearchAndCopy(char *from,char *to,int maxrecurse,struct Attributes attr,struct Promise *pp);
void VerifyCopy(char *source,char *destination,struct Attributes attr,struct Promise *pp);
void LinkCopy(char *sourcefile,char *destfile,struct stat *sb,struct Attributes attr, struct Promise *pp);
int cfstat(const char *path, struct stat *buf);
int cf_stat(char *file,struct stat *buf,struct Attributes attr, struct Promise *pp);
int cf_lstat(char *file,struct stat *buf,struct Attributes attr, struct Promise *pp);
struct cfdirent *cf_readdir(CFDIR *cfdirh,struct Attributes attr, struct Promise *pp);
int CopyRegularFile(char *source,char *dest,struct stat sstat,struct stat dstat,struct Attributes attr, struct Promise *pp);
int CfReadLine(char *buff,int size,FILE *fp);
int cf_readlink(char *sourcefile,char *linkbuf,int buffsize,struct Attributes attr, struct Promise *pp);

/* files_names.c */

int IsNewerFileTree(char *dir,time_t reftime);
char *Titleize (char *str);
int DeEscapeQuotedString(const char *in, char *out);
int CompareCSVName(char *s1,char *s2);
int IsDir(char *path);
int EmptyString(char *s);
char *JoinPath(char *path, const char *leaf);
char *JoinSuffix(char *path,char *leaf);
int JoinMargin(char *path, const char *leaf, char **nextFree, int bufsize, int margin);
int StartJoin(char *path,char *leaf,int bufsize);
int Join(char *path, const char *leaf, int bufsize);
int JoinSilent(char *path, const char *leaf, int bufsize);
int EndJoin(char *path,char *leaf,int bufsize);
int IsAbsPath(char *path);
void AddSlash(char *str);
void DeleteSlash(char *str);
const char *LastFileSeparator(const char *str);
int ChopLastNode(char *str);
char *CanonifyName(const char *str);
void CanonifyNameInPlace(char *str);
char *CanonifyChar(const char *str,char ch);
const char *ReadLastNode(const char *str);
int CompressPath(char *dest,char *src);
void Chop(char *str);
void StripTrailingNewline(char *str);
bool IsStrIn(const char *str, const char **strs);
bool IsStrCaseIn(const char *str, const char **strs);
void FreeStringArray(char **strs);
int IsAbsoluteFileName(const char *f);
bool IsFileOutsideDefaultRepository(const char *f);
int RootDirLength(char *f);
char ToLower (char ch);
char ToUpper (char ch);
char *ToUpperStr (char *str);
char *ToLowerStr (char *str);
int SubStrnCopyChr(char *to,char *from,int len,char sep);
int CountChar(char *string,char sp);
void ReplaceChar(char *in, char *out, int outSz, char from, char to);
void ReplaceTrailingChar(char *str, char from, char to);
void ReplaceTrailingStr(char *str, char *from, char to);
int ReplaceStr(char *in, char *out, int outSz, char* from, char *to);
    
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
void *ThreadUniqueName(pthread_t tid);
#endif  /* HAVE PTHREAD */

/* files_operators.c */

int VerifyFileLeaf(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp);
int CfCreateFile(char *file,struct Promise *pp,struct Attributes attr);
FILE *CreateEmptyStream(void);
int ScheduleCopyOperation(char *destination,struct Attributes attr,struct Promise *pp);
int ScheduleLinkChildrenOperation(char *destination,char *source,int rec,struct Attributes attr,struct Promise *pp);
int ScheduleLinkOperation(char *destination,char *source,struct Attributes attr,struct Promise *pp);
int ScheduleEditOperation(char *filename,struct Attributes attr,struct Promise *pp);
struct FileCopy *NewFileCopy(struct Promise *pp);
void VerifyFileAttributes(char *file,struct stat *dstat,struct Attributes attr,struct Promise *pp);
void VerifyFileIntegrity(char *file,struct Attributes attr,struct Promise *pp);
int VerifyOwner(char *file,struct Promise *pp,struct Attributes attr,struct stat *statbuf);
void VerifyCopiedFileAttributes(char *file,struct stat *dstat,struct stat *sstat,struct Attributes attr,struct Promise *pp);
int MoveObstruction(char *from,struct Attributes attr,struct Promise *pp);
void TouchFile(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp);
int MakeParentDirectory(char *parentandchild,int force);
void RotateFiles(char *name,int number);
void CreateEmptyFile(char *name);
void VerifyFileChanges(char *file,struct stat *sb,struct Attributes attr,struct Promise *pp);
#ifndef MINGW
struct UidList *MakeUidList(char *uidnames);
struct GidList *MakeGidList(char *gidnames);
void AddSimpleUidItem(struct UidList **uidlist,uid_t uid,char *uidname);
void AddSimpleGidItem(struct GidList **gidlist,gid_t gid,char *gidname);
#endif  /* NOT MINGW */
void LogHashChange(char *file);

/* files_properties.c */

int ConsiderFile(const char *nodename,char *path,struct Attributes attr,struct Promise *pp);
void SetSearchDevice(struct stat *sb,struct Promise *pp);
int DeviceBoundary(struct stat *sb,struct Promise *pp);

/* files_repository.c */

int ArchiveToRepository(char *file,struct Attributes attr,struct Promise *pp);

/* files_select.c */

int SelectLeaf(char *path,struct stat *sb,struct Attributes attr,struct Promise *pp);
int GetOwnerName(char *path, struct stat *lstatptr, char *owner, int ownerSz);

/* fncall.c */

int IsBuiltinFnCall(void *rval,char rtype);
struct FnCall *NewFnCall(char *name, struct Rlist *args);
struct FnCall *CopyFnCall(struct FnCall *f);
int PrintFnCall(char *buffer, int bufsize,struct FnCall *fp);
void DeleteFnCall(struct FnCall *fp);
void ShowFnCall(FILE *fout,struct FnCall *fp);
struct Rval EvaluateFunctionCall(struct FnCall *fp,struct Promise *pp);
enum cfdatatype FunctionReturnType(const char *name);
FnCallType *FindFunction(const char *name);
void SetFnCallReturnStatus(char *fname,int status,char *message,char *fncall_classes);

/* generic_agent.c */

void GenericInitialize(int argc,char **argv,char *agents);
void GenericDeInitialize(void);
void InitializeGA(int argc,char **argv);
void CheckOpts(int argc,char **argv);
void Syntax(const char *comp, const struct option options[], const char *hints[], const char *id);
void ManPage(const char *component, const struct option options[], const char *hints[], const char *id);
void PrintVersionBanner(const char *component);
int CheckPromises(enum cfagenttype ag);
void ReadPromises(enum cfagenttype ag,char *agents);
int NewPromiseProposals(void);
void CompilationReport(char *filename);
void HashVariables(char *name);
void HashControls(void);
void Cf3CloseLog(void);
struct Constraint *ControlBodyConstraints(enum cfagenttype agent);
void SetFacility(const char *retval);
struct Bundle *GetBundle(char *name,char *agent);
struct SubType *GetSubTypeForBundle(char *type,struct Bundle *bp);
void CheckBundleParameters(char *scope,struct Rlist *args);
void PromiseBanner(struct Promise *pp);
void BannerBundle(struct Bundle *bp,struct Rlist *args);
void BannerSubBundle(struct Bundle *bp,struct Rlist *args);
void WritePID(char *filename);
void OpenCompilationReportFiles(const char *fname);

/* granules.c  */

char *ConvTimeKey (char *str);
char *GenTimeKey (time_t now);
int GetTimeSlot(time_t here_and_now);
int GetShiftSlot(time_t here_and_now);
time_t GetShiftSlotStart(time_t t);

/* hashes.c */

int RefHash(char *name);
int ElfHash(char *key);
int OatHash(const char *key);
void InitHashes(struct CfAssoc **table);
void CopyHashes(struct CfAssoc **newhash,struct CfAssoc **oldhash);
int GetHash(const char *name);
void PrintHashes(FILE *sp,struct CfAssoc **table,int html);
void DeleteHashes(struct CfAssoc **hashtable);

/* Deletes element from hashtable, returning whether element was found */
bool HashDeleteElement(CfAssoc **hashtable, const char *element);
/* Looks up element in hashtable, returns NULL if not found */
CfAssoc *HashLookupElement(CfAssoc **hashtable, const char *element);
/* Clear whole hash table */
void HashClear(CfAssoc **hashtable);
/* Insert element if it does not exist in hash table. Returns false if element
   already exists in table or if table is full. */
bool HashInsertElement(CfAssoc **hashtable, const char *element,
                       void *rval, char rtype, enum cfdatatype dtype);

/* Hash table iterators: call HashIteratorNext() until it returns NULL */
HashIterator HashIteratorInit(CfAssoc **hashtable);
CfAssoc *HashIteratorNext(HashIterator *iterator);

/* html.c */

void CfHtmlHeader(FILE *fp,char *title,char *css,char *webdriver,char *banner);
void CfHtmlFooter(FILE *fp,char *footer);
void CfHtmlTitle(FILE *fp,char *title);
int IsHtmlHeader(char *s);

/* item-lib.c */

void PrependFullItem(struct Item **liststart,char *itemstring,char *classes,int counter,time_t t);
void PurgeItemList(struct Item **list,char *name);
struct Item *ReturnItemIn(struct Item *list,char *item);
struct Item *ReturnItemInClass(struct Item *list,char *item,char *classes);
struct Item *ReturnItemAtIndex(struct Item *list,int index);
int GetItemIndex(struct Item *list,char *item);
struct Item *EndOfList(struct Item *start);
int IsItemInRegion(char *item,struct Item *begin,struct Item *end,struct Attributes a,struct Promise *pp);
void PrependItemList(struct Item **liststart,char *itemstring);
int SelectItemMatching(struct Item *s,char *regex,struct Item *begin,struct Item *end,struct Item **match,struct Item **prev,char *fl);
int SelectNextItemMatching(char *regexp,struct Item *begin,struct Item *end,struct Item **match,struct Item **prev);
int SelectLastItemMatching(char *regexp,struct Item *begin,struct Item *end,struct Item **match,struct Item **prev);
void InsertAfter(struct Item **filestart,struct Item *ptr,char *string);
int NeighbourItemMatches(struct Item *start,struct Item *location,char *string,enum cfeditorder pos,struct Attributes a,struct Promise *pp);
int RawSaveItemList(struct Item *liststart, char *file);
struct Item *SplitStringAsItemList(char *string,char sep);
struct Item *SplitString(char *string,char sep);
int DeleteItemGeneral (struct Item **filestart, char *string, enum matchtypes type);
int DeleteItemLiteral (struct Item **filestart, char *string);
int DeleteItemStarting (struct Item **list,char *string);
int DeleteItemNotStarting (struct Item **list,char *string);
int DeleteItemMatching (struct Item **list,char *string);
int DeleteItemNotMatching (struct Item **list,char *string);
int DeleteItemContaining (struct Item **list,char *string);
int DeleteItemNotContaining (struct Item **list,char *string);
int CompareToFile(struct Item *liststart,char *file,struct Attributes a,struct Promise *pp);
struct Item *String2List(char *string);
int ListLen (struct Item *list);
int ByteSizeList(const struct Item *list);
int IsItemIn (struct Item *list, const char *item);
int IsMatchItemIn(struct Item *list,char *item);
struct Item *ConcatLists (struct Item *list1, struct Item *list2);
void CopyList(struct Item **dest,struct Item *source);
int FuzzySetMatch(char *s1, char *s2);
int FuzzyMatchParse(char *item);
int FuzzyHostMatch(char *arg0, char *arg1,char *basename);
int FuzzyHostParse(char *arg1,char *arg2);
void IdempItemCount(struct Item **liststart,char *itemstring,char *classes);
struct Item *IdempPrependItem(struct Item **liststart,char *itemstring,char *classes);
struct Item *IdempPrependItemClass(struct Item **liststart,char *itemstring,char *classes);
struct Item *PrependItem(struct Item **liststart, const char *itemstring, const char *classes);
void AppendItem(struct Item **liststart, const char *itemstring, char *classes);
void DeleteItemList (struct Item *item);
void DeleteItem (struct Item **liststart, struct Item *item);
void DebugListItemList (struct Item *liststart);
struct Item *SplitStringAsItemList (char *string, char sep);
void IncrementItemListCounter (struct Item *ptr, char *string);
void SetItemListCounter (struct Item *ptr, char *string,int value);
struct Item *SortItemListNames(struct Item *list);
struct Item *SortItemListClasses(struct Item *list);
struct Item *SortItemListCounters(struct Item *list);
struct Item *SortItemListTimes(struct Item *list);
char *ItemList2CSV(struct Item *list);
int ItemListSize(struct Item *list);
int MatchRegion(char *chunk,struct Item *location,struct Item *begin,struct Item *end);
struct Item *DeleteRegion(struct Item **liststart,struct Item *begin,struct Item *end);

/* iteration.c */

struct Rlist *NewIterationContext(char *scopeid,struct Rlist *listvars);
void DeleteIterationContext(struct Rlist *lol);
int IncrementIterationContext(struct Rlist *iterators,int count);
int EndOfIteration(struct Rlist *iterator);
int NullIterators(struct Rlist *iterator);

/* instrumentation.c */

struct timespec BeginMeasure(void);
void EndMeasure(char *eventname,struct timespec start);
void EndMeasurePromise(struct timespec start,struct Promise *pp);
void NoteClassUsage(struct AlphaList list);
void LastSaw(char *username,char *ipaddress,unsigned char digest[EVP_MAX_MD_SIZE+1],enum roles role);
double GAverage(double anew,double aold,double p);
bool RemoveHostFromLastSeen(const char *hostname, char *hostkey);

/* install.c */

int RelevantBundle(char *agent,char *blocktype);
struct Bundle *AppendBundle(struct Bundle **start,char *name, char *type, struct Rlist *args);
struct Body *AppendBody(struct Body **start,char *name, char *type, struct Rlist *args);
struct SubType *AppendSubType(struct Bundle *bundle,char *typename);
struct Promise *AppendPromise(struct SubType *type,char *promiser, void *promisee,char petype,char *classes,char *bundle,char *bundletype);
void DeleteBundles(struct Bundle *bp);
void DeleteBodies(struct Body *bp);

/* interfaces.c */

void VerifyInterfacePromise(char *vifdev,char *vaddress,char *vnetmask,char *vbroadcast);

/* keyring.c */

int HostKeyAddressUnknown(char *value);

/* logging.c */

void BeginAudit(void);
void EndAudit(void);
void ClassAuditLog(struct Promise *pp,struct Attributes attr,char *str,char status,char *error);
void PromiseLog(char *s);
void FatalError(char *s, ...) FUNC_ATTR_NORETURN;
void AuditStatusMessage(FILE*fp,char status);

/* manual.c */

void TexinfoManual(char *mandir);

/* matching.c */

bool ValidateRegEx(const char *regex);
int FullTextMatch (char *regptr, const char *cmpptr);
char *ExtractFirstReference(char *regexp, const char *teststring);
int BlockTextMatch (char *regexp,char *teststring,int *s,int *e);
int IsRegexItemIn(struct Item *list,char *regex);
int IsPathRegex(char *str);
int IsRegex(char *str);
int MatchRlistItem(struct Rlist *listofregex,const char *teststring);
void EscapeSpecialChars(char *str, char *strEsc, int strEscSz, char *noEsc);
char *EscapeChar(char *str, int strSz, char esc);
void AnchorRegex(char *regex, char *out, int outSz);
int MatchPolicy(char *needle,char *haystack,struct Attributes a,struct Promise *pp);

/* mod_defaults.c */

char *GetControlDefault(char *bodypart);
char *GetBodyDefault(char *bodypart);

/* modes.c */

int ParseModeString (char *modestring, mode_t *plusmask, mode_t *minusmask);

/* net.c */

int SendTransaction (int sd, char *buffer,int len, char status);
int ReceiveTransaction (int sd, char *buffer,int *more);
int RecvSocketStream (int sd, char *buffer, int toget, int nothing);
int SendSocketStream (int sd, char *buffer, int toget, int flags);

/* nfs.c */

#ifndef MINGW
int LoadMountInfo(struct Rlist **list);
void DeleteMountInfo(struct Rlist *list);
int VerifyNotInFstab(char *name,struct Attributes a,struct Promise *pp);
int VerifyInFstab(char *name,struct Attributes a,struct Promise *pp);
int VerifyMount(char *name,struct Attributes a,struct Promise *pp);
int VerifyUnmount(char *name,struct Attributes a,struct Promise *pp);
void MountAll(void);
#endif  /* NOT MINGW */

/* ontology.c */

void AddInference(struct Inference **list,char *result,char *pre,char *qual);
struct Topic *IdempInsertTopic(char *classified_name);
struct Topic *InsertTopic(char *name,char *context);
struct Topic *FindTopic(char *name);
int GetTopicPid(char *typed_topic);
struct Topic *AddTopic(struct Topic **list,char *name,char *type);
void AddTopicAssociation(struct Topic *tp,struct TopicAssociation **list,char *fwd_name,char *bwd_name,struct Rlist *li,int ok,char *from_context,char *from_topic);
void AddOccurrence(struct Occurrence **list,char *reference,struct Rlist *represents,enum representations rtype,char *context);
struct Topic *TopicExists(char *topic_name,char *topic_type);
struct Topic *GetCanonizedTopic(struct Topic *list,char *topic_name);
struct Topic *GetTopic(struct Topic *list,char *topic_name);
struct TopicAssociation *AssociationExists(struct TopicAssociation *list,char *fwd,char *bwd);
struct Occurrence *OccurrenceExists(struct Occurrence *list,char *locator,enum representations repy_type,char *s);
void DeClassifyTopic(char *typdetopic,char *topic,char *type);

/* patches.c */

int IsPrivileged (void);
char *StrStr (char *s1,char *s2);
int StrnCmp (char *s1,char *s2,size_t n);
int cf_strcmp(const char *s1, const char *s2);
int cf_strncmp(const char *s1,const char *s2, size_t n);
char *cf_strcpy(char *s1, const char *s2);
char *cf_strncpy(char *s1, const char *s2, size_t n);
char *cf_strdup(const char *s);
int cf_strlen(const char *s);
char *cf_strchr(const char *s, int c);
char *MapName(char *s);
char *MapNameForward(char *s);
char *cf_ctime(const time_t *timep);
char *cf_strtimestamp_local(const time_t time, char *buf);
char *cf_strtimestamp_utc(const time_t time, char *buf);
int cf_closesocket(int sd);
int cf_mkdir(const char *path, mode_t mode);
int cf_chmod(const char *path, mode_t mode);
int cf_rename(const char *oldpath, const char *newpath);
void OpenNetwork(void);
void CloseNetwork(void);
void CloseWmi(void);
int LinkOrCopy(const char *from, const char *to, int sym);

/* pipes.c */

FILE *cf_popen(char *command,char *type);
FILE *cf_popensetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv,int background);
FILE *cf_popen_sh(char *command,char *type);
FILE *cf_popen_shsetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv,int background);
int cf_pclose(FILE *pp);
int cf_pclose_def(FILE *pfp,struct Attributes a,struct Promise *pp);
int VerifyCommandRetcode(int retcode, int fallback, struct Attributes a, struct Promise *pp);

#ifndef MINGW
int cf_pwait(pid_t pid);
#endif  /* NOT MINGW */

/* processes_select.c */

int SelectProcess(char *procentry,char **names,int *start,int *end,struct Attributes a,struct Promise *pp);
bool IsProcessNameRunning(char *procNameRegex);


/* promises.c */

char *BodyName(struct Promise *pp);
struct Body *IsBody(struct Body *list,char *key);
struct Bundle *IsBundle(struct Bundle *list,char *key);
struct Promise *DeRefCopyPromise(char *scopeid,struct Promise *pp);
struct Promise *ExpandDeRefPromise(char *scopeid,struct Promise *pp);
struct Promise *CopyPromise(char *scopeid,struct Promise *pp);
void DeletePromise(struct Promise *pp);
void DeletePromises(struct Promise *pp);
void PromiseRef(enum cfreport level,struct Promise *pp);
struct Promise *NewPromise(char *typename,char *promiser);
void HashPromise(char *salt,struct Promise *pp,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type);

/* recursion.c */

int DepthSearch(char *name,struct stat *sb,int rlevel,struct Attributes attr,struct Promise *pp);
int SkipDirLinks(char *path,const char *lastnode,struct Recursion r);

/* reporting.c */

void ShowContext(void);
void ShowPromises(struct Bundle *bundles,struct Body *bodies);
void ShowPromise(struct Promise *pp, int indent);
void ShowScopedVariables(void);
void SyntaxTree(void);
void ShowBody(struct Body *body,int ident);
void DebugBanner(char *s);
void ReportError(char *s);
void BannerSubType(char *bundlename,char *type,int p);
void BannerSubSubType(char *bundlename,char *type);
void Banner(char *s);
void ShowPromisesInReport(struct Bundle *bundles, struct Body *bodies);
void ShowPromiseInReport(const char *version, struct Promise* pp, int indent);

/* rlist.c */

int PrintRval(char *buffer,int bufsize,void *item,char type);
int PrintRlist(char *buffer,int bufsize,struct Rlist *list);
int GetStringListElement(char *strList, int index, char *outBuf, int outBufSz);
int StripListSep(char *strList, char *outBuf, int outBufSz);
struct Rlist *ParseShownRlist(char *string);
int IsStringIn(struct Rlist *list,char *s);
int IsIntIn(struct Rlist *list,int i);
struct Rlist *KeyInRlist(struct Rlist *list,char *key);
int RlistLen(struct Rlist *start);
void PopStack(struct Rlist **liststart, void **item,size_t size);
void PushStack(struct Rlist **liststart,void *item);
int IsInListOfRegex(struct Rlist *list,char *str);

void *CopyRvalItem(void *item, char type);
void DeleteRvalItem(void *rval, char type);
struct Rlist *CopyRlist(struct Rlist *list);
int CompareRval(void *rval1, char rtype1, void *rval2, char rtype2);
void DeleteRlist(struct Rlist *list);
void DeleteRlistEntry(struct Rlist **liststart,struct Rlist *entry);
struct Rlist *AppendRlistAlien(struct Rlist **start,void *item);
struct Rlist *PrependRlistAlien(struct Rlist **start,void *item);
struct Rlist *OrthogAppendRlist(struct Rlist **start,void *item, char type);
struct Rlist *IdempAppendRScalar(struct Rlist **start,void *item, char type);
struct Rlist *AppendRScalar(struct Rlist **start,void *item, char type);
struct Rlist *IdempAppendRlist(struct Rlist **start,void *item, char type);
struct Rlist *IdempPrependRScalar(struct Rlist **start,void *item, char type);
struct Rlist *PrependRScalar(struct Rlist **start,void *item, char type);
struct Rlist *PrependRlist(struct Rlist **start,void *item, char type);
struct Rlist *AppendRlist(struct Rlist **start,void *item, char type);
struct Rlist *PrependRlist(struct Rlist **start,void *item, char type);
struct Rlist *SplitStringAsRList(char *string,char sep);
struct Rlist *SplitRegexAsRList(char *string,char *regex,int max,int purge);
struct Rlist *SortRlist(struct Rlist *list, int (*CompareItems)());
struct Rlist *AlphaSortRListNames(struct Rlist *list);

void ShowRlist(FILE *fp,struct Rlist *list);
void ShowRval(FILE *fp,void *rval,char type);

int PrependListPackageItem(struct CfPackageItem **list,char *item,struct Attributes a,struct Promise *pp);
int PrependPackageItem(struct CfPackageItem **list,char *name,char *version,char* arch,struct Attributes a,struct Promise *pp);



/* scope.c */

void SetScope(char *id);
void SetNewScope(char *id);
void NewScope(char *name);
void DeleteScope(char *name);
struct Scope *GetScope(char *scope);
void CopyScope(char *new, char *old);
void DeleteAllScope(void);
void AugmentScope(char *scope,struct Rlist *lvals,struct Rlist *rvals);
void DeleteFromScope(char *scope,struct Rlist *args);
void PushThisScope(void);
void PopThisScope(void);
void ShowScope(char *name);

/* selfdiagnostic.c */

void SelfDiagnostic(void);
void TestVariableScan(void);
void TestExpandPromise(void);
void TestExpandVariables(void);

/* server_transform.c */

void KeepPromiseBundles(void);
void KeepControlPromises(void);
struct Auth *GetAuthPath(char *path,struct Auth *list);
void Summarize(void);

/* signals.c */

void HandleSignals(int signum);
void SelfTerminatePrelude(void);

/* sockaddr.c */

/* Not thread-safe */
char *sockaddr_ntop (struct sockaddr *sa);

/* Thread-safe. Returns boolean success.
   It's up to caller to provide large enough addr. */
bool sockaddr_pton (int af,void *src, void *addr);

/* storage_tools.c */

off_t GetDiskUsage(char *file, enum cfsizes type);

/* syntax.c */

int LvalWantsBody(char *stype,char *lval);
int CheckParseVariableName(char *name);
void CheckBundle(char *name,char *type);
void CheckBody(char *name,char *type);
struct SubTypeSyntax CheckSubType(char *btype,char *type);
void CheckConstraint(char *type,char *name,char *lval,void *rval,char rvaltype,struct SubTypeSyntax ss);
void CheckSelection(char *type,char *name,char *lval,void *rval,char rvaltype);
void CheckConstraintTypeMatch(char *lval,void *rval,char rvaltype,enum cfdatatype dt,char *range,int level);
int CheckParseClass(char *lv,char *s,char *range);
enum cfdatatype StringDataType(char *scopeid,char *string);
enum cfdatatype ExpectedDataType(char *lvalname);

/* sysinfo.c */

void GetNameInfo3(void);
void CfGetInterfaceInfo(enum cfagenttype ag);
void Get3Environment(void);
void BuiltinClasses(void);
void OSClasses(void);
void SetSignals(void);
int IsInterfaceAddress(char *adr);
int GetCurrentUserName(char *userName, int userNameLen);
#ifndef MINGW
void Unix_GetInterfaceInfo(enum cfagenttype ag);
void Unix_FindV6InterfaceInfo(void);
char *GetHome(uid_t uid);
#endif  /* NOT MINGW */

/* transaction.c */

void SummarizeTransaction(struct Attributes attr,struct Promise *pp,char *logname);
struct CfLock AcquireLock(char *operand,char *host,time_t now,struct Attributes attr,struct Promise *pp, int ignoreProcesses);
void YieldCurrentLock(struct CfLock this);
void GetLockName(char *lockname,char *locktype,char *base,struct Rlist *params);
int ThreadLock(enum cf_thread_mutex name);
int ThreadUnlock(enum cf_thread_mutex name);
void AssertThreadLocked(enum cf_thread_mutex name, char *fname);
void PurgeLocks(void);
int ShiftChange(void);

/* timeout.c */

void SetTimeOut(int timeout);
void TimeOut(void);
void SetReferenceTime(int setclasses);
void SetStartTime(int setclasses);

/* unix.c */

#ifndef MINGW
int Unix_GracefulTerminate(pid_t pid);
int Unix_GetCurrentUserName(char *userName, int userNameLen);
int Unix_ShellCommandReturnsZero(char *comm,int useshell);
int Unix_DoAllSignals(struct Item *siglist,struct Attributes a,struct Promise *pp);
int Unix_LoadProcessTable(struct Item **procdata);
void Unix_CreateEmptyFile(char *name);
int Unix_IsExecutable(const char *file);
char *Unix_GetErrorStr(void);
#endif  /* NOT MINGW */

/* vars.c */

void LoadSystemConstants(void);
void ForceScalar(char *lval,char *rval);
void NewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt);
void DeleteScalar(char *scope,char *lval);
void NewList(char *scope,char *lval,void *rval,enum cfdatatype dt);
enum cfdatatype GetVariable(const char *scope, const char *lval,void **returnv,char *rtype);
void DeleteVariable(char *scope,char *id);
int CompareVariable(const char *lval, struct CfAssoc *ap);
int CompareVariableValue(void *rval,char rtype,struct CfAssoc *ap);
int StringContainsVar(char *s,char *v);
int DefinedVariable(char *name);
int IsCf3VarString(char *str);
int BooleanControl(char *scope,char *name);
const char *ExtractInnerCf3VarString(const char *str,char *substr);
const char *ExtractOuterCf3VarString(const char *str, char *substr);
int UnresolvedVariables(struct CfAssoc *ap,char rtype);
int UnresolvedArgs(struct Rlist *args);
int IsQualifiedVariable(char *var);
int IsCfList(char *type);
int AddVariableHash(char *scope,char *lval,void *rval,char rtype,enum cfdatatype dtype,char *fname,int no);
void DeRefListsInHashtable(char *scope,struct Rlist *list,struct Rlist *reflist);

/* verify_databases.c */

void VerifyDatabasePromises(struct Promise *pp);

/* verify_environments.c */

void VerifyEnvironmentsPromise(struct Promise *pp);

/* verify_exec.c */

void VerifyExecPromise(struct Promise *pp);

/* verify_files.c */

void VerifyFilePromise(char *path,struct Promise *pp);

void LocateFilePromiserGroup(char *wildpath,struct Promise *pp,void (*fnptr)(char *path, struct Promise *ptr));
void *FindAndVerifyFilesPromises(struct Promise *pp);
int FileSanityChecks(char *path,struct Attributes a,struct Promise *pp);

/* verify_interfaces.c */

void VerifyInterface(struct Attributes a,struct Promise *pp);
void VerifyInterfacesPromise(struct Promise *pp);

/* verify_measurements.c */

void VerifyMeasurementPromise(double *this,struct Promise *pp);

/* verify_methods.c */

void VerifyMethodsPromise(struct Promise *pp);
int VerifyMethod(struct Attributes a,struct Promise *pp);

/* verify_packages.c */

void VerifyPackagesPromise(struct Promise *pp);
void ExecuteScheduledPackages(void);
void CleanScheduledPackages(void);

/* verify_processes.c */

void VerifyProcessesPromise(struct Promise *pp);
void VerifyProcesses(struct Attributes a, struct Promise *pp);
int LoadProcessTable(struct Item **procdata);
int DoAllSignals(struct Item *siglist,struct Attributes a,struct Promise *pp);
int GracefulTerminate(pid_t pid);
void GetProcessColumnNames(char *proc,char **names,int *start,int *end);


/* verify_services.c */

void VerifyServicesPromise(struct Promise *pp);

/* verify_storage.c */

void *FindAndVerifyStoragePromises(struct Promise *pp);
void VerifyStoragePromise(char *path,struct Promise *pp);

/* verify_reports.c */

void VerifyReportPromise(struct Promise *pp);

#endif
