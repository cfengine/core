/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

typedef enum {
    LogCritical,
    LogNormal,
    LogVerbose,
    LogDebug
} LogLevel;

/**
  @brief Initializes log system.
  @param path Log file
  */
void logInit();
/**
  @brief Finishes the log system.
  */
void logFinish();
/**
  @brief Write a log entry.
  @param level Log level.
  @param format Format of the entry
  @param ...
  */
void log_entry(LogLevel level, char *format, ...);

#endif // LOG_H
