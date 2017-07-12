/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <cf3.defs.h>
#include <acl_tools.h>

#ifdef HAVE_ACL_H
# include <acl.h>
#endif

#ifdef HAVE_SYS_ACL_H
# include <sys/acl.h>
#endif

#ifdef HAVE_ACL_LIBACL_H
# include <acl/libacl.h>
#endif

#ifdef HAVE_LIBACL

int CopyACLs(const char *src, const char *dst)
{
    acl_t acls;
    struct stat statbuf;
    int ret;

    acls = acl_get_file(src, ACL_TYPE_ACCESS);
    if (!acls)
    {
        if (errno == ENOTSUP)
        {
            return true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Can't copy ACLs from '%s'. (acl_get_file: %s)", src, GetErrorStr());
            return false;
        }
    }
    ret = acl_set_file(dst, ACL_TYPE_ACCESS, acls);
    acl_free(acls);
    if (ret != 0)
    {
        if (errno == ENOTSUP)
        {
            return true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Can't copy ACLs to '%s'. (acl_set_file: %s)", dst, GetErrorStr());
            return false;
        }
    }

    if (stat(src, &statbuf) != 0)
    {
        Log(LOG_LEVEL_ERR, "Can't copy ACLs from '%s'. (stat: %s)", src, GetErrorStr());
        return false;
    }
    if (!S_ISDIR(statbuf.st_mode))
    {
        return true;
    }

    // For directory, copy default ACL too.
    acls = acl_get_file(src, ACL_TYPE_DEFAULT);
    if (!acls)
    {
        Log(LOG_LEVEL_ERR, "Can't copy ACLs from '%s'. (acl_get_file: %s)", src, GetErrorStr());
        return false;
    }
    ret = acl_set_file(dst, ACL_TYPE_DEFAULT, acls);
    acl_free(acls);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Can't copy ACLs to '%s'. (acl_set_file: %s)", dst, GetErrorStr());
        return false;
    }

    return true;
}

#elif !defined(__MINGW32__) /* !HAVE_LIBACL */

int CopyACLs(ARG_UNUSED const char *src, ARG_UNUSED const char *dst)
{
    return true;
}

#endif
