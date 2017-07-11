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

#ifndef CFENGINE_CSV_WRITER_H
#define CFENGINE_CSV_WRITER_H

/*
 * This writer implements CSV as in RFC 4180
 */

#include <platform.h>
#include <writer.h>

typedef struct CsvWriter_ CsvWriter;

CsvWriter *CsvWriterOpen(Writer *w);

void CsvWriterField(CsvWriter *csvw, const char *str);
void CsvWriterFieldF(CsvWriter *csvw, const char *fmt, ...) FUNC_ATTR_PRINTF(2, 3);

void CsvWriterNewRecord(CsvWriter *csvw);

/* Does not close underlying Writer, but flushes all pending data */
void CsvWriterClose(CsvWriter *csvw);

/**
 * @return The instance of the underlying writer
 */
Writer *CsvWriterGetWriter(CsvWriter *csvw);

#endif
