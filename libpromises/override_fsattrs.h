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

#include <fsattrs.h>
#include <stdbool.h>
#include <stddef.h>
#include <utime.h>
#include <sys/types.h>

/**
 * @brief Creates a mutable copy of the original file
 * @param orig The original file (may be immutable)
 * @param copy Updated to contain the filename of the mutable copy
 * @param copy_len The size of the buffer to store the filename of the copy
 * @param override Whether to actually do override (i.e. temporarily clear it).
 * The original filename is copied to copy-buffer if false
 * @return false in case of failure
 */
bool OverrideImmutableBegin(
    const char *orig, char *copy, size_t copy_len, bool override);

/**
 * @brief Temporarily clears the immutable bit of the original file and
 * replaces it with the mutated copy
 * @param orig The original file (may be immutable)
 * @param copy The mutated copy to replace the original
 * @param override Whether to actually do override (i.e. temporarily clear it)
 * @param abort Whether to abort the override
 * @return false in case of failure
 * @note The immutable bit is reset to it's original state
 */
bool OverrideImmutableCommit(
    const char *orig, const char *copy, bool override, bool abort);

/**
 * @brief Change mode on an immutable file
 * @param filename Name of the file
 * @param mode The file mode
 * @param override Whether to actually do override (i.e. temporarily clear it)
 * @return false in case of failure
 * @note It uses safe_chmod() under the hood
 */
bool OverrideImmutableChmod(const char *filename, mode_t mode, bool override);

/**
 * @brief Temporarily clears the immutable bit of the old file and renames the
 * new to the old
 * @param old_filename Filename of the old file
 * @param new_filename Filename of the new file
 * @param override Whether to actually do override (i.e. temporarily clear it)
 * @return false in case of failure
 */
bool OverrideImmutableRename(
    const char *old_filename, const char *new_filename, bool override);

/**
 * @brief Delete immutable file
 * @param filename Name of the file to delete
 * @param override Whether to actually do override (i.e. temporarily clear it)
 * @return false in case of failure
 */
bool OverrideImmutableDelete(const char *filename, bool override);

/**
 * @brief Temporarily clears the immutable bit and changes access and
 * modification times of the inode
 * @param filename Name of the file to touch
 * @param override Whether to actually do override (i.e. temporarily clear it)
 * @param times Modification times (can be NULL)
 * @return false in case of failure
 */
bool OverrideImmutableUtime(
    const char *filename, bool override, const struct utimbuf *times);

/**
 * @brief Temporarily clears the immutable bit (best effort / no guarantees)
 * @param filename Name of the file to clear the immutable bit on
 * @param override Whether to actually do override (i.e. temporarily clear it)
 * @param is_immutable Whether or not the file actually was immutable
 * @return Result of clearing the immutable bit (no to be interpreted by the
 * caller)
 */
FSAttrsResult TemporarilyClearImmutableBit(
    const char *filename, bool override, bool *was_immutable);

/**
 * @brief Reset temporarily cleared immutable bit
 * @param filename Name of the file to clear the immutable bit on
 * @param override Whether to actually do override (i.e. temporarily clear it)
 * @param res The result from previously clearing it
 * @param is_immutable Whether or not the file actually was immutable
 */
void ResetTemporarilyClearedImmutableBit(
    const char *filename,
    bool override,
    FSAttrsResult res,
    bool was_immutable);

#endif /* CFENGINE_OVERRIDE_FSATTRS_H */
