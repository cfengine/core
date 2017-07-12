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

#include <verify_acl.h>

#include <actuator.h>
#include <acl_posix.h>
#include <files_names.h>
#include <promises.h>
#include <string_lib.h>
#include <rlist.h>
#include <eval_context.h>
#include <cf-agent-enterprise-stubs.h>
#include <cf-agent-windows-functions.h>

// Valid operations (first char of mode)
#define CF_VALID_OPS_METHOD_OVERWRITE "=+-"
#define CF_VALID_OPS_METHOD_APPEND "=+-"

static int CheckACLSyntax(const char *file, Acl acl, const Promise *pp);

static void SetACLDefaults(const char *path, Acl *acl);
static int CheckACESyntax(char *ace, char *valid_nperms, char *valid_ops, int deny_support, int mask_support,
                        const Promise *pp);
static int CheckModeSyntax(char **mode_p, char *valid_nperms, char *valid_ops, const Promise *pp);
static int CheckPermTypeSyntax(char *permt, int deny_support, const Promise *pp);
static int CheckAclDefault(const char *path, Acl *acl, const Promise *pp);


PromiseResult VerifyACL(EvalContext *ctx, const char *file, Attributes a, const Promise *pp)
{
    if (!CheckACLSyntax(file, a.acl, pp))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Syntax error in access control list for '%s'", file);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return PROMISE_RESULT_INTERRUPTED;
    }

    SetACLDefaults(file, &a.acl);

    PromiseResult result = PROMISE_RESULT_NOOP;

// decide which ACL API to use
    switch (a.acl.acl_type)
    {
    case ACL_TYPE_NONE: // fallthrough: acl_type defaults to generic
    case ACL_TYPE_GENERIC:

#if defined(__linux__)
        result = PromiseResultUpdate(result, CheckPosixLinuxACL(ctx, file, a.acl, a, pp));
#elif defined(__MINGW32__)
        result = PromiseResultUpdate(result, Nova_CheckNtACL(ctx, file, a.acl, a, pp));
#else
        Log(LOG_LEVEL_INFO, "ACLs are not yet supported on this system.");
#endif
        break;

    case ACL_TYPE_POSIX:

#if defined(__linux__)
        result = PromiseResultUpdate(result, CheckPosixLinuxACL(ctx, file, a.acl, a, pp));
#else
        Log(LOG_LEVEL_INFO, "Posix ACLs are not supported on this system");
#endif
        break;

    case ACL_TYPE_NTFS_:
#ifdef __MINGW32__
        result = PromiseResultUpdate(result, Nova_CheckNtACL(ctx, file, a.acl, a, pp));
#else
        Log(LOG_LEVEL_INFO, "NTFS ACLs are not supported on this system");
#endif
        break;

    default:
        Log(LOG_LEVEL_ERR, "Unknown ACL type - software error");
        break;
    }

    return result;
}

static int CheckACLSyntax(const char *file, Acl acl, const Promise *pp)
{
    int valid = true;
    int deny_support = false;
    int mask_support = false;
    char *valid_ops = NULL;
    char *valid_nperms = NULL;
    Rlist *rp;

// set unset fields to defautls
    SetACLDefaults(file, &acl);

// find valid values for op

    switch (acl.acl_method)
    {
    case ACL_METHOD_OVERWRITE:
        valid_ops = CF_VALID_OPS_METHOD_OVERWRITE;
        break;

    case ACL_METHOD_APPEND:
        valid_ops = CF_VALID_OPS_METHOD_APPEND;
        break;

    default:
        // never executed: should be set to a default value by now
        break;
    }

    switch (acl.acl_type)
    {
    case ACL_TYPE_GENERIC:        // generic ACL type: cannot include native or deny-type permissions
        valid_nperms = "";
        deny_support = false;
        mask_support = false;
        break;

    case ACL_TYPE_POSIX:
        valid_nperms = CF_VALID_NPERMS_POSIX;
        deny_support = false;   // posix does not support deny-type permissions
        mask_support = true;    // mask-ACE is allowed in POSIX
        break;

    case ACL_TYPE_NTFS_:
        valid_nperms = CF_VALID_NPERMS_NTFS;
        deny_support = true;
        mask_support = false;
        break;

    default:
        // never executed: should be set to a default value by now
        break;
    }

// check that acl_default is set to a valid value

    if (!CheckAclDefault(file, &acl, pp))
    {
        return false;
    }

    for (rp = acl.acl_entries; rp != NULL; rp = rp->next)
    {
        valid = CheckACESyntax(RlistScalarValue(rp), valid_ops, valid_nperms, deny_support, mask_support, pp);

        if (!valid)             // wrong syntax in this ace
        {
            Log(LOG_LEVEL_ERR, "ACL: The ACE '%s' contains errors", RlistScalarValue(rp));
            PromiseRef(LOG_LEVEL_ERR, pp);
            break;
        }
    }

    for (rp = acl.acl_default_entries; rp != NULL; rp = rp->next)
    {
        valid = CheckACESyntax(RlistScalarValue(rp), valid_ops, valid_nperms, deny_support, mask_support, pp);

        if (!valid)             // wrong syntax in this ace
        {
            Log(LOG_LEVEL_ERR, "ACL: The ACE '%s' contains errors", RlistScalarValue(rp));
            PromiseRef(LOG_LEVEL_ERR, pp);
            break;
        }
    }

    return valid;
}

/**
 * Set unset fields with documented defaults, to these defaults.
 **/

static void SetACLDefaults(const char *path, Acl *acl)
{
// default: acl_method => append

    if (acl->acl_method == ACL_METHOD_NONE)
    {
        acl->acl_method = ACL_METHOD_APPEND;
    }

// default: acl_type => generic

    if (acl->acl_type == ACL_TYPE_NONE)
    {
        acl->acl_type = ACL_TYPE_GENERIC;
    }

// default on directories: acl_default => nochange

    if ((acl->acl_default == ACL_DEFAULT_NONE) && (IsDir(path)))
    {
        acl->acl_default = ACL_DEFAULT_NO_CHANGE;
    }
}

static int CheckAclDefault(const char *path, Acl *acl, const Promise *pp)
/*
  Checks that acl_default is set to a valid value for this acl type.
  Returns true if so, or false otherwise.
*/
{
    int valid = false;

    switch (acl->acl_default)
    {
    case ACL_DEFAULT_NONE:      // unset is always valid
        valid = true;

        break;

    case ACL_DEFAULT_SPECIFY:        // NOTE: we assume all acls support specify

        // fallthrough
    case ACL_DEFAULT_ACCESS:

        // fallthrough
    default:

        if (IsDir(path))
        {
            valid = true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "acl_default can only be set on directories.");
            PromiseRef(LOG_LEVEL_ERR, pp);
            valid = false;
        }

        break;
    }

    return valid;
}

static int CheckACESyntax(char *ace, char *valid_ops, char *valid_nperms, int deny_support, int mask_support,
                          const Promise *pp)
{
    char *str;
    int chkid;
    int valid_mode;
    int valid_permt;

    str = ace;
    chkid = false;

// first element must be "user", "group" or "all"

    if (strncmp(str, "user:", 5) == 0)
    {
        str += 5;
        chkid = true;
    }
    else if (strncmp(str, "group:", 6) == 0)
    {
        str += 6;
        chkid = true;
    }
    else if (strncmp(str, "all:", 4) == 0)
    {
        str += 4;
        chkid = false;
    }
    else if (strncmp(str, "mask:", 5) == 0)
    {

        if (mask_support)
        {
            str += 5;
            chkid = false;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "This ACL type does not support mask ACE.");
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

    }
    else
    {
        Log(LOG_LEVEL_ERR, "ACL: ACE '%s' does not start with user:/group:/all", ace);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (chkid)                  // look for following "id:"
    {
        if (*str == ':')
        {
            Log(LOG_LEVEL_ERR, "ACL: ACE '%s': id cannot be empty or contain ':'", ace);
            return false;
        }

        // skip id-string (no check: allow any id-string)

        while (true)
        {
            str++;
            if (*str == ':')
            {
                str++;
                break;
            }
            else if (*str == '\0')
            {
                Log(LOG_LEVEL_ERR, "ACL: Nothing following id string in ACE '%s'", ace);
                return false;
            }
        }
    }

// check the mode-string (also skips to next field)
    valid_mode = CheckModeSyntax(&str, valid_ops, valid_nperms, pp);

    if (!valid_mode)
    {
        Log(LOG_LEVEL_ERR, "ACL: Malformed mode-string in ACE '%s'", ace);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (*str == '\0')           // mode was the last field
    {
        return true;
    }

    str++;

// last field; must be a perm_type field
    valid_permt = CheckPermTypeSyntax(str, deny_support, pp);

    if (!valid_permt)
    {
        Log(LOG_LEVEL_ERR, "ACL: Malformed perm_type syntax in ACE '%s'", ace);
        return false;
    }

    return true;
}

static int CheckModeSyntax(char **mode_p, char *valid_ops, char *valid_nperms, const Promise *pp)
/*
  Checks the syntax of a ':' or NULL terminated mode string.
  Moves the string pointer to the character following the mode
  (i.e. ':' or '\0')
*/
{
    char *mode;
    int valid;

    valid = false;
    mode = *mode_p;

// mode is allowed to be empty

    if ((*mode == '\0') || (*mode == ':'))
    {
        return true;
    }

    while (true)
    {
        mode = ScanPastChars(valid_ops, mode);

        mode = ScanPastChars(CF_VALID_GPERMS, mode);

        if (*mode == CF_NATIVE_PERMS_SEP_START)
        {
            mode++;

            mode = ScanPastChars(valid_nperms, mode);

            if (*mode == CF_NATIVE_PERMS_SEP_END)
            {
                mode++;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "ACL: Invalid native permission '%c', or missing native end separator", *mode);
                PromiseRef(LOG_LEVEL_ERR, pp);
                valid = false;
                break;
            }
        }

        if ((*mode == '\0') || (*mode == ':'))      // end of mode-string
        {
            valid = true;
            break;
        }
        else if (*mode == ',')  // one more iteration
        {
            mode++;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "ACL: Mode string contains invalid characters");
            PromiseRef(LOG_LEVEL_ERR, pp);
            valid = false;
            break;
        }
    }

    *mode_p = mode;             // move pointer to past mode-field

    return valid;
}

static int CheckPermTypeSyntax(char *permt, int deny_support, const Promise *pp)
/*
  Checks if the given string corresponds to the perm_type syntax.
  Only "allow" or "deny", followed by NULL-termination are valid.
  In addition, "deny" is only valid for ACL types supporting it.
 */
{
    int valid;

    valid = false;

    if (strcmp(permt, "allow") == 0)
    {
        valid = true;
    }
    else if (strcmp(permt, "deny") == 0)
    {
        if (deny_support)
        {
            valid = true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Deny permission not supported by this ACL type");
            PromiseRef(LOG_LEVEL_ERR, pp);
        }
    }

    return valid;
}
