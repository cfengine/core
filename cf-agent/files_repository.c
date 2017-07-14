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

#include <cf3.defs.h>

#include <files_names.h>
#include <files_interfaces.h>
#include <files_lib.h>
#include <files_copy.h>
#include <item_lib.h>
#include <mutex.h>
#include <policy.h>
#include <string_lib.h>                                       /* PathAppend */

/*********************************************************************/

static Item *VREPOSLIST = NULL; /* GLOBAL_X */
static char REPOSCHAR = '_'; /* GLOBAL_P */
static char *VREPOSITORY = NULL; /* GLOBAL_P */

/*********************************************************************/

void SetRepositoryLocation(const char *path)
{
    VREPOSITORY = xstrdup(path);
}

/*********************************************************************/

void SetRepositoryChar(char c)
{
    REPOSCHAR = c;
}

/*********************************************************************/

bool GetRepositoryPath(ARG_UNUSED const char *file, Attributes attr, char *destination)
{
    if ((attr.repository == NULL) && (VREPOSITORY == NULL))
    {
        return false;
    }

    size_t repopathlen;

    if (attr.repository != NULL)
    {
        repopathlen = strlcpy(destination, attr.repository, CF_BUFSIZE);
    }
    else
    {
        repopathlen = strlcpy(destination, VREPOSITORY, CF_BUFSIZE);
    }

    if (repopathlen >= CF_BUFSIZE)
    {
        Log(LOG_LEVEL_ERR, "Internal limit, buffer ran out of space for long filename");
        return false;
    }

    return true;
}

/*********************************************************************/

int ArchiveToRepository(const char *file, Attributes attr)
 /* Returns true if the file was backup up and false if not */
{
    char destination[CF_BUFSIZE];
    struct stat sb, dsb;

    if (!GetRepositoryPath(file, attr, destination))
    {
        return false;
    }

    if (attr.copy.backup == BACKUP_OPTION_NO_BACKUP)
    {
        return true;
    }

    if (IsItemIn(VREPOSLIST, file))
    {
        Log(LOG_LEVEL_INFO,
            "The file '%s' has already been moved to the repository once. Multiple update will cause loss of backup.",
              file);
        return true;
    }

    ThreadLock(cft_getaddr);
    PrependItemList(&VREPOSLIST, file);
    ThreadUnlock(cft_getaddr);

    if (!PathAppend(destination, sizeof(destination),
                    CanonifyName(file), FILE_SEPARATOR))
    {
        Log(LOG_LEVEL_ERR,
            "Internal limit reached in ArchiveToRepository(),"
            " path too long: '%s' + '%s'",
            destination, CanonifyName(file));
        return false;
    }

    if (!MakeParentDirectory(destination, attr.move_obstructions))
    {
    }

    if (stat(file, &sb) == -1)
    {
        Log(LOG_LEVEL_DEBUG, "File '%s' promised to archive to the repository but it disappeared!", file);
        return true;
    }

    stat(destination, &dsb);

    if (CopyRegularFileDisk(file, destination))
    {
        Log(LOG_LEVEL_INFO, "Moved '%s' to repository location '%s'", file, destination);
        return true;
    }
    else
    {
        Log(LOG_LEVEL_INFO, "Failed to move '%s' to repository location '%s'", file, destination);
        return false;
    }
}

bool FileInRepository(const char *filename)
{
    return IsItemIn(VREPOSLIST, filename);
}
