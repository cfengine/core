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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_WRITER_H
#define CFENGINE_WRITER_H

/*
 * Abstract "writer".
 *
 * Writes passed data either to
 *   passed FILE*, or
 *   memory buffer
 */

typedef struct Writer_ Writer;

#include <platform.h>
#include <compiler.h>

Writer *FileWriter(FILE *);
Writer *StringWriter(void);

size_t WriterWriteF(Writer *writer, const char *fmt, ...) FUNC_ATTR_PRINTF(2, 3);
size_t WriterWriteVF(Writer *writer, const char *fmt, va_list ap) FUNC_ATTR_PRINTF(2, 0);

size_t WriterWrite(Writer *writer, const char *str);
size_t WriterWriteLen(Writer *writer, const char *str, size_t len);
size_t WriterWriteChar(Writer *writer, char c);

size_t StringWriterLength(const Writer *writer);
const char *StringWriterData(const Writer *writer);

void WriterClose(Writer *writer);

/* Returns modifiable string and destroys itself */
char *StringWriterClose(Writer *writer) FUNC_WARN_UNUSED_RESULT;

/* Returns the open file and destroys itself */
FILE *FileWriterDetach(Writer *writer);
/* Commonly used on a FileWriter(stdout), ignoring return; so don't
 * try to warn on unused result ! */

 void WriterWriteHelp(Writer *w, const char *comp, const struct option options[], const char *const hints[], bool accepts_file_argument);

#endif
