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

#include <update.h>
#include <process.h>
#include <command_line.h>
#include <log.h>
#include <alloc-mini.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <assert.h>


extern char **environ;


#ifndef __MINGW32__
/* Unix implementation */
int private_copy_to_temporary_location(const char *source, const char *destination)
{
    struct stat source_stat;
    int source_fd = -1;
    int destination_fd = -1;

    source_fd = open(source, O_RDONLY);
    if (source_fd < 0)
    {
        goto bad_nofd;
    }
    fstat(source_fd, &source_stat);
    unlink (destination);
    destination_fd = open(destination, O_WRONLY|O_CREAT|O_EXCL, S_IRWXU|S_IRGRP|S_IROTH);
    if (destination_fd < 0)
    {
        goto bad_onefd;
    }
    char buffer[1024];
    int so_far = 0;
    do
    {
        int this_read = 0;
        int this_write = 0;
        this_read = read(source_fd, buffer, sizeof(buffer));
        if (this_read < 0)
        {
            log_entry(LogCritical, "Failed to read from %s (read so far: %d)",
                source, so_far);
            goto bad_twofd;
        }
        this_write = write(destination_fd, buffer, this_read);
        if (this_write < 0)
        {
            log_entry(LogCritical, "Failed to write to %s (written so far: %d)",
                destination, so_far);
            goto bad_twofd;
        }
        if (this_write != this_read)
        {
            log_entry(LogCritical, "Short write: read: %d, written: %d (prior progress: %d)",
                this_read, this_write, so_far);
            goto bad_twofd;
        }
        so_far += this_read;
    } while (so_far < source_stat.st_size);

    fsync(destination_fd);
    close(source_fd);
    close(destination_fd);
    return 0;
bad_twofd:
    close(destination_fd);
    unlink(destination);
bad_onefd:
    close(source_fd);
bad_nofd:
    return -1;
}
#else
/* Windows implementation */
int private_copy_to_temporary_location(const char *source, const char *destination)
{
    struct stat source_stat;
    int result = 0;
    int source_fd = -1;
    int destination_fd = -1;

    source_fd = open(source, O_BINARY|O_RDONLY);
    if (source_fd < 0)
    {
        goto bad_nofd;
    }
    result = fstat(source_fd, &source_stat);
    unlink (destination);
    destination_fd = open(destination, O_BINARY|O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
    if (destination_fd < 0)
    {
        goto bad_onefd;
    }

    char buffer[1024];
    int so_far = 0;
    int this_read;
    do
    {
        this_read = read(source_fd, buffer, sizeof(buffer));
        if (this_read < 0)
        {
            log_entry(LogCritical, "Failed to read from %s (read so far: %d)",
                      source, so_far);
            goto bad_twofd;
        }
        else if (this_read > 0)                        /* Successful read() */
        {
            so_far += this_read;
            int this_write = write(destination_fd, buffer, this_read);
            if (this_write < 0)
            {
                log_entry(LogCritical,
                          "Failed to write to %s (written so far: %d)",
                          destination, so_far);
                goto bad_twofd;
            }
            else if (this_write != this_read)
            {
                log_entry(LogCritical,
                          "Short write: read: %d, written: %d (prior progress: %d)",
                          this_read, this_write, so_far);
                goto bad_twofd;
            }
        }
    } while (this_read > 0);

    assert(this_read == 0);
    if (so_far != source_stat.st_size)
    {
        log_entry(LogCritical,
                  "Unexpected, file is at EOF while %d out of %d bytes have been read",
                  so_far, source_stat.st_size);
        log_entry(LogCritical,
                  "Trying to continue, maybe it changed size while reading");
    }

    close(source_fd);
    close(destination_fd);
    return 0;

bad_twofd:
    close(destination_fd);
    unlink(destination);
bad_onefd:
    close(source_fd);
bad_nofd:
    return -1;
}
#endif

int copy_to_temporary_location(const char *source, const char *destination)
{
    if (!source || !destination)
    {
        return -1;
    }
    return private_copy_to_temporary_location(source, destination);
}

int perform_backup(const char *backup_tool, const char *backup_path, const char *cfengine)
{
    char **args = NULL;
    char *envp[] = { NULL };
    args = xcalloc(4 + 1, sizeof(char *));
    args[0] = xstrdup(backup_tool);
    args[1] = xstrdup("BACKUP");
    args[2] = xstrdup(backup_path);
    args[3] = xstrdup(cfengine);
    args[4] = NULL;
    int result = 0;
    result =  run_process_wait(backup_tool, args, envp);
    free (args[0]);
    free (args[1]);
    free (args[2]);
    free (args[3]);
    free (args);
    return result;
}

int perform_restore(const char *backup_tool, const char *backup_path, const char *cfengine)
{
    char **args = NULL;
    char *envp[] = { NULL };
    args = xcalloc(4 + 1, sizeof(char *));
    args[0] = xstrdup(backup_tool);
    args[1] = xstrdup("RESTORE");
    args[2] = xstrdup(backup_path);
    args[3] = xstrdup(cfengine);
    args[4] = NULL;
    int result = 0;
    result =  run_process_wait(backup_tool, args, envp);
    free (args[0]);
    free (args[1]);
    free (args[2]);
    free (args[3]);
    free (args);
    return result;
}

/*
 * The update loop goes like this:
 * 1. Copy this program to a temporary location (by default /tmp/cf-upgrade).
 * 2. Execute this program requesting to perform the upgrade.
 * The following steps happen on the new copy:
 * 3. Run the backup script.
 * 4. Run the upgrade command.
 * 4a If success, finish.
 * 4b If failure, run the restore command from the backup script.
 */
int RunUpdate(const Configuration *configuration)
{
    if (!configuration)
    {
        return -1;
    }

    int result = 0;
    bool upgrade = ConfigurationPerformUpdate(configuration);
    if (upgrade)
    {
        /* Perform the upgrade */
        /* first perform the backup */
        const char *backup_path = ConfigurationBackupPath(configuration);
        const char *backup_tool = ConfigurationBackupTool(configuration);
        const char *cfengine = ConfigurationCFEnginePath(configuration);

        log_entry(LogVerbose, "Performing backup of '%s' to '%s' using '%s'",
                  cfengine, backup_path, backup_tool);

        result = perform_backup(backup_tool, backup_path, cfengine);
        if (result != 0)
        {
            log_entry(LogCritical, "Failed to backup %s to %s using %s",
                      cfengine, backup_path, backup_tool);
            return -1;
        }

        log_entry(LogVerbose, "Backup successful");

        /* run the upgrade process */
        const char *command = ConfigurationCommand(configuration);
        char *args[CF_UPGRADE_MAX_ARGUMENTS + 1];

        int i = 0;
        int total = ConfigurationNumberOfArguments(configuration);
        for (i = 0; i < total; ++i)
        {
            args[i] = xstrdup(ConfigurationArgument(configuration, i));
        }
        args[total] = NULL;

        log_entry(LogVerbose, "Running upgrade command: %s", command);

        result = run_process_wait(command, args, environ);
        /* Check that everything went according to plan */
        if (result == 0)
        {
            log_entry(LogNormal, "Upgrade succeeded!");
            return 0;
        }
        else
        {
            log_entry(LogCritical, "Upgrade failed! Performing restore...");
            /* Well, that is why we have a backup */
            result = perform_restore(backup_tool, backup_path, cfengine);
            if (result == 0)
            {
                log_entry(LogNormal, "Restore successful. "
                          "CFEngine has been successfully reverted to the previous version.");
                return -1;
            }
            else
            {
                log_entry(LogCritical,
                          "Failed to restore %s from %s using %s. "
                          "Your CFEngine installation might be damaged now.",
                          cfengine, backup_path, backup_tool);
                return -2;
            }
        }
    }
    else
    {
        /* Copy and run the copy */
        const char *copy = ConfigurationCopy(configuration);
        const char *current = ConfigurationCFUpgrade(configuration);

        log_entry(LogVerbose, "Copying '%s' to '%s'", current, copy);

        result = copy_to_temporary_location(current, copy);
        if (result < 0)
        {
            log_entry (LogCritical, "Could not copy %s to %s", current, copy);
            return -1;
        }

        /* prepare the data for running the copy */
        char *args[COMMAND_LINE_OPTIONS + 1];
        int counter = 0;
        args[counter++] = xstrdup(ConfigurationCopy(configuration));
        args[counter++] = xstrdup("-b");
        args[counter++] = xstrdup(ConfigurationBackupTool(configuration));
        args[counter++] = xstrdup("-f");
        args[counter++] = xstrdup(ConfigurationCFEnginePath(configuration));
        args[counter++] = xstrdup("-s");
        args[counter++] = xstrdup(ConfigurationBackupPath(configuration));
        /* set the perform update flag and copy the arguments */
        args[counter++] = xstrdup("-x");
        int i = 0;
        int total = ConfigurationNumberOfArguments(configuration);
        for (i = 0; i < total; ++i)
        {
            args[counter + i] = xstrdup(ConfigurationArgument(configuration, i));
        }
        /* Replace current process with the copy. */
        args[counter + total] = NULL;

        log_entry(LogVerbose, "Reexecuting cf-upgrade from the copy: %s",
                  copy);

        /* Effectively this does execvp(), i.e. preserves
           current environment. */
        result = run_process_replace(copy, args, environ);
    }

    assert(false);                                           /* unreachable */
    return -1;
}
