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

#ifndef CFENGINE_MISC_LIB_H
#define CFENGINE_MISC_LIB_H

#include <platform.h>

#include <compiler.h>


#define ProgrammingError(...) __ProgrammingError(__FILE__, __LINE__, __VA_ARGS__)

/* TODO UnexpectedError() needs
 * to be rate limited to avoid spamming the console.  */
#ifndef NDEBUG
# define UnexpectedError(...) __ProgrammingError(__FILE__, __LINE__, __VA_ARGS__)
#else
# define UnexpectedError(...) __UnexpectedError(__FILE__, __LINE__, __VA_ARGS__)
#endif


/**
 *  CF_ASSERT(condition, message...)
 *
 *  If NDEBUG is defined then the #message is printed and execution continues,
 *  else execution aborts.
 */

# define CF_ASSERT(condition, ...)                                      \
    do {                                                                \
        if (!(condition))                                               \
            UnexpectedError(__VA_ARGS__);                               \
    } while(0)

/**
 * CF_ASSERT_FIX(condition, fix, message...)
 *
 * If NDEBUG is defined then the #message is printed, the #fix is executed,
 * and execution continues. If not NDEBUG, the #fix is ignored and execution
 * is aborted.
 */
# define CF_ASSERT_FIX(condition, fix, ...)                             \
    {                                                                   \
        if (!(condition))                                               \
        {                                                               \
            UnexpectedError(__VA_ARGS__);                               \
            (fix);                                                      \
        }                                                               \
    }


#define ISPOW2(n)     (  (n)>0  &&  ((((n) & ((n)-1)) == 0))  )


/*
  In contrast to the standard C modulus operator (%), this gives
  you an unsigned modulus. So where -1 % 3 => -1,
  UnsignedModulus(-1, 3) => 2.
*/
unsigned long UnsignedModulus(long dividend, long divisor);

size_t UpperPowerOfTwo(size_t v);


void __ProgrammingError(const char *file, int lineno, const char *format, ...) \
    FUNC_ATTR_PRINTF(3, 4) FUNC_ATTR_NORETURN;

void __UnexpectedError(const char *file, int lineno, const char *format, ...) \
    FUNC_ATTR_PRINTF(3, 4);


/**
 * Unchecked versions of common functions, i.e. functions that no longer
 * return anything, but try to continue in case of failure.
 *
 * @NOTE call these only with arguments that will always succeed!
 */


void xclock_gettime(clockid_t clk_id, struct timespec *ts);
void xsnprintf(char *str, size_t str_size, const char *format, ...);

int setenv_wrapper(const char *name, const char *value, int overwrite);
int putenv_wrapper(const char *str);

#endif
