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
#include "cfstream.h"
#include "client_code.h"
#include "pipes.h"
#include "transaction.h"
#include "logging.h"
#include "string_lib.h"
#include "files_repository.h"
#include "files_lib.h"

#include <assert.h>

extern AgentConnection *COMS;


/*****************************************************************************/

int MoveObstruction(char *from, Attributes attr, Promise *pp)
{
    struct stat sb;
    char stamp[CF_BUFSIZE], saved[CF_BUFSIZE];
    time_t now_stamp = time((time_t *) NULL);

    if (lstat(from, &sb) == 0)
    {
        if (!attr.move_obstructions)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr, " !! Object %s exists and is obstructing our promise\n", from);
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
                snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&now_stamp)));
                strcat(saved, stamp);
            }

            strcat(saved, CF_SAVED);

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, attr, " -> Moving file object %s to %s\n", from, saved);

            if (cf_rename(from, saved) == -1)
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_rename", pp, attr, " !! Can't rename %s to %s\n", from, saved);
                return false;
            }

            if (ArchiveToRepository(saved, attr, pp))
            {
                unlink(saved);
            }

            return true;
        }

        if (S_ISDIR(sb.st_mode))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, attr, " -> Moving directory %s to %s%s\n", from, from, CF_SAVED);

            if (DONTDO)
            {
                return false;
            }

            saved[0] = '\0';
            strcpy(saved, from);

            snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&now_stamp)));
            strcat(saved, stamp);
            strcat(saved, CF_SAVED);
            strcat(saved, ".dir");

            if (cfstat(saved, &sb) != -1)
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, " !! Couldn't save directory %s, since %s exists already\n", from,
                     saved);
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to force link to existing directory %s\n", from);
                return false;
            }

            if (cf_rename(from, saved) == -1)
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_rename", pp, attr, "Can't rename %s to %s\n", from, saved);
                return false;
            }
        }
    }

    return true;
}

/*********************************************************************/

int SaveAsFile(SaveCallbackFn callback, void *param, const char *file, Attributes a, Promise *pp)
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

    if (cfstat(file, &statbuf) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "stat", pp, a, " !! Can no longer access file %s, which needed editing!\n", file);
        return false;
    }

    strcpy(backup, file);

    if (a.edits.backup == BACKUP_OPTION_TIMESTAMP)
    {
        snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&stamp_now)));
        strcat(backup, stamp);
    }

    strcat(backup, ".cf-before-edit");

    strcpy(new, file);
    strcat(new, ".cf-after-edit");
    unlink(new);                /* Just in case of races */

    if ((*callback)(new, file, param, a, pp) == false)
    {
        return false;
    }

    if (cf_rename(file, backup) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_rename", pp, a,
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
        if (ArchiveToRepository(backup, a, pp))
        {
            unlink(backup);
        }
    }

    else
    {
        unlink(backup);
    }

    if (cf_rename(new, file) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_rename", pp, a,
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", new, file);
        return false;
    }

    mask = umask(0);
    cf_chmod(file, statbuf.st_mode);    /* Restore file permissions etc */
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

static bool SaveItemListCallback(const char *dest_filename, const char *orig_filename, void *param, Attributes a, Promise *pp)
{
    Item *liststart = param, *ip;
    FILE *fp;

    //saving list to file
    if ((fp = fopen(dest_filename, "w")) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "fopen", pp, a, "Couldn't write file %s after editing\n", dest_filename);
        return false;
    }

    for (ip = liststart; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%s\n", ip->name);
    }

    if (fclose(fp) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "fclose", pp, a, "Unable to close file while writing");
        return false;
    }

    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " -> Edited file %s \n", orig_filename);
    return true;
}

/*********************************************************************/

int SaveItemListAsFile(Item *liststart, const char *file, Attributes a, Promise *pp)
{
    return SaveAsFile(&SaveItemListCallback, liststart, file, a, pp);
}
