#include <path.h>
#include <string_lib.h>

const char *Path_Basename(const char *path)
{
    assert(path != NULL);

    const char *filename = strrchr(path, '/');

    if (filename != NULL)
    {
        filename += 1;
    }
    else
    {
        filename = path;
    }

    if (filename[0] == '\0')
    {
        return NULL;
    }

    return filename;
}

char *Path_JoinAlloc(const char *dir, const char *leaf)
{
    if (StringEndsWith(dir, "/"))
    {
        return StringConcatenate(2, dir, leaf);
    }
    return StringConcatenate(3, dir, "/", leaf);
}
