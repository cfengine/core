#ifndef CFENGINE_VERIFY_FILES_UTILS_H
#define CFENGINE_VERIFY_FILES_UTILS_H

#include "cf3.defs.h"

int VerifyFileLeaf(char *path, struct stat *sb, Attributes attr, Promise *pp, const ReportContext *report_context);
int DepthSearch(char *name, struct stat *sb, int rlevel, Attributes attr, Promise *pp,
                const ReportContext *report_context);

#endif
