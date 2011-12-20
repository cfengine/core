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

#include "writer.h"

typedef enum WriterType
   {
   WT_STRING,
   WT_FILE,
   } WriterType;

struct StringWriterImpl
   {
   char *data;
   size_t len; /* Does not include trailing zero */
   size_t allocated; /* Includes trailing zero */
   };

struct Writer
   {
   WriterType type;
   union
      {
      struct StringWriterImpl string;
      FILE *file;
      };
   };

/*********************************************************************/

Writer *FileWriter(FILE *file)
{
Writer *stream = xcalloc(1, sizeof(struct Writer));
stream->type = WT_FILE;
stream->file = file;
return stream;
}

/*********************************************************************/

Writer *StringWriter(void)
{
Writer *stream = xcalloc(1, sizeof(struct Writer));
stream->type = WT_STRING;
stream->string.data = xstrdup("");
stream->string.allocated = 1;
}

/*********************************************************************/

static size_t StringWriterWrite(Writer *stream, const char *str)
{
size_t len = strlen(str);

if (stream->string.len + len + 1 > stream->string.allocated)
   {
   stream->string.allocated = MAX(stream->string.allocated * 2,
                                  stream->string.len + len + 1);
   stream->string.data = xrealloc(stream->string.data,
                                  stream->string.allocated);
   }

strlcpy(stream->string.data + stream->string.len, str, len + 1);
stream->string.len += len;

return len;
}

/*********************************************************************/

static size_t FileWriterWrite(Writer *stream, const char *str)
{
return fwrite(str, 1, strlen(str), stream->file);
}

/*********************************************************************/

size_t WriterWrite(Writer *stream, const char *str)
{
if (stream->type == WT_STRING)
   {
   return StringWriterWrite(stream, str);
   }
else
   {
   return FileWriterWrite(stream, str);
   }
}

/*********************************************************************/

size_t StringWriterLength(Writer *stream)
{
if (stream->type != WT_STRING)
   {
   FatalError("Wrong writer type");
   }

return stream->string.len;
}

/*********************************************************************/

const char *StringWriterData(Writer *stream)
{
if (stream->type != WT_STRING)
   {
   FatalError("Wrong writer type");
   }

return stream->string.data;
}

/*********************************************************************/

void WriterClose(Writer *stream)
{
if (stream->type == WT_STRING)
   {
   free(stream->string.data);
   }
else
   {
   fclose(stream->file);
   }
free(stream);
}
