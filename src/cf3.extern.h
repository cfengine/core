/*****************************************************************************/
/*                                                                           */
/* File: cf3.extern.h                                                        */
/*                                                                           */
/* Created: Thu Aug  2 12:51:18 2007                                         */
/*                                                                           */
/*****************************************************************************/

/* See variables in cf3globals.c and syntax.c */

extern struct PromiseParser P;

extern struct Bundle *BUNDLES;
extern struct Body *BODIES;
extern struct Scope *VSCOPE;
extern struct Audit *AUDITPTR;
extern struct Audit *VAUDIT; 
extern struct Rlist *VINPUTLIST;
extern struct Rlist *BODYPARTS;

extern int XML;
extern FILE *FOUT;
extern struct FnCallStatus FNCALL_STATUS;

extern struct SubTypeSyntax CF_NOSTYPE;
extern char *CF_DATATYPES[];


/***********************************************************/
/* SYNTAX MODULES                                          */
/***********************************************************/

#ifndef CF3_MOD_COMMON
extern struct SubTypeSyntax CF_COMMON_SUBTYPES[];
extern struct BodySyntax CF_BODY_TRANSACTION[];
extern struct BodySyntax CF_VARBODY[];
extern struct BodySyntax CF_CLASSBODY[];
extern struct BodySyntax CFG_CONTROL[];
extern struct BodySyntax CFA_CONTROL[];
extern struct BodySyntax CFS_CONTROL[];
extern struct BodySyntax CFE_CONTROL[];
extern struct BodySyntax CFR_CONTROL[];
extern struct BodySyntax CFEX_CONTROL[];
extern struct BodySyntax CF_TRIGGER_BODY[];

extern struct BodySyntax CF_TRANSACTION_BODY[];
extern struct BodySyntax CF_DEFINECLASS_BODY[];
extern struct BodySyntax CF_COMMON_BODIES[];

extern struct SubTypeSyntax *CF_ALL_SUBTYPES[];
extern struct SubTypeSyntax CF_ALL_BODIES[];
extern struct FnCallType CF_FNCALL_TYPES[];
#endif

#ifndef CF3_MOD_FILES
extern struct SubTypeSyntax CF_FILES_SUBTYPES[];
extern struct BodySyntax CF_APPEND_REPL_BODIES[];
extern struct BodySyntax CF_FILES_BODIES[];
extern struct BodySyntax CF_SELECTFILES_BODY[];
extern struct BodySyntax CF_COPYFROM_BODY[];
extern struct BodySyntax CF_LINKTO_BODY[];
extern struct BodySyntax CF_FILEFILTER_BODY[];
extern struct BodySyntax CF_CHANGEMGT_BODY[];
extern struct BodySyntax CF_TIDY_BODY[];
extern struct BodySyntax CF_RENAME_BODY[];
#endif

#ifndef CF3_MOD_EXEC
extern struct SubTypeSyntax CF_EXEC_SUBTYPES[];
#endif

#ifndef CF3_MOD_PROCESS
extern struct SubTypeSyntax CF_PROCESS_SUBTYPES[];
extern struct BodySyntax CF_MATCHCLASS_BODY[];
extern struct BodySyntax CF_PROCFILTER_BODY[];
extern struct BodySyntax CF_PROCESS_BODIES[];
#endif
