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

#include <platform.h>

#ifndef _SIMULATE_H_
#define _SIMULATE_H_

#include <set.h>                /* StringSet */

#define CHROOT_PKG_OPERATION_PRESENT "p"
#define CHROOT_PKG_OPERATION_ABSENT  "a"
#define CHROOT_PKG_OPERATION_INSTALL "i"
#define CHROOT_PKG_OPERATION_REMOVE  "r"

typedef enum {
    CHROOT_PKG_OPERATION_CODE_PRESENT = 'p',
    CHROOT_PKG_OPERATION_CODE_ABSENT  = 'a',
    CHROOT_PKG_OPERATION_CODE_INSTALL = 'i',
    CHROOT_PKG_OPERATION_CODE_REMOVE  = 'r',
} ChrootPkgOperationCode;

bool ManifestFile(const char *path, bool chrooted);
bool ManifestRename(const char *orig_name, const char *new_name);
bool ManifestChangedFiles(StringSet **audited_files);
bool ManifestAllFiles(StringSet **audited_files);
bool DiffChangedFiles(StringSet **audited_files);

bool DiffPkgOperations();
bool ManifestPkgOperations();

#endif  /* _SIMULATE_H_ */
