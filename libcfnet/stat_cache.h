/*
   Copyright 2018 Northern.tech AS

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
#ifndef CFENGINE_STAT_CACHE_H
#define CFENGINE_STAT_CACHE_H

#include <platform.h>
#include <cfnet.h>


typedef enum
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_LINK,
    FILE_TYPE_DIR,
    FILE_TYPE_FIFO,
    FILE_TYPE_BLOCK,
    FILE_TYPE_CHAR_,                             /* Conflict with winbase.h */
    FILE_TYPE_SOCK
} FileType;

typedef struct Stat_ Stat;
struct Stat_
{
    char *cf_filename;          /* What file are we statting? */
    char *cf_server;            /* Which server did this come from? */ //WHY? Isn't this in AgentConn?
    FileType cf_type;           /* enum filetype */
    mode_t cf_lmode;            /* Mode of link, if link */
    mode_t cf_mode;             /* Mode of remote file, not link */
    uid_t cf_uid;               /* User ID of the file's owner */
    gid_t cf_gid;               /* Group ID of the file's group */
    off_t cf_size;              /* File size in bytes */
    time_t cf_atime;            /* Time of last access */
    time_t cf_mtime;            /* Time of last data modification */
    time_t cf_ctime;            /* Time of last file status change */
    char cf_makeholes;          /* what we need to know from blksize and blks */
    char *cf_readlink;          /* link value or NULL */
    int cf_failed;              /* stat returned -1 */
    int cf_nlink;               /* Number of hard links */
    int cf_ino;                 /* inode number on server */
    dev_t cf_dev;               /* device number */
    Stat *next;
};

void DestroyStatCache(Stat *data);
int cf_remote_stat(AgentConnection *conn, bool encrypt, const char *file,
                   struct stat *statbuf, const char *stattype);
const Stat *StatCacheLookup(const AgentConnection *conn, const char *file_name,
                            const char *server_name);


#endif
