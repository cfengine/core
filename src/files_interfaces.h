#ifndef CFENGINE_FILES_INTERFACES_H
#define CFENGINE_FILES_INTERFACES_H

#include "cf3.defs.h"

void SourceSearchAndCopy(char *from, char *to, int maxrecurse, Attributes attr, Promise *pp, const ReportContext *report_context);
void VerifyCopy(char *source, char *destination, Attributes attr, Promise *pp, const ReportContext *report_context);
void LinkCopy(char *sourcefile, char *destfile, struct stat *sb, Attributes attr, Promise *pp, const ReportContext *report_context);
int cfstat(const char *path, struct stat *buf);
int cf_stat(char *file, struct stat *buf, Attributes attr, Promise *pp);
int cf_lstat(char *file, struct stat *buf, Attributes attr, Promise *pp);
int CopyRegularFile(char *source, char *dest, struct stat sstat, struct stat dstat, Attributes attr, Promise *pp, const ReportContext *report_context);
int CfReadLine(char *buff, size_t size, FILE *fp);
int cf_readlink(char *sourcefile, char *linkbuf, int buffsize, Attributes attr, Promise *pp);

#endif
