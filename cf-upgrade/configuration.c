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

#include <configuration.h>
#include <stdlib.h>
#include <alloc-mini.h>
#include <log.h>
#include <string.h>

#define CF_UPGRADE_COPY             "/tmp/cf-upgrade"
#define CF_UPGRADE_CFENGINE         "/var/cfengine/"

struct Configuration {
    char *cf_upgrade;   /*!< Path to cf-upgrade binary */
    char *backup_tool;  /*!< Path to the backup script */
    char *backup_path;  /*!< Path to the backup archive */
    char *copy_path;    /*!< Path to the copy of cf-upgrade, default /tmp/cf-upgrade */
    char *cfengine_path;    /*!< CFEngine folder, default /var/cfengine */
    int number_of_arguments;    /*!< Number of arguments to the upgrade command */
    bool perform_update; /*!< Internal flag, whether to copy and fork or not */
    char *arguments[CF_UPGRADE_MAX_ARGUMENTS]; /*!< upgrade command and arguments */
    bool help;   /*!< Internal flag, whether to print the help message or not */
    bool version;    /*!< Internal flag, whether to print the version or not */
};

Configuration *ConfigurationNew()
{
    Configuration *configuration = NULL;
    configuration = xcalloc(1, sizeof(Configuration));

    configuration->copy_path = xstrdup(CF_UPGRADE_COPY);
    configuration->cfengine_path = xstrdup(CF_UPGRADE_CFENGINE);

    return configuration;
}

void ConfigurationDestroy(Configuration **configuration)
{
    if (!configuration || !*configuration)
    {
        return;
    }
    free ((*configuration)->cf_upgrade);
    free ((*configuration)->backup_path);
    free ((*configuration)->backup_tool);
    free ((*configuration)->copy_path);
    free ((*configuration)->cfengine_path);
    free (*configuration);
    *configuration = NULL;
}

const char *ConfigurationBackupTool(const Configuration *configuration)
{
    return configuration ? configuration->backup_tool : NULL;
}

void ConfigurationSetBackupTool(Configuration *configuration, char *path)
{
    if (!configuration || !path)
    {
        return;
    }
    free (configuration->backup_tool);
    configuration->backup_tool = xstrdup(path);
}

const char *ConfigurationBackupPath(const Configuration *configuration)
{
    return configuration ? configuration->backup_path : NULL;
}

void ConfigurationSetBackupPath(Configuration *configuration, char *path)
{
    if (!configuration || !path)
    {
        return;
    }
    free (configuration->backup_path);
    configuration->backup_path = xstrdup(path);
}

const char *ConfigurationCopy(const Configuration *configuration)
{
    return configuration ? configuration->copy_path : NULL;
}

void ConfigurationSetCopy(Configuration *configuration, char *path)
{
    if (!configuration || !path)
    {
        return;
    }
    free (configuration->copy_path);
    configuration->copy_path = xstrdup(path);
}

const char *ConfigurationCFUpgrade(const Configuration *configuration)
{
    return configuration ? configuration->cf_upgrade : NULL;
}

void ConfigurationSetCFUpgrade(Configuration *configuration, char *path)
{
    if (!configuration || !path)
    {
        return;
    }
    free (configuration->cf_upgrade);
    configuration->cf_upgrade = xstrdup(path);
}

const char *ConfigurationCFEnginePath(const Configuration *configuration)
{
    return configuration ? configuration->cfengine_path : NULL;
}

void ConfigurationSetCFEnginePath(Configuration *configuration, char *path)
{
    if (!configuration || !path)
    {
        return;
    }
    free (configuration->cfengine_path);
    configuration->cfengine_path = xstrdup(path);
}

const char *ConfigurationCommand(const Configuration *configuration)
{
    return configuration ? configuration->arguments[0] : NULL;
}

const char *ConfigurationArgument(const Configuration *configuration, int number)
{
    if (!configuration || (number < 0) ||
            (number >= configuration->number_of_arguments))
    {
        return NULL;
    }
    return configuration->arguments[number];
}

void ConfigurationAddArgument(Configuration *configuration, char *argument)
{
    if (!configuration || !argument)
    {
        return;
    }
    if (configuration->number_of_arguments < CF_UPGRADE_MAX_ARGUMENTS)
    {
        configuration->arguments[configuration->number_of_arguments] =
                xstrdup(argument);
        ++configuration->number_of_arguments;
    }
    else
    {
        log_entry(LogCritical, "A maximum of %d arguments can be specified, aborting",
                  CF_UPGRADE_MAX_ARGUMENTS);
        exit(EXIT_FAILURE);
    }
}

int ConfigurationNumberOfArguments(const Configuration *configuration)
{
    return configuration ? configuration->number_of_arguments : -1;
}

bool ConfigurationPerformUpdate(const Configuration *configuration)
{
    return configuration ? configuration->perform_update : false;
}

void ConfigurationSetPerformUpdate(Configuration *configuration, bool perform)
{
    if (configuration)
    {
        configuration->perform_update = perform;
    }
}

bool ConfigurationVersion(Configuration *configuration)
{
    return configuration ? configuration->version : false;
}

void ConfigurationSetVersion(Configuration *configuration, bool version)
{
    if (configuration)
    {
        configuration->version = version;
    }
}

bool ConfigurationHelp(Configuration *configuration)
{
    return configuration ? configuration->help : false;
}

void ConfigurationSetHelp(Configuration *configuration, bool help)
{
    if (configuration)
    {
        configuration->help = help;
    }
}
