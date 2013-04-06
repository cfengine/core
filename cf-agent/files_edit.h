#ifndef CFENGINE_FILES_EDIT_H
#define CFENGINE_FILES_EDIT_H

#include "cf3.defs.h"

EditContext *NewEditContext(char *filename, Attributes a);
void FinishEditContext(EvalContext *ctx, EditContext *ec, Attributes a, Promise *pp);
int SaveItemListAsFile(EvalContext *ctx, Item *liststart, const char *file, Attributes a, Promise *pp);

#ifdef HAVE_LIBXML2
int LoadFileAsXmlDoc(xmlDocPtr *doc, const char *file, EditDefaults edits);
int SaveXmlDocAsFile(EvalContext *ctx, xmlDocPtr doc, const char *file, Attributes a, Promise *pp);
#endif

#endif
