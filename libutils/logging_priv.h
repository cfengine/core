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

#ifndef CFENGINE_LOGGING_PRIV_H
#define CFENGINE_LOGGING_PRIV_H

/*
 * This interface is private and intended only for use by logging extensions (such as one defined in libpromises).
 */

typedef struct LoggingPrivContext LoggingPrivContext;

typedef char *(*LoggingPrivLogHook)(LoggingPrivContext *context, LogLevel level, const char *message);

struct LoggingPrivContext
{
    LoggingPrivLogHook log_hook;
    void *param;

    /**
     * Generally the log_hook runs whenever the message is printed either to
     * console or to syslog. You can set this to *additionally* run the hook
     * when the message level is <= force_hook_level.
     *
     * @NOTE the default setting of 0 equals to CRIT level, which is good as
     *       default since the CRIT messages are always printed anyway, so
     *       the log_hook runs anyway.
     */
    LogLevel force_hook_level;
};

/**
 * @brief Attaches context to logging for current thread
 */
void LoggingPrivSetContext(LoggingPrivContext *context);

/**
 * @brief Retrieves logging context for current thread
 */
LoggingPrivContext *LoggingPrivGetContext(void);

/**
 * @brief Set logging (syslog) and reporting (stdout) level for current thread
 */
void LoggingPrivSetLevels(LogLevel log_level, LogLevel report_level);

#endif
