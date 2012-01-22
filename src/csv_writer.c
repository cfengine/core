#include "csv_writer.h"

struct CsvWriter_
   {
   Writer *w;
   bool beginning_of_line;
   };

/*****************************************************************************/

static void WriteCsvEscapedString(Writer *w, const char *str);
static void CsvWriterFieldVF(CsvWriter *csvw, const char *fmt, va_list ap)
    FUNC_ATTR_FORMAT(printf, 2, 0);

/*****************************************************************************/

CsvWriter *CsvWriterOpen(Writer *w)
{
CsvWriter *csvw = xmalloc(sizeof(CsvWriter));
csvw->w = w;
csvw->beginning_of_line = true;
return csvw;
}

/*****************************************************************************/

void CsvWriterField(CsvWriter *csvw, const char *str)
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

void CsvWriterFieldF(CsvWriter *csvw, const char *fmt, ...)
{
va_list ap;
va_start(ap, fmt);
CsvWriterFieldVF(csvw, fmt, ap);
va_end(ap);
}

/*****************************************************************************/

void CsvWriterNewRecord(CsvWriter *csvw)
{
WriterWrite(csvw->w, "\r\n");
csvw->beginning_of_line = true;
}

/*****************************************************************************/

void CsvWriterClose(CsvWriter *csvw)
{
if (!csvw->beginning_of_line)
   {
   WriterWrite(csvw->w, "\r\n");
   }
free(csvw);
}

/*****************************************************************************/

static void CsvWriterFieldVF(CsvWriter *csvw, const char *fmt, va_list ap)
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
