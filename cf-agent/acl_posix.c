/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "cf3.defs.h"

#include "cf_acl.h"
#include "acl_posix.h"
#include "promises.h"
#include "files_names.h"
#include "cfstream.h"
#include "logging.h"
#include "misc_lib.h"
#include "rlist.h"

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

static int CheckPosixLinuxAccessACEs(Rlist *aces, AclMethod method, char *file_path, Attributes a, Promise *pp);
static int CheckPosixLinuxInheritACEs(Rlist *aces, AclMethod method, AclInheritance directory_inherit,
                                    char *file_path, Attributes a, Promise *pp);
static int CheckPosixLinuxACEs(Rlist *aces, AclMethod method, char *file_path, acl_type_t acl_type, Attributes a,
                             Promise *pp);
static int CheckDefaultEqualsAccessACL(char *file_path, Attributes a, Promise *pp);
static int CheckDefaultClearACL(char *file_path, Attributes a, Promise *pp);
static int ParseEntityPosixLinux(char **str, acl_entry_t ace, int *is_mask);
static int ParseModePosixLinux(char *mode, acl_permset_t old_perms);
static acl_entry_t FindACE(acl_t acl, acl_entry_t ace_find);
static int ACLEquals(acl_t first, acl_t second);
static int ACECount(acl_t acl);
static int PermsetEquals(acl_permset_t first, acl_permset_t second);


int CheckPosixLinuxACL(char *file_path, Acl acl, Attributes a, Promise *pp)
{
    if (!CheckPosixLinuxAccessACEs(acl.acl_entries, acl.acl_method, file_path, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! Failed checking access ACL on %s", file_path);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return false;
    }

    if (IsDir(file_path))
    {
        if (!CheckPosixLinuxInheritACEs
            (acl.acl_inherit_entries, acl.acl_method, acl.acl_directory_inherit, file_path, a, pp))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! Failed checking inheritance ACL on %s", file_path);
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            return false;
        }
    }
    return true;
}

static int CheckPosixLinuxAccessACEs(Rlist *aces, AclMethod method, char *file_path, Attributes a, Promise *pp)
{
    return CheckPosixLinuxACEs(aces, method, file_path, ACL_TYPE_ACCESS, a, pp);
}

static int CheckPosixLinuxInheritACEs(Rlist *aces, AclMethod method, AclInheritance directory_inherit,
                                    char *file_path, Attributes a, Promise *pp)
{
    int result;

    switch (directory_inherit)
    {
    case ACL_INHERITANCE_NO_CHANGE:       // no change always succeeds

        result = true;
        break;

    case ACL_INHERITANCE_SPECIFY:        // default ALC is specified in promise

        result = CheckPosixLinuxACEs(aces, method, file_path, ACL_TYPE_DEFAULT, a, pp);
        break;

    case ACL_INHERITANCE_PARENT:         // default ACL should be the same as access ACL

        result = CheckDefaultEqualsAccessACL(file_path, a, pp);
        break;

    case ACL_INHERITANCE_CLEAR:          // default ALC should be empty

        result = CheckDefaultClearACL(file_path, a, pp);
        break;

    default:                   // unknown inheritance policy
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unknown inheritance policy - shouldn't happen");
        result = false;
        break;
    }

    return result;
}

/*
   Takes as input Cfengine-syntax ACEs and a path to a file.
   Checks if the Cfengine-syntax ACL translates to the POSIX Linux ACL
   set on the given file. If it doesn't, the ACL on the file is updated.
*/

static int CheckPosixLinuxACEs(Rlist *aces, AclMethod method, char *file_path, acl_type_t acl_type, Attributes a,
                             Promise *pp)
{
    acl_t acl_existing;
    acl_t acl_new;
    acl_t acl_tmp;
    acl_entry_t ace_parsed;
    acl_entry_t ace_current;
    acl_permset_t perms;
    char *cf_ace;
    int retv;
    int has_mask;
    Rlist *rp;
    char *acl_type_str;

    acl_new = NULL;
    acl_existing = NULL;
    acl_tmp = NULL;
    has_mask = false;

    acl_type_str = acl_type == ACL_TYPE_ACCESS ? "Access" : "Default";

// read existing acl

    if ((acl_existing = acl_get_file(file_path, acl_type)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "acl_get_file", "No ACL for %s could be read", file_path);
        return false;
    }

// allocate memory for temp ace (it needs to reside in a temp acl)

    if ((acl_tmp = acl_init(1)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_init", "New ACL could not be allocated.");
        acl_free((void *) acl_existing);
        return false;
    }

    if (acl_create_entry(&acl_tmp, &ace_parsed) != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_init", "New ACL could not be allocated.");
        acl_free((void *) acl_existing);
        acl_free((void *) acl_tmp);
        return false;
    }

// copy existing aces if we are appending

    if (method == ACL_METHOD_APPEND)
    {
        if ((acl_new = acl_dup(acl_existing)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_dup", "Error copying existing ACL");
            acl_free((void *) acl_existing);
            acl_free((void *) acl_tmp);
            return false;
        }
    }
    else                        // overwrite existing acl
    {
        if ((acl_new = acl_init(5)) == NULL)    // TODO: Always OK with 5 here ?
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_init", "New ACL could not be allocated.");
            acl_free((void *) acl_existing);
            acl_free((void *) acl_tmp);
            return false;
        }
    }

    for (rp = aces; rp != NULL; rp = rp->next)
    {
        cf_ace = (char *) rp->item;

        if (!ParseEntityPosixLinux(&cf_ace, ace_parsed, &has_mask))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Error parsing entity in 'cf_ace'.");
            acl_free((void *) acl_existing);
            acl_free((void *) acl_tmp);
            acl_free((void *) acl_new);
            return false;
        }

        // check if an ACE with this entity-type and id already exist in the Posix Linux ACL

        ace_current = FindACE(acl_new, ace_parsed);

        // create new entry in ACL if it did not exist

        if (ace_current == NULL)
        {
            if (acl_create_entry(&acl_new, &ace_current) != 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "acl_create_entry", "Failed to allocate ace");
                acl_free((void *) acl_existing);
                acl_free((void *) acl_tmp);
                acl_free((void *) acl_new);
                return false;
            }

            // copy parsed entity-type and id

            if (acl_copy_entry(ace_current, ace_parsed) != 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "acl_copy_entry", "Error copying Linux entry in 'cf_ace'");
                acl_free((void *) acl_existing);
                acl_free((void *) acl_tmp);
                acl_free((void *) acl_new);
                return false;
            }

            // clear ace_current's permissions to avoid ace_parsed from last
            // loop iteration to be taken into account when applying mode below
            if ((acl_get_permset(ace_current, &perms) != 0))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "acl_get_permset", "!! Error obtaining permset for 'ace_current'");
                acl_free((void *) acl_existing);
                acl_free((void *) acl_tmp);
                acl_free((void *) acl_new);
                return false;
            }

            if (acl_clear_perms(perms) != 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "acl_clear_perms", "!! Error clearing permset for 'ace_current'");
                acl_free((void *) acl_existing);
                acl_free((void *) acl_tmp);
                acl_free((void *) acl_new);
                return false;
            }
        }

        // mode string should be prefixed with an entry seperator

        if (*cf_ace != ':')
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! No separator before mode-string in 'cf_ace'");
            acl_free((void *) acl_existing);
            acl_free((void *) acl_tmp);
            acl_free((void *) acl_new);
            return false;
        }

        cf_ace += 1;

        if (acl_get_permset(ace_current, &perms) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Error obtaining permset for 'cf_ace'");
            acl_free((void *) acl_existing);
            acl_free((void *) acl_tmp);
            acl_free((void *) acl_new);
            return false;
        }

        if (!ParseModePosixLinux(cf_ace, perms))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Error parsing mode-string in 'cf_ace'");
            acl_free((void *) acl_existing);
            acl_free((void *) acl_tmp);
            acl_free((void *) acl_new);
            return false;
        }

        // only allow permissions exist on posix acls, so we do
        // not check what follows next
    }

// if no mask exists, calculate one (or both?): run acl_calc_mask and add one
    if (!has_mask)
    {
        if (acl_calc_mask(&acl_new) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Error calculating new acl mask");
            acl_free((void *) acl_existing);
            acl_free((void *) acl_tmp);
            acl_free((void *) acl_new);
            return false;
        }
    }

    if ((retv = ACLEquals(acl_existing, acl_new)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Error while comparing existing and new ACL, unable to repair.");
        acl_free((void *) acl_existing);
        acl_free((void *) acl_tmp);
        acl_free((void *) acl_new);
        return false;
    }

    if (retv == 1)              // existing and new acl differ, update existing
    {

        switch (a.transaction.action)
        {
        case cfa_warn:

            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a, " !! %s ACL on file '%s' needs to be updated", acl_type_str, file_path);
            break;

        case cfa_fix:

            if (!DONTDO)
            {
                if ((retv = acl_set_file(file_path, acl_type, acl_new)) != 0)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "!! Error setting new %s ACL on file '%s' (are required ACEs present?)",
                          acl_type_str, file_path);
                    acl_free((void *) acl_existing);
                    acl_free((void *) acl_tmp);
                    acl_free((void *) acl_new);
                    return false;
                }
            }

            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, "-> %s ACL on \"%s\" successfully changed.", acl_type_str, file_path);

            break;

        default:
            ProgrammingError("Cfengine: internal error: illegal file action\n");
        }

    }
    else
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a, "-> %s ACL on \"%s\" needs no modification.", acl_type_str, file_path);
    }

    acl_free((void *) acl_existing);
    acl_free((void *) acl_new);
    acl_free((void *) acl_tmp);
    return true;
}

/*
  Checks if the default ACL of the given file is the same as the
  access ACL. If not, the default ACL is set in this way.
  Returns 0 on success and -1 on failure.
 */

static int CheckDefaultEqualsAccessACL(char *file_path, Attributes a, Promise *pp)
{
    acl_t acl_access;
    acl_t acl_default;
    int equals;
    int result = false;

    acl_access = NULL;
    acl_default = NULL;

    if ((acl_access = acl_get_file(file_path, ACL_TYPE_ACCESS)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_get_file", "Could not find an ACL for %s", file_path);
        return false;
    }

    acl_default = acl_get_file(file_path, ACL_TYPE_DEFAULT);

    if (acl_default == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_get_file", "Could not find default ACL for %s", file_path);
        acl_free(acl_access);
        return false;
    }

    equals = ACLEquals(acl_access, acl_default);

    switch (equals)
    {
    case 0:                    // they equal, as desired
        cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a, "-> Default ACL on \"%s\" needs no modification.", file_path);
        result = true;
        break;

    case 1:                    // set access ACL as default ACL

        switch (a.transaction.action)
        {
        case cfa_warn:

            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a, " !! Default ACL on \"%s\" needs to be copied from access ACL.",
                 file_path);
            break;

        case cfa_fix:

            if (!DONTDO)
            {
                if ((acl_set_file(file_path, ACL_TYPE_DEFAULT, acl_access)) != 0)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not set default ACL to access");
                    acl_free(acl_access);
                    acl_free(acl_default);
                    return false;
                }
            }

            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, "-> Default ACL on \"%s\" successfully copied from access ACL.",
                 file_path);
            result = true;

            break;

        default:
            ProgrammingError("Cfengine: internal error: illegal file action\n");
            result = false;
        }

        break;

    default:
        result = false;
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unable to compare access and default ACEs");
    }

    acl_free(acl_access);
    acl_free(acl_default);
    return result;
}

/*
  Checks if the default ACL is empty. If not, it is cleared.
*/

int CheckDefaultClearACL(char *file_path, Attributes a, Promise *pp)
{
    acl_t acl_existing;
    acl_t acl_empty;
    acl_entry_t ace_dummy;
    int retv;
    int result = false;

    acl_existing = NULL;
    acl_empty = NULL;

    if ((acl_existing = acl_get_file(file_path, ACL_TYPE_DEFAULT)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_get_file", "Unable to read default acl for %s", file_path);
        return false;
    }

    retv = acl_get_entry(acl_existing, ACL_FIRST_ENTRY, &ace_dummy);

    switch (retv)
    {
    case -1:
        CfOut(OUTPUT_LEVEL_VERBOSE, "acl_get_entry", "Couldn't retrieve ACE for %s", file_path);
        result = false;
        break;

    case 0:                    // no entries, as desired
        cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a, "-> Default ACL on \"%s\" needs no modification.", file_path);
        result = true;
        break;

    case 1:                    // entries exist, set empty ACL

        if ((acl_empty = acl_init(0)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Could not reinitialize ACL for %s", file_path);
            result = false;
            break;
        }

        switch (a.transaction.action)
        {
        case cfa_warn:

            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a, " !! Default ACL on \"%s\" needs to be cleared", file_path);
            break;

        case cfa_fix:

            if (!DONTDO)
            {
                if (acl_set_file(file_path, ACL_TYPE_DEFAULT, acl_empty) != 0)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Could not reset ACL for %s", file_path);
                    result = false;
                    break;
                }
            }

            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, "-> Default ACL on \"%s\" successfully cleared", file_path);
            result = true;

            break;

        default:
            ProgrammingError("Cfengine: internal error: illegal file action\n");
            result = false;
        }

        break;

    default:
        result = false;
    }

    acl_free(acl_empty);
    acl_free(acl_existing);
    return result;
}

/*
  Walks through the acl given as the first parameter, looking for an
  ACE that has the same entity type and id as the ace in the second
  parameter. If a match is found, then the matching ace is returned.
  Else, NULL is returned.
*/

static acl_entry_t FindACE(acl_t acl, acl_entry_t ace_find)
{
    acl_entry_t ace_curr;
    acl_tag_t tag_curr;
    acl_tag_t tag_find;
    id_t *id_curr;
    id_t *id_find;
    int more_aces;
    int retv_tag;

    id_find = NULL;

    more_aces = acl_get_entry(acl, ACL_FIRST_ENTRY, &ace_curr);

    if (more_aces == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_get_entry", "Error reading acl");
        return NULL;
    }
    else if (more_aces == 0)
    {
        return NULL;
    }

/* find the tag type and id we are looking for */

    if (acl_get_tag_type(ace_find, &tag_find) != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_tag_type", "Error reading tag type");
        return NULL;
    }

    if (tag_find == ACL_USER || tag_find == ACL_GROUP)
    {
        id_find = acl_get_qualifier(ace_find);

        if (id_find == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_get_qualifier", "Error reading tag type");
            return NULL;
        }
    }

/* check if any of the aces match */

    while (more_aces)
    {
        if ((retv_tag = acl_get_tag_type(ace_curr, &tag_curr)) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_get_tag_type", "Unable to get tag type");
            acl_free(id_find);
            return NULL;
        }

        if (tag_curr == tag_find)
        {
            if (id_find == NULL)
            {
                return ace_curr;
            }

            id_curr = acl_get_qualifier(ace_curr);

            if (id_curr == NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "acl_get_qualifier", "!! Couldn't extract qualifier");
                return NULL;
            }

            if (*id_curr == *id_find)
            {
                acl_free(id_find);
                acl_free(id_curr);
                return ace_curr;
            }

            acl_free(id_curr);
        }

        more_aces = acl_get_entry(acl, ACL_NEXT_ENTRY, &ace_curr);
    }

    if (id_find != NULL)
    {
        acl_free(id_find);
    }

    return NULL;
}

/*
  Checks if the two ACLs contain equal ACEs and equally many.
  Does not assume that the ACEs lie in the same order.
  Returns 0 if so, and 1 if they differ, and -1 on error.
*/

static int ACLEquals(acl_t first, acl_t second)
{
    acl_entry_t ace_first;
    acl_entry_t ace_second;
    acl_permset_t perms_first;
    acl_permset_t perms_second;
    int first_cnt;
    int second_cnt;
    int more_aces;
    int retv_perms;

    if ((first_cnt = ACECount(first)) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Couldn't count ACEs");
        return -1;
    }

    if ((second_cnt = ACECount(second)) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Couldn't count ACEs");
        return -1;
    }

    if (first_cnt != second_cnt)
    {
        return 1;
    }

    if (first_cnt == 0)
    {
        return 0;
    }

// check that every ace of first acl exist in second acl

    more_aces = acl_get_entry(first, ACL_FIRST_ENTRY, &ace_first);

    if (more_aces != 1)         // first must contain at least one entry
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_get_entry", "Unable to read ACE");
        return -1;
    }

    while (more_aces)
    {
        /* no ace in second match entity-type and id of first */

        if ((ace_second = FindACE(second, ace_first)) == NULL)
        {
            return 1;
        }

        /* permissions must also match */

        if (acl_get_permset(ace_first, &perms_first) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_get_permset", "unable to read permissions");
            return -1;
        }

        if (acl_get_permset(ace_second, &perms_second) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_get_permset", "unable to read permissions");
            return -1;
        }

        retv_perms = PermsetEquals(perms_first, perms_second);

        if (retv_perms == -1)
        {
            return -1;
        }
        else if (retv_perms == 1)       // permissions differ
        {
            return 1;
        }

        more_aces = acl_get_entry(first, ACL_NEXT_ENTRY, &ace_first);
    }

    return 0;
}

static int ACECount(acl_t acl)
{
    int more_aces;
    int count;
    acl_entry_t ace;

    count = 0;

    more_aces = acl_get_entry(acl, ACL_FIRST_ENTRY, &ace);

    if (more_aces <= 0)
    {
        return more_aces;
    }

    while (more_aces)
    {
        more_aces = acl_get_entry(acl, ACL_NEXT_ENTRY, &ace);
        count++;
    }

    return count;
}

static int PermsetEquals(acl_permset_t first, acl_permset_t second)
{
    acl_perm_t perms_avail[3] = { ACL_READ, ACL_WRITE, ACL_EXECUTE };
    int first_set;
    int second_set;
    int i;

    for (i = 0; i < 3; i++)
    {
        first_set = acl_get_perm(first, perms_avail[i]);

        if (first_set == -1)
        {
            return -1;
        }

        second_set = acl_get_perm(second, perms_avail[i]);

        if (second_set == -1)
        {
            return -1;
        }

        if (first_set != second_set)
        {
            return 1;
        }
    }

    return 0;
}

/*
   Takes a ':' or null-terminated string "entity-type:id", and
   converts and stores these into a Posix Linux ace data structure
   (with unset permissions). Sets is_mask to true if this is a mask entry.
*/

static int ParseEntityPosixLinux(char **str, acl_entry_t ace, int *is_mask)
{
    struct passwd *pwd;
    struct group *grp;
    acl_tag_t etype;
    size_t idsz;
    id_t id;
    char *ids;
    char *id_end;
    int result = true;
    int i;

    ids = NULL;

// TODO: Support numeric id in addition to (user/group) name ?

// Posix language: tag type, qualifier, permissions

    if (strncmp(*str, "user:", 5) == 0)
    {
        *str += 5;

        // create null-terminated string for entity id
        id_end = index(*str, ':');

        if (id_end == NULL)     // entity id already null-terminated
        {
            idsz = strlen(*str);
        }
        else                    // copy entity-id to new null-terminated string
        {
            idsz = id_end - *str;
        }

        ids = xmalloc(idsz + 1);
        for (i = 0; i < idsz; i++)
            ids[i] = (*str)[i];
        ids[idsz] = '\0';

        *str += idsz;

        // file object owner

        if (strncmp(ids, "*", 2) == 0)
        {
            etype = ACL_USER_OBJ;
            id = 0;
        }
        else
        {
            etype = ACL_USER;
            pwd = getpwnam(ids);

            if (pwd == NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "getpwnnam", "Couldn't find user id for %s", ids);
                free(ids);
                return false;
            }

            id = pwd->pw_uid;
        }
    }
    else if (strncmp(*str, "group:", 6) == 0)
    {
        *str += 6;

        // create null-terminated string for entity id
        id_end = index(*str, ':');

        if (id_end == NULL)     // entity id already null-terminated
        {
            idsz = strlen(*str);
        }
        else                    // copy entity-id to new null-terminated string
        {
            idsz = id_end - *str;
        }

        ids = xmalloc(idsz + 1);
        for (i = 0; i < idsz; i++)
            ids[i] = (*str)[i];
        ids[idsz] = '\0';

        *str += idsz;

        // file group
        if (strncmp(ids, "*", 2) == 0)
        {
            etype = ACL_GROUP_OBJ;
            id = 0;             // TODO: Correct file group id ??
        }
        else
        {
            etype = ACL_GROUP;
            grp = getgrnam(ids);

            if (grp == NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Error looking up group id for %s", ids);
                free(ids);
                return false;
            }

            id = grp->gr_gid;
        }

    }
    else if (strncmp(*str, "all:", 4) == 0)
    {
        *str += 3;
        etype = ACL_OTHER;
    }
    else if (strncmp(*str, "mask:", 5) == 0)
    {
        *str += 4;
        etype = ACL_MASK;
        *is_mask = true;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "ace does not start with user:/group:/all:/mask:");
        return false;
    }

    if (acl_set_tag_type(ace, etype) != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "acl_set_tag_type", "Could not set ACE tag type.");
        result = false;
    }
    else if (etype == ACL_USER || etype == ACL_GROUP)
    {
        if ((acl_set_qualifier(ace, &id)) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_set_qualifier", "Could not set ACE qualifier.");
            result = false;
        }
    }

    if (ids != NULL)
    {
        free(ids);
    }

    return result;
}

/*
  Takes a Cfengine-syntax mode string and existing Posix
  Linux-formatted permissions on the file system object as
  arguments. The mode-string will be applied on the permissions.
  Returns true on success, false otherwise.
*/

static int ParseModePosixLinux(char *mode, acl_permset_t perms)
{
    int retv;
    int more_entries;
    acl_perm_t perm;
    enum { add, del } op;

    op = add;

    if (*mode == '\0' || *mode == ':')
    {
        if (acl_clear_perms(perms) != 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "acl_clear_perms", "Error clearing perms");
            return false;
        }
        else
        {
            return true;
        }
    }

    more_entries = true;

    while (more_entries)
    {
        switch (*mode)
        {
        case '+':
            op = add;
            mode++;
            break;

        case '-':
            op = del;
            mode++;
            break;

        case '=':
            mode++;
            // fallthrough

        default:
            // if mode does not start with + or -, we clear existing perms
            op = add;

            if (acl_clear_perms(perms) != 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "acl_clear_perms", "Unable to clear ACL permissions");
                return false;
            }
        }

        // parse generic perms (they are 1-1 on Posix)

        while (*mode != '\0' && strchr(CF_VALID_GPERMS, *mode))
        {
            if (*mode == '\0')
            {
                break;
            }
            switch (*mode)
            {
            case 'r':
                perm = ACL_READ;
                break;

            case 'w':
                perm = ACL_WRITE;
                break;

            case 'x':
                perm = ACL_EXECUTE;
                break;

            default:
                CfOut(OUTPUT_LEVEL_ERROR, "", "No linux support for generic permission flag '%c'", *mode);
                return false;
            }

            if (op == add)
            {
                retv = acl_add_perm(perms, perm);
            }
            else
            {
                retv = acl_delete_perm(perms, perm);
            }

            if (retv != 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "acl_[add|delete]_perms", "Could not change ACE permission.");
                return false;
            }
            mode++;
        }

        // parse native perms

        if (*mode == CF_NATIVE_PERMS_SEP_START)
        {
            mode++;

            while (*mode != '\0' && strchr(CF_VALID_NPERMS_POSIX, *mode))
            {
                switch (*mode)
                {
                case 'r':
                    perm = ACL_READ;
                    break;

                case 'w':
                    perm = ACL_WRITE;
                    break;

                case 'x':
                    perm = ACL_EXECUTE;
                    break;

                default:
                    CfOut(OUTPUT_LEVEL_ERROR, "", "!! No linux support for native permission flag '%c'", *mode);
                    return false;
                }

                if (op == add)
                {
                    retv = acl_add_perm(perms, perm);
                }
                else
                {
                    retv = acl_delete_perm(perms, perm);
                }

                if (retv != 0)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "acl_[add|delete]_perm", "Could not change ACE permission.");
                    return false;
                }
                mode++;
            }

            // scan past native perms end seperator
            mode++;
        }

        if (*mode == ',')
        {
            more_entries = true;
            mode++;
        }
        else
        {
            more_entries = false;
        }
    }

    return true;
}

#else /* HAVE_LIBACL */

int CheckPosixLinuxACL(char *file_path, Acl acl, Attributes a, Promise *pp)
{
    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
         "!! Posix ACLs are not supported on this Linux system - install the Posix acl library");
    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    return true;
}

#endif
