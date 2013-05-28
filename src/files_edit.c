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
/* File: files_edit.c                                                        */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include "files_lib.h"
#include "files_names.h"

/*****************************************************************************/

EditContext *NewEditContext(char *filename, Attributes a, Promise *pp)
{
    EditContext *ec;

    if (!IsAbsoluteFileName(filename))
    {
        CfOut(cf_error, "", "Relative file name %s was marked for editing but has no invariant meaning\n", filename);
        return NULL;
    }

    ec = xcalloc(1, sizeof(EditContext));

    ec->filename = filename;
    ec->empty_first = a.edits.empty_before_use;

    if (!LoadFileAsItemList(&(ec->file_start), filename, a, pp))
    {
        free(ec);
        return NULL;
    }

    if (a.edits.empty_before_use)
    {
        CfOut(cf_verbose, "", " -> Build file model from a blank slate (emptying)\n");
        DeleteItemList(ec->file_start);
        ec->file_start = NULL;
    }

    EDIT_MODEL = true;
    return ec;
}

/*****************************************************************************/

void FinishEditContext(EditContext *ec, Attributes a, Promise *pp)
{
    Item *ip;

    EDIT_MODEL = false;

    if (DONTDO || a.transaction.action == cfa_warn)
    {
        if (ec && !CompareToFile(ec->file_start, ec->filename, a, pp) && ec->num_edits > 0)
        {
            cfPS(cf_error, CF_WARN, "", pp, a, " -> Should edit file %s but only a warning promised", ec->filename);
        }
        return;
    }
    else if (ec && ec->num_edits > 0)
    {
        if (CompareToFile(ec->file_start, ec->filename, a, pp))
        {
            if (ec)
            {
                cfPS(cf_verbose, CF_NOP, "", pp, a, " -> No edit changes to file %s need saving", ec->filename);
            }
        }
        else
        {
            SaveItemListAsFile(ec->file_start, ec->filename, a, pp);
        }
    }
    else
    {
        if (ec)
        {
            cfPS(cf_verbose, CF_NOP, "", pp, a, " -> No edit changes to file %s need saving", ec->filename);
        }
    }

    if (ec != NULL)
    {
        for (ip = ec->file_classes; ip != NULL; ip = ip->next)
        {
            NewClass(ip->name);
        }

        DeleteItemList(ec->file_classes);
        DeleteItemList(ec->file_start);
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int LoadFileAsItemList(Item **liststart, char *file, Attributes a, Promise *pp)
{
    FILE *fp;
    struct stat statbuf;
    char line[CF_BUFSIZE], concat[CF_BUFSIZE];
    int join = false;

    if (cfstat(file, &statbuf) == -1)
    {
        CfOut(cf_verbose, "stat", " ** Information: the proposed file \"%s\" could not be loaded", file);
        return false;
    }

    if (a.edits.maxfilesize != 0 && statbuf.st_size > a.edits.maxfilesize)
    {
        CfOut(cf_inform, "", " !! File %s is bigger than the limit edit.max_file_size = %jd > %d bytes\n", file,
              (intmax_t) statbuf.st_size, a.edits.maxfilesize);
        return (false);
    }

    if (!S_ISREG(statbuf.st_mode))
    {
        cfPS(cf_inform, CF_INTERPT, "", pp, a, "%s is not a plain file\n", file);
        return false;
    }

    if ((fp = fopen(file, "r")) == NULL)
    {
        cfPS(cf_inform, CF_INTERPT, "fopen", pp, a, "Couldn't read file %s for editing\n", file);
        return false;
    }

    memset(line, 0, CF_BUFSIZE);
    memset(concat, 0, CF_BUFSIZE);

    while (!feof(fp))
    {
        CfReadLine(line, CF_BUFSIZE - 1, fp);

        if (a.edits.joinlines && *(line + strlen(line) - 1) == '\\')
        {
            join = true;
        }
        else
        {
            join = false;
        }

        if (join)
        {
            *(line + strlen(line) - 1) = '\0';
            JoinSuffix(concat, line);
        }
        else
        {
            JoinSuffix(concat, line);

            if (!feof(fp) || (strlen(concat) != 0))
            {
                AppendItem(liststart, concat, NULL);
            }

            concat[0] = '\0';
            join = false;
        }

        line[0] = '\0';
    }

    fclose(fp);
    return (true);
}

/*********************************************************************/

int SaveItemListAsFile(Item *liststart, char *file, Attributes a, Promise *pp)
{
    Item *ip;
    struct stat statbuf;
    char new[CF_BUFSIZE], backup[CF_BUFSIZE];
    FILE *fp;
    mode_t mask;
    char stamp[CF_BUFSIZE];
    time_t stamp_now;

    stamp_now = time((time_t *) NULL);

    if (cfstat(file, &statbuf) == -1)
    {
        cfPS(cf_error, CF_FAIL, "stat", pp, a, " !! Can no longer access file %s, which needed editing!\n", file);
        return false;
    }

    strcpy(backup, file);

    if (a.edits.backup == cfa_timestamp)
    {
        snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&stamp_now)));
        strcat(backup, stamp);
    }

    strcat(backup, ".cf-before-edit");

    strcpy(new, file);
    strcat(new, ".cf-after-edit");
    unlink(new);                /* Just in case of races */

    if ((fp = fopen(new, "w")) == NULL)
    {
        cfPS(cf_error, CF_FAIL, "fopen", pp, a, "Couldn't write file %s after editing\n", new);
        return false;
    }

    for (ip = liststart; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%s\n", ip->name);
    }

    if (fclose(fp) == -1)
    {
        cfPS(cf_error, CF_FAIL, "fclose", pp, a, "Unable to close file while writing");
        return false;
    }

    cfPS(cf_inform, CF_CHG, "", pp, a, " -> Edited file %s \n", file);

    if (!CopyFilePermissionsDisk(file, new))
    {
        cfPS(cf_error, CF_FAIL, "cf_rename", pp, a,
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", file, backup);
        return false;
    }

    unlink(backup);
#ifndef __MINGW32__
    if (link(file, backup) == -1)
    {
        CfOut(cf_verbose, "links", "Can't link '%s' to '%s' - falling back to copy.", file, backup);
#else
    /* No hardlinks on Windows, go straight to copying */
    {
#endif
        if (!CopyRegularFileDisk((char*)file, backup, a, pp))
        {
            cfPS(cf_error, CF_FAIL, "", pp, a,
                 "!! Can't copy '%s' to '%s' - so promised edits could not be moved into place.", file, backup);
            return false;
        }
        if (!CopyFilePermissionsDisk(file, backup))
        {
            cfPS(cf_error, CF_FAIL, "", pp, a,
                 "!! Can't copy permissions '%s' to '%s' - so promised edits could not be moved into place.", file, backup);
            return false;
        }
    }

    if (a.edits.backup == cfa_rotate)
    {
        RotateFiles(backup, a.edits.rotate);
        unlink(backup);
    }

    if (a.edits.backup != cfa_nobackup)
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
        cfPS(cf_error, CF_FAIL, "cf_rename", pp, a,
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", new, file);
        return false;
    }

    return true;
}

/*********************************************************************/

int AppendIfNoSuchLine(char *filename, char *line)
/* Appends line to the file with path filename if it is not already
   there. line should not contain newline.
   Returns true if the line is there on exit, false on error. */
{
    FILE *fread, *fappend;
    char lineCp[CF_MAXVARSIZE], lineBuf[CF_MAXVARSIZE];
    int lineExists = false;
    int result = false;
    size_t written = 0;

    if ((fread = fopen(filename, "rw")) == NULL)
    {
        CfOut(cf_error, "fopen", "!! Cannot open the file \"%s\" for read", filename);
        return false;
    }

    while (CfReadLine(lineBuf, sizeof(lineBuf), fread)) // strips newlines automatically
    {
        if (strcmp(line, lineBuf) == 0)
        {
            lineExists = true;
            result = true;
            break;
        }
    }

    fclose(fread);

    if (!lineExists)
        // we are at EOF and line does not exist already
    {
        if ((fappend = fopen(filename, "a")) == NULL)
        {
            CfOut(cf_error, "fopen", "!! Cannot open the file \"%s\" for append", filename);
            return false;
        }

        if (line[strlen(line) - 1] == '\n')
        {
            snprintf(lineCp, sizeof(lineCp), "%s", line);
        }
        else
        {
            snprintf(lineCp, sizeof(lineCp), "%s\n", line);
        }

        written = fwrite(lineCp, sizeof(char), strlen(lineCp), fappend);

        if (written == strlen(lineCp))
        {
            result = true;
        }
        else
        {
            CfOut(cf_error, "fwrite", "!! Could not write %zd characters to \"%s\" (wrote %zd)", strlen(lineCp),
                  filename, written);
            result = false;
        }

        fclose(fappend);
    }

    return result;
}

bool CopyFilePermissionsDisk(const char *source, const char *destination)
{
    struct stat statbuf;

    if (stat(source, &statbuf) == -1)
    {
        CfOut(cf_inform, "stat", "Can't copy permissions '%s'.", source);
        return false;
    }

    if (chmod(destination, statbuf.st_mode) != 0)
    {
        CfOut(cf_inform, "chmod", "Can't copy permissions '%s'.", destination);
        return false;
    }

    if (chown(destination, statbuf.st_uid, statbuf.st_gid) != 0)
    {
        CfOut(cf_inform, "chown", "Can't copy permissions '%s'.", destination);
        return false;
    }

    if (!CopyACLs(source, destination))
    {
        return false;
    }

#ifdef WITH_SELINUX
    int selinux_enabled = 0;
    security_context_t scontext = NULL;

    selinux_enabled = (is_selinux_enabled() > 0);

    if (selinux_enabled)
    {
        /* get current security context */
        if (getfilecon(source, &scontext) != 0)
        {
            if (errno != ENOTSUP && errno != ENODATA)
            {
                CfOut(cf_inform, "getfilecon", "Can't copy security context from '%s'.", source);
                return false;
            }
        }
        else
        {
            int ret = setfilecon(file, scontext);
            freecon(scontext);
            if (ret != 0)
            {
                if (errno != ENOTSUP)
                {
                    CfOut(cf_inform, "setfilecon", "Can't copy security context from '%s'.", destination);
                    return false;
                }
            }
        }
    }
#endif

    return true;
}
