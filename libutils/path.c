#include <path.h>
#include <string_lib.h>
#include <file_lib.h>

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

char *Path_GetQuoted(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    size_t path_len = strlen(path);
    if ((path[0] == '"') && (path[path_len - 1] == '"'))
    {
        /* already quoted, just duplicate */
        return SafeStringDuplicate(path);
    }

    bool needs_quoting = false;
    for (const char *cp = path; !needs_quoting && (*cp != '\0'); cp++)
    {
        /* let's quote everything that's not just alphanumerics, underscores and
         * dashes */
        needs_quoting = !(((*cp >= 'a') && (*cp <= 'z')) ||
                          ((*cp >= 'A') && (*cp <= 'Z')) ||
                          ((*cp >= '0') && (*cp <= '9')) ||
                          (*cp == '_') || (*cp == '-')   ||
                          IsFileSep(*cp));
    }
    if (needs_quoting)
    {
        return StringConcatenate(3, "\"", path, "\"");
    }
    else
    {
        return SafeStringDuplicate(path);
    }
}
