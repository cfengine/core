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

#include <files_operators.h>

#include <actuator.h>
#include <verify_acl.h>
#include <eval_context.h>
#include <promises.h>
#include <dir.h>
#include <dbm_api.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <files_copy.h>
#include <vars.h>
#include <item_lib.h>
#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <matching.h>
#include <attributes.h>
#include <client_code.h>
#include <pipes.h>
#include <locks.h>
#include <string_lib.h>
#include <files_repository.h>
#include <files_lib.h>
#include <buffer.h>


int MoveObstruction(EvalContext *ctx, char *from, Attributes attr, const Promise *pp, PromiseResult *result)
{
    struct stat sb;
    char stamp[CF_BUFSIZE], saved[CF_BUFSIZE];
    time_t now_stamp = time((time_t *) NULL);

    if (lstat(from, &sb) == 0)
    {
        if (!attr.move_obstructions)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Object '%s' exists and is obstructing our promise", from);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
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

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, attr, "Moving file object '%s' to '%s'", from, saved);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

            if (rename(from, saved) == -1)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Can't rename '%s' to '%s'. (rename: %s)", from, saved, GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
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
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, attr, "Moving directory '%s' to '%s%s'", from, from, CF_SAVED);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

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
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Couldn't save directory '%s', since '%s' exists already", from,
                     saved);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                Log(LOG_LEVEL_ERR, "Unable to force link to existing directory '%s'", from);
                return false;
            }

            if (rename(from, saved) == -1)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Can't rename '%s' to '%s'. (rename: %s)",
                     from, saved, GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                return false;
            }
        }
    }

    return true;
}

/*********************************************************************/

bool SaveAsFile(SaveCallbackFn callback, void *param, const char *file, Attributes a, NewLineMode new_line_mode)
{
    struct stat statbuf;
    char new[CF_BUFSIZE], backup[CF_BUFSIZE];
    char stamp[CF_BUFSIZE];
    time_t stamp_now;
    Buffer *deref_file = BufferNewFrom(file, strlen(file));
    Buffer *pretty_file = BufferNew();
    int ret = false;

    BufferPrintf(pretty_file, "'%s'", file);

    stamp_now = time((time_t *) NULL);

    while (1)
    {
        if (lstat(BufferData(deref_file), &statbuf) == -1)
        {
            Log(LOG_LEVEL_ERR, "Can no longer access file %s, which needed editing. (lstat: %s)", BufferData(pretty_file), GetErrorStr());
            goto end;
        }
#ifndef __MINGW32__
        if (S_ISLNK(statbuf.st_mode))
        {
            char buf[statbuf.st_size + 1];
            // Careful. readlink() doesn't add '\0' byte.
            ssize_t linksize = readlink(BufferData(deref_file), buf, statbuf.st_size);
            if (linksize == 0)
            {
                Log(LOG_LEVEL_WARNING, "readlink() failed with 0 bytes. Should not happen (bug?).");
                goto end;
            }
            else if (linksize < 0)
            {
                Log(LOG_LEVEL_ERR, "Could not read link %s. (readlink: %s)", BufferData(pretty_file), GetErrorStr());
                goto end;
            }
            buf[linksize] = '\0';
            if (!IsAbsPath(buf))
            {
                char dir[BufferSize(deref_file) + 1];
                strcpy(dir, BufferData(deref_file));
                ChopLastNode(dir);
                BufferPrintf(deref_file, "%s/%s", dir, buf);
            }
            else
            {
                BufferSet(deref_file, buf, linksize);
            }
            BufferPrintf(pretty_file, "'%s' (from symlink '%s')", BufferData(deref_file), file);
        }
        else
#endif
        {
            break;
        }
    }

    strcpy(backup, BufferData(deref_file));

    if (a.edits.backup == BACKUP_OPTION_TIMESTAMP)
    {
        snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(ctime(&stamp_now)));
        strcat(backup, stamp);
    }

    strcat(backup, ".cf-before-edit");

    strcpy(new, BufferData(deref_file));
    strcat(new, ".cf-after-edit");
    unlink(new);                /* Just in case of races */

    if ((*callback)(new, param, new_line_mode) == false)
    {
        goto end;
    }

    if (!CopyFilePermissionsDisk(BufferData(deref_file), new))
    {
        Log(LOG_LEVEL_ERR, "Can't copy file permissions from %s to '%s' - so promised edits could not be moved into place.",
            BufferData(pretty_file), new);
        goto end;
    }

    unlink(backup);
#ifndef __MINGW32__
    if (link(BufferData(deref_file), backup) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Can't link %s to '%s' - falling back to copy. (link: %s)",
            BufferData(pretty_file), backup, GetErrorStr());
#else
    /* No hardlinks on Windows, go straight to copying */
    {
#endif
        if (!CopyRegularFileDisk(BufferData(deref_file), backup))
        {
            Log(LOG_LEVEL_ERR, "Can't copy %s to '%s' - so promised edits could not be moved into place.",
                BufferData(pretty_file), backup);
            goto end;
        }
        if (!CopyFilePermissionsDisk(BufferData(deref_file), backup))
        {
            Log(LOG_LEVEL_ERR, "Can't copy permissions %s to '%s' - so promised edits could not be moved into place.",
                BufferData(pretty_file), backup);
            goto end;
        }
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

    if (rename(new, BufferData(deref_file)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Can't rename '%s' to %s - so promised edits could not be moved into place. (rename: %s)",
            new, BufferData(pretty_file), GetErrorStr());
        goto end;
    }

    ret = true;

end:
    BufferDestroy(pretty_file);
    BufferDestroy(deref_file);
    return ret;
}

/*********************************************************************/

static bool SaveItemListCallback(const char *dest_filename, void *param, NewLineMode new_line_mode)
{
    Item *liststart = param, *ip;
    FILE *fp;

    //saving list to file
    if ((fp = safe_fopen(dest_filename, (new_line_mode == NewLineMode_Native) ? "wt" : "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open destination file '%s' for writing. (fopen: %s)",
            dest_filename, GetErrorStr());
        return false;
    }

    for (ip = liststart; ip != NULL; ip = ip->next)
    {
        if (fprintf(fp, "%s\n", ip->name) < 0)
        {
            Log(LOG_LEVEL_ERR, "Unable to write into destination file '%s'. (fprintf: %s)",
                dest_filename, GetErrorStr());
            fclose(fp);
            return false;
        }
    }

    if (fclose(fp) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to close file '%s' after writing. (fclose: %s)",
            dest_filename, GetErrorStr());
        return false;
    }

    return true;
}

/*********************************************************************/

bool SaveItemListAsFile(Item *liststart, const char *file, Attributes a, NewLineMode new_line_mode)
{
    return SaveAsFile(&SaveItemListCallback, liststart, file, a, new_line_mode);
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

static int ItemListsEqual(EvalContext *ctx, const Item *list1, const Item *list2, int warnings,
                          Attributes a, const Promise *pp, PromiseResult *result)
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
                    cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, "File content wants to change from from/to full/empty but only a warning promised");
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                }
                else
                {
                    if (ip1 != NULL)
                    {
                        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, " ! edit_line change warning promised: (remove) %s",
                             ip1->name);
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                    }

                    if (ip2 != NULL)
                    {
                        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, " ! edit_line change warning promised: (add) %s", ip2->name);
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
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

                cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, "edit_line warning promised: - %s", ip1->name);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                retval = false;
            }
        }

        ip1 = NextItem(ip1);
        ip2 = NextItem(ip2);
    }

    return retval;
}

/* returns true if file on disk is identical to file in memory */

int CompareToFile(EvalContext *ctx, const Item *liststart, const char *file, Attributes a, const Promise *pp,
                  PromiseResult *result)
{
    struct stat statbuf;
    Item *cmplist = NULL;

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

    if (!ItemListsEqual(ctx, cmplist, liststart, (a.transaction.action == cfa_warn), a, pp, result))
    {
        DeleteItemList(cmplist);
        return false;
    }

    DeleteItemList(cmplist);
    return (true);
}
