#ifndef CFENGINE_FILES_EDIT_H
#define CFENGINE_FILES_EDIT_H

#include "cf3.defs.h"

EditContext *NewEditContext(EvalContext *ctx, char *filename, Attributes a, const Promise *pp);
void FinishEditContext(EvalContext *ctx, EditContext *ec, Attributes a, Promise *pp);
int SaveItemListAsFile(EvalContext *ctx, Item *liststart, const char *file, Attributes a, Promise *pp);

#ifdef HAVE_LIBXML2
int LoadFileAsXmlDoc(EvalContext *ctx, xmlDocPtr *doc, const char *file, Attributes a, const Promise *pp);
int SaveXmlDocAsFile(EvalContext *ctx, xmlDocPtr doc, const char *file, Attributes a, Promise *pp);
#endif

#endif
