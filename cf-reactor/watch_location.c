/*
  Copyright 2026 Northern.tech AS

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

/* Whether a watched file is on a remote file system, whose checks may block
 * for as long as the server doesn't respond, so they get their own thread
 * (see watcher.c). Only ever decided from the path and the mount table: any
 * call on the path itself (stat(), realpath()) could block just the same. */

#include <watch_location.h>
#include <logging.h>
#include <alloc.h>
#include <string_lib.h>         /* StringEqual(), StringStartsWith(), IsStrIn() */
#include <file_lib.h>           /* FileReadFromFd() */
#include <writer.h>             /* StringWriterClose() */

/* More than any real mount table, against reading forever */
#define MOUNTINFO_MAX_SIZE (4 * 1024 * 1024)

/* Field 5 of a line of /proc/self/mountinfo (counting from 1) is the mount
 * point, and the file system type follows the "-" ending the optional
 * fields. See proc(5). */
#define MOUNTINFO_MOUNT_POINT_FIELD 5

/**
 * Whether files of this type may hang when their server doesn't respond.
 * FUSE counts as remote: its daemon can hang just the same, whatever it
 * serves, and so can virtiofs's on the host.
 */
static bool FsTypeIsRemote(const char *fstype)
{
    assert(fstype != NULL);

    static const char *const REMOTE_FS_TYPES[] = {
        "nfs", "nfs4", "cifs", "smb3", "smbfs", "ncpfs", "9p", "ceph", "glusterfs",
        "afs", "lustre", "beegfs", "panfs", "ocfs2", "gfs2", "virtiofs",
        "fuse", "fuseblk", NULL
    };
    return IsStrIn(fstype, REMOTE_FS_TYPES) || StringStartsWith(fstype, "fuse.");
}

/**
 * Decodes, in place, the octal escapes mountinfo writes for the characters
 * that would break its format, e.g. "\040" for a space.
 */
static void MountinfoUnescape(char *s)
{
    assert(s != NULL);

    char *out = s;
    const char *in = s;
    while (*in != '\0')
    {
        if (in[0] == '\\' &&
            in[1] >= '0' && in[1] <= '3' &&
            in[2] >= '0' && in[2] <= '7' &&
            in[3] >= '0' && in[3] <= '7')
        {
            *out++ = (char) (((in[1] - '0') << 6) | ((in[2] - '0') << 3) | (in[3] - '0'));
            in += 4;
        }
        else
        {
            *out++ = *in++;
        }
    }
    *out = '\0';
}

typedef struct
{
    char *mount_point;
    size_t length;
    bool remote;
} MountEntry;

/* The mount points of a mount table, in mount order */
typedef struct
{
    MountEntry *entries;
    size_t count;
} MountTable;

/**
 * Finds, in place, the mount point and the file system type of a line of
 * mountinfo, the only fields used. Returns false for a line it can't make
 * sense of.
 */
static bool MountinfoParseLine(char *line, char **mount_point, const char **fstype)
{
    assert(line != NULL);
    assert(mount_point != NULL);
    assert(fstype != NULL);

    char *field = line;
    for (int i = 1; i < MOUNTINFO_MOUNT_POINT_FIELD; i++)
    {
        field = strchr(field, ' ');
        if (field == NULL)
        {
            return false;
        }
        field++;
    }
    char *end = strchr(field, ' ');
    if (end == NULL)
    {
        return false;
    }
    *end = '\0';
    *mount_point = field;

    /* Past the mount options and the optional fields, to the "-" */
    for (char *next = end + 1; ; )
    {
        char *separator = strchr(next, ' ');
        if (separator == NULL)
        {
            return false;
        }
        if (separator == next + 1 && next[0] == '-')
        {
            char *type = separator + 1;
            char *type_end = strchr(type, ' ');
            if (type_end != NULL)
            {
                *type_end = '\0';
            }
            *fstype = type;
            return (type[0] != '\0');
        }
        next = separator + 1;
    }
}

static MountTable *MountTableParse(const char *mountinfo)
{
    assert(mountinfo != NULL);

    MountTable *table = xcalloc(1, sizeof(MountTable));
    size_t capacity = 0;
    char *text = xstrdup(mountinfo);
    for (char *line = text; line != NULL && *line != '\0'; )
    {
        char *line_end = strchr(line, '\n');
        if (line_end != NULL)
        {
            *line_end = '\0';
        }

        char *mount_point;
        const char *fstype;
        if (MountinfoParseLine(line, &mount_point, &fstype))
        {
            if (table->count == capacity)
            {
                capacity = (capacity == 0) ? 64 : capacity * 2;
                table->entries = xrealloc(table->entries, capacity * sizeof(MountEntry));
            }
            MountinfoUnescape(mount_point);
            MountEntry *entry = &table->entries[table->count++];
            entry->mount_point = xstrdup(mount_point);
            entry->length = strlen(mount_point);
            entry->remote = FsTypeIsRemote(fstype);
        }
        line = (line_end != NULL) ? line_end + 1 : NULL;
    }
    free(text);
    return table;
}

static void MountTableDestroy(MountTable *table)
{
    if (table != NULL)
    {
        for (size_t i = 0; i < table->count; i++)
        {
            free(table->entries[i].mount_point);
        }
        free(table->entries);
        free(table);
    }
}

/**
 * Length of the mount point of `entry` if `path` is under it (or is it),
 * else 0. Only compares whole components, so /mnt/nfs doesn't contain
 * /mnt/nfs2.
 */
static size_t MountPointMatch(const MountEntry *entry, const char *path)
{
    assert(entry != NULL);
    assert(path != NULL);

    if (StringEqual(entry->mount_point, "/"))
    {
        return 1;
    }
    const size_t length = entry->length;
    if (length == 0 || !StringEqualN(entry->mount_point, path, length))
    {
        return 0;
    }
    return (path[length] == '/' || path[length] == '\0') ? length : 0;
}

/**
 * The absolute `path` without repeated or trailing slashes, "." or "..", as
 * in the mount points. Lexical: "/mnt/link/.." is "/mnt" even if link is a
 * symlink elsewhere, where the kernel would resolve the link first.
 */
static char *NormalizePath(const char *path)
{
    assert(path != NULL);
    assert(path[0] == '/');

    char *normalized = xmalloc(strlen(path) + 2);
    size_t length = 0;
    const char *component = path;
    while (*component != '\0')
    {
        if (*component == '/')
        {
            component++;
            continue;
        }
        const char *slash = strchr(component, '/');
        const size_t component_length = (slash != NULL) ? (size_t) (slash - component) :
                                                          strlen(component);
        if (component_length == 2 && component[0] == '.' && component[1] == '.')
        {
            /* Drop the last component, the root's parent is the root */
            while (length > 0 && normalized[length - 1] != '/')
            {
                length--;
            }
            if (length > 0)
            {
                length--;
            }
        }
        else if (component_length != 1 || component[0] != '.')
        {
            normalized[length++] = '/';
            memcpy(normalized + length, component, component_length);
            length += component_length;
        }
        component += component_length;
    }
    if (length == 0)
    {
        normalized[length++] = '/';
    }
    normalized[length] = '\0';
    return normalized;
}

/**
 * Whether `path` is on a remote file system according to `table`: that of the
 * longest mount point containing it, the last mounted of equal ones, as it
 * hides those below. Purely lexical, so a symlink to a remote file system
 * isn't seen, nor is a relative path.
 */
static bool MountTablePathIsRemote(const MountTable *table, const char *path)
{
    assert(table != NULL);
    assert(path != NULL);

    if (path[0] != '/')
    {
        return false;
    }

    char *normalized = NormalizePath(path);
    size_t longest = 0;
    bool remote = false;
    for (size_t i = 0; i < table->count; i++)
    {
        const size_t match = MountPointMatch(&table->entries[i], normalized);
        if (match > 0 && match >= longest)
        {
            longest = match;
            remote = table->entries[i].remote;
        }
    }
    free(normalized);
    return remote;
}

/**
 * Whether `path` is on a remote file system according to `mountinfo`, the
 * text of /proc/self/mountinfo, see MountTablePathIsRemote()
 */
bool MountinfoPathIsRemote(const char *mountinfo, const char *path)
{
    assert(mountinfo != NULL);
    assert(path != NULL);

    MountTable *table = MountTableParse(mountinfo);
    const bool remote = MountTablePathIsRemote(table, path);
    MountTableDestroy(table);
    return remote;
}

static bool IsPathSeparator(char c)
{
    return (c == '\\' || c == '/');
}

/**
 * Whether `path` is a Windows UNC path (\\server\share, //server/share,
 * \\?\UNC\server\share). Lexical, so available everywhere for the tests.
 */
bool WatchPathIsUNC(const char *path)
{
    assert(path != NULL);

    if (!IsPathSeparator(path[0]) || !IsPathSeparator(path[1]))
    {
        return false;
    }
    if ((path[2] == '?' || path[2] == '.') && IsPathSeparator(path[3]))
    {
        /* A long path (\\?\C:\...) or a device (\\.\...), unless \\?\UNC\ */
        return (path[2] == '?' && strncasecmp(path + 4, "UNC", 3) == 0 &&
                IsPathSeparator(path[7]));
    }
    return (path[2] != '\0');
}

#ifdef __MINGW32__
static bool WindowsPathIsRemote(const char *path)
{
    assert(path != NULL);

    if (WatchPathIsUNC(path))
    {
        return true;
    }

    const char *drive = path;
    if (IsPathSeparator(path[0]) && IsPathSeparator(path[1]) && path[2] == '?' &&
        IsPathSeparator(path[3]))
    {
        drive = path + 4;
    }
    if (!isalpha((unsigned char) drive[0]) || drive[1] != ':')
    {
        return false;
    }
    /* Asks about the drive, not the path, so it doesn't wait for its server */
    const wchar_t root[] = { (wchar_t) drive[0], L':', L'\\', L'\0' };
    return (GetDriveTypeW(root) == DRIVE_REMOTE);
}
#else  /* !__MINGW32__ */
/**
 * The text of the mount table in the format of /proc/self/mountinfo, NULL if
 * there is none (no other Unix has one we can read without risking to block).
 * A procfs file, so reading it doesn't block.
 */
static char *ReadMountTable(void)
{
# ifdef __linux__
    const char *const file = "/proc/self/mountinfo";
    const int fd = open(file, O_RDONLY);
    if (fd == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to open the mount table '%s' (open: %s)", file, GetErrorStr());
        return NULL;
    }
    bool truncated = false;
    Writer *contents = FileReadFromFd(fd, MOUNTINFO_MAX_SIZE, &truncated);
    close(fd);
    if (contents == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to read the mount table '%s'", file);
        return NULL;
    }
    if (truncated)
    {
        Log(LOG_LEVEL_VERBOSE, "The mount table '%s' is larger than %d bytes, only reading the start",
            file, MOUNTINFO_MAX_SIZE);
    }
    return StringWriterClose(contents);
# else
    return NULL;
# endif
}
#endif  /* !__MINGW32__ */

/* The mount table, read and parsed once per policy (re-)read, as all its
 * watchers are registered at once, NULL until needed. Only used by the main
 * thread. */
static MountTable *mount_table = NULL;

/* Replaces the mount table of the host, for the tests */
static char *mount_table_override = NULL;

/**
 * Forgets the mount table, so that it is read again for the next watcher.
 * Called once the watchers of a policy are registered.
 */
void WatchLocationForgetMountTable(void)
{
    MountTableDestroy(mount_table);
    mount_table = NULL;
}

/**
 * Makes WatchTargetIsRemote() use `mountinfo`, in the format of
 * /proc/self/mountinfo, instead of the mount table of the host, until called
 * with NULL. For the tests, not used on Windows (no mount table).
 */
void WatchLocationSetMountTable(const char *mountinfo)
{
    free(mount_table_override);
    mount_table_override = (mountinfo != NULL) ? xstrdup(mountinfo) : NULL;
    WatchLocationForgetMountTable();
}

/**
 * Whether checks of the watcher of `target` may block on a remote file system.
 * Only files can be remote. Without a mount table, everything is local.
 */
bool WatchTargetIsRemote(const ReactorWatchTarget *target)
{
    assert(target != NULL);

    if (target == NULL || target->type != REACTOR_WATCH_TARGET_FILE || target->path == NULL)
    {
        return false;
    }

#ifdef __MINGW32__
    return WindowsPathIsRemote(target->path);
#else
    if (mount_table == NULL)
    {
        char *text = (mount_table_override == NULL) ? ReadMountTable() : NULL;
        const char *mountinfo = (mount_table_override != NULL) ? mount_table_override : text;
        mount_table = MountTableParse((mountinfo != NULL) ? mountinfo : "");
        free(text);
    }
    return MountTablePathIsRemote(mount_table, target->path);
#endif
}
