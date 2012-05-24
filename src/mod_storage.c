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
#include "mod_storage.h"

static const BodySyntax CF_CHECKVOL_BODY[] =
{
    {"check_foreign", cf_opts, CF_BOOL, "true/false verify storage that is mounted from a foreign system on this host",
     "false"},
    {"freespace", cf_str, "[0-9]+[MBkKgGmb%]",
     "Absolute or percentage minimum disk space that should be available before warning"},
    {"sensible_size", cf_int, CF_VALRANGE,
     "Minimum size in bytes that should be used on a sensible-looking storage device"},
    {"sensible_count", cf_int, CF_VALRANGE,
     "Minimum number of files that should be defined on a sensible-looking storage device"},
    {"scan_arrivals", cf_opts, CF_BOOL, "true/false generate pseudo-periodic disk change arrival distribution",
     "false"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_MOUNT_BODY[] =
{
    {"edit_fstab", cf_opts, CF_BOOL, "true/false add or remove entries to the file system table (\"fstab\")", "false"},
    {"mount_type", cf_opts, "nfs,nfs2,nfs3,nfs4", "Protocol type of remote file system"},
    {"mount_source", cf_str, CF_ABSPATHRANGE, "Path of remote file system to mount"},
    {"mount_server", cf_str, "", "Hostname or IP or remote file system server"},
    {"mount_options", cf_slist, "", "List of option strings to add to the file system table (\"fstab\")"},
    {"unmount", cf_opts, CF_BOOL, "true/false unmount a previously mounted filesystem", "false"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_STORAGE_BODIES[] =
{
    {"mount", cf_body, CF_MOUNT_BODY, "Criteria for mounting foreign file systems"},
    {"volume", cf_body, CF_CHECKVOL_BODY, "Criteria for monitoring/probing mounted volumes"},
    {NULL, cf_notype, NULL, NULL}
};

const SubTypeSyntax CF_STORAGE_SUBTYPES[] =
{
    {"agent", "storage", CF_STORAGE_BODIES},
    {NULL, NULL, NULL},
};
