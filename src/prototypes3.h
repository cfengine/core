/* 

        Copyright (C) 1994-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
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

/* assoc.c */

struct CfAssoc *NewAssoc(char *lval,void *rval,char rtype,enum cfdatatype dt);
void DeleteAssoc(struct CfAssoc *ap);
struct CfAssoc *CopyAssoc(struct CfAssoc *old);
void ShowAssoc (struct CfAssoc *cp);
    
/* cfpromises.c */

void Cf3ParseFile(char *filename);
void Initialize(int argc,char **argv);
void CheckOpts(int argc,char **argv);
void Report(char *filename);
void Syntax(void);
void Version(void);
void HashVariables(void);
void CheckControlPromises(char *scope,char *agent,struct Constraint *controllist);
void CheckVariablePromises(char *scope,struct Promise *varlist);
void SetAuditVersion(void);
void VerifyPromises(void);

/* conversion.c */

enum cfdatatype Typename2Datatype(char *name);
enum cfdatatype GetControlDatatype(char *varname,struct BodySyntax *bp);

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

/* install.c */

struct Bundle *AppendBundle(struct Bundle **start,char *name, char *type, struct Rlist *args);
struct Body *AppendBody(struct Body **start,char *name, char *type, struct Rlist *args);
struct SubType *AppendSubType(struct Bundle *bundle,char *typename);
struct SubType *AppendBodyType(struct Body *body,char *typename);
struct Promise *AppendPromise(struct SubType *type,char *promiser, void *promisee,char petype,char *classes,char *bundle);
struct Constraint *AppendConstraint(struct Constraint **conlist,char *lval, void *rval, char type,char *classes);
struct Body *IsBody(struct Body *list,char *key);

/* iteration.c */

struct Rlist *NewIterationContext(char *scopeid,struct Rlist *listvars);
void DeleteIterationContext(struct Rlist *lol);
int IncrementIterationContext(struct Rlist *iterators,int count);
int EndOfIteration(struct Rlist *iterator);

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

/* syntax.c */

void CheckBundle(char *name,char *type);
void CheckBody(char *name,char *type);
struct SubTypeSyntax CheckSubType(char *btype,char *type);
void CheckConstraint(char *type,char *name,char *lval,void *rval,char rvaltype,struct SubTypeSyntax ss);
void CheckSelection(char *type,char *name,char *lval,void *rval,char rvaltype);
void CheckConstraintTypeMatch(char *lval,void *rval,char rvaltype,enum cfdatatype dt,char *range);
void CheckParseString(char *lv,char *s,char *range);
void CheckParseClass(char *lv,char *s,char *range);
void CheckParseInt(char *lv,char *s,char *range);
void CheckParseReal(char *lv,char *s,char *range);
void CheckParseOpts(char *lv,char *s,char *range);
void CheckFnCallType(char *lval,char *s,enum cfdatatype dtype,char *range);
enum cfdatatype StringDataType(char *scopeid,char *string);

/* sysinfo.c */

void GetNameInfo3(void);
void GetInterfaceInfo3(void);
    
/* scope.c */

void SetNewScope(char *id);
void NewScope(char *name);
void DeleteScope(char *name);
struct Scope *GetScope(char *scope);
void CopyScope(char *new, char *old);

/* vars.c */

void NewScalar(char *scope,char *lval,char *rval,enum cfdatatype dt);
void NewList(char *scope,char *lval,void *rval,enum cfdatatype dt);
enum cfdatatype GetVariable(char *scope,char *lval,void **returnv,char *rtype);
void DeleteVariable(char *scope,char *id);
int CompareVariable(char *lval,struct CfAssoc *ap);
void DeleteAllVariables(char *scope);
int StringContainsVar(char *s,char *v);
int DefinedVariable(char *name);
int IsCf3VarString(char *str);

/* expand.c */

void ExpandPromise(char *scopeid,struct Promise *pp);
void ExpandPromiseAndDo(char *scope,struct Promise *p,struct Rlist *scalarvars,struct Rlist *listvars);
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
int IsNakedList(char *str);
void GetNaked(char *s1, char *s2);

/* promises.c */

struct Promise *DeRefCopyPromise(char *scopeid,struct Promise *pp);
void DeletePromise(struct Promise *pp);
struct Promise *ExpandDeRefPromise(char *scopeid,struct Promise *pp);
void DeleteDeRefPromise(char *scopeid,struct Promise *pp);

/* selfdiagnostic.c */

void SelfDiagnostic(void);
void TestVariableScan(void);
void TestExpandPromise(void);
void TestExpandVariables(void);

/* args.c */

int MapBodyArgs(char *scopeid,struct Rlist *give,struct Rlist *take);
struct Rlist *NewExpArgs(struct FnCall *fp, struct Promise *pp);
void ArgTemplate(struct FnCall *fp,char **argtemplate, enum cfdatatype *argtypes);

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



