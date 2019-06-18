/*
   Copyright 2019 Northern.tech AS

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

#include <cf3.defs.h>
#include <cfnet.h>                                       /* AgentConnection */

#ifndef CFENGINE_VERIFY_FILES_HASHES_H
#define CFENGINE_VERIFY_FILES_HASHES_H

bool CompareFileHashes(const char *file1, const char *file2, const struct stat *sstat, const struct stat *dstat, const FileCopy *fc, AgentConnection *conn);
bool CompareBinaryFiles(const char *file1, const char *file2, const struct stat *sstat, const struct stat *dstat, const FileCopy *fc, AgentConnection *conn);

#endif
