/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_MISC_LIB_H
#define CFENGINE_MISC_LIB_H

#include "compiler.h"

/*
  In contrast to the standard C modulus operator (%), this gives
  you an unsigned modulus. So where -1 % 3 => -1,
  UnsignedModulus(-1, 3) => 2.
*/
unsigned long UnsignedModulus(long dividend, long divisor);

void __ProgrammingError(const char *file, int lineno, const char *format, ...) \
    FUNC_ATTR_PRINTF(3, 4) FUNC_ATTR_NORETURN;
#define ProgrammingError(...) __ProgrammingError(__FILE__, __LINE__, __VA_ARGS__)

void __UnexpectedError(const char *file, int lineno, const char *format, ...) \
    FUNC_ATTR_PRINTF(3, 4);
#define UnexpectedError(...) __UnexpectedError(__FILE__, __LINE__, __VA_ARGS__)

#endif
