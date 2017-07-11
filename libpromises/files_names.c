/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <files_names.h>

#include <policy.h>
#include <promises.h>
#include <cf3.defs.h>
#include <dir.h>
#include <item_lib.h>
#include <files_interfaces.h>
#include <string_lib.h>
#include <known_dirs.h>
#include <conversion.h>

#include <cf-windows-functions.h>

/*********************************************************************/

int IsNewerFileTree(const char *dir, time_t reftime)
{
    const struct dirent *dirp;
    Dir *dirh;
    struct stat sb;

// Assumes that race conditions on the file path are unlikely and unimportant

    if (lstat(dir, &sb) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to stat directory '%s' in IsNewerFileTree. (stat: %s)", dir, GetErrorStr());
        // return true to provoke update
        return true;
    }

    if (S_ISDIR(sb.st_mode))
    {
        if (sb.st_mtime > reftime)
        {
            Log(LOG_LEVEL_VERBOSE, " >> Detected change in %s", dir);
            return true;
        }
    }

    if ((dirh = DirOpen(dir)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open directory '%s' in IsNewerFileTree. (opendir: %s)", dir, GetErrorStr());
        return false;
    }
    else
    {
        for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
        {
            if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
            {
                continue;
            }

            char path[CF_BUFSIZE];
            size_t ret = (size_t) snprintf(path, sizeof(path), "%s%c%s",
                                           dir, FILE_SEPARATOR, dirp->d_name);

            if (ret >= sizeof(path))
            {
                Log(LOG_LEVEL_ERR,
                    "Internal limit reached in IsNewerFileTree(),"
                    " path too long: '%s' + '%s'",
                    dir, dirp->d_name);
                DirClose(dirh);
                return false;
            }

            if (lstat(path, &sb) == -1)
            {
                Log(LOG_LEVEL_ERR, "Unable to stat directory '%s' in IsNewerFileTree. (lstat: %s)", path, GetErrorStr());
                DirClose(dirh);
                // return true to provoke update
                return true;
            }

            if (S_ISDIR(sb.st_mode))
            {
                if (sb.st_mtime > reftime)
                {
                    Log(LOG_LEVEL_VERBOSE, " >> Detected change in %s", path);
                    DirClose(dirh);
                    return true;
                }
                else
                {
                    if (IsNewerFileTree(path, reftime))
                    {
                        DirClose(dirh);
                        return true;
                    }
                }
            }
        }
    }

    DirClose(dirh);
    return false;
}

/*********************************************************************/

int IsDir(const char *path)
/*
Checks if the object pointed to by path exists and is a directory.
Returns true if so, false otherwise.
*/
{
#ifdef __MINGW32__
    return NovaWin_IsDir(path);
#else
    struct stat sb;

    if (stat(path, &sb) != -1)
    {
        if (S_ISDIR(sb.st_mode))
        {
            return true;
        }
    }

    return false;
#endif /* !__MINGW32__ */
}

/*********************************************************************/

/**
 * @TODO fix the dangerous path lengths
 */
char *JoinSuffix(char *path, const char *leaf)
{
    int len = strlen(leaf);

    if (Chop(path, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }
    DeleteSlash(path);

    if ((strlen(path) + len) > (CF_BUFSIZE - CF_BUFFERMARGIN))
    {
        Log(LOG_LEVEL_ERR, "Internal limit 2: Buffer ran out of space constructing string. Tried to add %s to %s",
              leaf, path);
        return NULL;
    }

    strcat(path, leaf);
    return path;
}

int IsAbsPath(const char *path)
{
    if (IsFileSep(*path))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Append a slash, of the kind that the string already has, only if the string
 * doesn't end in one.
 */
void AddSlash(char *str)
{
    char *sp, *sep = FILE_SEPARATOR_STR;
    int f = false, b = false;

    if (str == NULL)
    {
        return;
    }

    if (strlen(str) == 0)
    {
        strcpy(str, sep);
        return;
    }

/* Try to see what convention is being used for filenames
   in case this is a cross-system copy from Win/Unix */

    for (sp = str; *sp != '\0'; sp++)
    {
        switch (*sp)
        {
        case '/':
            f = true;
            break;
        case '\\':
            b = true;
            break;
        default:
            break;
        }
    }

    if (f && (!b))
    {
        sep = "/";
    }
    else if (b && (!f))
    {
        sep = "\\";
    }

    if (!IsFileSep(str[strlen(str) - 1]))
    {
        strcat(str, sep);
    }
}

/*********************************************************************/

char *GetParentDirectoryCopy(const char *path)
/**
 * WARNING: Remember to free return value.
 **/
{
    assert(path);
    assert(strlen(path) > 0);

    char *path_copy = xstrdup(path);

    if(strcmp(path_copy, "/") == 0)
    {
        return path_copy;
    }

    char *sp = (char *)LastFileSeparator(path_copy);

    if(!sp)
    {
        Log(LOG_LEVEL_ERR, "Path %s does not contain file separators (GetParentDirectory())", path_copy);
        free(path_copy);
        return NULL;
    }

    if(sp == FirstFileSeparator(path_copy))  // don't chop off first path separator
    {
        *(sp + 1) = '\0';
    }
    else
    {
        *sp = '\0';
    }

    return path_copy;
}

/*********************************************************************/

// Can remove several slashes if they are redundant.
void DeleteSlash(char *str)
{
    int size = strlen(str);
    if ((size == 0) || (str == NULL))
    {
        return;
    }

    int root = RootDirLength(str);
    while (IsFileSep(str[size - 1]) && size - 1 > root)
    {
        size--;
    }
    str[size] = '\0'; /* no-op if we didn't change size */
}

/*********************************************************************/

void DeleteRedundantSlashes(char *str)
{
    int move_from;
    // Invariant: newpos <= oldpos
    int oldpos = RootDirLength(str);
    int newpos = oldpos;
    while (str[oldpos] != '\0')
    {
        // Skip over subsequent separators.
        while (IsFileSep(str[oldpos]))
        {
            oldpos++;
        }
        move_from = oldpos;

        // And then skip over the next path component.
        while (str[oldpos] != '\0' && !IsFileSep(str[oldpos]))
        {
            oldpos++;
        }

        // If next character is file separator, move past it, since we want to keep one.
        if (IsFileSep(str[oldpos]))
        {
            oldpos++;
        }

        int move_len = oldpos - move_from;
        memmove(&str[newpos], &str[move_from], move_len);
        newpos += move_len;
    }

    str[newpos] = '\0';
}

/*********************************************************************/

const char *FirstFileSeparator(const char *str)
{
    assert(str);
    assert(strlen(str) > 0);

    if(strncmp(str, "\\\\", 2) == 0)  // windows share
    {
        return str + 1;
    }

    for(const char *pos = str; *pos != '\0'; pos++)
    {
        if(IsFileSep(*pos))
        {
            return pos;
        }
    }

    return NULL;
}

/*********************************************************************/

const char *LastFileSeparator(const char *str)
  /* Return pointer to last file separator in string, or NULL if 
     string does not contains any file separtors */
{
    const char *sp;

/* Walk through string backwards */

    sp = str + strlen(str) - 1;

    while (sp >= str)
    {
        if (IsFileSep(*sp))
        {
            return sp;
        }
        sp--;
    }

    return NULL;
}

/*********************************************************************/

bool ChopLastNode(char *str)
  /* Chop off trailing node name (possible blank) starting from
     last character and removing up to the first / encountered 
     e.g. /a/b/c -> /a/b
     /a/b/ -> /a/b
     Will also collapse redundant/repeating path separators.
  */
{
    char *sp;
    int ret;

    DeleteRedundantSlashes(str);

/* Here cast is necessary and harmless, str is modifiable */
    if ((sp = (char *) LastFileSeparator(str)) == NULL)
    {
        int pos = RootDirLength(str);
        if (str[pos] == '\0')
        {
            ret = false;
        }
        else
        {
            str[pos] = '.';
            str[pos + 1] = '\0';
            ret = true;
        }
    }
    else
    {
        // Don't chop the root slash in an absolute path.
        if (IsAbsoluteFileName(str) && FirstFileSeparator(str) == sp)
        {
            *(++sp) = '\0';
        }
        else
        {
            *sp = '\0';
        }
        ret = true;
    }

    return ret;
}

/*********************************************************************/

void CanonifyNameInPlace(char *s)
{
    for (; *s != '\0'; s++)
    {
        if (!isalnum((unsigned char) *s))
        {
            *s = '_';
        }
    }
}

/*********************************************************************/

void TransformNameInPlace(char *s, char from, char to)
{
    for (; *s != '\0'; s++)
    {
        if (*s == from)
        {
            *s = to;
        }
    }
}

/*********************************************************************/

/* TODO remove, kill, burn this function! */
char *CanonifyName(const char *str)
{
    static char buffer[CF_BUFSIZE]; /* GLOBAL_R, no initialization needed */

    strlcpy(buffer, str, CF_BUFSIZE);
    CanonifyNameInPlace(buffer);
    return buffer;
}

/*********************************************************************/

char *CanonifyChar(const char *str, char ch)
{
    static char buffer[CF_BUFSIZE]; /* GLOBAL_R, no initialization needed */
    char *sp;

    strlcpy(buffer, str, CF_BUFSIZE);

    for (sp = buffer; *sp != '\0'; sp++)
    {
        if (*sp == ch)
        {
            *sp = '_';
        }
    }

    return buffer;
}

/*********************************************************************/

int CompareCSVName(const char *s1, const char *s2)
{
    const char *sp1, *sp2;
    char ch1, ch2;

    for (sp1 = s1, sp2 = s2; (*sp1 != '\0') || (*sp2 != '\0'); sp1++, sp2++)
    {
        ch1 = (*sp1 == ',') ? '_' : *sp1;
        ch2 = (*sp2 == ',') ? '_' : *sp2;

        if (ch1 > ch2)
        {
            return 1;
        }
        else if (ch1 < ch2)
        {
            return -1;
        }
    }

    return 0;
}

/*********************************************************************/

const char *ReadLastNode(const char *str)
/* Return the last node of a pathname string  */
{
    const char *sp;

    if ((sp = LastFileSeparator(str)) == NULL)
    {
        return str;
    }
    else
    {
        return sp + 1;
    }
}

/*********************************************************************/

/**
 * @TODO fix the dangerous path lengths
 */
int CompressPath(char *dest, const char *src)
{
    char node[CF_BUFSIZE];
    int nodelen;
    int rootlen;

    memset(dest, 0, CF_BUFSIZE);

    rootlen = RootDirLength(src);
    memcpy(dest, src, rootlen);

    for (const char *sp = src + rootlen; *sp != '\0'; sp++)
    {
        if (IsFileSep(*sp))
        {
            continue;
        }

        for (nodelen = 0; (sp[nodelen] != '\0') && (!IsFileSep(sp[nodelen])); nodelen++)
        {
            if (nodelen > CF_MAXLINKSIZE)
            {
                Log(LOG_LEVEL_ERR, "Link in path suspiciously large");
                return false;
            }
        }

        strncpy(node, sp, nodelen);
        node[nodelen] = '\0';

        sp += nodelen - 1;

        if (strcmp(node, ".") == 0)
        {
            continue;
        }

        if (strcmp(node, "..") == 0)
        {
            if (!ChopLastNode(dest))
            {
                Log(LOG_LEVEL_DEBUG, "used .. beyond top of filesystem!");
                return false;
            }

            continue;
        }

        AddSlash(dest);

        /* TODO use dest_size parameter instead of CF_BUFSIZE. */
        size_t ret = strlcat(dest, node, CF_BUFSIZE);

        if (ret >= CF_BUFSIZE)
        {
            Log(LOG_LEVEL_ERR,
                "Internal limit reached in CompressPath(),"
                " path too long: '%s' + '%s'",
                dest, node);
            return false;
        }
    }

    return true;
}

/*********************************************************************/

FilePathType FilePathGetType(const char *file_path)
{
    if (IsAbsoluteFileName(file_path))
    {
        return FILE_PATH_TYPE_ABSOLUTE;
    }
    else if (*file_path == '.')
    {
        return FILE_PATH_TYPE_RELATIVE;
    }
    else
    {
        return FILE_PATH_TYPE_NON_ANCHORED;
    }
}

bool IsFileOutsideDefaultRepository(const char *f)
{
    return !StringStartsWith(f, GetInputDir());
}

/*******************************************************************/

static int UnixRootDirLength(const char *f)
{
    if (IsFileSep(*f))
    {
        return 1;
    }

    return 0;
}

#ifdef _WIN32
static int NTRootDirLength(const char *f)
{
    int len;

    if (f[0] == '\\' && f[1] == '\\')
    {
        /* UNC style path */

        /* Skip over host name */
        for (len = 2; f[len] != '\\'; len++)
        {
            if (f[len] == '\0')
            {
                return len;
            }
        }

        /* Skip over share name */

        for (len++; f[len] != '\\'; len++)
        {
            if (f[len] == '\0')
            {
                return len;
            }
        }

        /* Skip over file separator */
        len++;

        return len;
    }

    if (isalpha(f[0]) && f[1] == ':')
    {
        if (IsFileSep(f[2]))
        {
            return 3;
        }

        return 2;
    }

    return UnixRootDirLength(f);
}
#endif

int RootDirLength(const char *f)
  /* Return length of Initial directory in path - */
{
#ifdef _WIN32
    return NTRootDirLength(f);
#else
    return UnixRootDirLength(f);
#endif
}

/* Buffer should be at least CF_MAXVARSIZE large */
const char *GetSoftwareCacheFilename(char *buffer)
{
    snprintf(buffer, CF_MAXVARSIZE, "%s/%s", GetStateDir(), SOFTWARE_PACKAGES_CACHE);
    MapName(buffer);
    return buffer;
}

/* Buffer should be at least CF_MAXVARSIZE large */
const char *GetSoftwarePatchesFilename(char *buffer)
{
    snprintf(buffer, CF_MAXVARSIZE, "%s/%s", GetStateDir(), SOFTWARE_PATCHES_CACHE);
    MapName(buffer);
    return buffer;
}

const char *RealPackageManager(const char *manager)
{
    assert(manager);

    const char *pos = strchr(manager, ' ');
    if (strncmp(manager, "env ", 4) != 0
        && (!pos || pos - manager < 4 || strncmp(pos - 4, "/env", 4) != 0))
    {
        return CommandArg0(manager);
    }

    // Look for variable assignments.
    const char *last_pos;
    bool eq_sign_found = false;
    while (true)
    {
        if (eq_sign_found)
        {
            last_pos = pos + 1;
        }
        else
        {
            last_pos = pos + strspn(pos, " "); // Skip over consecutive spaces.
        }
        pos = strpbrk(last_pos, "= ");
        if (!pos)
        {
            break;
        }
        if (*pos == '=')
        {
            eq_sign_found = true;
        }
        else if (eq_sign_found)
        {
            eq_sign_found = false;
        }
        else
        {
            return CommandArg0(last_pos);
        }
    }

    // Reached the end? Weird. Must be env command with no real command.
    return CommandArg0(manager);
}
