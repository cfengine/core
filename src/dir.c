#include "cf3.defs.h"
#include "cf3.extern.h"

#include "dir.h"
#include "dir_priv.h"

/*********************************************************************/

Dir *OpenDirForPromise(const char *dirname, Attributes attr, Promise *pp)
{
    if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item, "localhost") == 0)
    {
        return OpenDirLocal(dirname);
    }
    else
    {
        /* -> client_code.c to talk to server */
        return OpenDirRemote(dirname, attr, pp);
    }
}

/*********************************************************************/

static const struct dirent *ReadDirRemote(Dir *dir)
{
    const char *ret = NULL;

    if (dir->listpos != NULL)
    {
        ret = dir->listpos->name;
        dir->listpos = dir->listpos->next;
    }

    return (struct dirent *) ret;
}

/*********************************************************************/

static void CloseDirRemote(Dir *dir)
{
    if (dir->list)
    {
        DeleteItemList(dir->list);
    }
    free(dir);
}

/*********************************************************************/

const struct dirent *ReadDir(Dir *dir)
{
    if (dir->list)
    {
        return ReadDirRemote(dir);
    }
    else if (dir->dirh)
    {
        return ReadDirLocal(dir);
    }
    else
    {
        FatalError("Dir passed has no list nor directory handle open");
    }
}

/*********************************************************************/

void CloseDir(Dir *dir)
{
    if (dir->dirh)
    {
        CloseDirLocal(dir);
    }
    else
    {
        CloseDirRemote(dir);
    }
}
