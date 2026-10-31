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

#include <watcher_report.h>
#include <reactor_watch_backend.h>  // REACTOR_WATCHERS_REPORT_FILE
#include <logging.h>
#include <alloc.h>
#include <file_lib.h>           // safe_fopen()
#include <writer.h>             // StringWriter()
#include <string_lib.h>         // StringEqual()
#include <known_dirs.h>         // GetStateDir()

/* What the report file was last written with. Only used by the thread that
 * writes the reports (the main thread). */
static char *last_written = NULL;

static bool ReportPaths(char *path, size_t path_size, char *tmp_path, size_t tmp_path_size)
{
    int ret = snprintf(path, path_size, "%s%c%s",
                       GetStateDir(), FILE_SEPARATOR, REACTOR_WATCHERS_REPORT_FILE);
    if (ret < 0 || (size_t) ret >= path_size)
    {
        return false;
    }
    ret = snprintf(tmp_path, tmp_path_size, "%s.tmp", path);
    return (ret >= 0 && (size_t) ret < tmp_path_size);
}

void WatchersReportWrite(const JsonElement *report)
{
    assert(report != NULL);
    if (report == NULL)
    {
        return;
    }

    Writer *writer = StringWriter();
    JsonWrite(writer, report, 0);
    char *content = StringWriterClose(writer);

    /* Rewriting it with the same content would still change its mtime, which
     * the reader takes for the time the watchers were updated */
    if (last_written != NULL && StringEqual(last_written, content))
    {
        free(content);
        return;
    }

    char path[PATH_MAX];
    char tmp_path[PATH_MAX];
    if (!ReportPaths(path, sizeof(path), tmp_path, sizeof(tmp_path)))
    {
        Log(LOG_LEVEL_VERBOSE, "The path of the reactor watchers report is too long");
        free(content);
        return;
    }

    FILE *file = safe_fopen(tmp_path, "w");
    if (file == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to write the reactor watchers report '%s': %s",
            tmp_path, GetErrorStr());
        free(content);
        return;
    }

    const size_t length = strlen(content);
    const bool written = (fwrite(content, 1, length, file) == length);
    const bool closed = (fclose(file) == 0);
    if (!written || !closed)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to write the reactor watchers report '%s': %s",
            tmp_path, GetErrorStr());
        unlink(tmp_path);
        free(content);
        return;
    }

    if (rename(tmp_path, path) != 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to rename '%s' to '%s': %s",
            tmp_path, path, GetErrorStr());
        unlink(tmp_path);
        free(content);
        return;
    }

    free(last_written);
    last_written = content;
}

void WatchersReportRemove(void)
{
    char path[PATH_MAX];
    char tmp_path[PATH_MAX];
    if (ReportPaths(path, sizeof(path), tmp_path, sizeof(tmp_path)) &&
        unlink(path) != 0 && errno != ENOENT)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to remove the reactor watchers report '%s': %s",
            path, GetErrorStr());
    }

    free(last_written);
    last_written = NULL;
}
