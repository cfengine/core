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
#include "cfstream.h"

static Item *SUSPICIOUSLIST = NULL;

void AddFilenameToListOfSuspicious(const char *pattern)
{
    PrependItem(&SUSPICIOUSLIST, pattern, NULL);
}

static bool SuspiciousFile(const char *filename)
{
    return IsItemIn(SUSPICIOUSLIST, filename);
}

static const char *SKIPFILES[] =
{
    ".",
    "..",
    "lost+found",
    ".cfengine.rm",
    NULL
};

int ConsiderFile(EvalContext *ctx, const char *nodename, char *path, Attributes attr, Promise *pp)
{
    int i;
    struct stat statbuf;
    const char *sp;


    if (strlen(nodename) < 1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Empty (null) filename detected in %s\n", path);
        return true;
    }

    if (SuspiciousFile(nodename))
    {
        struct stat statbuf;

        if (cfstat(nodename, &statbuf) != -1)
        {
            if (S_ISREG(statbuf.st_mode))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Suspicious file %s found in %s\n", nodename, path);
                return false;
            }
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

    char buf[CF_BUFSIZE];

    snprintf(buf, sizeof(buf), "%s/%s", path, nodename);
    MapName(buf);

    for (sp = nodename; *sp != '\0'; sp++)      /* Check for files like ".. ." */
    {
        if ((*sp != '.') && (!isspace((int)*sp)))
        {
            return true;
        }
    }

    if (cf_lstat(ctx, buf, &statbuf, attr, pp) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "lstat", "Couldn't stat %s", buf);
        return true;
    }

    if ((statbuf.st_size == 0) && (!(VERBOSE || INFORM)))   /* No sense in warning about empty files */
    {
        return false;
    }

    CfOut(OUTPUT_LEVEL_ERROR, "", "Suspicious looking file object \"%s\" masquerading as hidden file in %s\n", nodename, path);
    CfDebug("Filename looks suspicious\n");

    if (S_ISLNK(statbuf.st_mode))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "   %s is a symbolic link\n", nodename);
    }
    else if (S_ISDIR(statbuf.st_mode))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "   %s is a directory\n", nodename);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "[%s] has size %ld and full mode %o\n", nodename, (unsigned long) (statbuf.st_size),
          (unsigned int) (statbuf.st_mode));
    return true;
}
