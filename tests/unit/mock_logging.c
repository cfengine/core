#include <eval_context.h>

#include <stdarg.h>

void Log(LogLevel level, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "Log[%d] ", level);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
