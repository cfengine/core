/* cfengine for GNU
 
        Copyright (C) 1995
        Free Software Foundation, Inc.
 
   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
 

/*******************************************************************/
/*                                                                 */
/*  cfengine function prototypes                                   */
/*                                                                 */
/*  contributed by Stuart Sharpe, September 2000                   */
/*                                                                 */
/*******************************************************************/


/* pub/full-write.c */

int cf_full_write (int desc, char *ptr, size_t len);

/* checksum_db.c */

int ReadChecksum(DB *dbp,char type,char *name,unsigned char digest[EVP_MAX_MD_SIZE+1], unsigned char *attr);
int WriteChecksum(DB *dbp,char type,char *name,unsigned char digest[EVP_MAX_MD_SIZE+1], unsigned char *attr);
void DeleteChecksum(DB *dbp,char type,char *name);
DBT *NewChecksumKey(char type,char *name);
void DeleteChecksumKey(DBT *key);
DBT *NewChecksumValue(unsigned char digest[EVP_MAX_MD_SIZE+1],unsigned char attr[EVP_MAX_MD_SIZE+1]);
void DeleteChecksumValue(DBT *value);



/* 2Dlist.c */

void Set2DList (struct TwoDimList *list);
char *Get2DListEnt (struct TwoDimList *list);
void Build2DListFromVarstring (struct TwoDimList **TwoDimlist,char *varstring,char sep,short tied);
int IncrementTwoDimList (struct TwoDimList *from);
int TieIncrementTwoDimList (struct TwoDimList *from);
struct TwoDimList *list;void AppendTwoDimItem (struct TwoDimList **liststart,struct Item *itemlist);
void Delete2DList (struct TwoDimList *item);
int EndOfTwoDimList (struct TwoDimList *list);

/* acl.c */

void aclsortperror (int error);

#if defined SOLARIS && defined HAVE_SYS_ACL_H
struct acl;
enum cffstype StringToFstype (char *string);
struct CFACL *GetACL (char *acl_alias);
int ParseSolarisMode (char* mode, mode_t oldmode);
int BuildAclEntry (struct stat *sb, char *acltype, char *name, struct acl *newaclbufp);
#endif

void InstallACL (char *alias, char *classes);

void AddACE (char *acl, char *string, char *classes);
int CheckACLs (char *filename, enum fileactions action, struct Item *acl_aliases);
enum cffstype StringToFstype (char *string);
struct CFACL *GetACL (char *acl_alias);

int CheckPosixACE (struct CFACE *aces, char method, char *filename, enum fileactions action);

/* cfd.c 

  Function prototypes for cfd.c are in cfd.c itself, 
  since they are not called from elsewhere.
*/

/* cfagent.c

  Function prototypes for cfengine.c are in cfengine.c itself, 
  since they are not called from elsewhere.
*/

/* cflex.l */

int yylex (void);

/* cfparse.y */

void yyerror (char *s);
int yyparse (void);

/* cfrun.c

  Function prototypes for cfrun.c are in cfrun.c itself, 
  since they are not called from elsewhere.
*/

/* checksums.c */

int CompareCheckSums (char *file1, char *file2, struct Image *ip, struct stat *sstat, struct stat *dstat);
int CompareBinarySums (char *file1, char *file2, struct Image *ip, struct stat *sstat, struct stat *dstat);
char ChecksumType(char *typestr);
char *ChecksumName(char type);
int ChecksumSize(char type);

    
/* functions.c */

void BuildClassEnvironment (void);

char *EvaluateFunction (char *function, char *value);
enum builtin FunctionStringToCode (char *str);
int IsBuiltinFunction  (char *function);
int CheckForModule (char *actiontxt, char *args);
void HandleStatInfo (enum builtin fn,char *args,char *value);
void HandleCompareStat (enum builtin fn,char *args,char *value);
void GetRandom (char* args,char *value);
void HandleFunctionExec (char* args,char *value,int useshell);
void HandleStatInfo (enum builtin fn,char* args,char *value);
void HandleCompareStat (enum builtin fn,char* args,char *value);
void HandleReturnsZero (char* args,char *value,int useshell);
void HandleIPRange (char* args,char *value);
void HandleHostRange (char* args,char *value);
void HandleIsDefined (char* args,char *value);
void HandleStrCmp (char* args,char *value);
void HandleGreaterThan (char* args,char *value,char plusminus);
void HandleRegCmp (char* args,char *value);
void HandleClassRegex(char *args,char *value);
void HandleShowState (char* args,char *value);
void HandleReadFile (char *args,char *value);
void HandleReadArray (char *args,char *value);
void HandlePrintFile (char *args,char *value);
void HandleReadList (char *args,char *value);
void HandleReadTCP (char *args,char *value);
void HandleReadTable (char *args,char *value);
void HandleReturnValues (char *args,char *value);
void HandleReturnClasses (char *args,char *value);
void HandleSyslogFn (char *args,char *value);
void HandleSelectPLeader (char *args,char *value);
void HandleSelectPGroup (char *args,char *value);
void HandleUserExists (char *args,char *value);
void HandleGroupExists (char *args,char *value);
void HandleSetState (char *args,char *value);
void HandleUnsetState (char *args,char *value);
void HandlePrepModule (char *args,char *value);
void HandleAssociation (char *args,char *value);
int FunctionArgs (char *args,char argv[CF_MAXFARGS][CF_EXPANDSIZE],int number);
int FileFormat (char *args,char argv[CF_MAXFARGS][CF_EXPANDSIZE],int number,char sep);
int IsSocketType (char *s);
int IsTCPType (char *s);
int IsProcessType (char *s);
void HandleFriendStatus (char *args,char *value);

/* granules.c  */

char *ConvTimeKey (char *str);
char *GenTimeKey (time_t now);

/* chflags.c */

void ParseFlagString (char *flagstring, u_long *plusmask, u_long *minusmask);

/* client.c */

int OpenServerConnection (struct Image *ip);
void CloseServerConnection (void);
int cf_rstat (char *file, struct stat *buf, struct Image *ip, char *stattype);
CFDIR *cf_ropendir (char *dirname, struct Image *ip);
void FlushClientCache (struct Image *ip);
int CompareMD5Net (char *file1, char *file2, struct Image *ip);
int CopyRegNet (char *source, char *new, struct Image *ip, off_t size);
int GetCachedStatData (char *file, struct stat *statbuf, struct Image *ip, char *stattype);
void CacheData (struct cfstat *data, struct Image *ip);
void FlushToEnd (int sd, int toget);
int cfprintf(char *out, int len2, char *in1, char *in2, char *in3);
struct cfagent_connection *NewAgentConn (void);
void DeleteAgentConn (struct cfagent_connection *ap);
int RemoteConnect (char *host,char forceipv4,short oldport,char *newport);

/* comparray.c */

int FixCompressedArrayValue (int i, char *value, struct CompressedArray **start);
void DeleteCompressedArray (struct CompressedArray *start);
int CompressedArrayElementExists (struct CompressedArray *start, int key);
char *CompressedArrayValue (struct CompressedArray *start, int key);

/* copy.c */

void CheckForHoles (struct stat *sstat, struct Image *ip);
int CopyRegDisk (char *source, char *new, struct Image *ip);
int EmbeddedWrite (char *new,int dd,char *buf,struct Image *ip,int towrite,int *last_write_made_hole,int n_read);

/* dce_acl.c */

#ifndef HAVE_DCE_DACLIF_H
int CheckDFSACE(struct CFACE *aces, char method, char *filename, enum fileactions action);
#endif

/* df.c */

int GetDiskUsage  (char *file, enum cfsizes type);

/* do.c */

void DoMethods (void);
void GetHomeInfo (void);
void GetMountInfo (void);
void MakePaths (void);
void MakeChildLinks (void);
void MakeLinks (void);
void MailCheck (void);
void ExpiredUserCheck (char *spooldir, int always);
void MountFileSystems (void);
void CheckRequired (void);
int ScanDiskArrivals (char *name, struct stat *sb, int rlevel);
void TidyFiles (void);
void TidyHome (void);
void TidyHomeDir (struct TidyPattern *ptr, char *subdir);
void Scripts (void);
void GetSetuidLog (void);
void CheckFiles (void);
void SaveSetuidLog (void);
void DisableFiles (void);
void MountHomeBinServers (void);
void MountMisc (void);
void Unmount (void);
void EditFiles (void);
void CheckResolv (void);
void MakeImages (void);
void ConfigureInterfaces (void);
void CheckTimeZone (void);
void CheckProcesses (void);
void CheckPackages (void);
int RequiredFileSystemOkay (char *name);
void InstallMountedItem (char *host, char *mountdir);
void InstallMountableItem (char *path, char *mnt_opts, flag readonly);
void AddToFstab (char *host, char *mountpt, char *rmountpt, char *mode, char *options, int ismounted);
int CheckFreeSpace (char *file, struct Disk *ptr);
void CheckHome (struct File *ptr);
void EditItemsInResolvConf (struct Item *from, struct Item **list);
int TZCheck (char *tzsys, char *tzlist);
void ExpandWildCardsAndDo (char *wildpath, char *buffer, void (*function)(char *path, void *ptr), void *argptr);
int TouchDirectory (struct File *ptr);
void RecFileCheck (char *startpath, void *vp);
int MatchStringInFstab (char *str);
int ScanFileSystemArrivals (char *name,int rlevel,struct stat *sb, DB *dbp);
void RecordFileSystemArrivals (DB *dbp, time_t mtime);


/* edittools.c */

int DoRecursiveEditFiles (char *name, int level, struct Edit *ptr,struct stat *sb);
void DoEditHomeFiles (struct Edit *ptr);
void WrapDoEditFile (struct Edit *ptr, char *filename);
void DoEditFile (struct Edit *ptr, char *filename);
int IncrementEditPointer (char *str, struct Item *liststart);
int ResetEditSearch  (char *str, struct Item *list);
int ReplaceEditLineWith  (char *string);
int ExpandAllVariables  (struct Item *list);
int RunEditScript  (char *script, char *fname, struct Item **filestart, struct Edit *ptr);
void DoFixEndOfLine (struct Item *list, char *type);
void HandleAutomountResources (struct Item **filestart, char *opts);
void CheckEditSwitches (char *filename, struct Edit *ptr);
void AddEditfileClasses  (struct Edit *list, int editsdone);
struct Edlist *ThrowAbort (struct Edlist *from);
struct Edlist *SkipToEndGroup (struct Edlist *ep, char *filename);
int BinaryEditFile (struct Edit *ptr, char *filename);
int LoadBinaryFile (char *source, off_t size, void *memseg);
int SaveBinaryFile (char *file, off_t size, void *memseg, char *repository);
void WarnIfContainsRegex (void *memseg, off_t size, char *data, char *filename);
void WarnIfContainsFilePattern (void *memseg, off_t size, char *data, char *filename);
int BinaryReplaceRegex (void *memseg, off_t size, char *search, char *replace, char *filename);

/* crypto.c */

void RandomSeed (void);
void LoadSecretKeys (void);
void MD5Random (unsigned char digest[EVP_MAX_MD_SIZE+1]);
int EncryptString (char *in, char *out, unsigned char *key, int len);
int DecryptString (char *in, char *out, unsigned char *key, int len);
RSA *HavePublicKey (char *ipaddress);
void SavePublicKey (char *ipaddress, RSA *key);
void DeletePublicKey (char *ipaddress);
void GenerateRandomSessionKey (void);
char *KeyPrint(RSA *key);

/* errors.c */

void FatalError (char *s);
void Warning (char *s);
void ResetLine (char *s);

/* eval.c */

int ShowClass(char *c1,char *c2);
int CountParentheses(char *str);
int NestedParentheses(char *str);
int Day2Number (char *s);
int Month2Number (char *s);
void AddInstallable (char *classlist);
void AddMultipleClasses (char *classlist);
void AddTimeClass (char *str);
void AddClassToHeap (char *class);
void DeleteClassFromHeap (char *class);
int IsHardClass (char *sp);
int IsSpecialClass (char *class);
int IsExcluded (char *exception);
int IsDefinedClass (char *class);
int IsInstallable (char *class);
void AddPrefixedMultipleClasses (char *prefix,char *class);
void NegateCompoundClass (char *class, struct Item **heap);
int EvaluateORString (char *class, struct Item *list);
int EvaluateANDString (char *class, struct Item *list);
int GetORAtom (char *start, char *buffer);
int GetANDAtom (char *start, char *buffer);
int CountEvalAtoms (char *class);
enum actions ActionStringToCode  (char *str);
int IsBracketed (char *s);
void DeleteClassesFromContext  (char *s);

/* filedir.c */

int IsHomeDir (char *name);
int EmptyDir (char *path);
int RecursiveCheck (char *name, int recurse, int rlevel, struct File *ptr,struct stat *sb);
#ifdef DARWIN
int CheckFinderType (char *file, enum fileactions action, char *cf_findertype, struct stat *statbuf); 
#endif
void CheckExistingFile (char *cf_findertype,char *file, struct stat *dstat, struct File *ptr);
void CheckCopiedFile (char *cf_findertype,char *file,struct stat *dstat, struct stat *sstat, struct Image *ptr);
int CheckOwner (char *file, struct File *ptr, struct stat *statbuf);
int CheckHomeSubDir (char *testpath, char *tidypath, int recurse);
int FileIsNewer (char *file1, char *file2);
int IgnoreFile  (char *pathto, char *name, struct Item *ignores);
void CompressFile (char *file);

/* filenames.c */

int IsIn(char c,char *s);
int IsAbsoluteFileName (char *f);
void CreateEmptyFile (char *f);
int RootDirLength (char *f);
void AddSlash (char *str);
void DeleteSlash (char *str);
void DeleteNewline (char *str);
char *LastFileSeparator (char *str);
int ChopLastNode (char *str);
char *CanonifyName (char *str);
char *Space2Score (char *str);
char *ASUniqueName (char *str);
char *ReadLastNode (char *str);
int MakeDirectoriesFor (char *file, char force);
int BufferOverflow (char *str1, char *str2);
int ExpandOverflow (char *str1, char *str2);
void Chop (char *str);
int CompressPath (char *dest, char *src);
char ToLower  (char ch);
char ToUpper  (char ch);
char *ToUpperStr  (char *str);
char *ToLowerStr  (char *str);

/* filters.c */

void InstallFilter (char *filter);
void CheckFilters (void);
void InstallFilterTest (char *alias, char *type, char *data);
enum filternames FilterActionsToCode (char *filtertype);
int FilterExists (char *name);
int ProcessFilter (char *proc, struct Item *filterlist,char **names,int *start,int *stop);
void SplitLine (char *proc, char **names,int *start,int *stop,char **line);
int FileObjectFilter (char *file, struct stat *statptr, struct Item *filterlist, enum actions context);
time_t Date2Number (char *string, time_t now);
void Size2Number (char *buffer);
int FilterTypeMatch (struct stat *ptr,char *match);
int FilterOwnerMatch (struct stat *lstatptr,char *crit);
int FilterGroupMatch (struct stat *lstatptr,char *crit);
int FilterModeMatch (struct stat *lstatptr,char *crit);
int FilterTimeMatch (time_t stattime,char *from,char *to);
int FilterNameRegexMatch (char *file,char *crit);
int FilterExecRegexMatch (char *file,char *crit);
int FilterExecMatch (char *file,char *crit);
int FilterIsSymLinkTo (char *file,char *crit);
void DoFilter (struct Item **attr,char **crit,struct stat *lstatptr,char *filename);
void GetProcessColumns (char *proc,char **names,int *start,int *stop);
int FilterProcMatch (char *name1,char *name2,char *expr,char **names,char **line);
int FilterProcSTimeMatch  (char *name1,char *name2,char *expr1,char *expr2,char **names,char **line);
int FilterProcTTimeMatch  (char *name1,char *name2,char *expr1,char *expr2,char **names,char **line);
void DoProc (struct Item **attr,char **crit,char **names,char **line);
/*
 * HvB: Bas van der Vlies
*/
void ParseTTime (char *line,char *time_str);



/* ifconf.c */

void IfConf  (char *vifdev, char *address,char *vnetmask, char *vbroadcast);
int GetIfStatus (int sk, char *vifdev, char *address, char *vnetmask, char *vbroadcast);
void SetIfStatus (int sk, char *vifdev, char *address, char *vnetmask, char *vbroadcast);
void GetBroadcastAddr (char *ipaddr, char *vifdev, char *vnetmask, char *vbroadcast);
void SetDefaultRoute (void);

/* image.c */

void GetRemoteMethods (void);
void RecursiveImage (struct Image *ip, char *from, char *to, int maxrecurse);
void CheckHomeImages (struct Image *ip);
void CheckImage (char *source, char *destination, struct Image *ip);
void PurgeFiles (struct Item *filelist, char *directory, struct Item *exclusions);
void ImageCopy (char *sourcefile, char *destfile, struct stat sourcestatbuf, struct Image *ip);
int cfstat (char *file, struct stat *buf, struct Image *ip);
int cflstat (char *file, struct stat *buf, struct Image *ip);
int cfreadlink (char *sourcefile, char *linkbuf, int buffsize, struct Image *ip);
CFDIR *cfopendir (char *name, struct Image *ip);
struct cfdirent *cfreaddir (CFDIR *cfdirh, struct Image *ip);
void cfclosedir (CFDIR *dirh);
int CopyReg  (char *source, char *dest, struct stat sstat, struct stat dstat, struct Image *ip);
void RegisterHardLink (int i, char *value, struct Image *ip);

/* init.c */

void CheckWorkDirectories (void);
void SetSignals (void);
void ActAsDaemon (int preserve);
int IsInterfaceAddress (char *s);

/* install.c */

void InstallControlRValue (char *lvalue,char *varvalue);
void HandleEdit (char *file, char *edit, char *string);
void HandleOptionalFileAttribute (char *item);
void HandleOptionalMountablesAttribute (char *item);
void HandleOptionalImageAttribute (char *item);
void HandleOptionalRequired (char *item);
void HandleOptionalInterface (char *item);
void HandleOptionalUnMountAttribute (char *item);
void HandleOptionalMiscMountsAttribute (char *item);
void HandleOptionalTidyAttribute (char *item);
void HandleOptionalDirAttribute (char *item);
void HandleOptionalDisableAttribute (char *item);
void HandleOptionalLinkAttribute (char *item);
void HandleOptionalProcessAttribute (char *item);
void HandleOptionalScriptAttribute (char *item);
void HandleOptionalAlertsAttribute (char *item);
void HandleOptionalPackagesAttribute (char *item);
void HandleOptionalMethodsAttribute (char *item);
void HandleChDir (char *value);
void HandleChRoot (char *value);
void HandleFileItem (char *item);
void InstallObject (char *name);
void InstallBroadcastItem (char *item);
void InstallDefaultRouteItem (char *item);
void InstallGroupRValue (char *rval, enum itemtypes type);
void HandleHomePattern (char *pattern);
void AppendNameServer (char *item);
void AppendImport (char *item);
void InstallHomeserverItem (char *item);
void InstallBinserverItem (char *item);
void InstallMailserverPath (char *path);
void InstallLinkItem  (char *from, char *to);
void InstallLinkChildrenItem  (char *from, char *to);
void InstallRequiredPath (char *path, int freespace);
void AppendMountable (char *path);
void AppendUmount (char *path, char deldir, char delfstab, char force);
void AppendMiscMount (char *from, char *onto, char * mode,char *opts);
void AppendIgnore (char *path);
void InstallPending (enum actions action);
int EditFileExists (char *file);
int GetExecOutput (char *command, char *buffer,int useshell);
void InstallEditFile (char *file, char *edit, char *data);
void AddEditAction (char *file, char *edit, char *data);
enum editnames EditActionsToCode (char *edit);
void AppendInterface (char *ifname, char *ip, char *netmask, char *broadcast);
void AppendScript (char *item, int timeout, char useshell, char *uidname, char *gidname);
void AppendSCLI(char *item, int timeout, char useshell, char *uidname, char *gidname);
void AppendDisable (char *path, char *type, short int rotate, char comp, int size);
void InstallTidyItem  (char *path, char *wild, int rec, short int age, char travlinks, int tidysize, char type, char ldirs, char tidydirs, char *classes);
void InstallMakePath (char *path, mode_t plus, mode_t minus, char *uidnames, char *gidnames);
void HandleTravLinks (char *value);
void HandleTidySize (char *value);
void HandleUmask (char *value);
void HandleDisableSize (char *value);
void HandleCopySize (char *value);
void HandleRequiredSize (char *value);
void HandleTidyType (char *value);
void HandleTidyLinkDirs (char *value);
void HandleTidyRmdirs (char *value);
void HandleCopyBackup (char *value);
void HandleTimeOut (char *value);
void HandleUseShell (char *value);
void HandleFork (char *value);
void HandleChecksum (char *value);
void HandleTimeStamps (char *value);
int GetFileAction (char *action);
void InstallFileListItem (char *path, mode_t plus, mode_t minus, enum fileactions action, char *uidnames, char *gidnames, int recurse, char travlinks, char chksum);
void InstallProcessItem (char *expr, char *restart, short int matches, char comp, short int signal, char action, char *classes, char useshell, char *uidname, char *gidname);
void InstallImageItem (char *cf_findertype, char *path, mode_t plus, mode_t minus, char *destination, char *action, char *uidnames, char *gidnames, int size, char comp, int rec, char type, char lntype, char *server);
void InstallMethod (char *function, char *file);
void InstallAuthItem (char *path, char *attribute, struct Auth **list, struct Auth **listtop, char *classes);
void InstallPackagesItem (char *name, char *ver, enum cmpsense sense, enum pkgmgrs mgr, enum pkgactions action);
int GetCmpSense (char *sense);
int GetPkgMgr (char *mgr);
int GetPkgAction (char *pkgaction);
int GetCommAttribute (char *s);
void HandleRecurse (char *value);
void HandleCopyType (char *value);
void HandleDisableFileType (char *value);
void HandleDisableRotate (char *value);
void HandleAge (char *days);
void HandleProcessMatches (char *value);
void HandleProcessSignal (char *value);
void HandleNetmask (char *value);
void HandleIPAddress (char *value);
void HandleBroadcast (char *value);
void AppendToActionSequence  (char *action);
void AppendToAccessList  (char *user);
void HandleLinkAction (char *value);
void HandleDeadLinks (char *value);
void HandleLinkType (char *value);
void HandleServer (char *value);
void HandleDefine (char *value);
void HandleElseDefine (char *value);
void HandleFailover (char *value);
struct UidList *MakeUidList (char *uidnames);
struct GidList *MakeGidList (char *gidnames);
void InstallTidyPath (char *path, char *wild, int rec, short int age, char travlinks, int tidysize, char type, char ldirs,char tidydirs, char *classes);
void AddTidyItem (char *path, char *wild, int rec, short int age, char travlinks, int tidysize, char type, char ldirs, short int tidydirs, char *classes);
int TidyPathExists (char *path);
void AddSimpleUidItem (struct UidList **uidlist, int uid, char *uidname);
void AddSimpleGidItem (struct GidList **gidlist, int gid, char *gidname);
void InstallAuthPath (char *path, char *hostname, char *classes, struct Auth **list, struct Auth **listtop);
void AddAuthHostItem (char *path, char *attribute, char *classes, struct Auth **list);
int AuthPathExists (char *path, struct Auth *list);
int HandleAdmitAttribute (struct Auth *ptr, char *attribute);
void PrependTidy (struct TidyPattern **list, char *wild, int rec, short int age, char travlinks, int tidysize, char type, char ldirs,char tidydirs, char *classes);
void HandleShortSwitch (char *name,char *value,short *flag);
void HandleCharSwitch (char *name,char *value,char *flag);
void HandleIntSwitch (char *name,char *value,int *flag,int min, int max);
void PrependAuditFile(char *file);
void VersionAuditFile(void);

/* ip.c */

char *sockaddr_ntop (struct sockaddr *sa);
void *sockaddr_pton (int af,void *src);
void CfenginePort (void);
void StrCfenginePort (void);
int IsIPV4Address (char *name);
int IsIPV6Address (char *name);
char *Hostname2IPString (char *name);
char *IPString2Hostname (char *name);
char *IPString2UQHostname (char *name);


/* instrument.c */

void RecordPerformance(char *name, time_t t, double value);
void RecordClassUsage(struct Item *list);
void LastSeen (char *host,enum roles role);
void CheckFriendConnections(int hours);
void CheckFriendReliability(void);
DBT *NewDBKey(char *name);
void DeleteDBKey(DBT *key);
DBT *NewDBValue(void *ptr,int size);
void DeleteDBValue(DBT *value);
int ReadDB(DB *dbp,char *name,void *ptr,int size);
int WriteDB(DB *dbp,char *name,void *ptr,int size);
void DeleteDB(DB *dbp,char *name);
double GAverage(double anew,double aold,double trust);

/* item-ext.c */

int OrderedListsMatch (struct Item *list1, struct Item *list2);
int RegexOK (char *string);
int IsWildItemIn (struct Item *list, char *item);
void InsertItemAfter  (struct Item **filestart, struct Item *ptr, char *string);
void InsertFileAfter  (struct Item **filestart, struct Item *ptr, char *string);
struct Item *LocateNextItemContaining (struct Item *list,char *string);
struct Item *LocateNextItemMatching (struct Item *list,char *string);
struct Item *LocateNextItemStarting (struct Item *list,char *string);
struct Item *LocateItemMatchingRegExp (struct Item *list,char *string);
struct Item *LocateItemContainingRegExp (struct Item *list,char *string);
int DeleteToRegExp (struct Item **filestart, char *string);
int DeleteItemGeneral (struct Item **filestart, char *string, enum matchtypes type);
int DeleteItemLiteral (struct Item **filestart, char *string);
int DeleteItemStarting (struct Item **list,char *string);
int DeleteItemNotStarting (struct Item **list,char *string);
int DeleteItemMatching (struct Item **list,char *string);
int DeleteItemNotMatching (struct Item **list,char *string);
int DeleteItemContaining (struct Item **list,char *string);
int DeleteItemNotContaining (struct Item **list,char *string);
int DeleteLinesWithFileItems (struct Item **list,char *string,enum editnames code);
int AppendLinesFromFile (struct Item **filestart,char *filename);
int CommentItemStarting (struct Item **list, char *string, char *comm, char *end);
int CommentItemContaining (struct Item **list, char *string, char *comm, char *end);
int CommentItemMatching (struct Item **list, char *string, char *comm, char *end);
int UnCommentItemMatching (struct Item **list, char *string, char *comm, char *end);
int UnCommentItemContaining (struct Item **list, char *string, char *comm, char *end);
int CommentToRegExp (struct Item **filestart, char *string, char *comm, char *end);
int UnCommentToRegExp(struct Item **filestart,char *string,char *comm,char *end);
int DeleteSeveralLines  (struct Item **filestart, char *string);
struct Item *GotoLastItem (struct Item *list);
int LineMatches  (char *line, char *regexp);
int GlobalReplace (struct Item **liststart, char *search, char *replace);
int SingleReplace (struct Item **liststart, char *search, char *replace);
int CommentSeveralLines  (struct Item **filestart, char *string, char *comm, char *end);
int UnCommentSeveralLines  (struct Item **filestart, char *string, char *comm, char *end);
int ItemMatchesRegEx (char *item, char *regex);
void ReplaceWithFieldMatch (struct Item **filestart, char *field, char *replace, char split, char *filename);
void AppendToLine (struct Item *current, char *text, char *filename);
int CfRegcomp (regex_t *preg, const char *regex, int cflags);

/* item-file.c */

int LoadItemList (struct Item **liststart, char *file);
int SaveItemList (struct Item *liststart, char *file, char *repository);
int CompareToFile (struct Item *liststart, char *file);

/* item.c */

struct Item *String2List(char *string);
int ListLen (struct Item *list);
int ByteSizeList (struct Item *list);
void AppendItems  (struct Item **liststart, char *itemstring, char *classes);
int IsItemIn (struct Item *list, char *item);
int IsClassedItemIn (struct Item *list, char *item);
int IsFuzzyItemIn (struct Item *list, char *item);
int GetItemListCounter (struct Item *list, char *item);
struct Item *ConcatLists (struct Item *list1, struct Item *list2);
void CopyList (struct Item **dest,struct Item *source);
int FuzzySetMatch (char *s1, char *s2);
int FuzzyMatchParse (char *item);
int FuzzyHostMatch (char *arg0, char *arg1,char *basename);
int FuzzyHostParse (char *arg1,char *arg2);
void PrependItem  (struct Item **liststart, char *itemstring, char *classes);
void AppendItem  (struct Item **liststart, char *itemstring, char *classes);
void InstallItem  (struct Item **liststart, char *itemstring, char *classes, int ifelapsed, int expireafter);
void DeleteItemList (struct Item *item);
void DeleteItem (struct Item **liststart, struct Item *item);
void DebugListItemList (struct Item *liststart);
int ItemListsEqual (struct Item *list1, struct Item *list2);
struct Item *SplitStringAsItemList (char *string, char sep);
struct Item *ListFromArgs (char *string);
void IncrementItemListCounter (struct Item *ptr, char *string);
void SetItemListCounter (struct Item *ptr, char *string,int value);
struct Item *SortItemListNames(struct Item *list);
struct Item *SortItemListCounters(struct Item *list);

/* link.c */

struct Link;

int LinkChildFiles (char *from, char *to, struct Link *ptr);
void LinkChildren (char *path, char type, struct stat *rootstat, uid_t uid, gid_t gid,struct Link *ptr);
int RecursiveLink (struct Link *lp, char *from, char *to, int maxrecurse);
int LinkFiles (char *from, char *to,struct Link *ptr);
int RelativeLink (char *from, char *to,struct Link *ptr);
int AbsoluteLink (char *from, char *to,struct Link *ptr);
int DoLink  (char *from, char *to, char *defines);
void KillOldLink (char *name, char *defines);
int HardLinkFiles (char *from, char *to,struct Link *ptr);
void DoHardLink  (char *from, char *to, char *defines);
int ExpandLinks (char *dest, char *from, int level);
char *AbsLinkPath (char *from, char *relto);

/* locks.c */

void WritePID(char *file);
void PreLockState (void);
void SaveExecLock (void);
void RestoreExecLock (void);
void InitializeLocks (void);
void CloseLocks (void);
void HandleSignal (int signum);
int GetLock (char *operator, char *operand, int ifelapsed, int expireafter, char *host, time_t now);
void ReleaseCurrentLock (void);
int CountActiveLocks (void);
time_t GetLastLock (void);
time_t CheckOldLock (void);
void SetLock (void);
void LockLog (int pid, char *str, char *operator, char *operand);
int PutLock (char *name);
int DeleteLock (char *name);
time_t GetLockTime (char *name);
pid_t GetLockPid (char *name);
void ExtractOpLock(char *op);

/* log.c */

void AuditLog(char doaudit,struct Audit *ap,int lineno,char *str,char status);
void CfLog (enum cfoutputlevel level, char *string, char *errstr);
void ResetOutputRoute  (char log, char inform);
void ShowAction (void);
void CfOpenLog (void);
void CfCheckAudit(void);
void CloseAuditLog(void);
void AuditStatusMessage(char status);

    
/* macro.c */

void SetContext (char *id);
int ScopeIsMethod  (void);
void InitHashTable (char **table);
void BlankHashTable (char *scope);
void PrintHashTable (char **table);
int Hash (char *name);
int ElfHash (char *name);
void AddMacroValue (char *scope, char *name, char *value);
char *GetMacroValue (char *scope,char *name);
void RecordMacroId (char *name);
int CompareMacro (char *name, char *macro);
void DeleteMacros (char *scope);
void DeleteMacro  (char *scope,char *name);
struct cfObject *ObjectContext (char *scope);
void DispatchMethodReply (void);
void EncapsulateReply (char *name);

/* HvB */
int OptionIs (char *scope, char *name, short on);

/* methods.c */

void CheckForMethod (void);
void CheckMethodReply (void);
void DispatchNewMethod (struct Method *ptr);
struct Item *GetPendingMethods (int state);
int ChildLoadMethodPackage (char *name, char *md5);
int ParentLoadReplyPackage (char *name);
char *GetMethodFilename (struct Method *ptr);
void EvaluatePendingMethod (char *name);
void DeleteMethodList (struct Method *ptr);
void EncapsulateMethod (struct Method *ptr,char *name);
enum methproto ConvertMethodProto (char *name);
struct Method *IsDefinedMethod (char *name,char *digeststr);
int CountAttachments (char *name);
void SplitMethodName (char *name,char *client,char *server,char *methodname,char *digeststr,char *extra);
int CheckForMethodPackage (char *name);


/* misc.c */

int VM_version (void);
int linux_fedora_version (void);
int linux_redhat_version (void);
int linux_mandrake_version (void);
int linux_suse_version (void);
int debian_version (void);
int lsb_version (void);
char * UnQuote (char *name);
int DirPush (char *name,struct stat *sb);
void DirPop (int goback,char *name,struct stat *sb);
void CheckLinkSecurity (struct stat *sb, char *name);
void GetNameInfo (void);
void GetEnvironment(void);
void AddNetworkClass (char *netmask);
void TruncateFile (char *name);
int FileSecure  (char *name);
int ChecksumChanged (char *filename, unsigned char digest[EVP_MAX_MD_SIZE+1], int warnlevel, int refresh, char type);
char *ChecksumPrint   (char type,unsigned char digest[EVP_MAX_MD_SIZE+1]);
void ChecksumFile  (char *filename,unsigned char digest[EVP_MAX_MD_SIZE+1],char type);
void ChecksumList  (struct Item *list,unsigned char digest[EVP_MAX_MD_SIZE+1],char type);
int ChecksumsMatch (unsigned char digest1[EVP_MAX_MD_SIZE+1],unsigned char digest2[EVP_MAX_MD_SIZE+1],char type);
void ChecksumPurge (void);
void ChecksumString  (char *buffer,int len,unsigned char digest[EVP_MAX_MD_SIZE+1],char type);
int IgnoredOrExcluded (enum actions action, char *file, struct Item *inclusions, struct Item *exclusions);
void Banner (char *string);
void SetDomainName (char *sp);
void GetInterfaceInfo (void);
void DeleteInterfaceInfo (char *regex);
void GetV6InterfaceInfo (void);
void DebugBinOut (char *string, int len);
int ShellCommandReturnsZero (char *comm, int useshell);
void SetClassesOnScript (char *comm, char *classes, char *elseclasses, int useshell);
void IDClasses (void);
void AddListSeparator(char *s);
void ChopListSeparator(char *s);

void SetReferenceTime(int setclasses);
void SetStartTime(int setclasses);

/* modes.c */

void ParseModeString (char *modestring, mode_t *plusmask, mode_t *minusmask);
void CheckModeState (enum modestate stateA, enum modestate stateB,enum modesort modeA, enum modesort modeB, char ch);
void SetMask (char action, int value, int affected, mode_t *p, mode_t *m);

/* mount.c */

int MountPathDefined (void);
int MatchAFileSystem (char *server, char *lastlink);
int IsMountedFileSystem  (struct stat *childstat, char *dir, int rlevel);

/* net.c */

void TimeOut (void);
int SendTransaction (int sd, char *buffer,int len, char status);
int ReceiveTransaction (int sd, char *buffer,int *more);
int RecvSocketStream (int sd, char *buffer, int toget, int nothing);
int SendSocketStream (int sd, char *buffer, int toget, int flags);


/* strategies.c */

void InstallStrategy (char *value, char *classes);
void AddClassToStrategy (char *alias,char *class,char *value);
void SetStrategies (void);
void GetNonMarkov (void);

/* parse.c */

int ParseInputFile (char *file,int audit);
void ParseFile (char *f,char *env,int audit);
void ParseStdin (void);
void NewParser (void);
int RemoveEscapeSequences (char *from,char *to);
void DeleteParser (void);
void SetAction (enum actions action);
void HandleLValue (char *id);
void HandleBraceObjectClassifier (char *id);
void HandleBraceObjectID (char *id);
void HandleClass (char *id);
void HandleServerRule (char *obj);
void HandleGroupRValue (char *item);
void HandleFunctionObject  (char *fn);
void HandleQuotedString  (char *qstring);
void HandleVarObject  (char *path);
void HandleVarpath  (char *varpath);
void HandleOption (char *option);
int CompoundId (char *id);
void InitializeAction (void);
void SetMountPath  (char *value);
void SetRepository  (char *value);
char *FindInputFile  (char *result, char *filename);


/* patches.c */

int IntMin (int a,int b);
char *StrStr (char *s1,char *s2);
int StrnCmp (char *s1,char *s2,size_t n);

#ifndef HAVE_GETNETGRENT

int setnetgrent (const char *netgroup);
int getnetgrent (char **host, char **user, char **domain);
void endnetgrent (void);
#endif

#ifndef HAVE_UNAME
int uname  (struct utsname *name);
#endif

#ifndef HAVE_STRSTR
char *strstr (char *s1,char *s2);
#endif

#ifndef HAVE_STRDUP
char *strdup (char *str);
#endif

#ifndef HAVE_STRRCHR
char *strrchr (char *str,char ch);
#endif

#ifndef HAVE_STRERROR
char *strerror (int err);
#endif

#ifndef HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif
    
#ifndef HAVE_PUTENV
int putenv  (char *s);
#endif

#ifndef HAVE_SETEUID
int seteuid (uid_t euid);
#endif

#ifndef HAVE_SETEUID
int setegid (gid_t egid);
#endif

int IsPrivileged (void);

/* alerts.c */

void DoAlerts (void);

/* package.c */
int PackageCheck (struct Package *ptr,char *package, enum pkgmgrs pkgmgr, char *version, enum cmpsense cmp);
int PackageList (struct Package *ptr,char *package, enum pkgmgrs pkgmgr, char *version, enum cmpsense cmp, struct Item **pkglist);
int UpgradePackage (struct Package *ptr,char *package, enum pkgmgrs pkgmgr, char *version, enum cmpsense cmp);
int InstallPackage (struct Package *ptr,enum pkgmgrs pkgmgr, struct Item **pkglist);
int RemovePackage (struct Package *ptr,enum pkgmgrs pkgmgr, struct Item **pkglist);
void ProcessPendingPackages (struct Package *ptr,enum pkgmgrs pkgmgr, enum pkgactions action, struct Item **pkglist);

/* popen.c */

FILE *cfpopensetuid (char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv);
FILE *cfpopen (char *command, char *type);
FILE *cfpopen_sh (char *command, char *type);
FILE *cfpopen_shsetuid (char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv);
int cfpclose (FILE *pp);
int cfpclose_def (FILE *pp, char *defines, char *elsedef);
int SplitCommand (char *comm, char (*arg)[CF_BUFSIZE]);

/* process.c */

int LoadProcessTable (struct Item **procdata, char *psopts);
void DoProcessCheck (struct Process *pp, struct Item *procdata);
int FindMatches (struct Process *pp, struct Item *procdata, struct Item **killlist);
void DoSignals (struct Process *pp,struct Item *list);

/* proto.c */
int IdentifyForVerification (int sd,char *localip, int family);
int KeyAuthentication (struct Image *ip);
int BadProtoReply  (char *buf);
int OKProtoReply  (char *buf);
int FailedProtoReply  (char *buf);
void CheckRemoteVersion(void);

/* read.c */
int ReadLine (char *buff, int size, FILE *fp);

/* report.c */

void ListDefinedVariables (void);
void ListDefinedClasses (void);
void ListDefinedMethods (char *classes);
void ListDefinedAlerts (char *classes);
void ListDefinedStrategies (char *classes);
void ListDefinedInterfaces (char *classes);
void ListDefinedHomePatterns (char *classes);
void ListDefinedBinservers (char *classes);
void ListDefinedLinks (char *classes);
void ListDefinedLinkchs (char *classes);
void ListDefinedResolvers (char *classes);
void ListDefinedScripts (char *classes);
void ListDefinedSCLI (char *classes);
void ListDefinedImages (char *classes);
void ListDefinedTidy (char *classes);
void ListDefinedMountables (char *classes);
void ListMiscMounts (char *classes);
void ListDefinedRequired (char *classes);
void ListDefinedHomeservers (char *classes);
void ListDefinedDisable (char *classes);
void ListDefinedMakePaths (char *classes);
void ListDefinedImports (void);
void ListDefinedIgnore (char *classes);
void ListDefinedPackages (char *classes);
void ListFiles (char *classes);
void ListActionSequence (void);
void ListUnmounts (char *classes);
void ListProcesses (char *classes);
void ListACLs (void);
void ListFileEdits (char *classes);
void ListFilters (char *classes);

void InterfacePromise(struct Interface *ifp);
void LinkPromise(struct Link *ptr, char *type);
void PromiseItem(struct Item *ptr);
void PromiseMethod(struct Method *ptr);
void PromiseShellCommand(struct ShellComm *ptr);
void PromiseFileCopy(struct Image *ptr);
void PromiseTidy(struct Tidy *ptr,char *classes);
void PromiseMountable(struct Mountables *ptr);
void PromiseMiscMount(struct MiscMount *ptr);
void DiskPromises(struct Disk *ptr);
void PromiseDisable(struct Disable *ptr);
void PromiseDirectories(struct File *ptr);
void PromiseFiles(struct File *ptr);
void PromiseUnmount(struct UnMount *ptr);
void PromiseFileEdits(struct Edit *ptr,char *classes);
void PromiseProcess(struct Process *ptr);
void PromisePackages(struct Package *ptr);

/* repository.c */

int Repository (char *file, char *repository);

/* rotate.c */

void RotateFiles (char *name, int number);

/* scli.c */

void SCLIScript(void);

/* sensible.c */

int SensibleFile (char *nodename, char *path, struct Image *ip);
void RegisterRecursionRootDevice (dev_t device);
int DeviceChanged (dev_t thisdevice);

/* state.c */

void AddPersistentClass (char *name,unsigned int ttl_minutes, enum statepolicy policy);
void DeletePersistentClass (char *name);
void PersistentClassesToHeap (void);
void DePort (char *tcpbuffer);


/* tidy.c */

int RecursiveHomeTidy (char *name, int level,struct stat *sb);
int TidyHomeFile (char *path, char *name,struct stat *statbuf, int level);
int RecursiveTidySpecialArea (char *name, struct Tidy *tp, int maxrecurse, struct stat *sb);
void TidyParticularFile (char *path, char *name, struct Tidy *tp, struct stat *statbuf, int is_dir, int level,int usepath);
void DoTidyFile (char *path, char *name, struct TidyPattern *tlp, struct stat *statbuf, short int logging_this, int isdir,int usepath);
void DeleteTidyList (struct TidyPattern *list);

/* varstring.c */

void GetSepElement(char *from,char *to,int index,char sep);
int IsListVar(char *name,char sep);
int VarListLen(char *name, char sep);
int TrueVar (char *var);
int CheckVarID (char *var);
int IsVarString (char *str);
int ExpandVarstring (char *string,char buffer[CF_EXPANDSIZE], char *bserver);
char *ExtractInnerVarString (char *string, char *substr);
char *ExtractOuterVarString (char *string, char *substr);
int ExpandVarbinserv (char *string, char *buffer, char *bserver);
enum vnames ScanVariable (char *name);
struct Item *SplitVarstring (char *varstring);
struct Item *SplitString (char *varstring,char sep);

/* wildcard.c */

int IsWildCard (char *str);
int WildMatch (char *wildptr,char *cmpptr);
char *AfterSubString (char *big, char *small, int status, char lastwild);

/* wrapper.c */

void TidyWrapper (char *startpath, void *vp);
void RecHomeTidyWrapper (char *startpath, void *vp);
void CheckFileWrapper (char *startpath, void *vp);
void DirectoriesWrapper (char *dir, void *vp);


#ifdef HPuUX
int Error;
#endif
