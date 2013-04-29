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

/*
 * This module provides functionality of resolving regular expressions in
 * promisers, such as in featured by files promise type and storage promise
 * type.
 */

#include "cf3.defs.h"

#include "policy.h"
#include "matching.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "promises.h"
#include "dir.h"
#include "files_properties.h"
#include "scope.h"
#include "logging_old.h"
#include "item_lib.h"

void LocateFilePromiserGroup(EvalContext *ctx, char *wildpath, Promise *pp, void (*fnptr) (EvalContext *ctx, char *path, Promise *ptr))
{
    Item *path, *ip, *remainder = NULL;
    char pbuffer[CF_BUFSIZE];
    struct stat statbuf;
    int count = 0, lastnode = false, expandregex = false;
    uid_t agentuid = getuid();
    int create = PromiseGetConstraintAsBoolean(ctx, "create", pp);
    char *pathtype = ConstraintGetRvalValue(ctx, "pathtype", pp, RVAL_TYPE_SCALAR);

    CfDebug("LocateFilePromiserGroup(%s)\n", wildpath);

/* Do a search for promiser objects matching wildpath */

    if ((!IsPathRegex(wildpath)) || (pathtype && (strcmp(pathtype, "literal") == 0)))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using literal pathtype for %s\n", wildpath);
        (*fnptr) (ctx, wildpath, pp);
        return;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using regex pathtype for %s (see pathtype)\n", wildpath);
    }

    pbuffer[0] = '\0';
    path = SplitString(wildpath, '/');  // require forward slash in regex on all platforms

    for (ip = path; ip != NULL; ip = ip->next)
    {
        if ((ip->name == NULL) || (strlen(ip->name) == 0))
        {
            continue;
        }

        if (ip->next == NULL)
        {
            lastnode = true;
        }

        /* No need to chdir as in recursive descent, since we know about the path here */

        if (IsRegex(ip->name))
        {
            remainder = ip->next;
            expandregex = true;
            break;
        }
        else
        {
            expandregex = false;
        }

        if (!JoinPath(pbuffer, ip->name))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Buffer has limited size in LocateFilePromiserGroup\n");
            return;
        }

        if (stat(pbuffer, &statbuf) != -1)
        {
            if ((S_ISDIR(statbuf.st_mode)) && ((statbuf.st_uid) != agentuid) && ((statbuf.st_uid) != 0))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "",
                      "Directory %s in search path %s is controlled by another user (uid %ju) - trusting its content is potentially risky (possible race)\n",
                      pbuffer, wildpath, (uintmax_t)statbuf.st_uid);
                PromiseRef(OUTPUT_LEVEL_INFORM, pp);
            }
        }
    }

    if (expandregex)            /* Expand one regex link and hand down */
    {
        char nextbuffer[CF_BUFSIZE], nextbufferOrig[CF_BUFSIZE], regex[CF_BUFSIZE];
        const struct dirent *dirp;
        Dir *dirh;

        memset(regex, 0, CF_BUFSIZE);

        strncpy(regex, ip->name, CF_BUFSIZE - 1);

        if ((dirh = DirOpen(pbuffer)) == NULL)
        {
            // Could be a dummy directory to be created so this is not an error.
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using best-effort expanded (but non-existent) file base path %s\n", wildpath);
            (*fnptr) (ctx, wildpath, pp);
            DeleteItemList(path);
            return;
        }
        else
        {
            count = 0;

            for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
            {
                if (!ConsiderLocalFile(dirp->d_name, pbuffer))
                {
                    continue;
                }

                if ((!lastnode) && (!S_ISDIR(statbuf.st_mode)))
                {
                    CfDebug("Skipping non-directory %s\n", dirp->d_name);
                    continue;
                }

                if (FullTextMatch(regex, dirp->d_name))
                {
                    CfDebug("Link %s matched regex %s\n", dirp->d_name, regex);
                }
                else
                {
                    continue;
                }

                count++;

                strncpy(nextbuffer, pbuffer, CF_BUFSIZE - 1);
                AddSlash(nextbuffer);
                strcat(nextbuffer, dirp->d_name);

                for (ip = remainder; ip != NULL; ip = ip->next)
                {
                    AddSlash(nextbuffer);
                    strcat(nextbuffer, ip->name);
                }

                /* The next level might still contain regexs, so go again as long as expansion is not nullpotent */

                if ((!lastnode) && (strcmp(nextbuffer, wildpath) != 0))
                {
                    LocateFilePromiserGroup(ctx, nextbuffer, pp, fnptr);
                }
                else
                {
                    Promise *pcopy;

                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using expanded file base path %s\n", nextbuffer);

                    /* Now need to recompute any back references to get the complete path */

                    snprintf(nextbufferOrig, sizeof(nextbufferOrig), "%s", nextbuffer);
                    MapNameForward(nextbuffer);

                    if (!FullTextMatch(pp->promiser, nextbuffer))
                    {
                        CfDebug("Error recomputing references for \"%s\" in: %s", pp->promiser, nextbuffer);
                    }

                    /* If there were back references there could still be match.x vars to expand */

                    pcopy = ExpandDeRefPromise(ctx, ScopeGetCurrent()->scope, pp);
                    (*fnptr) (ctx, nextbufferOrig, pcopy);
                    PromiseDestroy(pcopy);
                }
            }

            DirClose(dirh);
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using file base path %s\n", pbuffer);
        (*fnptr) (ctx, pbuffer, pp);
    }

    if (count == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "No promiser file objects matched as regular expression %s\n", wildpath);

        if (create)
        {
            (*fnptr)(ctx, pp->promiser, pp);
        }
    }

    DeleteItemList(path);
}
