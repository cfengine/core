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

#include "cf3.defs.h"

#include "dir.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "matching.h"
#include "cfstream.h"

#define CF_RECURSION_LIMIT 100

static int PushDirState(char *name, struct stat *sb);
static void PopDirState(int goback, char *name, struct stat *sb, Recursion r);
static void CheckLinkSecurity(struct stat *sb, char *name);

/*********************************************************************/
/* Depth searches                                                    */
/*********************************************************************/

int DepthSearch(char *name, struct stat *sb, int rlevel, Attributes attr, Promise *pp,
                const ReportContext *report_context)
{
    Dir *dirh;
    int goback;
    const struct dirent *dirp;
    char path[CF_BUFSIZE];
    struct stat lsb;

    if (!attr.havedepthsearch)  /* if the search is trivial, make sure that we are in the parent dir of the leaf */
    {
        char basedir[CF_BUFSIZE];

        CfDebug(" -> Direct file reference %s, no search implied\n", name);
        snprintf(basedir, sizeof(basedir), "%s", name);
        ChopLastNode(basedir);
        chdir(basedir);
        return VerifyFileLeaf(name, sb, attr, pp, report_context);
    }

    if (rlevel > CF_RECURSION_LIMIT)
    {
        CfOut(cf_error, "", "WARNING: Very deep nesting of directories (>%d deep): %s (Aborting files)", rlevel, name);
        return false;
    }

    if (rlevel > CF_RECURSION_LIMIT)
    {
        CfOut(cf_error, "", "WARNING: Very deep nesting of directories (>%d deep): %s (Aborting files)", rlevel, name);
        return false;
    }

    memset(path, 0, CF_BUFSIZE);

    CfDebug("To iterate is Human, to recurse is Divine...(%s)\n", name);

    if (!PushDirState(name, sb))
    {
        return false;
    }

    if ((dirh = OpenDirLocal(".")) == NULL)
    {
        CfOut(cf_inform, "opendir", "Could not open existing directory %s\n", name);
        return false;
    }

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (!ConsiderFile(dirp->d_name, name, attr, pp))
        {
            continue;
        }

        strcpy(path, name);
        AddSlash(path);

        if (!JoinPath(path, dirp->d_name))
        {
            CloseDir(dirh);
            return true;
        }

        if (lstat(dirp->d_name, &lsb) == -1)
        {
            CfOut(cf_verbose, "lstat", "Recurse was looking at %s when an error occurred:\n", path);
            continue;
        }

        if (S_ISLNK(lsb.st_mode))       /* should we ignore links? */
        {
            if (!KillGhostLink(path, attr, pp))
            {
                VerifyFileLeaf(path, &lsb, attr, pp, report_context);
            }
            else
            {
                continue;
            }
        }

        /* See if we are supposed to treat links to dirs as dirs and descend */

        if ((attr.recursion.travlinks) && (S_ISLNK(lsb.st_mode)))
        {
            if ((lsb.st_uid != 0) && (lsb.st_uid != getuid()))
            {
                CfOut(cf_inform, "",
                      "File %s is an untrusted link: cfengine will not follow it with a destructive operation", path);
                continue;
            }

            /* if so, hide the difference by replacing with actual object */

            if (cfstat(dirp->d_name, &lsb) == -1)
            {
                CfOut(cf_error, "stat", "Recurse was working on %s when this failed:\n", path);
                continue;
            }
        }

        if ((attr.recursion.xdev) && (DeviceBoundary(&lsb, pp)))
        {
            CfOut(cf_verbose, "", "Skipping %s on different device - use xdev option to change this\n", path);
            continue;
        }

        if (S_ISDIR(lsb.st_mode))
        {
            if (SkipDirLinks(path, dirp->d_name, attr.recursion))
            {
                continue;
            }

            if ((attr.recursion.depth > 1) && (rlevel <= attr.recursion.depth))
            {
                CfOut(cf_verbose, "", " ->>  Entering %s (%d)\n", path, rlevel);
                goback = DepthSearch(path, &lsb, rlevel + 1, attr, pp, report_context);
                PopDirState(goback, name, sb, attr.recursion);
                VerifyFileLeaf(path, &lsb, attr, pp, report_context);
            }
            else
            {
                VerifyFileLeaf(path, &lsb, attr, pp, report_context);
            }
        }
        else
        {
            VerifyFileLeaf(path, &lsb, attr, pp, report_context);
        }
    }

    CloseDir(dirh);
    return true;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static int PushDirState(char *name, struct stat *sb)
{
    if (chdir(name) == -1)
    {
        CfOut(cf_inform, "chdir", "Could not change to directory %s, mode %jo in tidy", name, (uintmax_t)(sb->st_mode & 07777));
        return false;
    }
    else
    {
        CfDebug("Changed directory to %s\n", name);
    }

    CheckLinkSecurity(sb, name);
    return true;
}

/**********************************************************************/

static void PopDirState(int goback, char *name, struct stat *sb, Recursion r)
{
    if (goback && (r.travlinks))
    {
        if (chdir(name) == -1)
        {
            CfOut(cf_error, "chdir", "Error in backing out of recursive travlink descent securely to %s", name);
            HandleSignals(SIGTERM);
        }

        CheckLinkSecurity(sb, name);
    }
    else if (goback)
    {
        if (chdir("..") == -1)
        {
            CfOut(cf_error, "chdir", "Error in backing out of recursive descent securely to %s", name);
            HandleSignals(SIGTERM);
        }
    }
}

/**********************************************************************/

int SkipDirLinks(char *path, const char *lastnode, Recursion r)
{
    CfDebug("SkipDirLinks(%s,%s)\n", path, lastnode);

    if (r.exclude_dirs)
    {
        if ((MatchRlistItem(r.exclude_dirs, path)) || (MatchRlistItem(r.exclude_dirs, lastnode)))
        {
            CfOut(cf_verbose, "", "Skipping matched excluded directory %s\n", path);
            return true;
        }
    }

    if (r.include_dirs)
    {
        if (!((MatchRlistItem(r.include_dirs, path)) || (MatchRlistItem(r.include_dirs, lastnode))))
        {
            CfOut(cf_verbose, "", "Skipping matched non-included directory %s\n", path);
            return true;
        }
    }

    return false;
}

/**********************************************************************/

static void CheckLinkSecurity(struct stat *sb, char *name)
{
    struct stat security;

    CfDebug("Checking the inode and device to make sure we are where we think we are...\n");

    if (cfstat(".", &security) == -1)
    {
        CfOut(cf_error, "stat", "Could not stat directory %s after entering!", name);
        return;
    }

    if ((sb->st_dev != security.st_dev) || (sb->st_ino != security.st_ino))
    {
        CfOut(cf_error, "",
              "SERIOUS SECURITY ALERT: path race exploited in recursion to/from %s. Not safe for agent to continue - aborting",
              name);
        HandleSignals(SIGTERM);
        /* Exits */
    }
}
