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

#include <command_line.h>
#include <log.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define COPY_PARAM          "-c"
#define BACKUP_TOOL_PARAM   "-b"
#define BACKUP_PATH_PARAM   "-s"
#define CFENGINE_PATH       "-f"
#define UPDATE_ARGS         "-i"
#define UPDATE_2ND_ARGS     "-x"
#define HELP_REQUESTED      "-h"
#define VERSION_REQUESTED   "-v"

int parse(int argc, char *argv[], Configuration **configuration)
{
    if (!configuration || !argv || (argc < 2))
    {
        log_entry(LogDebug, "configuration or argv are NULL");
        return -1;
    }
    /* Create the configuration structure */
    *configuration = ConfigurationNew();
    /* The first value is the current executable */
    ConfigurationSetCFUpgrade(*configuration, argv[0]);
    /* Some flags to check for errors */
    bool backup_tool = false;
    bool command_arguments = false;
    bool backup_path = false;
    int i = 1;
    do
    {
        char *current = argv[i];
        log_entry(LogDebug, "current: %s", current);
        if (0 == strcmp(COPY_PARAM, current))
        {
            char *copy = argv[i + 1];
            log_entry(LogDebug, "copy: %s", copy);
            ConfigurationSetCopy(*configuration, copy);
            i += 2;
        }
        else if (0 == strcmp(HELP_REQUESTED, current))
        {
            log_entry(LogDebug, "help requested");
            ConfigurationSetHelp(*configuration, 1);
            return 0;
        }
        else if (0 == strcmp(VERSION_REQUESTED, current))
        {
            log_entry(LogDebug, "version requested");
            ConfigurationSetVersion(*configuration, 1);
            return 0;
        }
        else if (0 == strcmp(BACKUP_TOOL_PARAM, current))
        {
            char *path = argv[i + 1];
            log_entry(LogDebug, "backup tool: %s", path);
            ConfigurationSetBackupTool(*configuration, path);
            i += 2;
            backup_tool = 1;
        }
        else if (0 == strcmp(BACKUP_PATH_PARAM, current))
        {
            char *path = argv[i + 1];
            log_entry(LogDebug, "backup path: %s", path);
            ConfigurationSetBackupPath(*configuration, path);
            i += 2;
            backup_path = true;
        }
        else if (0 == strcmp(CFENGINE_PATH, current))
        {
            char *path = argv[i + 1];
            log_entry(LogDebug, "cfengine path: %s", path);
            ConfigurationSetCFEnginePath(*configuration, path);
            i += 2;
        }
        /*
         * Once we find the -i/-x command we stop parsing and assume that
         * the rest is just update command and its arguments.
         */
        else if (0 == strcmp(UPDATE_ARGS, current))
        {
            log_entry(LogDebug, "Copying and forking");
            int j = 0;
            for (j = i + 1; j < argc; ++j)
            {
                command_arguments = 1;
                char *argument = argv[j];
                log_entry(LogDebug, "argument: %s", argument);
                ConfigurationAddArgument(*configuration, argument);
            }
            break;
        }
        else if (0 == strcmp(UPDATE_2ND_ARGS, current))
        {
            log_entry(LogDebug, "Performing upgrade");
            int j = 0;
            for (j = i + 1; j < argc; ++j)
            {
                command_arguments = 1;
                char *argument = argv[j];
                log_entry(LogDebug, "argument: %s", argument);
                ConfigurationAddArgument(*configuration, argument);
            }
            ConfigurationSetPerformUpdate(*configuration, true);
            break;
        }
        else
        {
            log_entry (LogCritical, "Unrecognized option: %s", current);
            ConfigurationDestroy(configuration);
            return -1;
        }
    } while (i < argc);

    if (!backup_tool || !command_arguments || !backup_path)
    {
        log_entry(LogCritical, "Need to specify -s, -b and -i");
        ConfigurationDestroy(configuration);
        return -1;
    }
    log_entry(LogDebug, "parsed %d options", i);
    return 0;
}
