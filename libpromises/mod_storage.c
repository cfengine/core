/*
   Copyright 2017 Northern.tech AS

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

#include <mod_storage.h>

#include <syntax.h>

static const ConstraintSyntax volume_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewBool("check_foreign", "true/false verify storage that is mounted from a foreign system on this host. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("freespace", "[0-9]+[MBkKgGmb%]", "Absolute or percentage minimum disk space that should be available before warning", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("sensible_size", CF_VALRANGE, "Minimum size in bytes that should be used on a sensible-looking storage device", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("sensible_count", CF_VALRANGE, "Minimum number of files that should be defined on a sensible-looking storage device", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("scan_arrivals", "true/false generate pseudo-periodic disk change arrival distribution. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax volume_body = BodySyntaxNew("volume", volume_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax mount_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewBool("edit_fstab", "true/false add or remove entries to the file system table (\"fstab\"). Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("mount_type", "nfs,nfs2,nfs3,nfs4", "Protocol type of remote file system", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("mount_source", CF_ABSPATHRANGE, "Path of remote file system to mount", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("mount_server", "", "Hostname or IP or remote file system server", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("mount_options", "", "List of option strings to add to the file system table (\"fstab\")", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("unmount", "true/false unmount a previously mounted filesystem. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax mount_body = BodySyntaxNew("mount", mount_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax storage_constraints[] =
{
    ConstraintSyntaxNewBody("mount", &mount_body, "Criteria for mounting foreign file systems", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("volume", &volume_body, "Criteria for monitoring/probing mounted volumes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_STORAGE_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "storage", storage_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
