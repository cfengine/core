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

#include <cf3.defs.h>

#include <actuator.h>
#include <verify_acl.h>
#include <acl_posix.h>
#include <promises.h>
#include <files_names.h>
#include <misc_lib.h>
#include <rlist.h>
#include <eval_context.h>
#include <unix.h>               /* GetGroupID(), GetUserID() */

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

static bool CheckPosixLinuxAccessACEs(EvalContext *ctx, Rlist *aces, AclMethod method, const char *file_path,
                                      const Attributes *a, const Promise *pp, PromiseResult *result);
static bool CheckPosixLinuxDefaultACEs(EvalContext *ctx, Rlist *aces, AclMethod method, AclDefault acl_default,
                                       const char *file_path, const Attributes *a, const Promise *pp, PromiseResult *result);
static bool CheckPosixLinuxACEs(EvalContext *ctx, Rlist *aces, AclMethod method, const char *file_path, acl_type_t acl_type, const Attributes *a,
                                const Promise *pp, PromiseResult *result);
static bool CheckDefaultEqualsAccessACL(EvalContext *ctx, const char *file_path, const Attributes *a, const Promise *pp, PromiseResult *result);
static bool CheckDefaultClearACL(EvalContext *ctx, const char *file_path, const Attributes *a, const Promise *pp, PromiseResult *result);
static bool ParseEntityPosixLinux(char **str, acl_entry_t ace, int *is_mask);
static bool ParseModePosixLinux(char *mode, acl_permset_t old_perms);
static acl_entry_t FindACE(acl_t acl, acl_entry_t ace_find);
static int ACLEquals(acl_t first, acl_t second);
static int ACECount(acl_t acl);
static int PermsetEquals(acl_permset_t first, acl_permset_t second);


PromiseResult CheckPosixLinuxACL(EvalContext *ctx, const char *file_path, Acl acl, const Attributes *a, const Promise *pp)
{
    assert(a != NULL);
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (!CheckPosixLinuxAccessACEs(ctx, acl.acl_entries, acl.acl_method, file_path, a, pp, &result))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        return result;
    }

    if (IsDir(ToChangesPath(file_path)))
    {
        if (!CheckPosixLinuxDefaultACEs(ctx, acl.acl_default_entries, acl.acl_method, acl.acl_default,
                                        file_path, a, pp, &result))
        {
            PromiseRef(LOG_LEVEL_ERR, pp);
            return result;
        }
    }
    return result;
}

static bool CheckPosixLinuxAccessACEs(EvalContext *ctx, Rlist *aces, AclMethod method, const char *file_path,
                                      const Attributes *a, const Promise *pp, PromiseResult *result)
{
    assert(a != NULL);
    return CheckPosixLinuxACEs(ctx, aces, method, file_path, ACL_TYPE_ACCESS, a, pp, result);
}

static bool CheckPosixLinuxDefaultACEs(EvalContext *ctx, Rlist *aces, AclMethod method, AclDefault acl_default,
                                       const char *file_path, const Attributes *a, const Promise *pp, PromiseResult *result)
{
    assert(a != NULL);
    bool retval;

    switch (acl_default)
    {
    case ACL_DEFAULT_NO_CHANGE:       // no change always succeeds

        RecordNoChange(ctx, pp, a, "No change required for '%s' with acl_default=nochange", file_path);
        retval = true;
        break;

    case ACL_DEFAULT_SPECIFY:        // default ACL is specified in promise

        /* CheckPosixLinuxACEs() records changes and updates 'result' */
        retval = CheckPosixLinuxACEs(ctx, aces, method, file_path, ACL_TYPE_DEFAULT, a, pp, result);
        break;

    case ACL_DEFAULT_ACCESS:         // default ACL should be the same as access ACL

        retval = CheckDefaultEqualsAccessACL(ctx, file_path, a, pp, result);
        break;

    case ACL_DEFAULT_CLEAR:          // default ACL should be empty

        retval = CheckDefaultClearACL(ctx, file_path, a, pp, result);
        break;

    default:                   // unknown inheritance policy
        Log(LOG_LEVEL_ERR, "Unknown ACL inheritance policy - shouldn't happen");
        debug_abort_if_reached();
        retval = false;
        break;
    }

    return retval;
}

/**
 * Takes as input CFEngine-syntax ACEs and a path to a file.  Checks if the
 * CFEngine-syntax ACL translates to the POSIX Linux ACL set on the given
 * file. If it doesn't, the ACL on the file is updated.
 */
static bool CheckPosixLinuxACEs(EvalContext *ctx, Rlist *aces, AclMethod method, const char *file_path, acl_type_t acl_type,
                                const Attributes *a, const Promise *pp, PromiseResult *result)
{
    assert(a != NULL);
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
    char *acl_text_str;

    acl_new = NULL;
    acl_existing = NULL;
    acl_tmp = NULL;
    has_mask = false;

    acl_type_str = acl_type == ACL_TYPE_ACCESS ? "Access" : "Default";

    const char *changes_path = file_path;
    if (ChrootChanges())
    {
        changes_path = ToChangesChroot(file_path);
    }

// read existing acl

    if ((acl_existing = acl_get_file(changes_path, acl_type)) == NULL)
    {
        RecordFailure(ctx, pp, a,
                      "No %s ACL for '%s' could be read. (acl_get_file: %s)",
                      acl_type_str, file_path, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

// allocate memory for temp ace (it needs to reside in a temp acl)

    if ((acl_tmp = acl_init(1)) == NULL)
    {
        RecordFailure(ctx, pp, a,
                      "New ACL could not be allocated (acl_init: %s)",
                      GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        acl_free(acl_existing);
        return false;
    }

    if (acl_create_entry(&acl_tmp, &ace_parsed) != 0)
    {
        RecordFailure(ctx, pp, a,
                      "New ACL could not be allocated (acl_create_entry: %s)", GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        acl_free(acl_existing);
        acl_free(acl_tmp);
        return false;
    }

// copy existing aces if we are appending

    if (method == ACL_METHOD_APPEND)
    {

        if ((acl_new = acl_dup(acl_existing)) == NULL)
        {
            RecordFailure(ctx, pp, a,
                          "Error copying existing ACL (acl_dup: %s)", GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            acl_free(acl_existing);
            acl_free(acl_tmp);
            return false;
        }
    }
    else                        // overwrite existing acl
    {
        if ((acl_new = acl_init(5)) == NULL)    // TODO: Always OK with 5 here ?
        {
            RecordFailure(ctx, pp, a,
                          "New ACL could not be allocated (acl_init: %s)", GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            acl_free(acl_existing);
            acl_free(acl_tmp);
            return false;
        }
    }

    for (rp = aces; rp != NULL; rp = rp->next)
    {
        cf_ace = RlistScalarValue(rp);

        if (!ParseEntityPosixLinux(&cf_ace, ace_parsed, &has_mask))
        {
            RecordFailure(ctx, pp, a, "ACL: Error parsing entity in '%s'", cf_ace);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            acl_free(acl_existing);
            acl_free(acl_tmp);
            acl_free(acl_new);
            return false;
        }

        // check if an ACE with this entity-type and id already exist in the Posix Linux ACL

        ace_current = FindACE(acl_new, ace_parsed);

        // create new entry in ACL if it did not exist

        if (ace_current == NULL)
        {
            if (acl_create_entry(&acl_new, &ace_current) != 0)
            {
                RecordFailure(ctx, pp, a,
                              "Failed to allocate ace (acl_create_entry: %s)", GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                acl_free(acl_existing);
                acl_free(acl_tmp);
                acl_free(acl_new);
                return false;
            }

            // copy parsed entity-type and id

            if (acl_copy_entry(ace_current, ace_parsed) != 0)
            {
                RecordFailure(ctx, pp, a,
                              "Error copying Linux ACL entry for '%s' (acl_copy_entry: %s)",
                              file_path, GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                acl_free(acl_existing);
                acl_free(acl_tmp);
                acl_free(acl_new);
                return false;
            }

            // clear ace_current's permissions to avoid ace_parsed from last
            // loop iteration to be taken into account when applying mode below
            if ((acl_get_permset(ace_current, &perms) != 0))
            {
                RecordFailure(ctx, pp, a,
                              "Error obtaining permission set for 'ace_current' (acl_get_permset: %s)",
                              GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                acl_free(acl_existing);
                acl_free(acl_tmp);
                acl_free(acl_new);
                return false;
            }

            if (acl_clear_perms(perms) != 0)
            {
                RecordFailure(ctx, pp, a,
                              "Error clearing permission set for 'ace_current'. (acl_clear_perms: %s)",
                              GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                acl_free(acl_existing);
                acl_free(acl_tmp);
                acl_free(acl_new);
                return false;
            }
        }

        // mode string should be prefixed with an entry seperator

        if (*cf_ace != ':')
        {
            RecordFailure(ctx, pp, a, "ACL: No separator before mode-string '%s'", cf_ace);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            acl_free(acl_existing);
            acl_free(acl_tmp);
            acl_free(acl_new);
            return false;
        }

        cf_ace += 1;

        if (acl_get_permset(ace_current, &perms) != 0)
        {
            RecordFailure(ctx, pp, a,
                          "ACL: Error obtaining permission set for '%s'. (acl_get_permset: %s)",
                          cf_ace, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            acl_free(acl_existing);
            acl_free(acl_tmp);
            acl_free(acl_new);
            return false;
        }

        if (!ParseModePosixLinux(cf_ace, perms))
        {
            RecordFailure(ctx, pp, a, "ACL: Error parsing mode-string in '%s'", cf_ace);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            acl_free(acl_existing);
            acl_free(acl_tmp);
            acl_free(acl_new);
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
            RecordFailure(ctx, pp, a, "Error calculating new ACL mask");
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            acl_free(acl_existing);
            acl_free(acl_tmp);
            acl_free(acl_new);
            return false;
        }
    }

    if ((retv = ACLEquals(acl_existing, acl_new)) == -1)
    {
        RecordFailure(ctx, pp, a, "Error while comparing existing and new ACL, unable to repair");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        acl_free(acl_existing);
        acl_free(acl_tmp);
        acl_free(acl_new);
        return false;
    }

    if (retv == 1)              // existing and new acl differ, update existing
    {
        if (MakingChanges(ctx, pp, a, result, "update ACL %s on file '%s'", acl_type_str, file_path))
        {
            int last = -1;
            acl_text_str = acl_to_any_text(acl_new, NULL, ',', 0);
            Log(LOG_LEVEL_DEBUG, "ACL: new acl is `%s'", acl_text_str);

            if ((retv = acl_check(acl_new, &last)) != 0)
            {
                RecordFailure(ctx, pp, a, "Invalid ACL in '%s' at index %d (acl_check: %s)",
                              acl_text_str,
                              last,
                              acl_error(retv));
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                acl_free(acl_existing);
                acl_free(acl_tmp);
                acl_free(acl_new);
                acl_free(acl_text_str);
                return false;
            }
            if ((retv = acl_set_file(changes_path, acl_type, acl_new)) != 0)
            {
                RecordFailure(ctx, pp, a,
                              "Error setting new %s ACL(%s) on file '%s' (acl_set_file: %s), are required ACEs present ?",
                              acl_type_str,
                              acl_text_str,
                              file_path,
                              GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                acl_free(acl_existing);
                acl_free(acl_tmp);
                acl_free(acl_new);
                acl_free(acl_text_str);
                return false;
            }
            acl_free(acl_text_str);

            RecordChange(ctx, pp, a, "%s ACL on '%s' successfully changed", acl_type_str, file_path);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }
    }
    else
    {
        RecordNoChange(ctx, pp, a, "'%s' ACL on '%s' needs no modification.", acl_type_str, file_path);
    }

    acl_free(acl_existing);
    acl_free(acl_new);
    acl_free(acl_tmp);
    return true;
}

/**
 * Checks if the default ACL of the given file is the same as the access ACL. If
 * not, the default ACL is set in this way.
 *
 * @return 0 on success and -1 on failure.
 */
static bool CheckDefaultEqualsAccessACL(EvalContext *ctx, const char *file_path, const Attributes *a, const Promise *pp, PromiseResult *result)
{
    assert(a != NULL);
    acl_t acl_access;
    acl_t acl_default;
    int equals;
    bool retval = false;

    const char *changes_path = file_path;
    if (ChrootChanges())
    {
        changes_path = ToChangesChroot(file_path);
    }

    acl_access = NULL;
    acl_default = NULL;

    if ((acl_access = acl_get_file(changes_path, ACL_TYPE_ACCESS)) == NULL)
    {
        RecordFailure(ctx, pp, a, "Could not find an ACL for '%s'. (acl_get_file: %s)",
                      file_path, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    acl_default = acl_get_file(changes_path, ACL_TYPE_DEFAULT);

    if (acl_default == NULL)
    {
        RecordFailure(ctx, pp, a, "Could not find default ACL for '%s'. (acl_get_file: %s)",
                      file_path, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        acl_free(acl_access);
        return false;
    }

    equals = ACLEquals(acl_access, acl_default);

    switch (equals)
    {
    case 0:                    // they equal, as desired
        RecordNoChange(ctx, pp, a, "Default ACL on '%s' as promised", file_path);
        retval = true;
        break;

    case 1:                    // set access ACL as default ACL

        if (MakingChanges(ctx, pp, a, result, "copy default ACL on '%s' from access ACL", file_path))
        {
            if ((acl_set_file(changes_path, ACL_TYPE_DEFAULT, acl_access)) != 0)
            {
                RecordFailure(ctx, pp, a,
                              "Could not set default ACL to access on '%s'. (acl_set_file: %s)",
                              file_path, GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                acl_free(acl_access);
                acl_free(acl_default);
                return false;
            }
            RecordChange(ctx, pp, a,
                         "Default ACL on '%s' successfully copied from access ACL.",
                         file_path);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            retval = true;
        }

        break;

    default:
        RecordFailure(ctx, pp, a, "ACL: Unable to compare access and default ACEs");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        retval = false;
    }

    acl_free(acl_access);
    acl_free(acl_default);
    return retval;
}

/*
  Checks if the default ACL is empty. If not, it is cleared.
*/

static bool CheckDefaultClearACL(EvalContext *ctx, const char *file_path, const Attributes *a, const Promise *pp, PromiseResult *result)
{
    acl_t acl_existing;
    acl_t acl_empty;
    acl_entry_t ace_dummy;
    int retv;
    bool retval = false;

    const char *changes_path = file_path;
    if (ChrootChanges())
    {
        changes_path = ToChangesChroot(file_path);
    }

    acl_existing = NULL;
    acl_empty = NULL;

    if ((acl_existing = acl_get_file(changes_path, ACL_TYPE_DEFAULT)) == NULL)
    {
        RecordFailure(ctx, pp, a, "Unable to read default acl for '%s'. (acl_get_file: %s)", file_path, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    retv = acl_get_entry(acl_existing, ACL_FIRST_ENTRY, &ace_dummy);

    switch (retv)
    {
    case -1:
        RecordFailure(ctx, pp, a, "Couldn't retrieve ACE for '%s'. (acl_get_entry: %s)",
                      file_path, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        retval = false;
        break;

    case 0:                    // no entries, as desired
        RecordNoChange(ctx, pp, a, "Default ACL on '%s' as promised", file_path);
        retval = true;
        break;

    case 1:                    // entries exist, set empty ACL

        if ((acl_empty = acl_init(0)) == NULL)
        {
            RecordFailure(ctx, pp, a, "Could not reinitialize ACL for '%s'. (acl_init: %s)",
                          file_path, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            retval = false;
            break;
        }

        if (MakingChanges(ctx, pp, a, result, "clean default ACL on '%s'", file_path))
        {
            if (acl_set_file(changes_path, ACL_TYPE_DEFAULT, acl_empty) != 0)
            {
                RecordFailure(ctx, pp, a, "Could not reset ACL for '%s'. (acl_set_file: %s)",
                              file_path, GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                retval = false;
            }

            RecordChange(ctx, pp, a, "Default ACL on '%s' successfully cleared", file_path);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            retval = true;
        }

        break;

    default:
        retval = false;
    }

    acl_free(acl_empty);
    acl_free(acl_existing);
    return retval;
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
        Log(LOG_LEVEL_ERR, "Error reading ACL. (acl_get_entry: %s)", GetErrorStr());
        return NULL;
    }
    else if (more_aces == 0)
    {
        return NULL;
    }

/* find the tag type and id we are looking for */

    if (acl_get_tag_type(ace_find, &tag_find) != 0)
    {
        Log(LOG_LEVEL_ERR, "Error reading tag type. (acl_get_tag_type: %s)", GetErrorStr());
        return NULL;
    }

    if (tag_find == ACL_USER || tag_find == ACL_GROUP)
    {
        id_find = acl_get_qualifier(ace_find);

        if (id_find == NULL)
        {
            Log(LOG_LEVEL_ERR, "Error reading tag type. (acl_get_qualifier: %s)", GetErrorStr());
            return NULL;
        }
    }

/* check if any of the aces match */

    while (more_aces)
    {
        if ((retv_tag = acl_get_tag_type(ace_curr, &tag_curr)) != 0)
        {
            Log(LOG_LEVEL_ERR, "Unable to get tag type. (acl_get_tag_type: %s)", GetErrorStr());
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
                Log(LOG_LEVEL_ERR, "Couldn't extract qualifier. (acl_get_qualifier: %s)", GetErrorStr());
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
        Log(LOG_LEVEL_ERR, "Couldn't count ACEs while comparing ACLs");
        return -1;
    }

    if ((second_cnt = ACECount(second)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't count ACEs while comparing ACLs");
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
        Log(LOG_LEVEL_ERR, "Unable to read ACE. (acl_get_entry: %s)", GetErrorStr());
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
            Log(LOG_LEVEL_ERR, "Unable to read permissions. (acl_get_permset: %s)", GetErrorStr());
            return -1;
        }

        if (acl_get_permset(ace_second, &perms_second) != 0)
        {
            Log(LOG_LEVEL_ERR, "Unable to read permissions. (acl_get_permset: %s)", GetErrorStr());
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

static bool ParseEntityPosixLinux(char **str, acl_entry_t ace, int *is_mask)
{
    acl_tag_t etype;
    size_t idsz;
    id_t id;
    char *ids;
    char *id_end;
    bool result = true;
    size_t i;

    ids = NULL;

// TODO: Support numeric id in addition to (user/group) name ?

// Posix language: tag type, qualifier, permissions

    if (strncmp(*str, "user:", 5) == 0)
    {
        *str += 5;

        // create null-terminated string for entity id
        id_end = strchr(*str, ':');

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
            if (!GetUserID(ids, &id, LOG_LEVEL_ERR))
            {
                /* error already logged */
                free(ids);
                return false;
            }
        }
    }
    else if (strncmp(*str, "group:", 6) == 0)
    {
        *str += 6;

        // create null-terminated string for entity id
        id_end = strchr(*str, ':');

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
            if (!GetGroupID(ids, &id, LOG_LEVEL_ERR))
            {
                /* error already logged */
                free(ids);
                return false;
            }
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
        Log(LOG_LEVEL_ERR, "ACE does not start with user:/group:/all:/mask:");
        return false;
    }

    if (acl_set_tag_type(ace, etype) != 0)
    {
        Log(LOG_LEVEL_ERR, "Could not set ACE tag type. (acl_set_tag_type: %s)", GetErrorStr());
        result = false;
    }
    else if (etype == ACL_USER || etype == ACL_GROUP)
    {
        if ((acl_set_qualifier(ace, &id)) != 0)
        {
            Log(LOG_LEVEL_ERR, "Could not set ACE qualifier. (acl_set_qualifier: %s)", GetErrorStr());
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
  Takes a CFEngine-syntax mode string and existing Posix
  Linux-formatted permissions on the file system object as
  arguments. The mode-string will be applied on the permissions.
  Returns true on success, false otherwise.
*/

static bool ParseModePosixLinux(char *mode, acl_permset_t perms)
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
            Log(LOG_LEVEL_ERR, "Error clearing permissions. (acl_clear_perms: %s)", GetErrorStr());
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
                Log(LOG_LEVEL_ERR, "Unable to clear ACL permissions. (acl_clear_perms: %s)", GetErrorStr());
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
                Log(LOG_LEVEL_ERR, "No Linux support for generic permission flag '%c'", *mode);
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
                Log(LOG_LEVEL_ERR, "Could not change ACE permission. (acl_[add|delete]_perm: %s)", GetErrorStr());
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
                    Log(LOG_LEVEL_ERR, "No Linux support for native permission flag '%c'", *mode);
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
                    Log(LOG_LEVEL_ERR, "Could not change ACE permission. (acl_[add|delete]_perm: %s)", GetErrorStr());
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

#else /* !HAVE_LIBACL */

PromiseResult CheckPosixLinuxACL(EvalContext *ctx, ARG_UNUSED const char *file_path, ARG_UNUSED Acl acl, const Attributes *a, const Promise *pp)
{
    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
         "Posix ACLs are not supported on this Linux system - install the Posix acl library");
    PromiseRef(LOG_LEVEL_ERR, pp);
    return PROMISE_RESULT_FAIL;
}

#endif
