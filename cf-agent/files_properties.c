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

#include "files_properties.h"

#include "files_names.h"
#include "files_interfaces.h"
#include "item_lib.h"
#include "logging_old.h"

static Item *SUSPICIOUSLIST = NULL;

void AddFilenameToListOfSuspicious(const char *pattern)
{
    PrependItem(&SUSPICIOUSLIST, pattern, NULL);
}

static const char *SKIPFILES[] =
{
    ".",
    "..",
    "lost+found",
    ".cfengine.rm",
    NULL
};

static bool ConsiderFile(const char *nodename, const char *path, struct stat *stat)
{
    int i;
    const char *sp;

    if (strlen(nodename) < 1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Empty (null) filename detected in %s\n", path);
        return true;
    }

    if (IsItemIn(SUSPICIOUSLIST, nodename))
    {
        if (stat && (S_ISREG(stat->st_mode) || S_ISLNK(stat->st_mode)))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Suspicious file %s found in %s\n", nodename, path);
                return false;
        }
    }

    if (strcmp(nodename, "...") == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Possible DFS/FS cell node detected in %s...\n", path);
        return true;
    }

    for (i = 0; SKIPFILES[i] != NULL; i++)
    {
        if (strcmp(nodename, SKIPFILES[i]) == 0)
        {
            CfDebug("Filename %s/%s is classified as ignorable\n", path, nodename);
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "cf_lstat", "Couldn't stat %s/%s", path, nodename);
        return true;
    }

    if ((stat->st_size == 0) && (!(VERBOSE || INFORM)))   /* No sense in warning about empty files */
    {
        return false;
    }

    CfOut(OUTPUT_LEVEL_ERROR, "", "Suspicious looking file object \"%s\" masquerading as hidden file in %s\n", nodename, path);
    CfDebug("Filename looks suspicious\n");

    if (S_ISLNK(stat->st_mode))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "   %s is a symbolic link\n", nodename);
    }
    else if (S_ISDIR(stat->st_mode))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "   %s is a directory\n", nodename);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "[%s] has size %ld and full mode %o\n", nodename, (unsigned long) (stat->st_size),
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
    struct stat stat;
    char buf[CF_BUFSIZE];
    snprintf(buf, sizeof(buf), "%s/%s", filename, directory);
    MapName(buf);

    if (cf_lstat(buf, &stat, fc, conn) == -1)
    {
        return ConsiderFile(filename, directory, NULL);
    }
    else
    {
        return ConsiderFile(filename, directory, &stat);
    }
}
