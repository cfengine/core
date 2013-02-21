#ifndef CFENGINE_FILES_EDIT_H
#define CFENGINE_FILES_EDIT_H

#include "cf3.defs.h"

EditContext *NewEditContext(char *filename, Attributes a, const Promise *pp);
void FinishEditContext(EditContext *ec, Attributes a, Promise *pp);
int SaveItemListAsFile(Item *liststart, const char *file, Attributes a, Promise *pp);
int AppendIfNoSuchLine(const char *filename, const char *line);

#ifdef HAVE_LIBXML2
int LoadFileAsXmlDoc(xmlDocPtr *doc, const char *file, Attributes a, const Promise *pp);
int SaveXmlDocAsFile(xmlDocPtr doc, const char *file, Attributes a, Promise *pp);
#endif

#endif
