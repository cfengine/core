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

#include <csv_writer.h>

#include <alloc.h>

struct CsvWriter_
{
    Writer *w;
    bool beginning_of_line;
};

/*****************************************************************************/

static void WriteCsvEscapedString(Writer *w, const char *str);
static void CsvWriterFieldVF(CsvWriter * csvw, const char *fmt, va_list ap) FUNC_ATTR_PRINTF(2, 0);

/*****************************************************************************/

CsvWriter *CsvWriterOpen(Writer *w)
{
    CsvWriter *csvw = xmalloc(sizeof(CsvWriter));

    csvw->w = w;
    csvw->beginning_of_line = true;
    return csvw;
}

/*****************************************************************************/

void CsvWriterField(CsvWriter * csvw, const char *str)
{
    if (csvw->beginning_of_line)
    {
        csvw->beginning_of_line = false;
    }
    else
    {
        WriterWriteChar(csvw->w, ',');
    }

    if (strpbrk(str, "\",\r\n"))
    {
        WriteCsvEscapedString(csvw->w, str);
    }
    else
    {
        WriterWrite(csvw->w, str);
    }
}

/*****************************************************************************/

void CsvWriterFieldF(CsvWriter * csvw, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    CsvWriterFieldVF(csvw, fmt, ap);
    va_end(ap);
}

/*****************************************************************************/

void CsvWriterNewRecord(CsvWriter * csvw)
{
    WriterWrite(csvw->w, "\r\n");
    csvw->beginning_of_line = true;
}

/*****************************************************************************/

void CsvWriterClose(CsvWriter * csvw)
{
    if (!csvw->beginning_of_line)
    {
        WriterWrite(csvw->w, "\r\n");
    }
    free(csvw);
}

/*****************************************************************************/

static void CsvWriterFieldVF(CsvWriter * csvw, const char *fmt, va_list ap)
{
/*
 * We are unable to avoid allocating memory here, as we need full string
 * contents before deciding whether there is a " in string, and hence whether
 * the field needs to be escaped in CSV
 */
    char *str;

    xvasprintf(&str, fmt, ap);
    CsvWriterField(csvw, str);
    free(str);
}

/*****************************************************************************/

static void WriteCsvEscapedString(Writer *w, const char *s)
{
    WriterWriteChar(w, '"');
    while (*s)
    {
        if (*s == '"')
        {
            WriterWriteChar(w, '"');
        }
        WriterWriteChar(w, *s);
        s++;
    }
    WriterWriteChar(w, '"');
}

Writer *CsvWriterGetWriter(CsvWriter *csvw)
{
    return csvw->w;
}
