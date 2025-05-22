/*
  Copyright 2025 Northern.tech AS

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

#ifndef CFENGINE_OVERRIDE_FSATTRS_H
#define CFENGINE_OVERRIDE_FSATTRS_H

#include <stdbool.h>
#include <stddef.h>
#include <utime.h>

/**
 * @brief Creates a mutable copy of the original file
 * @param orig The original file (may be immutable)
 * @param copy Updated to contain the filename of the mutable copy
 * @param copy_len The size of the buffer to store the filename of the copy
 * @param override Whether to actually do override (original filename is
 * copied to copy buffer if false)
 * @return false in case of failure
 */
bool OverrideImmutableBegin(
    const char *orig, char *copy, size_t copy_len, bool override);

/**
 * @brief Temporarily clears the immutable bit of the original file and
 * replaces it with the mutated copy
 * @param orig The original file (may be immutable)
 * @param copy The mutated copy to replace the original
 * @param override Whether to actually do override
 * @param abort Whether to abort the override
 * @return false in case of failure
 * @note The immutable bit is reset to it's original state
 */
bool OverrideImmutableCommit(
    const char *orig, const char *copy, bool override, bool abort);

/**
 * @brief Temporarily clears the immutable bit of the old file and renames the
 * new to the old
 * @param old_filename Filename of the old file
 * @param new_filename Filename of the new file
 * @param override Whether to actually do override
 * @return false in case of failure
 */
bool OverrideImmutableRename(
    const char *old_filename, const char *new_filename, bool override);

/**
 * @brief Delete immutable file
 * @param filename Name of the file to delete
 * @param override Whether to actually do override
 * @return false in case of failure
 */
bool OverrideImmutableDelete(const char *filename, bool override);

/**
 * @brief Temporarily clears the immutable bit and changes access and
 * modification times of the inode
 * @param filename Name of the file to touch
 * @param override Whether to actually do override
 * @param times Modification times (can be NULL)
 * @return false in case of failure
 */
bool OverrideImmutableUtime(
    const char *filename, bool override, const struct utimbuf *times);

#endif /* CFENGINE_OVERRIDE_FSATTRS_H */
