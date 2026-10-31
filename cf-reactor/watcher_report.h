/*
  Copyright 2026 Northern.tech AS

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

#ifndef CFENGINE_WATCHER_REPORT_H
#define CFENGINE_WATCHER_REPORT_H

#include <json.h>

/**
 * @brief Writes the report of the watchers to REACTOR_WATCHERS_REPORT_FILE in
 * the state directory, for Enterprise reporting.
 *
 * Written aside and renamed over, so that a reader never sees half of it, and
 * only if it differs from the last one written, to keep the file's mtime (the
 * reader's "updated at") for when the watchers changed. Failures are logged,
 * not returned: the report is best effort, and a failed write keeps the old one.
 */
void WatchersReportWrite(const JsonElement *report);

/**
 * @brief Removes the report, so that the watchers of a stopped cf-reactor
 * don't linger in reporting.
 */
void WatchersReportRemove(void);

#endif // CFENGINE_WATCHER_REPORT_H
