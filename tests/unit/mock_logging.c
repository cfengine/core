#include "logging.h"
#include "env_context.h"

#include <stdarg.h>

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "CfOut[%d,%s] ", level, errstr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
