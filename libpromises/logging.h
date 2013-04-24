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

#ifndef CFENGINE_LOGGING_H
#define CFENGINE_LOGGING_H

#include "cf3.defs.h"

/* Obsolete, please use Log instead */
void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...) FUNC_ATTR_PRINTF(3, 4);
void CfVOut(OutputLevel level, const char *errstr, const char *fmt, va_list ap);

typedef enum
{
    LOG_LEVEL_CRIT,
    LOG_LEVEL_ERR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_NOTICE,
    LOG_LEVEL_INFO,
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_DEBUG,
} LogLevel;

void Log(LogLevel level, const char *fmt, ...) FUNC_ATTR_PRINTF(2, 3);
void VLog(LogLevel level, const char *fmt, va_list ap);

/* Promise-specific. To be split out of main logging functionality. */

#include "policy.h"

/**
 * @brief Binds logging in current thread to EvalContext.
 */
void LoggingInit(const EvalContext *ctx);

/**
 * @brief Calculates and sets logging context for the promise.
 */
void LoggingPromiseEnter(const EvalContext *ctx, const Promise *pp);

/**
 * @brief Finishes processing the promise and looks up the last error message associated with it.
 *
 * @return Last log message recorded for the promise, or NULL if there were none. Caller owns the memory.
 */
char *LoggingPromiseFinish(const EvalContext *ctx, const Promise *pp);

/**
 * @brief Unbinds logging from EvalContext.
 */
void LoggingFinish(const EvalContext *ctx);

#endif
