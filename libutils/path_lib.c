#include "path_lib.h"

#include "list.h"
#include "buffer.h"
#include "alloc.h"

struct Path_
{
    List *comps;
    bool is_absolute;
};

static const char PATH_SEPARATOR = '/';

Path *PathNew(void)
{
    Path *p = xmalloc(sizeof(Path));

    p->is_absolute = false;
    p->comps = ListNew(NULL, NULL, free);

    return p;
}

void PathDestroy(Path *path)
{
    if (path)
    {
        ListDestroy(&path->comps);
    }
}

static void PathAppend(Path *path, const char *leaf)
{
    ListAppend(path->comps, (void *)xstrdup(leaf));
}

Path *PathFromString(const char *path)
{
    Path *p = PathNew();

    size_t len = strlen(path);
    if (len == 0)
    {
        return p;
    }

    p->is_absolute = path[0] == PATH_SEPARATOR;

    Buffer *buf = BufferNew();
    for (size_t i = 0; i < len; i++)
    {
        if (path[i] == PATH_SEPARATOR)
        {
            if (BufferSize(buf) > 0)
            {
                PathAppend(p, BufferData(buf));
                BufferZero(buf);
            }
        }
        else
        {
            BufferAppend(buf, &path[i], 1);
        }
    }
    if (BufferSize(buf) > 0)
    {
        PathAppend(p, BufferData(buf));
    }

    BufferDestroy(&buf);

    return p;
}

char *PathToString(const Path *path)
{
    Buffer *buf = BufferNew();

    if (path->is_absolute)
    {
        BufferAppend(buf, &PATH_SEPARATOR, 1);
    }

    {
        ListIterator *it = NULL;
        if (ListIteratorGet(path->comps, &it) == 0)
        {
            size_t i = 0;
            do
            {
                const char *comp = ListIteratorData(it);
                BufferAppend(buf, comp, strlen(comp));

                if (i < (ListCount(path->comps) - 1))
                {
                    BufferAppend(buf, &PATH_SEPARATOR, 1);
                }

                i++;
            } while (ListIteratorNext(it) == 0);
            ListIteratorDestroy(&it);
        }
    }


    char *ret = xstrdup(BufferData(buf));
    BufferDestroy(&buf);

    return ret;
}

bool PathIsAbsolute(const Path *path)
{
    return path->is_absolute;
}
