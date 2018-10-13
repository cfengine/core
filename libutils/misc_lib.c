/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <misc_lib.h>

#include <platform.h>
#include <alloc.h>
#include <logging.h>
#include <sequence.h>

#include <stdarg.h>

unsigned long UnsignedModulus(long dividend, long divisor)
{
    return ((dividend % divisor) + divisor) % divisor;
}

size_t UpperPowerOfTwo(size_t v)
{
    // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    va_list ap;
    char *fmt = NULL;

    va_start(ap, format);
    xasprintf(&fmt, "%s:%d: Programming Error: %s\n", file, lineno, format);
    vfprintf(stdout, fmt, ap);
    va_end(ap);

    free(fmt);
#ifdef NDEBUG
    exit(255);
#else
    abort();
#endif
}

/**
  @brief Log unexpected runtime error to stderr, do not exit program.
*/

void __UnexpectedError(const char *file, int lineno, const char *format, ...)
{
    va_list ap;
    char *fmt = NULL;

    va_start(ap, format);
    xasprintf(&fmt,
              "%s:%d: Unexpected Error - this is a BUG, please report it: %s\n",
              file, lineno, format);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    free(fmt);
}



void xclock_gettime(clockid_t clk_id, struct timespec *ts)
{
    int ret = clock_gettime(clk_id, ts);
    if (ret != 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "clock_gettime() failed (%s), falling back to time()",
            GetErrorStr());
        *ts = (struct timespec) { .tv_sec = time(NULL) };
    }
}

/**
 * Unchecked version of snprintf(). For when you're *sure* the result fits in
 * the buffer, and you don't want to check it. In other words, NO PART OF THE
 * OUTPUT SHOULD BE DEPENDENT ON USER DATA!
 *
 * Only exception is usage in the unit tests, where we use it all over the
 * place in order to flag stupid programming mistakes.
 */
void xsnprintf(char *str, size_t str_size, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    int ret = vsnprintf(str, str_size, format, ap);
    va_end(ap);

    if (ret < 0)                                                /* error */
    {
        *str = '\0';
        Log(LOG_LEVEL_WARNING, "Unexpected failure from snprint(\"%s\"): %s",
            format, GetErrorStr());
    }
    else if (ret >= str_size)                           /* output truncated */
    {
#ifdef NDEBUG
        UnexpectedError("Result of snprintf(\"%s\") truncated at %zu chars",
                        format, str_size);
#else
        ProgrammingError("Result of snprintf(\"%s\") truncated at %zu chars",
                         format, str_size);
#endif
    }
}

static Seq *PUTENV_STATIC_STRINGS;

// Similar to putenv, but stores the string in a Sequence for free later
// This doesn't make a copy - usually, but not always, you want to:
// putenv_static(xstrdup(s));
// The string is always stored, so you don't have to free on error
int putenv_static(char *s)
{
    if (PUTENV_STATIC_STRINGS == NULL)
    {
        PUTENV_STATIC_STRINGS = SeqNew(10, free);
    }
    SeqAppend(PUTENV_STATIC_STRINGS, s);
    return putenv(s);
}

// WARNING: This function might destroy shared environment
void putenv_static_destroy()
{
    if (PUTENV_STATIC_STRINGS == NULL)
    {
        return;
    }
    size_t len = SeqLength(PUTENV_STATIC_STRINGS);
    for (int i = 0; i < len; ++i)
    {
        const char *env_var = SeqAt(PUTENV_STATIC_STRINGS, len);
        if (env_var == NULL)
        {
            continue;
        }
        size_t len = strlen(env_var);
        char temp[len + 1];
        strncpy(temp, env_var, len);
        temp[len] = '\0'; // strncpy doesn't guarantee termination
        char *equal_sign = strchr(temp, '=');
        if (equal_sign == NULL)
        {
            // We don't care about errors, we're just cleaning up
            continue;
        }
        *equal_sign = '\0';
        unsetenv(env_var);
    }
    DESTROY_AND_NULL(SeqDestroy, PUTENV_STATIC_STRINGS);
}
