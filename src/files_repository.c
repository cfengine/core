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

#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "item_lib.h"
#include "cfstream.h"

/*********************************************************************/

static Item *VREPOSLIST;
static char REPOSCHAR;
static char *VREPOSITORY = NULL;

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

bool GetRepositoryPath(const char *file, Attributes attr, char *destination)
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

    if (!JoinPath(destination, file))
    {
        CfOut(cf_error, "", "Internal limit: Buffer ran out of space for long filename\n");
        return false;
    }

    for (char *s = destination + repopathlen; *s; s++)
    {
        if (*s == FILE_SEPARATOR)
        {
            *s = REPOSCHAR;
        }
    }

    return true;
}

/*********************************************************************/

int ArchiveToRepository(char *file, Attributes attr, Promise *pp, const ReportContext *report_context)
 /* Returns true if the file was backup up and false if not */
{
    char destination[CF_BUFSIZE];
    struct stat sb, dsb;

    if (!GetRepositoryPath(file, attr, destination))
    {
        return false;
    }

    if (attr.copy.backup == cfa_nobackup)
    {
        return true;
    }

    if (IsItemIn(VREPOSLIST, file))
    {
        CfOut(cf_inform, "",
              "The file %s has already been moved to the repository once. Multiple update will cause loss of backup.",
              file);
        return true;
    }

    ThreadLock(cft_getaddr);
    PrependItemList(&VREPOSLIST, file);
    ThreadUnlock(cft_getaddr);

    CfDebug("Repository(%s)\n", file);
    
    JoinPath(destination, CanonifyName(file));

    if (!MakeParentDirectory(destination, attr.move_obstructions, report_context))
    {
    }

    if (cfstat(file, &sb) == -1)
    {
        CfDebug("File %s promised to archive to the repository but it disappeared!\n", file);
        return true;
    }

    cfstat(destination, &dsb);

    CheckForFileHoles(&sb, pp);

    if (CopyRegularFileDisk(file, destination, pp->makeholes))
    {
        CfOut(cf_inform, "", "Moved %s to repository location %s\n", file, destination);
        return true;
    }
    else
    {
        CfOut(cf_inform, "", "Failed to move %s to repository location %s\n", file, destination);
        return false;
    }
}

bool FileInRepository(const char *filename)
{
    return IsItemIn(VREPOSLIST, filename);
}
