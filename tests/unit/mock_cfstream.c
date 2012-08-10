#include "cf3.defs.h"

#include <stdarg.h>

void FatalError(char *s, ...)
{
    va_list ap;

    va_start(ap, s);
    vfprintf(stderr, s, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

void cfPS(enum cfreport level, char status, char *errstr, Promise *pp, Attributes attr, char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "cfPS[%d,%c,%s,%p] ", level, status, errstr, pp);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

void CfOut(enum cfreport level, const char *errstr, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "CfOut[%d,%s] ", level, errstr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
