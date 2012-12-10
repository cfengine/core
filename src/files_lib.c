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

#include "files_lib.h"

#include "files_interfaces.h"
#include "files_operators.h"
#include "item_lib.h"
#include "cfstream.h"

#include <assert.h>


static Item *NextItem(const Item *ip);
static int ItemListsEqual(const Item *list1, const Item *list2, int report, Attributes a, const Promise *pp);

/*********************************************************************/

bool FileCanOpen(const char *path, const char *modes)
{
    FILE *test = NULL;

    if ((test = fopen(path, modes)) != NULL)
    {
        fclose(test);
        return true;
    }
    else
    {
        return false;
    }
}

/*********************************************************************/

void PurgeItemList(Item **list, char *name)
{
    Item *ip, *copy = NULL;
    struct stat sb;

    CopyList(&copy, *list);

    for (ip = copy; ip != NULL; ip = ip->next)
    {
        if (cfstat(ip->name, &sb) == -1)
        {
            CfOut(cf_verbose, "", " -> Purging file \"%s\" from %s list as it no longer exists", ip->name, name);
            DeleteItemLiteral(list, ip->name);
        }
    }

    DeleteItemList(copy);
}

/*********************************************************************/

int RawSaveItemList(const Item *liststart, const char *file)
{
    char new[CF_BUFSIZE], backup[CF_BUFSIZE];
    FILE *fp;

    strcpy(new, file);
    strcat(new, CF_EDITED);

    strcpy(backup, file);
    strcat(backup, CF_SAVED);

    unlink(new);                /* Just in case of races */

    if ((fp = fopen(new, "w")) == NULL)
    {
        CfOut(cf_error, "fopen", "Couldn't write file %s\n", new);
        return false;
    }

    for (const Item *ip = liststart; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%s\n", ip->name);
    }

    if (fclose(fp) == -1)
    {
        CfOut(cf_error, "fclose", "Unable to close file while writing");
        return false;
    }

    if (cf_rename(new, file) == -1)
    {
        CfOut(cf_inform, "cf_rename", "Error while renaming %s\n", file);
        return false;
    }

    return true;
}

/*********************************************************************/

int CompareToFile(const Item *liststart, const char *file, Attributes a, const Promise *pp)
/* returns true if file on disk is identical to file in memory */
{
    struct stat statbuf;
    Item *cmplist = NULL;

    CfDebug("CompareToFile(%s)\n", file);

    if (cfstat(file, &statbuf) == -1)
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

    if (!LoadFileAsItemList(&cmplist, file, a, pp))
    {
        return false;
    }

    if (!ItemListsEqual(cmplist, liststart, (a.transaction.action == cfa_warn), a, pp))
    {
        DeleteItemList(cmplist);
        return false;
    }

    DeleteItemList(cmplist);
    return (true);
}

/*********************************************************************/

static int ItemListsEqual(const Item *list1, const Item *list2, int warnings, Attributes a, const Promise *pp)
// Some complex logic here to enable warnings of diffs to be given
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
                    cfPS(cf_error, CF_WARN, "", pp, a,
                         " ! File content wants to change from from/to full/empty but only a warning promised");
                }
                else
                {
                    if (ip1 != NULL)
                    {
                        cfPS(cf_error, CF_WARN, "", pp, a, " ! edit_line change warning promised: (remove) %s",
                             ip1->name);
                    }

                    if (ip2 != NULL)
                    {
                        cfPS(cf_error, CF_WARN, "", pp, a, " ! edit_line change warning promised: (add) %s", ip2->name);
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

                cfPS(cf_error, CF_WARN, "", pp, a, " ! edit_line warning promised: - %s", ip1->name);
                cfPS(cf_error, CF_WARN, "", pp, a, " ! edit_line warning promised: + %s", ip2->name);
                retval = false;
            }
        }

        ip1 = NextItem(ip1);
        ip2 = NextItem(ip2);
    }

    return retval;
}

/*********************************************************************/

ssize_t FileRead(const char *filename, char *buffer, size_t bufsize)
{
    FILE *f = fopen(filename, "rb");

    if (f == NULL)
    {
        return -1;
    }
    ssize_t ret = fread(buffer, bufsize, 1, f);

    if (ferror(f))
    {
        fclose(f);
        return -1;
    }
    fclose(f);
    return ret;
}

/*********************************************************************/

bool FileWriteOver(char *filename, char *contents)
{
    FILE *fp = fopen(filename, "w");

    if(fp == NULL)
    {
        return false;
    }

    int bytes_to_write = strlen(contents);

    size_t bytes_written = fwrite(contents, 1, bytes_to_write, fp);

    bool res = true;

    if(bytes_written != bytes_to_write)
    {
        res = false;
    }

    if(fclose(fp) != 0)
    {
        res = false;
    }

    return res;
}


/*********************************************************************/

ssize_t FileReadMax(char **output, char *filename, size_t size_max)
// TODO: there is CfReadFile and FileRead with slightly different semantics, merge
// free(output) should be called on positive return value
{
    assert(size_max > 0);

    struct stat sb;
    if (cfstat(filename, &sb) == -1)
    {
        return -1;
    }

    FILE *fin;

    if ((fin = fopen(filename, "r")) == NULL)
    {
        return -1;
    }

    ssize_t bytes_to_read = MIN(sb.st_size, size_max);
    *output = xcalloc(bytes_to_read + 1, 1);
    ssize_t bytes_read = fread(*output, 1, bytes_to_read, fin);

    if (ferror(fin))
    {
        CfOut(cf_error, "ferror", "FileContentsRead: Error while reading file %s", filename);
        fclose(fin);
        free(*output);
        *output = NULL;
        return -1;
    }

    if (fclose(fin) != 0)
    {
        CfOut(cf_error, "fclose", "FileContentsRead: Could not close file %s", filename);
    }

    return bytes_read;
}

/*********************************************************************/
/* helpers                                                           */
/*********************************************************************/

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
