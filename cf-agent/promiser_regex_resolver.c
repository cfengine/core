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

/*
 * This module provides functionality of resolving regular expressions in
 * promisers, such as in featured by files promise type and storage promise
 * type.
 */

#include <cf3.defs.h>

#include <actuator.h>
#include <policy.h>
#include <matching.h>
#include <match_scope.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <promises.h>
#include <dir.h>
#include <files_properties.h>
#include <scope.h>
#include <item_lib.h>
#include <string_lib.h>                                       /* PathAppend */

PromiseResult LocateFilePromiserGroup(EvalContext *ctx, char *wildpath, const Promise *pp,
                                      PromiseResult (*fnptr) (EvalContext *ctx, char *path, const Promise *ptr))
{
    Item *path, *ip, *remainder = NULL;
    char pbuffer[CF_BUFSIZE];
    struct stat statbuf;
    int count = 0, lastnode = false, expandregex = false;
    uid_t agentuid = getuid();
    int create = PromiseGetConstraintAsBoolean(ctx, "create", pp);
    char *pathtype = PromiseGetConstraintAsRval(pp, "pathtype", RVAL_TYPE_SCALAR);

/* Do a search for promiser objects matching wildpath */

    if ((!IsPathRegex(wildpath)) || (pathtype && (strcmp(pathtype, "literal") == 0)))
    {
        Log(LOG_LEVEL_VERBOSE, "Using literal pathtype for '%s'", wildpath);
        return (*fnptr) (ctx, wildpath, pp);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Using regex pathtype for '%s' (see pathtype)", wildpath);
    }

    pbuffer[0] = '\0';
    path = SplitString(wildpath, '/');  // require forward slash in regex on all platforms

    PromiseResult result = PROMISE_RESULT_SKIPPED;
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

        if (!PathAppend(pbuffer, sizeof(pbuffer), ip->name, FILE_SEPARATOR))
        {
            Log(LOG_LEVEL_ERR,
                "Internal limit reached in LocateFilePromiserGroup(),"
                " path too long: '%s' + '%s'",
                pbuffer, ip->name);
            Log(LOG_LEVEL_ERR, "Buffer has limited size in LocateFilePromiserGroup");
            return result;
        }

        if (stat(pbuffer, &statbuf) != -1)
        {
            if ((S_ISDIR(statbuf.st_mode)) && ((statbuf.st_uid) != agentuid) && ((statbuf.st_uid) != 0))
            {
                Log(LOG_LEVEL_INFO,
                    "Directory '%s' in search path '%s' is controlled by another user (uid %ju) - trusting its content is potentially risky (possible race condition)",
                      pbuffer, wildpath, (uintmax_t)statbuf.st_uid);
                PromiseRef(LOG_LEVEL_INFO, pp);
            }
        }
    }

    if (expandregex)            /* Expand one regex link and hand down */
    {
        char nextbuffer[CF_BUFSIZE], nextbufferOrig[CF_BUFSIZE], regex[CF_BUFSIZE];
        const struct dirent *dirp;
        Dir *dirh;

        strlcpy(regex, ip->name, CF_BUFSIZE);

        if ((dirh = DirOpen(pbuffer)) == NULL)
        {
            // Could be a dummy directory to be created so this is not an error.
            Log(LOG_LEVEL_VERBOSE, "Using best-effort expanded (but non-existent) file base path '%s'", wildpath);
            result = PromiseResultUpdate(result, (*fnptr) (ctx, wildpath, pp));
            DeleteItemList(path);
            return result;
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
                    Log(LOG_LEVEL_DEBUG, "Skipping non-directory '%s'", dirp->d_name);
                    continue;
                }

                if (FullTextMatch(ctx, regex, dirp->d_name))
                {
                    Log(LOG_LEVEL_DEBUG, "Link '%s' matched regex '%s'", dirp->d_name, regex);
                }
                else
                {
                    continue;
                }

                count++;

                strlcpy(nextbuffer, pbuffer, CF_BUFSIZE);
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
                    result = PromiseResultUpdate(result, LocateFilePromiserGroup(ctx, nextbuffer, pp, fnptr));
                }
                else
                {
                    Promise *pcopy;

                    Log(LOG_LEVEL_VERBOSE, "Using expanded file base path '%s'", nextbuffer);

                    /* Now need to recompute any back references to get the complete path */

                    snprintf(nextbufferOrig, sizeof(nextbufferOrig), "%s", nextbuffer);
                    MapNameForward(nextbuffer);

                    if (!FullTextMatch(ctx, pp->promiser, nextbuffer))
                    {
                        Log(LOG_LEVEL_DEBUG, "Error recomputing references for '%s' in '%s'", pp->promiser, nextbuffer);
                    }

                    /* If there were back references there could still be match.x vars to expand */

                    bool excluded = false;
                    pcopy = ExpandDeRefPromise(ctx, pp, &excluded);
                    if (excluded)
                    {
                        result = PromiseResultUpdate(result, PROMISE_RESULT_SKIPPED);
                    }
                    else
                    {
                        result = PromiseResultUpdate(result, (*fnptr) (ctx, nextbufferOrig, pcopy));
                    }

                    PromiseDestroy(pcopy);
                }
            }

            DirClose(dirh);
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Using file base path '%s'", pbuffer);
        result = PromiseResultUpdate(result, (*fnptr) (ctx, pbuffer, pp));
    }

    if (count == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "No promiser file objects matched as regular expression '%s'", wildpath);

        if (create)
        {
            result = PromiseResultUpdate(result, (*fnptr)(ctx, pp->promiser, pp));
        }
    }

    DeleteItemList(path);

    return result;
}
