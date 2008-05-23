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
/* File: prototypes3.h                                                       */
/*                                                                           */
/* Created: Sun Aug  5 13:43:56 2007                                         */
/*                                                                           */
/*****************************************************************************/

/* Generic stubs for the agents */
    
void ThisAgentInit(void);
void KeepPromises(void);

/* agent.c */


/* agentdiagnostic.c */

void AgentDiagnostic(void);


/* args.c */

int MapBodyArgs(char *scopeid,struct Rlist *give,struct Rlist *take);
struct Rlist *NewExpArgs(struct FnCall *fp, struct Promise *pp);
void ArgTemplate(struct FnCall *fp,char **argtemplate, enum cfdatatype *argtypes,struct Rlist *finalargs);
void DeleteExpArgs(struct Rlist *args);

/* assoc.c */

struct CfAssoc *NewAssoc(char *lval,void *rval,char rtype,enum cfdatatype dt);
void DeleteAssoc(struct CfAssoc *ap);
struct CfAssoc *CopyAssoc(struct CfAssoc *old);
void ShowAssoc (struct CfAssoc *cp);
    
/* cfpromises.c */

void SetAuditVersion(void);
void VerifyPromises(enum cfagenttype ag);
void CompilePromises(void);

/* client_code.c */

struct cfagent_connection *NewServerConnection(struct FileAttr attr,struct Promise *pp);
struct cfagent_connection *ServerConnection(char *server,struct FileAttr attr,struct Promise *pp);
void ServerDisconnection(struct cfagent_connection *conn,struct FileAttr attr,struct Promise *pp);
int cf_remote_stat(char *file,struct stat *buf,char *stattype,struct FileAttr attr,struct Promise *pp);
CFDIR *cf_remote_opendir(char *dirname,struct FileAttr attr,struct Promise *pp);
void NewClientCache(struct cfstat *data,struct Promise *pp);
void DeleteClientCache(struct FileAttr attr,struct Promise *pp);
int CompareHashNet(char *file1,char *file2,struct FileAttr attr,struct Promise *pp);
int CopyRegularFileNet(char *source,char *new,off_t size,struct FileAttr attr,struct Promise *pp);
int ServerConnect(struct cfagent_connection *conn,char *host,struct FileAttr attr, struct Promise *pp);
int CacheStat(char *file,struct stat *statbuf,char *stattype,struct FileAttr attr,struct Promise *pp);
void FlushFileStream(int sd,int toget);
int ServerOffline(char *server);
struct cfagent_connection *ServerConnectionReady(char *server);
void MarkServerOffline(char *server);
void CacheServerConnection(struct cfagent_connection *conn,char *server);

/* client_protocols.c */

int IdentifyAgent(int sd,char *localip,int family);
int AuthenticateAgent(struct cfagent_connection *conn,struct FileAttr attr,struct Promise *pp);
void CheckServerVersion(struct cfagent_connection *conn,struct FileAttr attr, struct Promise *pp);
void SetSessionKey(struct cfagent_connection *conn);

/* constraint.c */

struct Constraint *AppendConstraint(struct Constraint **conlist,char *lval, void *rval, char type,char *classes);
void DeleteConstraintList(struct Constraint *conlist);
void *GetConstraint(char *lval,struct Constraint *list,char type);
int GetBooleanConstraint(char *lval,struct Constraint *list);
int GetIntConstraint(char *lval,struct Constraint *list);
struct Rlist *GetListConstraint(char *lval,struct Constraint *list);
void ReCheckAllConstraints(struct Promise *pp);
void PostCheckConstraint(char *type,char *bundle,char *lval,void *rval,char rvaltype);
int ControlBool(enum cfagenttype id,enum cfacontrol promiseoption);

/* conversion.c */

enum cfhashes String2HashType(char *typestr);
enum cfcomparison String2Comparison(char *s);
enum cflinktype String2LinkType(char *s);
enum cfdatatype Typename2Datatype(char *name);
enum cfdatatype GetControlDatatype(char *varname,struct BodySyntax *bp);
enum cfagenttype Agent2Type(char *name);
enum cfsbundle Type2Cfs(char *name);
int GetBoolean(char *val);
long Str2Int(char *s);
int Str2Double(char *s);
void IntRange2Int(char *intrange,long *min,long *max,struct Promise *pp);
struct UidList *Rlist2UidList(struct Rlist *uidnames, struct Promise *pp);
struct GidList *Rlist2GidList(struct Rlist *gidnames, struct Promise *pp);

/* evalfunction.c */

struct Rval FnCallRandomInt(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallGetUid(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallGetGid(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallExecResult(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallReadTcp(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallReturnsZero(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallIsNewerThan(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallIsAccessedBefore(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallIsChangedBefore(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallStatInfo(struct FnCall *fp,struct Rlist *finalargs,enum fncalltype fn);
struct Rval FnCallIPRange(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallHostRange(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallIsVariable(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallStrCmp(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallRegCmp(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallGreaterThan(struct FnCall *fp,struct Rlist *finalargs,char c);
struct Rval FnCallUserExists(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallGroupExists(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallIRange(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallRRange(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallOnDate(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallAgoDate(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallAccumulatedDate(struct FnCall *fp,struct Rlist *finalargs);
struct Rval FnCallNow(struct FnCall *fp,struct Rlist *finalargs);

/* expand.c */

void ExpandPromise(enum cfagenttype ag,char *scopeid,struct Promise *pp,void *fnptr);
void ExpandPromiseAndDo(enum cfagenttype ag,char *scope,struct Promise *p,struct Rlist *scalarvars,struct Rlist *listvars,void (*fnptr)());
struct Rval ExpandDanglers(char *scope,struct Rval rval,struct Promise *pp);
void ScanRval(char *scope,struct Rlist **los,struct Rlist **lol,void *string,char type);
void ScanScalar(char *scope,struct Rlist **los,struct Rlist **lol,char *string,int level);

int IsExpandable(char *str);
int ExpandScalar(char *string,char buffer[CF_EXPANDSIZE]);
int ExpandPrivateScalar(char *contextid,char *string,char buffer[CF_EXPANDSIZE]);
struct FnCall *ExpandFnCall(char *contextid,struct FnCall *f);
struct Rval ExpandPrivateRval(char *contextid,void *rval,char type);
struct Rlist *ExpandList(char *scopeid,struct Rlist *list);
struct Rval EvaluateFinalRval(char *scopeid,void *rval,char rtype,int forcelist,struct Promise *pp);
int IsNakedVar(char *str,char vtype);
void GetNaked(char *s1, char *s2);
char *JoinPath(char *path,char *leaf);
char *JoinSuffix(char *path,char *leaf);
int IsAbsPath(char *path);


/* files_copy.c */

void *CopyFileSources(char *destination,struct FileAttr attr,struct Promise *pp);
int CopyRegularFileDisk(char *source,char *new,struct FileAttr attr,struct Promise *pp);
void CheckForFileHoles(struct stat *sstat,struct FileAttr attr,struct Promise *pp);
int FSWrite(char *new,int dd,char *buf,int towrite,int *last_write_made_hole,int n_read,struct FileAttr attr,struct Promise *pp);

/* files_links.c */

int VerifyLink(char *destination,char *source,struct FileAttr attr,struct Promise *pp);
int VerifyAbsoluteLink(char *destination,char *source,struct FileAttr attr,struct Promise *pp);
int VerifyRelativeLink(char *destination,char *source,struct FileAttr attr,struct Promise *pp);
int KillGhostLink(char *name,struct FileAttr attr,struct Promise *pp);
int MakeLink (char *from,char *to,struct FileAttr attr,struct Promise *pp);

/* files_hashes.c */

int FileHashChanged(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1],int warnlevel,enum cfhashes type,struct FileAttr attr,struct Promise *pp);
void PurgeHashes(struct FileAttr attr,struct Promise *pp);
int ReadHash(DB *dbp,enum cfhashes type,char *name,unsigned char digest[EVP_MAX_MD_SIZE+1], unsigned char *attr);
int WriteHash(DB *dbp,enum cfhashes type,char *name,unsigned char digest[EVP_MAX_MD_SIZE+1], unsigned char *attr);
void DeleteHash(DB *dbp,enum cfhashes type,char *name);
DBT *NewHashKey(char type,char *name);
void DeleteHashKey(DBT *key);
DBT *NewHashValue(unsigned char digest[EVP_MAX_MD_SIZE+1],unsigned char attr[EVP_MAX_MD_SIZE+1]);
void DeleteHashValue(DBT *value);

int CompareFileHashes(char *file1,char *file2,struct stat *sstat,struct stat *dstat,struct FileAttr attr,struct Promise *pp);
int CompareBinaryFiles(char *file1,char *file2,struct stat *sstat,struct stat *dstat,struct FileAttr attr,struct Promise *pp);
void HashFile(char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type);
void HashList(struct Item *list,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type);
void HashString(char *buffer,int len,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type);
int HashesMatch(unsigned char digest1[EVP_MAX_MD_SIZE+1],unsigned char digest2[EVP_MAX_MD_SIZE+1],enum cfhashes type);
char *HashPrint(enum cfhashes type,unsigned char digest[EVP_MAX_MD_SIZE+1]);
char *FileHashName(enum cfhashes id);
int FileHashSize(enum cfhashes id);

/* files_interfaces.c */

void SourceSearchAndCopy(char *from,char *to,int maxrecurse,struct FileAttr attr,struct Promise *pp);
void VerifyCopy(char *source,char *destination,struct FileAttr attr,struct Promise *pp);
void PurgeLocalFiles(struct Item *filelist,char *directory,struct FileAttr attr,struct Promise *pp);
void CopyFile(char *sourcefile,char *destfile,struct stat sourcestatbuf,struct FileAttr attr, struct Promise *pp);
int CompareForFileCopy(char *sourcefile,char *destfile,struct stat *ssb, struct stat *dsb,struct FileAttr attr,struct Promise *pp);
void LinkCopy(char *sourcefile,char *destfile,struct stat *sb,struct FileAttr attr, struct Promise *pp);
int cf_stat(char *file,struct stat *buf,struct FileAttr attr, struct Promise *pp);
int cf_lstat(char *file,struct stat *buf,struct FileAttr attr, struct Promise *pp);
int cf_readlink(char *sourcefile,char *linkbuf,int buffsize,struct FileAttr attr, struct Promise *pp);
CFDIR *cf_opendir(char *name,struct FileAttr attr, struct Promise *pp);
struct cfdirent *cf_readdir(CFDIR *cfdirh,struct FileAttr attr, struct Promise *pp);
void cf_closedir(CFDIR *dirh);
int CopyRegularFile(char *source,char *dest,struct stat sstat,struct stat dstat,struct FileAttr attr, struct Promise *pp);
void RegisterAHardLink(int i,char *value,struct FileAttr attr, struct Promise *pp);

/* files_operators.c */

int VerifyFileLeaf(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp);
int CreateFile(char *file,struct Promise *pp,struct FileAttr attr);
int ScheduleCopyOperation(char *destination,struct FileAttr attr,struct Promise *pp);
int ScheduleLinkOperation(char *destination,struct FileAttr attr,struct Promise *pp);
int ScheduleEditOperation(char *destination,struct FileAttr attr,struct Promise *pp);
struct FileCopy *NewFileCopy(struct Promise *pp);
void DeleteFileCopy(struct FileCopy *fcp);
void VerifyFileAttributes(char *file,struct stat *dstat,struct FileAttr attr,struct Promise *pp);
void VerifyFileIntegrity(char *file,struct Promise *pp,struct FileAttr attr);
int VerifyOwner(char *file,struct Promise *pp,struct FileAttr attr,struct stat *statbuf);
void VerifyCopiedFileAttributes(char *file,struct stat *dstat,struct stat *sstat,struct FileAttr attr,struct Promise *pp);
void VerifySetUidGid(char *file,struct stat *dstat,mode_t newperm,struct Promise *pp,struct FileAttr attr);
int TransformFile(char *file,struct FileAttr attr,struct Promise *pp);
int MoveObstruction(char *from,struct FileAttr attr,struct Promise *pp);
void VerifyName(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp);
void VerifyDelete(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp);
void TouchFile(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp); 
int MakeParentDirectory(char *parentandchild,int force);


/* files_properties.c */

int ConsiderFile(char *nodename,char *path,struct FileAttr attr,struct Promise *pp);
void SetSearchDevice(struct stat *sb,struct Promise *pp);
int DeviceBoundary(struct stat *sb,struct Promise *pp);

/* files_repository.c */

int ArchiveToRepository(char *file,struct FileAttr attr,struct Promise *pp);

/* files_select.c */

int SelectLeaf(char *path,struct stat *sb,struct FileAttr attr,struct Promise *pp);
int SelectTypeMatch(struct stat *lstatptr,struct Rlist *crit);
int SelectOwnerMatch(struct stat *lstatptr,struct Rlist *crit);
int SelectGroupMatch(struct stat *lstatptr,struct Rlist *crit);
int SelectModeMatch(struct stat *lstatptr,mode_t plus,mode_t minus);
int SelectTimeMatch(time_t stattime,time_t fromtime,time_t totime);
int SelectNameRegexMatch(char *filename,char *crit);
int SelectPathRegexMatch(char *filename,char *crit);
int SelectExecRegexMatch(char *filename,char *crit);
int SelectIsSymLinkTo(char *filename,struct Rlist *crit);
int SelectExecProgram(char *filename,char *crit);

/* files_transform.c */

struct FileAttr GetFileAttributes(struct Promise *pp);
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
void ShowAttributes(struct FileAttr a);

/* fncall.c */

struct FnCall *NewFnCall(char *name, struct Rlist *args);
struct FnCall *CopyFnCall(struct FnCall *f);
void PrintFunctions(void);
void DeleteFnCall(struct FnCall *fp);
void ShowFnCall(FILE *fout,struct FnCall *fp);
struct Rval EvaluateFunctionCall(struct FnCall *fp,struct Promise *pp);
enum cfdatatype FunctionReturnType(char *name);
enum fncalltype FnCallName(char *name);
void ClearFnCallStatus(void);
void SetFnCallReturnStatus(char *fname,int status,char *message,char *fncall_classes);

/* generic_agent.c */

void GenericInitialize(int argc,char **argv,char *agents);
void PromiseManagement(char *agent);
void InitializeGA(int argc,char **argv);
void CheckOpts(int argc,char **argv);
void CheckWorkingDirectories(void);
void Syntax(char *comp);
void Version(char *comp);
void Cf3ParseFile(char *filename);
void Cf3ParseFiles(void);
void Report(char *filename);
void HashVariables(void);
void TheAgent(enum cfagenttype ag);
void Cf3OpenLog(void);
void *ExitCleanly(int signum);
struct Constraint *ControlBodyConstraints(enum cfagenttype agent);
void SetFacility(char *retval);
struct Bundle *GetBundle(char *name,char *agent);
struct SubType *GetSubTypeForBundle(char *type,struct Bundle *bp);
void CheckControlPromises(char *scope,char *agent,struct Constraint *controllist);
void CheckVariablePromises(char *scope,struct Promise *varlist);
void CheckBundleParameters(char *scope,struct Rlist *args);
void PromiseBanner(struct Promise *pp);
void BannerSubType(char *bundlename,char *type);
void BannerBundle(struct Bundle *bp,struct Rlist *args);


/* hashes.c */

void InitHashes(struct CfAssoc **table);
void CopyHashes(struct CfAssoc **newhash,struct CfAssoc **oldhash);
void BlankHashes(char *scope);
int GetHash(char *name);
void PrintHashes(FILE *sp,struct CfAssoc **table);
int AddVariableHash(char *scope,char *lval,void *rval,char rtype,enum cfdatatype dtype,char *fname,int no);
void DeleteHashes(struct CfAssoc **hashtable);
void EditHashValue(char *scopeid,char *lval,void *rval);
void DeRefListsInHashtable(char *scope,struct Rlist *list,struct Rlist *reflist);
    
/* iteration.c */

struct Rlist *NewIterationContext(char *scopeid,struct Rlist *listvars);
void DeleteIterationContext(struct Rlist *lol);
int IncrementIterationContext(struct Rlist *iterators,int count);
int EndOfIteration(struct Rlist *iterator);

/* instrumentation.c */

void LastSaw(char *hostname,enum roles role);

/* install.c */

struct Bundle *AppendBundle(struct Bundle **start,char *name, char *type, struct Rlist *args);
struct Body *AppendBody(struct Body **start,char *name, char *type, struct Rlist *args);
struct SubType *AppendSubType(struct Bundle *bundle,char *typename);
struct SubType *AppendBodyType(struct Body *body,char *typename);
struct Promise *AppendPromise(struct SubType *type,char *promiser, void *promisee,char petype,char *classes,char *bundle);
void DeleteBundles(struct Bundle *bp);
void DeleteSubTypes(struct SubType *tp);
void DeleteBodies(struct Body *bp);

/* logging.c */

void BeginAudit(void);
void EndAudit(void);
void ClassAuditLog(struct Promise *pp,struct FileAttr attr,char *str,char status);
void AddAllClasses(struct Rlist *list);
void ExtractOperationLock(char *op);

/* matching.c */

int FullTextMatch (char *regptr,char *cmpptr);
int IsRegexItemIn(struct Item *list,char *regex);
int IsPathRegex(char *str);
int IsRegex(char *str);
int MatchRlistItem(struct Rlist *listofregex,char *teststring);


/* promises.c */

struct Body *IsBody(struct Body *list,char *key);
struct Promise *DeRefCopyPromise(char *scopeid,struct Promise *pp);
struct Promise *ExpandDeRefPromise(char *scopeid,struct Promise *pp);
void DeletePromise(struct Promise *pp);
void DeletePromises(struct Promise *pp);
void DeleteDeRefPromise(char *scopeid,struct Promise *pp);
void PromiseRef(enum cfoutputlevel level,struct Promise *pp);

/* recursion.c */

int DepthSearch(char *name,struct stat *sb,int rlevel,struct FileAttr attr,struct Promise *pp);
int PushDirState(char *name,struct stat *sb);
void PopDirState(int goback,char * name,struct stat *sb,struct Recursion r);
int SkipDirLinks(char *path,char *lastnode,struct Recursion r);

/* report.c */

void ShowContext(void);
void ShowPromises(struct Bundle *bundles,struct Body *bodies);
void ShowPromise(struct Promise *pp, int indent);
void ShowScopedVariables(FILE *fout);
void Indent(int i);
void ReportBanner(char *s);
void SyntaxTree(void);
void ShowDataTypes(void);
void ShowBundleTypes(void);
void ShowPromiseTypesFor(char *s);
void ShowBodyParts(struct BodySyntax *bs);
void ShowRange(char *);
void ShowBuiltinFunctions(void);
void ShowBody(struct Body *body,int ident);
void DebugBanner(char *s);
void ReportError(char *s);

/* rlist.c */

struct Rlist *KeyInRlist(struct Rlist *list,char *key);
int RlistLen(struct Rlist *start);
void PopStack(struct Rlist **liststart, void **item,size_t size);
void PushStack(struct Rlist **liststart,void *item);

void *CopyRvalItem(void *item, char type);
void DeleteRvalItem(void *rval, char type);
struct Rlist *CopyRlist(struct Rlist *list);
void DeleteRlist(struct Rlist *list);
void DeleteReferenceRlist(struct Rlist *list);
void ShowRlistState(FILE *fp,struct Rlist *list);

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

void ShowRlist(FILE *fp,struct Rlist *list);
void ShowRval(FILE *fp,void *rval,char type);

    
/* scope.c */

void SetNewScope(char *id);
void NewScope(char *name);
void DeleteScope(char *name);
struct Scope *GetScope(char *scope);
void CopyScope(char *new, char *old);
void DeleteAllScope(void);
void AugmentScope(char *scope,struct Rlist *lvals,struct Rlist *rvals);
void DeleteFromScope(char *scope,struct Rlist *args);


/* selfdiagnostic.c */

void SelfDiagnostic(void);
void TestVariableScan(void);
void TestExpandPromise(void);
void TestExpandVariables(void);
void TestSearchFilePromiser(void);


/* server_transform.c */

void KeepPromiseBundles(void);
void KeepControlPromises(void);
void KeepServerPromise(struct Promise *pp);
void InstallServerAuthPath(char *path,struct Auth **list,struct Auth **listtop);
struct Auth *GetAuthPath(char *path,struct Auth *list);
void Summarize(void);

/* signals.c */

void HandleSignals(int signum);

/* syntax.c */

void CheckBundle(char *name,char *type);
void CheckBody(char *name,char *type);
struct SubTypeSyntax CheckSubType(char *btype,char *type);
void CheckConstraint(char *type,char *name,char *lval,void *rval,char rvaltype,struct SubTypeSyntax ss);
void CheckSelection(char *type,char *name,char *lval,void *rval,char rvaltype);
void CheckConstraintTypeMatch(char *lval,void *rval,char rvaltype,enum cfdatatype dt,char *range,int level);
void CheckParseString(char *lv,char *s,char *range);
void CheckParseClass(char *lv,char *s,char *range);
void CheckParseInt(char *lv,char *s,char *range);
void CheckParseReal(char *lv,char *s,char *range);
void CheckParseRealRange(char *lval,char *s,char *range);
void CheckParseIntRange(char *lval,char *s,char *range);
void CheckParseOpts(char *lv,char *s,char *range);
void CheckFnCallType(char *lval,char *s,enum cfdatatype dtype,char *range);
enum cfdatatype StringDataType(char *scopeid,char *string);

/* sysinfo.c */

void GetNameInfo3(void);
void GetInterfaceInfo3(void);
void Get3Environment(void);

/* transaction.c */

void SummarizeTransaction(struct Promise *pp,struct FileAttr attr,char result);
struct CfLock AcquireLock(char *operator,char *operand,char *host,time_t now,struct FileAttr attr,struct Promise *pp);
void YieldCurrentLock(struct CfLock this);
time_t FindLock(char *last);
int WriteLock(char *lock);
int RemoveLock(char *name);
void LogLockCompletion(char *cflog,int pid,char *str,char *operator,char *operand);
time_t FindLockTime(char *name);
pid_t FindLockPid(char *name);
DB *OpenLock(void);
void CloseLock(DB *dbp);

/* vars.c */

void NewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt);
void DeleteScalar(char *scope,char *lval);
void NewList(char *scope,char *lval,void *rval,enum cfdatatype dt);
enum cfdatatype GetVariable(char *scope,char *lval,void **returnv,char *rtype);
void DeleteVariable(char *scope,char *id);
int CompareVariable(char *lval,struct CfAssoc *ap);
void DeleteAllVariables(char *scope);
int StringContainsVar(char *s,char *v);
int DefinedVariable(char *name);
int IsCf3VarString(char *str);
int BooleanControl(char *scope,char *name,int bool);
char *ExtractInnerCf3VarString(char *str,char *substr);
char *ExtractOuterCf3VarString(char *str,char *substr);


/* verify_files.c */

void FindFilePromiserObjects(struct Promise *pp);
void LocateFilePromiserGroup(char *wildpath,struct Promise *pp,void (*fnptr)(char *path, struct Promise *ptr));
void FindAndVerifyFilesPromises(struct Promise *pp);
void VerifyFilePromise(char *path,struct Promise *pp);
int SanityChecks(char *path,struct FileAttr a,struct Promise *pp);

/* verify_processes.c */

void VerifyProcessesPromise(struct Promise *pp);

/* verify_exec.c */

void VerifyExecPromise(struct Promise *pp);
