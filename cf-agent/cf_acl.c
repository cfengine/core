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

#include "cf_acl.h"

#include "acl_posix.h"
#include "files_names.h"
#include "promises.h"
#include "cfstream.h"
#include "logging.h"
#include "string_lib.h"
#include "rlist.h"

/*****************************************************************************/

// Valid operations (first char of mode)
#define CF_VALID_OPS_METHOD_OVERWRITE "=+-"
#define CF_VALID_OPS_METHOD_APPEND "=+-"

static int CheckACLSyntax(char *file, Acl acl, Promise *pp);

static void SetACLDefaults(char *path, Acl *acl);
static int CheckACESyntax(char *ace, char *valid_nperms, char *valid_ops, int deny_support, int mask_support,
                        Promise *pp);
static int CheckModeSyntax(char **mode_p, char *valid_nperms, char *valid_ops, Promise *pp);
static int CheckPermTypeSyntax(char *permt, int deny_support, Promise *pp);
static int CheckDirectoryInherit(char *path, Acl *acl, Promise *pp);


void VerifyACL(char *file, Attributes a, Promise *pp)
{
    if (!CheckACLSyntax(file, a.acl, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Syntax error in access control list for \"%s\"", file);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return;
    }

    SetACLDefaults(file, &a.acl);

// decide which ACL API to use
    switch (a.acl.acl_type)
    {
    case ACL_TYPE_NONE: // fallthrough: acl_type defaults to generic
    case ACL_TYPE_GENERIC:

#if defined(__linux__)
        CheckPosixLinuxACL(file, a.acl, a, pp);
#elif defined(__MINGW32__)
        Nova_CheckNtACL(file, a.acl, a, pp);
#else
        CfOut(OUTPUT_LEVEL_INFORM, "", "!! ACLs are not yet supported on this system.");
#endif
        break;

    case ACL_TYPE_POSIX:

#if defined(__linux__)
        CheckPosixLinuxACL(file, a.acl, a, pp);
#else
        CfOut(OUTPUT_LEVEL_INFORM, "", "!! Posix ACLs are not supported on this system");
#endif
        break;

    case ACL_TYPE_NTFS_:

#if defined(__MINGW32__)
        Nova_CheckNtACL(file, a.acl, a, pp);
#else
        CfOut(OUTPUT_LEVEL_INFORM, "", "!! NTFS ACLs are not supported on this system");
#endif
        break;

    default:
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unknown ACL type - software error");
        break;
    }
}

static int CheckACLSyntax(char *file, Acl acl, Promise *pp)
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

// check that acl_directory_inherit is set to a valid value

    if (!CheckDirectoryInherit(file, &acl, pp))
    {
        return false;
    }

    for (rp = acl.acl_entries; rp != NULL; rp = rp->next)
    {
        valid = CheckACESyntax(RlistScalarValue(rp), valid_ops, valid_nperms, deny_support, mask_support, pp);

        if (!valid)             // wrong syntax in this ace
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "The ACE \"%s\" contains errors", RlistScalarValue(rp));
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            break;
        }
    }

    for (rp = acl.acl_inherit_entries; rp != NULL; rp = rp->next)
    {
        valid = CheckACESyntax(rp->item, valid_ops, valid_nperms, deny_support, mask_support, pp);

        if (!valid)             // wrong syntax in this ace
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "The ACE \"%s\" contains errors", RlistScalarValue(rp));
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            break;
        }
    }

    return valid;
}

/**
 * Set unset fields with documented defaults, to these defaults.
 **/

static void SetACLDefaults(char *path, Acl *acl)
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

// default on directories: acl_directory_inherit => parent

    if ((acl->acl_directory_inherit == ACL_INHERITANCE_NONE) && (IsDir(path)))
    {
        acl->acl_directory_inherit = ACL_INHERITANCE_NO_CHANGE;
    }
}

static int CheckDirectoryInherit(char *path, Acl *acl, Promise *pp)
/*
  Checks that acl_directory_inherit is set to a valid value for this acl type.
  Returns true if so, or false otherwise.
*/
{
    int valid = false;

    switch (acl->acl_directory_inherit)
    {
    case ACL_INHERITANCE_NONE:      // unset is always valid
        valid = true;

        break;

    case ACL_INHERITANCE_SPECIFY:        // NOTE: we assume all acls support specify

        // fallthrough
    case ACL_INHERITANCE_PARENT:

        // fallthrough
    default:

        if (IsDir(path))
        {
            valid = true;
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "acl_directory_inherit can only be set on directories.");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            valid = false;
        }

        break;
    }

    return valid;
}

static int CheckACESyntax(char *ace, char *valid_ops, char *valid_nperms, int deny_support, int mask_support, Promise *pp)
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
            CfOut(OUTPUT_LEVEL_ERROR, "", "This ACL type does not support mask ACE.");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            return false;
        }

    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "ACE '%s' does not start with user:/group:/all", ace);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return false;
    }

    if (chkid)                  // look for following "id:"
    {
        if (*str == ':')
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "ACE '%s': id cannot be empty or contain ':'", ace);
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "Nothing following id string in ACE '%s'", ace);
                return false;
            }
        }
    }

// check the mode-string (also skips to next field)
    valid_mode = CheckModeSyntax(&str, valid_ops, valid_nperms, pp);

    if (!valid_mode)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Malformed mode-string in ACE '%s'", ace);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
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
        CfOut(OUTPUT_LEVEL_ERROR, "", "Malformed perm_type syntax in ACE '%s'", ace);
        return false;
    }

    return true;
}

static int CheckModeSyntax(char **mode_p, char *valid_ops, char *valid_nperms, Promise *pp)
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "Invalid native permission '%c', or missing native end separator", *mode);
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
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
            CfOut(OUTPUT_LEVEL_ERROR, "", "Mode string contains invalid characters");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            valid = false;
            break;
        }
    }

    *mode_p = mode;             // move pointer to past mode-field

    return valid;
}

static int CheckPermTypeSyntax(char *permt, int deny_support, Promise *pp)
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
            CfOut(OUTPUT_LEVEL_ERROR, "", "Deny permission not supported by this ACL type");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        }
    }

    return valid;
}
