#ifndef CFENGINE_FILES_EDIT_H
#define CFENGINE_FILES_EDIT_H

#include "cf3.defs.h"

EditContext *NewEditContext(char *filename, Attributes a);
void FinishEditContext(EvalContext *ctx, EditContext *ec, Attributes a, const Promise *pp);

#ifdef HAVE_LIBXML2
int LoadFileAsXmlDoc(xmlDocPtr *doc, const char *file, EditDefaults ed);
int SaveXmlDocAsFile(xmlDocPtr doc, const char *file, Attributes a);
#endif

#endif
