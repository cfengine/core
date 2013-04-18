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

#include "mod_storage.h"

#include "syntax.h"

static const ConstraintSyntax CF_CHECKVOL_BODY[] =
{
    ConstraintSyntaxNewBool("check_foreign", "true/false verify storage that is mounted from a foreign system on this host", "false"),
    ConstraintSyntaxNewString("freespace", "[0-9]+[MBkKgGmb%]", "Absolute or percentage minimum disk space that should be available before warning", NULL),
    ConstraintSyntaxNewInt("sensible_size", CF_VALRANGE, "Minimum size in bytes that should be used on a sensible-looking storage device", NULL),
    ConstraintSyntaxNewInt("sensible_count", CF_VALRANGE, "Minimum number of files that should be defined on a sensible-looking storage device", NULL),
    ConstraintSyntaxNewBool("scan_arrivals", "true/false generate pseudo-periodic disk change arrival distribution", "false"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_MOUNT_BODY[] =
{
    ConstraintSyntaxNewBool("edit_fstab", "true/false add or remove entries to the file system table (\"fstab\")", "false"),
    ConstraintSyntaxNewOption("mount_type", "nfs,nfs2,nfs3,nfs4", "Protocol type of remote file system", NULL),
    ConstraintSyntaxNewString("mount_source", CF_ABSPATHRANGE, "Path of remote file system to mount", NULL),
    ConstraintSyntaxNewString("mount_server", "", "Hostname or IP or remote file system server", NULL),
    ConstraintSyntaxNewStringList("mount_options", "", "List of option strings to add to the file system table (\"fstab\")"),
    ConstraintSyntaxNewBool("unmount", "true/false unmount a previously mounted filesystem", "false"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_STORAGE_BODIES[] =
{
    ConstraintSyntaxNewBody("mount", CF_MOUNT_BODY, "Criteria for mounting foreign file systems"),
    ConstraintSyntaxNewBody("volume", CF_CHECKVOL_BODY, "Criteria for monitoring/probing mounted volumes"),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_STORAGE_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "storage", ConstraintSetSyntaxNew(CF_STORAGE_BODIES, NULL)),
    PromiseTypeSyntaxNewNull()
};
