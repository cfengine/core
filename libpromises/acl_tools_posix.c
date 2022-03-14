/*
  Copyright 2021 Northern.tech AS

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
#include <unix.h>        /* GetUserID() */

bool CopyACLs(const char *src, const char *dst, bool *change)
{
    acl_t acls;
    struct stat statbuf;
    int ret;

    acls = acl_get_file(src, ACL_TYPE_ACCESS);
    if (!acls)
    {
        if (errno == ENOTSUP)
        {
            if (change != NULL)
            {
                *change = false;
            }
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
            if (change != NULL)
            {
                *change = false;
            }
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
        if (change != NULL)
        {
            *change = false;
        }
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
    if (change != NULL)
    {
        *change = true;
    }
    return true;
}

bool AllowAccessForUsers(const char *path, StringSet *users, bool allow_writes, bool allow_execute)
{
    assert(path != NULL);
    assert(users != NULL);

    acl_t acl = acl_get_file(path, ACL_TYPE_ACCESS);
    if (acl == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to get ACLs for '%s': %s", path, GetErrorStr());
        return false;
    }

    acl_entry_t entry;
    int ret = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
    while (ret > 0)
    {
        acl_tag_t entry_tag;
        if (acl_get_tag_type(entry, &entry_tag) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to get ACL entry type: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }
        if (entry_tag == ACL_USER)
        {
            if (acl_delete_entry(acl, entry) == -1)
            {
                Log(LOG_LEVEL_ERR, "Failed to remove user ACL entry: %s", GetErrorStr());
                acl_free(acl);
                return false;
            }
        }
        ret = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);
    }
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to get ACL entries: %s", GetErrorStr());
        acl_free(acl);
        return false;
    }

    StringSetIterator iter = StringSetIteratorInit(users);
    const char *user = NULL;
    while ((user = StringSetIteratorNext(&iter)) != NULL)
    {
        uid_t uid;
        if (!GetUserID(user, &uid, LOG_LEVEL_ERR))
        {
            /* errors already logged */
            acl_free(acl);
            return false;
        }

        if (acl_create_entry(&acl, &entry) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to create a new ACL entry: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }
        if (acl_set_tag_type(entry, ACL_USER) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to set ACL entry type: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }
        if (acl_set_qualifier(entry, &uid) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to set ACL entry qualifier: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }

        acl_permset_t permset;
        if (acl_get_permset(entry, &permset) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to get permset: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }
        if (acl_clear_perms(permset) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to clear permset: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }

        if (acl_add_perm(permset, ACL_READ) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to add read permission to set: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }

        if (allow_writes && (acl_add_perm(permset, ACL_WRITE) == -1))
        {
            Log(LOG_LEVEL_ERR, "Failed to add write permission to set: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }

        if (allow_execute && (acl_add_perm(permset, ACL_EXECUTE) == -1))
        {
            Log(LOG_LEVEL_ERR, "Failed to add execute permission to set: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }

        if (acl_set_permset(entry, permset) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to set permset: %s", GetErrorStr());
            acl_free(acl);
            return false;
        }
    }

    if (acl_calc_mask(&acl) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to recalculate mask: %s", GetErrorStr());
        acl_free(acl);
        return false;
    }

    if (acl_valid(acl) != 0)
    {
        Log(LOG_LEVEL_ERR, "Ended up with an invalid ACL");
        acl_free(acl);
        return false;
    }

    if (acl_set_file(path, ACL_TYPE_ACCESS, acl) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to set ACL: %s", GetErrorStr());
        acl_free(acl);
        return false;
    }

    acl_free(acl);
    return true;
}

#elif !defined(__MINGW32__) /* !HAVE_LIBACL */

bool CopyACLs(ARG_UNUSED const char *src, ARG_UNUSED const char *dst, bool *change)
{
    if (change != NULL)
    {
        *change = false;
    }
    return true;
}

bool AllowAccessForUsers(ARG_UNUSED const char *path, ARG_UNUSED StringSet *users,
                         ARG_UNUSED bool allow_writes, ARG_UNUSED bool allow_execute)
{
    Log(LOG_LEVEL_ERR, "ACL manipulation not supported");
    return false;
}
#endif
