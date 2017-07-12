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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>
#define CF_UPGRADE_MAX_ARGUMENTS    32
typedef struct Configuration Configuration;

/**
  @brief Creates a new Configuration structure.
  @return A new Configuration structure or NULL in case of error.
  */
Configuration *ConfigurationNew();
/**
  @brief Destroys a Configuration structure.
  @param configuration Structure to be destroyed.
  */
void ConfigurationDestroy(Configuration **configuration);
/**
  @brief Backup tool path.
  @param configuration Structure.
  @return The path of the backup tool, or NULL in case of error.
  */
const char *ConfigurationBackupTool(const Configuration *configuration);
/**
  @brief Sets the backup tool path.
  @param configuration Structure.
  @param path Path of the backup tool.
  */
void ConfigurationSetBackupTool(Configuration *configuration, char *path);
/**
  @brief Path to create the backup archive.
  @param configuration Structure.
  @return A const char to the path of the backup or NULL in case of error.
  */
const char *ConfigurationBackupPath(const Configuration *configuration);
/**
  @brief Sets the path to write the backup archive.
  @param configuration Structure.
  @param path Path to the backup archive.
  */
void ConfigurationSetBackupPath(Configuration *configuration, char *path);
/**
  @brief Path to cf-upgrade.
  @param configuration Structure.
  @return A const char to the path of cf-upgrade or NULL in case of error.
  */
const char *ConfigurationCFUpgrade(const Configuration *configuration);
/**
  @brief Sets the path to cf-upgrade.
  @param configuration Structure.
  @param path Path to cf-upgrade.
  */
void ConfigurationSetCFUpgrade(Configuration *configuration, char *path);
/**
  @brief Path to the copy of cf-upgrade.
  @param configuration Structure.
  @return A const char to the path of the copy or NULL in case of error.
  */
const char *ConfigurationCopy(const Configuration *configuration);
/**
  @brief Sets the path to the copy of cf-update.
  @param configuration Structure.
  @param path Path to the copy of cf-update.
  */
void ConfigurationSetCopy(Configuration *configuration, char *path);
/**
  @brief CFEngine path, by default /var/cfengine.
  @param configuration Structure.
  @return A const char to the path of CFEngine or NULL in case of error.
  */
const char *ConfigurationCFEnginePath(const Configuration *configuration);
/**
  @brief Sets the CFEngine path.
  @param configuration Structure.
  @param path Path to CFEngine.
  */
void ConfigurationSetCFEnginePath(Configuration *configuration, char *path);
/**
  @brief Command to run to perform the actual update.
  @param configuration Structure.
  @return A const char to the command to run the update process.
  */
const char *ConfigurationCommand(const Configuration *configuration);
/**
  @brief Arguments for the update command.
  @param configuration Structure.
  @return A const char pointer the nth Argument.
  */
const char *ConfigurationArgument(const Configuration *configuration, int number);
/**
  @brief Sets the arguments for the update command.
  @remarks At most 31 parameters can be passed to the update command.
  @param configuration Structure.
  @param argument The argument to add to the arguments.
  */
void ConfigurationAddArgument(Configuration *configuration, char *argument);
/**
  @brief Number of arguments to the upgrade command.
  @param configuration Structure.
  @return The number of arguments for the update command or -1 in case of error.
  */
int ConfigurationNumberOfArguments(const Configuration *configuration);
/**
  @brief Whether we should perform update or copy.
  @param configuration Structure.
  @return True if update, false otherwise.
  */
bool ConfigurationPerformUpdate(const Configuration *configuration);
/**
  @brief Sets whether we should perform update or copy.
  @param configuration Structure.
  @param perform True if update should be performed.
  */
void ConfigurationSetPerformUpdate(Configuration *configuration, bool perform);
/**
  @brief cf-upgrade version
  @param configuration
  @return True if version was requested, false in other case.
  */
bool ConfigurationVersion(Configuration *configuration);
/**
  @brief Sets the version requested flag.
  @param configuration
  */
void ConfigurationSetVersion(Configuration *configuration, bool version);
/**
  @brief cf-upgrade help
  @param configuration
  @return True if help was requested, false in other case.
  */
bool ConfigurationHelp(Configuration *configuration);
/**
  @brief Sets the help requested flag.
  @param configuration
  */
void ConfigurationSetHelp(Configuration *configuration, bool help);

#endif // CONFIGURATION_H
