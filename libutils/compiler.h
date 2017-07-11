/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#ifndef CFENGINE_COMPILER_H
#define CFENGINE_COMPILER_H

/* Compiler-specific options/defines */


#if defined(__GNUC__) && (__GNUC__ >= 3)


#  define FUNC_ATTR_NORETURN  __attribute__((noreturn))
#  define FUNC_ATTR_PRINTF(string_index, first_to_check) \
     __attribute__((format(__printf__, string_index, first_to_check)))
#  define FUNC_UNUSED __attribute__((unused))
#  define ARG_UNUSED __attribute__((unused))
#  define FUNC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

#  if (__GNUC__ >= 4) && (__GNUC_MINOR__ >=5)
#    define FUNC_DEPRECATED(msg) __attribute__((deprecated(msg)))
#  else
#    define FUNC_DEPRECATED(msg) __attribute__((deprecated))
#  endif


#else /* not gcc >= 3.0 */


#  define FUNC_ATTR_NORETURN
#  define FUNC_ATTR_PRINTF(string_index, first_to_check)
#  define FUNC_UNUSED
#  define ARG_UNUSED
#  define FUNC_WARN_UNUSED_RESULT

#  define FUNC_DEPRECATED(msg)


#endif  /* gcc >= 3.0 */



/**
 *  If you have a variable or function parameter unused under specific
 *  conditions (like ifdefs), you can suppress the "unused variable" warning
 *  by just doing UNUSED(x).
 */
#define UNUSED(x) (void)(x)



#endif  /* CFENGINE_COMPILER_H */
