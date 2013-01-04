/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_COMPILER_H
#define CFENGINE_COMPILER_H

/* Compiler-specific options/defines */

#if defined(__GNUC__) && (__GNUC__ * 100 >= 3)
# define FUNC_ATTR_NORETURN  __attribute__((noreturn))
#else /* not gcc >= 3.0 */
# define FUNC_ATTR_NORETURN
#endif

#if defined(__GNUC__)
# if defined (__MINGW32__)
#  define FUNC_ATTR_PRINTF(string_index, first_to_check) \
    __attribute__((format(gnu_printf, string_index, first_to_check)))
# else
#  define FUNC_ATTR_PRINTF(string_index, first_to_check) \
    __attribute__((format(printf, string_index, first_to_check)))
# endif
#else
# define FUNC_ATTR_PRINTF(string_index, first_to_check)
#endif

#if defined(__GNUC__)
#  define FUNC_DEPRECATED \
    __attribute__((deprecated))
#else
#  define FUNC_DEPRECATED(warning_text)
#endif

#endif
