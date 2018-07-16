#include <cf3.defs.h>

/*
 * Stubs which allow CFEngine compiled with gcov support to link against unit
 * test code which has gcov disabled.
 */

void __gcov_init(ARG_UNUSED void *p)
{
}

void __gcov_merge_add(ARG_UNUSED void *p, ARG_UNUSED unsigned n_counters)
{
}

int __gcov_execv(const char *path, char *const argv[])
{
    return execv(path, argv);
}

int __gcov_execl(const char *path, char *arg, ...)
{
    va_list ap, aq;
    unsigned i, length;
    char **args;

    va_start(ap, arg);
    va_copy(aq, ap);

    length = 2;
    while (va_arg(ap, char *))
             length++;

    va_end(ap);

    args = (char **) xmalloc(length * sizeof(void *));
    args[0] = arg;
    for (i = 1; i < length; i++)
        args[i] = va_arg(aq, char *);

    va_end(aq);

    return execv(path, args);
}

pid_t __gcov_fork(void)
{
/* well, for now, if windows, don't fork at all, what will happen? */
#ifndef __MINGW32__
    return fork();
#else
    return 0;
#endif
}
