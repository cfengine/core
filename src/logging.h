#ifndef CFENGINE_LOGGING_H
#define CFENGINE_LOGGING_H

#include "cf3.defs.h"

void BeginAudit(void);
void EndAudit(void);
void ClassAuditLog(const Promise *pp, Attributes attr, char status, char *reason);
void PromiseLog(char *s);
void FatalError(char *s, ...) FUNC_ATTR_NORETURN FUNC_ATTR_PRINTF(1, 2);

void __ProgrammingError(const char *file, int lineno, const char *format, ...);
#define ProgrammingError(...) __ProgrammingError(__FILE__, __LINE__, __VA_ARGS__)

#endif
