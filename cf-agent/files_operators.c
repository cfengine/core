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

#include "files_operators.h"

#include "cf_acl.h"
#include "env_context.h"
#include "promises.h"
#include "dir.h"
#include "dbm_api.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "files_copy.h"
#include "vars.h"
#include "item_lib.h"
#include "conversion.h"
#include "expand.h"
#include "scope.h"
#include "matching.h"
#include "attributes.h"
#include "logging_old.h"
#include "client_code.h"
#include "pipes.h"
#include "locks.h"
#include "string_lib.h"
#include "files_repository.h"
#include "files_lib.h"

#include <assert.h>

int MoveObstruction(EvalContext *ctx, char *from, Attributes attr, const Promise *pp)
{
    struct stat sb;
    char stamp[CF_BUFSIZE], saved[CF_BUFSIZE];
    time_t now_stamp = time((time_t *) NULL);

    if (lstat(from, &sb) == 0)
    {
        if (!attr.move_obstructions)
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_FAIL, "", pp, attr, " !! Object %s exists and is obstructing our promise\n", from);
            return false;
        }

        if (!S_ISDIR(sb.st_mode))
        {
            if (DONTDO)
            {
                return false;
            }

            saved[0] = '\0';
            strcpy(saved, from);

            if (attr.copy.backup == BACKUP_OPTION_TIMESTAMP || attr.edits.backup == BACKUP_OPTION_TIMESTAMP)
            {
                snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(ctime(&now_stamp)));
                strcat(saved, stamp);
            }

            strcat(saved, CF_SAVED);

            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, attr, " -> Moving file object %s to %s\n", from, saved);

            if (rename(from, saved) == -1)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "rename", pp, attr, " !! Can't rename %s to %s\n", from, saved);
                return false;
            }

            if (ArchiveToRepository(saved, attr))
            {
                unlink(saved);
            }

            return true;
        }

        if (S_ISDIR(sb.st_mode))
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, attr, " -> Moving directory %s to %s%s\n", from, from, CF_SAVED);

            if (DONTDO)
            {
                return false;
            }

            saved[0] = '\0';
            strcpy(saved, from);

            snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(ctime(&now_stamp)));
            strcat(saved, stamp);
            strcat(saved, CF_SAVED);
            strcat(saved, ".dir");

            if (stat(saved, &sb) != -1)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "", pp, attr, " !! Couldn't save directory %s, since %s exists already\n", from,
                     saved);
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to force link to existing directory %s\n", from);
                return false;
            }

            if (rename(from, saved) == -1)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "rename", pp, attr, "Can't rename %s to %s\n", from, saved);
                return false;
            }
        }
    }

    return true;
}

/*********************************************************************/

int SaveAsFile(SaveCallbackFn callback, void *param, const char *file, Attributes a)
{
    struct stat statbuf;
    char new[CF_BUFSIZE], backup[CF_BUFSIZE];
    mode_t mask;
    char stamp[CF_BUFSIZE];
    time_t stamp_now;

#ifdef WITH_SELINUX
    int selinux_enabled = 0;
    security_context_t scontext = NULL;

    selinux_enabled = (is_selinux_enabled() > 0);

    if (selinux_enabled)
    {
        /* get current security context */
        getfilecon(file, &scontext);
    }
#endif

    stamp_now = time((time_t *) NULL);

    if (stat(file, &statbuf) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "stat", " !! Can no longer access file %s, which needed editing!\n", file);
        return false;
    }

    strcpy(backup, file);

    if (a.edits.backup == BACKUP_OPTION_TIMESTAMP)
    {
        snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(ctime(&stamp_now)));
        strcat(backup, stamp);
    }

    strcat(backup, ".cf-before-edit");

    strcpy(new, file);
    strcat(new, ".cf-after-edit");
    unlink(new);                /* Just in case of races */

    if ((*callback)(new, param) == false)
    {
        return false;
    }

    if (rename(file, backup) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "rename",
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", file, backup);
        return false;
    }

    if (a.edits.backup == BACKUP_OPTION_ROTATE)
    {
        RotateFiles(backup, a.edits.rotate);
        unlink(backup);
    }

    if (a.edits.backup != BACKUP_OPTION_NO_BACKUP)
    {
        if (ArchiveToRepository(backup, a))
        {
            unlink(backup);
        }
    }

    else
    {
        unlink(backup);
    }

    if (rename(new, file) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "rename",
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", new, file);
        return false;
    }

    mask = umask(0);
    chmod(file, statbuf.st_mode);    /* Restore file permissions etc */
    if (chown(file, statbuf.st_uid, statbuf.st_gid) != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to restore file permissions for '%s'\n", file);
    }
    umask(mask);

#ifdef WITH_SELINUX
    if (selinux_enabled)
    {
        /* restore file context */
        setfilecon(file, scontext);
    }
#endif

    return true;
}

/*********************************************************************/

static bool SaveItemListCallback(const char *dest_filename, void *param)
{
    Item *liststart = param, *ip;
    FILE *fp;

    //saving list to file
    if ((fp = fopen(dest_filename, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Unable to open destination file %s for writing", dest_filename);
        return false;
    }

    for (ip = liststart; ip != NULL; ip = ip->next)
    {
        if (fprintf(fp, "%s\n", ip->name) < 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fprintf", "Unable to write into destination file %s", dest_filename);
            return false;
        }
    }

    if (fclose(fp) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fclose", "Unable to close file %s after writing", dest_filename);
        return false;
    }

    return true;
}

/*********************************************************************/

int SaveItemListAsFile(Item *liststart, const char *file, Attributes a)
{
    return SaveAsFile(&SaveItemListCallback, liststart, file, a);
}

// Some complex logic here to enable warnings of diffs to be given

static Item *NextItem(const Item *ip)
{
    if (ip)
    {
        return ip->next;
    }
    else
    {
        return NULL;
    }
}

static int ItemListsEqual(EvalContext *ctx, const Item *list1, const Item *list2, int warnings, Attributes a, const Promise *pp)
{
    int retval = true;

    const Item *ip1 = list1;
    const Item *ip2 = list2;

    while (true)
    {
        if ((ip1 == NULL) && (ip2 == NULL))
        {
            return retval;
        }

        if ((ip1 == NULL) || (ip2 == NULL))
        {
            if (warnings)
            {
                if ((ip1 == list1) || (ip2 == list2))
                {
                    cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                         " ! File content wants to change from from/to full/empty but only a warning promised");
                }
                else
                {
                    if (ip1 != NULL)
                    {
                        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, " ! edit_line change warning promised: (remove) %s",
                             ip1->name);
                    }

                    if (ip2 != NULL)
                    {
                        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, " ! edit_line change warning promised: (add) %s", ip2->name);
                    }
                }
            }

            if (warnings)
            {
                if (ip1 || ip2)
                {
                    retval = false;
                    ip1 = NextItem(ip1);
                    ip2 = NextItem(ip2);
                    continue;
                }
            }

            return false;
        }

        if (strcmp(ip1->name, ip2->name) != 0)
        {
            if (!warnings)
            {
                // No need to wait
                return false;
            }
            else
            {
                // If we want to see warnings, we need to scan the whole file

                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, " ! edit_line warning promised: - %s", ip1->name);
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, " ! edit_line warning promised: + %s", ip2->name);
                retval = false;
            }
        }

        ip1 = NextItem(ip1);
        ip2 = NextItem(ip2);
    }

    return retval;
}

/* returns true if file on disk is identical to file in memory */

int CompareToFile(EvalContext *ctx, const Item *liststart, const char *file, Attributes a, const Promise *pp)
{
    struct stat statbuf;
    Item *cmplist = NULL;

    CfDebug("CompareToFile(%s)\n", file);

    if (stat(file, &statbuf) == -1)
    {
        return false;
    }

    if ((liststart == NULL) && (statbuf.st_size == 0))
    {
        return true;
    }

    if (liststart == NULL)
    {
        return false;
    }

    if (!LoadFileAsItemList(&cmplist, file, a.edits))
    {
        return false;
    }

    if (!ItemListsEqual(ctx, cmplist, liststart, (a.transaction.action == cfa_warn), a, pp))
    {
        DeleteItemList(cmplist);
        return false;
    }

    DeleteItemList(cmplist);
    return (true);
}
