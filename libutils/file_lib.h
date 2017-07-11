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

#ifndef CFENGINE_FILE_LIB_H
#define CFENGINE_FILE_LIB_H

#include <platform.h>
#include <writer.h>

typedef enum
{
    NewLineMode_Unix,   // LF everywhere
    NewLineMode_Native  // CRLF on Windows, LF elsewhere
} NewLineMode;

/**
 * Reads up to size_max bytes from filename and returns a Writer.
 */
Writer *FileRead(const char *filename, size_t size_max, bool *truncated);
/**
 * Reads up to size_max bytes from fd and returns a Writer.
 */
Writer *FileReadFromFd(int fd, size_t size_max, bool *truncated);

bool FileCanOpen(const char *path, const char *modes);

/* Write LEN bytes at PTR to descriptor DESC, retrying if interrupted.
   Return LEN upon success, write's (negative) error code otherwise.  */
ssize_t FullWrite(int desc, const char *ptr, size_t len);

/* Read up to LEN bytes (or EOF) to PTR from descriptor DESC, retrying if interrupted.
   Return amount of bytes read upon success, -1 otherwise */
ssize_t FullRead(int desc, char *ptr, size_t len);

int IsDirReal(const char *path);

/**
 * Returns what type of line endings the file is using.
 *
 * @param file File to check.
 * @return Always returns NewLineMode_Unix on Unix. On Windows it may return
 *         NewLineMode_Native if the file has CRLF line endings.
 *         If the file cannot be opened, or the line endings are mixed it will
 *         return NewLineMode_Native. Note that only the first CF_BUFSIZE bytes
 *         are checked.
 */
NewLineMode FileNewLineMode(const char *file);

/* File node separator (cygwin can use \ or / but prefer \ for communicating
 * with native windows commands). */

#ifdef _WIN32
# define IsFileSep(c) ((c) == '\\' || (c) == '/')
# define FILE_SEPARATOR '\\'
# define FILE_SEPARATOR_STR "\\"
#else
# define IsFileSep(c) ((c) == '/')
# define FILE_SEPARATOR '/'
# define FILE_SEPARATOR_STR "/"
#endif

bool IsAbsoluteFileName(const char *f);
char *MapName(char *s);
char *MapNameCopy(const char *s);
char *MapNameForward(char *s);

int safe_open(const char *pathname, int flags, ...);
FILE *safe_fopen(const char *path, const char *mode);

int safe_chdir(const char *path);
int safe_chown(const char *path, uid_t owner, gid_t group);
int safe_chmod(const char *path, mode_t mode);
#ifndef __MINGW32__
int safe_lchown(const char *path, uid_t owner, gid_t group);
#endif
int safe_creat(const char *pathname, mode_t mode);

/**
 * @brief Sets whether a file descriptor should be closed on
 *        exec()/CreateProcess().
 * @param fd      File descriptor.
 * @param inherit Whether to enable close-on-exec or not.
 * @return true on success, false otherwise.
 */
bool SetCloseOnExec(int fd, bool enable);

/**
 * @brief Deletes directory path recursively. Symlinks are not followed.
 *        Note that this function only deletes the contents of the directory, not the directory itself.
 * @param path
 * @return true if directory was deleted successfully, false if one or more files were not deleted.
 */
bool DeleteDirectoryTree(const char *path);

bool FileSparseWrite(int fd, const void *buf, size_t count,
                     bool *wrote_hole);
bool FileSparseCopy(int sd, const char *src_name,
                    int dd, const char *dst_name,
                    size_t blk_size,
                    size_t *total_bytes_written,
                    bool   *last_write_was_a_hole);
bool FileSparseClose(int fd, const char *filename,
                     bool do_sync,
                     size_t total_bytes_written,
                     bool last_write_was_hole);

/**
 * @brief Works exactly like posix 'getline', EXCEPT it does not include carriage return at the end.
 * @return -1 on error OR EOF, so check. Or bytes in buff without excluding terminator.
 */
ssize_t CfReadLine(char **buff, size_t *size, FILE *fp);

#endif
