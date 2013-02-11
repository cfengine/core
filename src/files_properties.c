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

/*****************************************************************************/
/*                                                                           */
/* File: files_properties.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include "files_names.h"

/*********************************************************************/
/* Files to be ignored when parsing directories                      */
/*********************************************************************/

/*********************************************************************/

int ConsiderFile(const char *nodename, char *path, Attributes attr, Promise *pp)
{
    int i;
    struct stat statbuf;
    char vbuff[CF_BUFSIZE];
    const char *sp;

    static char *skipfiles[] =
{
        ".",
        "..",
        "lost+found",
        ".cfengine.rm",
        NULL
    };

    if (strlen(nodename) < 1)
    {
        CfOut(cf_error, "", "Empty (null) filename detected in %s\n", path);
        return true;
    }

    if (IsItemIn(SUSPICIOUSLIST, nodename))
    {
        struct stat statbuf;

        if (cfstat(nodename, &statbuf) != -1)
        {
            if (S_ISREG(statbuf.st_mode))
            {
                CfOut(cf_error, "", "Suspicious file %s found in %s\n", nodename, path);
                return false;
            }
        }
    }

    if (strcmp(nodename, "...") == 0)
    {
        CfOut(cf_verbose, "", "Possible DFS/FS cell node detected in %s...\n", path);
        return true;
    }

    for (i = 0; skipfiles[i] != NULL; i++)
    {
        if (strcmp(nodename, skipfiles[i]) == 0)
        {
            CfDebug("Filename %s/%s is classified as ignorable\n", path, nodename);
            return false;
        }
    }

    if ((strcmp("[", nodename) == 0) && (strcmp("/usr/bin", path) == 0))
    {
        if (VSYSTEMHARDCLASS == linuxx)
        {
            return true;
        }
    }

    for (sp = nodename; *sp != '\0'; sp++)
    {
        if ((*sp > 31) && (*sp < 127))
        {
            break;
        }
    }

    strcpy(vbuff, path);
    AddSlash(vbuff);
    strcat(vbuff, nodename);

    for (sp = nodename; *sp != '\0'; sp++)      /* Check for files like ".. ." */
    {
        if ((*sp != '.') && !isspace(*sp))
        {
            return true;
        }
    }

    if (cf_lstat(vbuff, &statbuf, attr, pp) == -1)
    {
        CfOut(cf_verbose, "lstat", "Couldn't stat %s", vbuff);
        return true;
    }

    if (statbuf.st_size == 0 && !(VERBOSE || INFORM))   /* No sense in warning about empty files */
    {
        return false;
    }

    CfOut(cf_error, "", "Suspicious looking file object \"%s\" masquerading as hidden file in %s\n", nodename, path);
    CfDebug("Filename looks suspicious\n");

    if (S_ISLNK(statbuf.st_mode))
    {
        CfOut(cf_inform, "", "   %s is a symbolic link\n", nodename);
    }
    else if (S_ISDIR(statbuf.st_mode))
    {
        CfOut(cf_inform, "", "   %s is a directory\n", nodename);
    }

    CfOut(cf_verbose, "", "[%s] has size %ld and full mode %o\n", nodename, (unsigned long) (statbuf.st_size),
          (unsigned int) (statbuf.st_mode));
    return true;
}

/********************************************************************/

void SetSearchDevice(struct stat *sb, Promise *pp)
{
    CfDebug("Registering root device as %" PRIdMAX "\n", (intmax_t) sb->st_dev);
    pp->rootdevice = sb->st_dev;
}

/********************************************************************/

int DeviceBoundary(struct stat *sb, Promise *pp)
{
    if (sb->st_dev == pp->rootdevice)
    {
        return false;
    }
    else
    {
        CfOut(cf_verbose, "", "Device change from %jd to %jd\n", (intmax_t) pp->rootdevice, (intmax_t) sb->st_dev);
        return true;
    }
}
