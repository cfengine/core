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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif

#if !HAVE_DECL_DIRFD
int dirfd(DIR *dirp);
#endif

/* Under Solaris, HP-UX and AIX dirfd(3) is just looking inside DIR structure */

#if defined(__sun)

int dirfd(DIR *dirp)
{
    return dirp->d_fd != -1 ? dirp->d_fd : ENOTSUP;
}

#elif defined(__hpux) || defined(_AIX)

int dirfd(DIR *dirp)
{
    return dirp->dd_fd != -1 ? dirp->dd_fd : ENOTSUP;
}

#endif
