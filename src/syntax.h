#ifndef CFENGINE_SYNTAX_H
#define CFENGINE_SYNTAX_H

#include "cf3.defs.h"

int LvalWantsBody(char *stype,char *lval);
int CheckParseVariableName(char *name);
void CheckBundle(char *name,char *type);
void CheckBody(char *name,char *type);
struct SubTypeSyntax CheckSubType(char *btype,char *type);
void CheckConstraint(char *type,char *name,char *lval,void *rval,char rvaltype,struct SubTypeSyntax ss);
void CheckSelection(char *type,char *name,char *lval,void *rval,char rvaltype);
void CheckConstraintTypeMatch(char *lval,void *rval,char rvaltype,enum cfdatatype dt,char *range,int level);
void CheckPromise(struct Promise *pp);
int CheckParseClass(char *lv,char *s,char *range);
enum cfdatatype StringDataType(char *scopeid,char *string);
enum cfdatatype ExpectedDataType(char *lvalname);
bool IsDataType(const char *s);

void SyntaxPrintAsJson(FILE *out);
void PolicyPrintAsJson(FILE *out, const char *filename, struct Bundle *bundles, struct Body *bodies);


#endif
