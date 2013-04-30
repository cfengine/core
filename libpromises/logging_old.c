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

#include "logging_old.h"
#include "logging.h"

#include "misc_lib.h"
#include "string_lib.h"

static LogLevel OutputLevelToLogLevel(OutputLevel level)
{
    switch (level)
    {
    case OUTPUT_LEVEL_ERROR: return LOG_LEVEL_NOTICE; /* default level includes warnings and notices */
    case OUTPUT_LEVEL_VERBOSE: return LOG_LEVEL_VERBOSE;
    case OUTPUT_LEVEL_INFORM: return LOG_LEVEL_INFO;
    }

    ProgrammingError("Unknown output level passed to OutputLevelToLogLevel: %d", level);
}

void CfVOut(OutputLevel level, const char *errstr, const char *fmt, va_list ap)
{
    const char *GetErrorStr(void);

    if (strchr(fmt, '\n'))
    {
        char *fmtcopy = xstrdup(fmt);
        Chop(fmtcopy, strlen(fmtcopy));
        VLog(OutputLevelToLogLevel(level), fmtcopy, ap);
        free(fmtcopy);
    }
    else
    {
        VLog(OutputLevelToLogLevel(level), fmt, ap);
    }

    if (errstr && strlen(errstr) > 0)
    {
        Log(OutputLevelToLogLevel(level), " !!! System reports error for %s: \"%s\"", errstr, GetErrorStr());
    }
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CfVOut(level, errstr, fmt, ap);
    va_end(ap);
}
