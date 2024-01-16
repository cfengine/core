/*
  Copyright 2024 Northern.tech AS

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

#ifndef CFENGINE_ACL_TOOLS_H
#define CFENGINE_ACL_TOOLS_H

bool CopyACLs(const char *src, const char *dst, bool *change);

/**
 * Allow access to the given file ONLY for the given users using ACLs. Existing
 * 'user:...' ACL entries are replaced by new entries allowing access for the
 * given users.
 *
 * @param path  file to set the ACLs on
 * @param users users to allow access for
 * @param allow_writes whether to allow write access (read access is always allowed)
 * @param allow_execute whether to allow execute access (read access is always allowed)
 * @return Whether the change of ACLs was successful or not
 */
bool AllowAccessForUsers(const char *path, StringSet *users, bool allow_writes, bool allow_execute);

#endif // CFENGINE_ACL_TOOLS_H
