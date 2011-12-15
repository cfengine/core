#ifndef CFENGINE_RLIST_H
#define CFENGINE_RLIST_H

#include "cf.defs.h"
#include "conf.h"

struct Rval
   {
   void *item;        /* (char *), (struct Rlist *), or (struct FnCall)  */
   char rtype;        /* Points to CF_SCALAR, CF_LIST, CF_FNCALL usually */
   };

struct Rlist
   {
   void *item;
   char type;
   struct Rlist *state_ptr; /* Points to "current" state/element of sub-list */
   struct Rlist *next;
   };

char *ScalarValue(struct Rlist *rlist);
struct FnCall *FnCallValue(struct Rlist *rlist);
struct Rlist *ListValue(struct Rlist *rlist);

int PrintRval(char *buffer,int bufsize, struct Rval rval);
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

void *CopyRvalItem(const void *item, char type);
void DeleteRvalItem(struct Rval rval);
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

struct Rlist *RlistAppendReference(struct Rlist **start,void *item, char type);

void ShowRlist(FILE *fp,struct Rlist *list);
void ShowRval(FILE *fp, struct Rval rval);

#endif
