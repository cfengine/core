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

#include <files_properties.h>

#include <files_names.h>
#include <files_interfaces.h>
#include <item_lib.h>

static Item *SUSPICIOUSLIST = NULL; /* GLOBAL_P */

void AddFilenameToListOfSuspicious(const char *pattern)
{
    PrependItem(&SUSPICIOUSLIST, pattern, NULL);
}

static const char *const SKIPFILES[] =
{
    ".",
    "..",
    "lost+found",
    ".cfengine.rm",
    NULL
};

/* TODO rework to accept only one param: path+nodename */
static bool ConsiderFile(const char *nodename, const char *path, struct stat *stat)
{
    int i;
    const char *sp;

    if (strlen(nodename) < 1)
    {
        Log(LOG_LEVEL_ERR, "Empty (null) filename detected in '%s'", path);
        return true;
    }

    if (stat != NULL && (S_ISREG(stat->st_mode) || S_ISLNK(stat->st_mode)) &&
        IsItemIn(SUSPICIOUSLIST, nodename))
    {
        Log(LOG_LEVEL_ERR, "Suspicious file '%s' found in '%s'", nodename, path);
        return false;
    }

    if (strcmp(nodename, "...") == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Possible DFS/FS cell node detected in '%s' ...", path);
        return true;
    }

    for (i = 0; SKIPFILES[i] != NULL; i++)
    {
        if (strcmp(nodename, SKIPFILES[i]) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "Filename '%s/%s' is classified as ignorable", path, nodename);
            return false;
        }
    }

    if ((strcmp("[", nodename) == 0) && (strcmp("/usr/bin", path) == 0))
    {
#if defined(__linux__)
            return true;
#endif
    }

    for (sp = nodename; *sp != '\0'; sp++)
    {
        if ((*sp > 31) && (*sp < 127))
        {
            break;
        }
    }

    for (sp = nodename; *sp != '\0'; sp++)      /* Check for files like ".. ." */
    {
        if ((*sp != '.') && (!isspace((int)*sp)))
        {
            return true;
        }
    }

    if (stat == NULL)
    {
        /* stat is NULL so we can't make more checks, call this function again
         * but pass the stat info as well. */
        return true;
    }

    if ((stat->st_size == 0) && LogGetGlobalLevel() < LOG_LEVEL_INFO)   /* No sense in warning about empty files */
    {
        return false;
    }

    Log(LOG_LEVEL_ERR, "Suspicious looking file object '%s' masquerading as hidden file in '%s'", nodename, path);

    if (S_ISLNK(stat->st_mode))
    {
        Log(LOG_LEVEL_INFO, "'%s' is a symbolic link", nodename);
    }
    else if (S_ISDIR(stat->st_mode))
    {
        Log(LOG_LEVEL_INFO, "'%s' is a directory", nodename);
    }

    Log(LOG_LEVEL_VERBOSE, "'%s' has size %ld and full mode %o", nodename, (unsigned long) (stat->st_size),
          (unsigned int) (stat->st_mode));
    return true;
}

bool ConsiderLocalFile(const char *filename, const char *directory)
{
    struct stat stat;
    if (lstat(filename, &stat) == -1)
    {
        return ConsiderFile(filename, directory, NULL);
    }
    else
    {
        return ConsiderFile(filename, directory, &stat);
    }
}

bool ConsiderAbstractFile(const char *filename, const char *directory, FileCopy fc, AgentConnection *conn)
{
    /* First check if the file should be avoided, e.g. ".." - before sending
     * anything over the network*/

    if (!ConsiderFile(filename, directory, NULL))
    {
        return false;
    }

    /* TODO this function should accept the joined path in the first place
     * since it's joined elsewhere as well, if split needed do it here. */
    char buf[CF_BUFSIZE];
    int ret = snprintf(buf, sizeof(buf), "%s/%s", directory, filename);
    if (ret < 0 || ret >= sizeof(buf))
    {
        Log(LOG_LEVEL_ERR,
            "Filename too long! Directory '%s' filename '%s'",
            directory, filename);
        return false;
    }

    /* Second, send the STAT command. */

    struct stat stat;
    if (cf_lstat(buf, &stat, fc, conn) == -1)
    {
        return false;                                      /* stat() failed */
    }
    else
    {
        /* Reconsider, but using stat info this time. */
        return ConsiderFile(filename, directory, &stat);
    }
}
