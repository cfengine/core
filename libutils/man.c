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

#include "man.h"

#include "string_lib.h"

#include <time.h>

static void WriteCopyright(Writer *out)
{
    static const char *copyright =
        ".\\\"Copyright (C) CFEngine AS\n"
        ".\\\"\n"
        ".\\\"This file is part of CFEngine 3 - written and maintained by CFEngine AS.\n"
        ".\\\"\n"
        ".\\\"This program is free software; you can redistribute it and/or modify it\n"
        ".\\\"under the terms of the GNU General Public License as published by the\n"
        ".\\\"Free Software Foundation; version 3.\n"
        ".\\\"\n"
        ".\\\"This program is distributed in the hope that it will be useful,\n"
        ".\\\"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        ".\\\"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        ".\\\"GNU General Public License for more details.\n"
        ".\\\"\n"
        ".\\\"You should have received a copy of the GNU General Public License\n"
        ".\\\"along with this program; if not, write to the Free Software\n"
        ".\\\"Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA\n"
        ".\\\"\n"
        ".\\\"To the extent this program is licensed as part of the Enterprise\n"
        ".\\\"versions of CFEngine, the applicable Commerical Open Source License\n"
        ".\\\"(COSL) may apply to this file if you as a licensee so wish it. See\n"
        ".\\\"included file COSL.txt.\n";

    WriterWrite(out, copyright);
}

static void WriteHeader(Writer *out, const char *program, time_t last_modified)
{
    char program_upper[256] = { 0 };
    snprintf(program_upper, 255, "%s", program);
    ToUpperStrInplace(program_upper);

    char date_modified[20] = { 0 };
    {
        struct tm t;
        gmtime_r(&last_modified, &t);
        strftime(date_modified, 19, "%Y-%m-%d", &t);
    }

    WriterWriteF(out, ".TH %s 8 \"%s\" \"CFEngine\" \"System Administration\"\n", program_upper, date_modified);
}

static void WriteAvailability(Writer *out, const char *program)
{
    static const char *availability =
        ".SH AVAILABILITY\n"
        "%s is part of CFEngine.\n"
        ".br\n"
        "Binary packages may be downloaded from http://cfengine.com/downloads/.\n"
        ".br\n"
        "The source code is avaiable at http://github.com/cfengine/\n";

    WriterWriteF(out, availability, program);
}

static void WriteAuthor(Writer *out)
{
    static const char *author =
        ".SH AUTHOR\n"
        "Mark Burgess and CFEngine AS\n";

    WriterWrite(out, author);
}

static void WriteName(Writer *out, const char *program, const char *short_description)
{
    static const char *author =
        ".SH NAME\n"
        "%s \\- %s\n";

    WriterWriteF(out, author, program, short_description);
}

static void WriteSynopsis(Writer *out, const char *program, bool accepts_file_argument)
{
    static const char *synopsis =
        ".SH SYNOPSIS\n"
        ".B %s\n"
        ".RI [ OPTION ]...\n";
    WriterWriteF(out, synopsis, program);
    if (accepts_file_argument)
    {
        WriterWrite(out, ".RI [ FILE ]\n");
    }
    else
    {
        WriterWrite(out, "\n");
    }
}

static void WriteDescription(Writer *out, const char *description)
{
    WriterWriteF(out, ".SH DESCRIPTION\n%s\n", description);
}

static void WriteOptions(Writer *out, const struct option options[], const char *option_hints[])
{
    WriterWrite(out, ".SH OPTIONS\n");

    for (int i = 0; options[i].name != NULL; i++)
    {
        if (options[i].has_arg)
        {
            WriterWriteF(out, ".IP \"--%s, -%c\" value\n%s\n", options[i].name, (char) options[i].val, option_hints[i]);
        }
        else
        {
            WriterWriteF(out, ".IP \"--%s, -%c\"\n%s\n", options[i].name, (char) options[i].val, option_hints[i]);
        }
    }
}

static void WriteSeeAlso(Writer *out)
{
    static const char *see_also =
            ".SH \"SEE ALSO\"\n"
            ".BR cf-promises (8),\n"
            ".BR cf-agent (8),\n"
            ".BR cf-serverd (8),\n"
            ".BR cf-execd (8),\n"
            ".BR cf-monitord (8),\n"
            ".BR cf-runagent (8),\n"
            ".BR cf-key (8)\n";

    WriterWrite(out, see_also);
}

static void WriteBugs(Writer *out)
{
    static const char *bugs =
            ".SH BUGS\n"
            "Please see the public bug-tracker at http://bug.cfengine.com/.\n"
            ".br\n"
            "GitHub pull-requests may be submitted to http://github.com/cfengine/core/.\n";

    WriterWrite(out, bugs);
}

void ManPageWrite(Writer *out, const char *program, time_t last_modified, const char *short_description,
                  const char *long_description, const struct option options[], const char *option_hints[], bool accepts_file_argument)
{
    WriteCopyright(out);
    WriteHeader(out, program, last_modified);
    WriteName(out, program, short_description);
    WriteSynopsis(out, program, accepts_file_argument);
    WriteDescription(out, long_description);
    WriteOptions(out, options, option_hints);
    WriteAvailability(out, program);
    WriteBugs(out);
    WriteSeeAlso(out);
    WriteAuthor(out);
}
