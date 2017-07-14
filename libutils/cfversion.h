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

#ifndef CFVERSION_H
#define CFVERSION_H

#include <buffer.h>

/**
  @brief Version is a data structure to handle versions of software.

  */

typedef struct Version Version;

/**
  @brief Creates a new empty version. Use set version to initialize it.
  @return An empty Version structure or NULL in case of error.
  */
Version *VersionNew();
/**
  @brief Creates a new version based on the given char pointer.
  @param version Char pointer containing the version string.
  @param size Length of the version string.
  @return A fully initialized Version structure or NULL in case of error.
  */
Version *VersionNewFromCharP(const char *version, unsigned int size);
/**
  @brief Creates a new version based on the Buffer structure.
  @param buffer Buffer containing the version string.
  @return A fully initialized Version structure or NULL in case of error.
  */
Version *VersionNewFrom(Buffer *buffer);
/**
  @brief Destroys a Version structure.
  @param version Version structure.
  */
void VersionDestroy(Version **version);
/**
  @brief Compares two versions.
  @param a version a
  @param b version b
  @return <0 if a < b, 0 if a == b, >0 if a > b
  @remarks The extra field is never included in the comparison.
  */
int VersionCompare(Version *a, Version *b);
/**
  @param version Version to operate on.
  @return Returns the major version or -1 in case of error.
  */
int VersionMajor(Version *version);
/**
  @param version Version to operate on.
  @return Returns the minor version or -1 in case of error.
  */
int VersionMinor(Version *version);
/**
  @param version Version to operate on.
  @return Returns the patch version or -1 in case of error.
  */
int VersionPatch(Version *version);
/**
  @param version Version to operate on.
  @return Returns the extra version or -1 in case of error.
  */
int VersionExtra(Version *version);
/**
  @param version Version to operate on.
  @return Returns the build version or -1 in case of error.
  */
int VersionBuild(Version *version);

#endif // CFVERSION_H
