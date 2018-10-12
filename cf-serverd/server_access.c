/*
   Copyright 2018 Northern.tech AS

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

#include "server_access.h"
#include "strlist.h"
#include "server.h"

#include <addr_lib.h>                                     /* FuzzySetMatch */
#include <string_lib.h>                      /* StringMatchFull TODO REMOVE */
#include <misc_lib.h>
#include <file_lib.h>
#include <regex.h>


struct acl *paths_acl;
struct acl *classes_acl;
struct acl *vars_acl;
struct acl *literals_acl;
struct acl *query_acl;
struct acl *bundles_acl;
struct acl *roles_acl;


/**
 * Run this function on every resource (file, class, var etc) access to
 * grant/deny rights. Currently it checks if:
 *  1. #ipaddr matches the subnet expression in {admit,deny}_ips
 *  2. #hostname matches the subdomain expression in {admit,deny}_hostnames
 *  3. #key is searched as-is in {admit,deny}_keys
 *  4. #username is searched as-is in {admit,deny}_usernames
 *
 * @param #found If not NULL then it returns whether the denial was implicit
 *               (found=false) or explicit (found=true). If not NULL it also
 *               changes the way the ACLs are traversed to a SLOWER mode,
 *               since every entry has to be checked for explicit denial.
 *
 * @return Default is false, i.e. deny. If a match is found in #acl->admit.*
 *         then return true, unless a match is also found in #acl->deny.* in
 *         which case return false.
 *
 * @TODO preprocess our global ACL the moment a client connects, and store in
 *       ServerConnectionState a list of objects that he can access. That way
 *       only his relevant resources will be stored in e.g. {admit,deny}_paths
 *       lists, and running through these two lists on every file request will
 *       be much faster.
 */
static bool access_CheckResource(const struct resource_acl *acl,
                                 bool *found,
                                 const char *ipaddr, const char *hostname,
                                 const char *key, const char *username)
{
    bool access     = false;                 /* DENY by default */
    bool have_match = false;                 /* No matching rule found yet */

    /* First we check for admission, secondly for denial, so that denial takes
     * precedence. */

    if (!NULL_OR_EMPTY(ipaddr) && acl->admit.ips != NULL)
    {
        /* Still using legacy code here, doing linear search over all IPs in
         * textual representation... too CPU intensive! TODO store the ACL as
         * one list of struct sockaddr_storage, together with CIDR notation
         * subnet length.
         */

        const char *rule = NULL;
        for (int i = 0; i < StrList_Len(acl->admit.ips); i++)
        {
            if (FuzzySetMatch(StrList_At(acl->admit.ips, i), ipaddr) == 0 ||
                /* Legacy regex matching, TODO DEPRECATE */
                StringMatchFull(StrList_At(acl->admit.ips, i), ipaddr))
            {
                rule = StrList_At(acl->admit.ips, i);
                break;
            }
        }

        if (rule != NULL)
        {
            Log(LOG_LEVEL_DEBUG,
                "Admit IP due to rule: %s",
                rule);
            access = true;
            have_match = true;
        }
    }
    if (!access && !NULL_OR_EMPTY(hostname) &&
        acl->admit.hostnames != NULL)
    {
        size_t pos = StrList_SearchLongestPrefix(acl->admit.hostnames,
                                                 hostname, 0,
                                                 '.', false);

        /* === Legacy regex matching, slow, TODO DEPRECATE === */
        if (pos == (size_t) -1)
        {
            for (int i = 0; i < StrList_Len(acl->admit.hostnames); i++)
            {
                if (StringMatchFull(StrList_At(acl->admit.hostnames, i),
                                    hostname))
                {
                    pos = i;
                    break;
                }
            }
        }
        /* =================================================== */

        if (pos != (size_t) -1)
        {
            Log(LOG_LEVEL_DEBUG,
                "Admit hostname due to rule: %s",
                StrList_At(acl->admit.hostnames, pos));
            access = true;
            have_match = true;
        }
    }
    if (!access && !NULL_OR_EMPTY(key) &&
        acl->admit.keys != NULL)
    {
        size_t pos;
        bool ret = StrList_BinarySearch(acl->admit.keys, key, &pos);
        if (ret)
        {
            Log(LOG_LEVEL_DEBUG,
                "Admit key due to rule: %s",
                StrList_At(acl->admit.keys, pos));
            access = true;
            have_match = true;
        }
    }
    if (!access && !NULL_OR_EMPTY(username) &&
        acl->admit.usernames != NULL)
    {
        size_t pos;
        bool ret = StrList_BinarySearch(acl->admit.usernames, username, &pos);
        if (ret)
        {
            Log(LOG_LEVEL_DEBUG,
                "Admit username due to rule: %s",
                StrList_At(acl->admit.usernames, pos));
            access = true;
            have_match = true;
        }
    }


    /* An admit rule was not found, and we don't care whether the denial is
     * explicit or implicit: we can finish now. */
    if (!access && found == NULL)
    {
        assert(!have_match);
        return false;                                      /* EARLY RETURN! */
    }

    /* If access has been granted, we might need to deny it based on ACL. */
    /* Same goes if access has not been granted and "found" is not NULL, in
     * which case we have to return in "found", whether an explicit denial
     * rule matched or not. */

    assert((access && have_match) ||
           (!access && !have_match && found != NULL));

    if ((access || !have_match) &&
        !NULL_OR_EMPTY(ipaddr) &&
        acl->deny.ips != NULL)
    {
        const char *rule = NULL;
        for (int i = 0; i < StrList_Len(acl->deny.ips); i++)
        {
            if (FuzzySetMatch(StrList_At(acl->deny.ips, i), ipaddr) == 0 ||
                /* Legacy regex matching, TODO DEPRECATE */
                StringMatchFull(StrList_At(acl->deny.ips, i), ipaddr))
            {
                rule = StrList_At(acl->deny.ips, i);
                break;
            }
        }

        if (rule != NULL)
        {
            Log(LOG_LEVEL_DEBUG,
                "Deny IP due to rule: %s",
                rule);
            access = false;
            have_match = true;
        }
    }
    if ((access || !have_match) &&
        !NULL_OR_EMPTY(hostname) &&
        acl->deny.hostnames != NULL)
    {
        size_t pos = StrList_SearchLongestPrefix(acl->deny.hostnames,
                                                 hostname, 0,
                                                 '.', false);

        /* === Legacy regex matching, slow, TODO DEPRECATE === */
        if (pos == (size_t) -1)
        {
            for (int i = 0; i < StrList_Len(acl->deny.hostnames); i++)
            {
                if (StringMatchFull(StrList_At(acl->deny.hostnames, i),
                                    hostname))
                {
                    pos = i;
                    break;
                }
            }
        }
        /* =================================================== */

        if (pos != (size_t) -1)
        {
            Log(LOG_LEVEL_DEBUG,
                "Deny hostname due to rule: %s",
                StrList_At(acl->deny.hostnames, pos));
            access = false;
            have_match = true;
        }
    }
    if ((access || !have_match) &&
        !NULL_OR_EMPTY(key) &&
        acl->deny.keys != NULL)
    {
        size_t pos;
        bool ret = StrList_BinarySearch(acl->deny.keys, key, &pos);
        if (ret)
        {
            Log(LOG_LEVEL_DEBUG,
                "Deny key due to rule: %s",
                StrList_At(acl->deny.keys, pos));
            access = false;
            have_match = true;
        }
    }
    if ((access || !have_match) &&
        !NULL_OR_EMPTY(username) &&
        acl->deny.usernames != NULL)
    {
        size_t pos;
        bool ret = StrList_BinarySearch(acl->deny.usernames, username, &pos);
        if (ret)
        {
            Log(LOG_LEVEL_DEBUG,
                "Deny username due to rule: %s",
                StrList_At(acl->deny.usernames, pos));
            access = false;
            have_match = true;
        }
    }

    /* We can't have implicit admittance,
       admittance must always be explicit. */
    assert(! (access && !have_match));

    if (found != NULL)
    {
        *found = have_match;
    }
    return access;
}


/**
 * Search #req_path in #acl, if found check its rules. The longest parent
 * directory of #req_path is searched, or an exact match. Directories *must*
 * end with FILE_SEPARATOR in the ACL list.
 *
 * @return If ACL entry is found, and host is listed in there return
 *         true. Else return false.
 */
bool acl_CheckPath(const struct acl *acl, const char *reqpath,
                   const char *ipaddr, const char *hostname,
                   const char *key)
{
    bool access = false;                          /* Deny access by default */
    size_t reqpath_len = strlen(reqpath);

    /* CHECK 1: Search for parent directory or exact entry in ACL. */
    size_t pos = StrList_SearchLongestPrefix(acl->resource_names,
                                             reqpath, reqpath_len,
                                             FILE_SEPARATOR, true);

    if (pos != (size_t) -1)                          /* acl entry was found */
    {
        const struct resource_acl *racl = &acl->acls[pos];
        bool ret = access_CheckResource(racl, NULL, ipaddr, hostname, key, NULL);
        if (ret == true)                  /* entry found that grants access */
        {
            access = true;
        }
        Log(LOG_LEVEL_DEBUG,
            "acl_CheckPath: '%s' found in ACL entry '%s', admit=%s",
            reqpath, acl->resource_names->list[pos]->str,
            ret == true ? "true" : "false");
    }

    /* CHECK 2: replace ACL entry parts with special variables (if applicable),
     * e.g. turn "/path/to/192.168.1.1.json"
     *      to   "/path/to/$(connection.ip).json" */
    char mangled_path[PATH_MAX];
    memcpy(mangled_path, reqpath, reqpath_len + 1);
    size_t mangled_path_len =
        ReplaceSpecialVariables(mangled_path, sizeof(mangled_path),
                                ipaddr,   "$(connection.ip)",
                                hostname, "$(connection.hostname)",
                                key,      "$(connection.key)");

    /* If there were special variables replaced */
    if (mangled_path_len != 0 &&
        mangled_path_len != (size_t) -1) /* Overflow, TODO handle separately. */
    {
        size_t pos2 = StrList_SearchLongestPrefix(acl->resource_names,
                                                  mangled_path, mangled_path_len,
                                                  FILE_SEPARATOR, true);

        if (pos2 != (size_t) -1)                   /* acl entry was found */
        {
            /* TODO make sure this match is more specific than the other one. */
            const struct resource_acl *racl = &acl->acls[pos2];
            /* Check if the magic strings are allowed or denied. */
            bool ret =
                access_CheckResource(racl, NULL,
                                     "$(connection.ip)",
                                     "$(connection.hostname)",
                                     "$(connection.key)", NULL);
            if (ret == true)                  /* entry found that grants access */
            {
                access = true;
            }
            Log(LOG_LEVEL_DEBUG,
                "acl_CheckPath: '%s' found in ACL entry '%s', admit=%s",
                mangled_path, acl->resource_names->list[pos2]->str,
                ret == true ? "true" : "false");
        }
    }

    return access;
}

bool acl_CheckExact(const struct acl *acl, const char *req_string,
                    const char *ipaddr, const char *hostname,
                    const char *key)
{
    bool access = false;

    size_t pos = -1;
    bool found = StrList_BinarySearch(acl->resource_names, req_string, &pos);
    if (found)
    {
        const struct resource_acl *racl = &acl->acls[pos];
        bool ret = access_CheckResource(racl, NULL,
                                        ipaddr, hostname, key, NULL);
        if (ret == true)                  /* entry found that grants access */
        {
            access = true;
        }
    }

    return access;
}

/**
 * Go linearly over all the #acl and check every rule if it matches.
 * ADMIT only if at least one rule matches admit and none matches deny.
 * DENY if no rule matches OR if at least one matches deny.
 */
bool acl_CheckRegex(const struct acl *acl, const char *req_string,
                    const char *ipaddr, const char *hostname,
                    const char *key, const char *username)
{
    bool retval = false;

    /* For all ACLs */
    for (size_t i = 0; i < acl->len; i++)
    {
        const char *regex = acl->resource_names->list[i]->str;

        /* Does this ACL matches the req_string? */
        if (StringMatchFull(regex, req_string))
        {
            const struct resource_acl *racl = &acl->acls[i];

            /* Does this ACL apply to this host? */
            bool found;
            bool admit = access_CheckResource(racl, &found,
                                              ipaddr, hostname, key, username);
            if (found && !admit)
            {
                return false;
            }
            else if (found && admit)
            {
                retval = true;
            }
            else
            {
                /* If it's not found, there should be no admittance. */
                assert(!found);
                assert(!admit);
                /* We are not touching retval, because it was possibly found
                 * before and retval has been set to "true". */
            }
        }
    }

    return retval;
}


/**
 * Search the list of resources for the handle. If found return the index of
 * the resource ACL that corresponds to that handle, else add the handle with
 * empty ACL, reallocating if necessary. The new handle is inserted in the
 * proper position to keep the acl->resource_names list sorted.
 *
 * @note acl->resource_names list should already be sorted, no problem if all
 *       inserts are done with this function.
 *
 * @return the index of the resource_acl corresponding to handle. -1 means
 *         reallocation failed, but existing values are still valid.
 */
size_t acl_SortedInsert(struct acl **a, const char *handle)
{
    assert(handle != NULL);

    struct acl *acl = *a;                                    /* for clarity */

    size_t position = (size_t) -1;
    bool found = StrList_BinarySearch(acl->resource_names,
                                      handle, &position);
    if (found)
    {
        /* Found it, return existing entry. */
        assert(position < acl->len);
        return position;
    }

    /* handle is not in acl, we must insert at the position returned. */
    assert(position <= acl->len);

    /* 1. Check if reallocation is needed. */
    if (acl->len == acl->alloc_len)
    {
        size_t new_alloc_len = acl->alloc_len * 2;
        if (new_alloc_len == 0)
        {
            new_alloc_len = 1;
        }

        struct acl *p =
            realloc(acl, sizeof(*p) + sizeof(*p->acls) * new_alloc_len);
        if (p == NULL)
        {
            return (size_t) -1;
        }

        acl = p;
        acl->alloc_len = new_alloc_len;
        *a = acl;                          /* Change the caller's variable */
    }

    /* 2. We now have enough space, so insert the resource at the proper
          index. */
    size_t ret = StrList_Insert(&acl->resource_names,
                                handle, position);
    if (ret == (size_t) -1)
    {
        /* realloc() failed but the data structure is still valid. */
        return (size_t) -1;
    }

    /* 3. Make room. */
    memmove(&acl->acls[position + 1], &acl->acls[position],
            (acl->len - position) * sizeof(acl->acls[position]));
    acl->len++;

    /* 4. Initialise all ACLs for the resource as empty. */
    acl->acls[position] = (struct resource_acl) { {0}, {0} }; /*  NULL acls <=> empty */

    Log(LOG_LEVEL_DEBUG, "Inserted in ACL position %zu: %s",
        position, handle);

    assert(acl->len == StrList_Len(acl->resource_names));

    return position;
}

void acl_Free(struct acl *a)
{
    StrList_Free(&a->resource_names);

    size_t i;
    for (i = 0; i < a->len; i++)
    {
        StrList_Free(&a->acls[i].admit.ips);
        StrList_Free(&a->acls[i].admit.hostnames);
        StrList_Free(&a->acls[i].admit.usernames);
        StrList_Free(&a->acls[i].admit.keys);
        StrList_Free(&a->acls[i].deny.ips);
        StrList_Free(&a->acls[i].deny.hostnames);
        StrList_Free(&a->acls[i].deny.keys);
    }

    free(a);
}

void acl_Summarise(const struct acl *acl, const char *title)
{
    assert(acl->len == StrList_Len(acl->resource_names));

    size_t i, j;
    for (i = 0; i < acl->len; i++)
    {
        Log(LOG_LEVEL_VERBOSE, "\t%s: %s",
            title, StrList_At(acl->resource_names, i));

        const struct resource_acl *racl = &acl->acls[i];

        for (j = 0; j < StrList_Len(racl->admit.ips); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit_ips: %s",
                StrList_At(racl->admit.ips, j));
        }
        for (j = 0; j < StrList_Len(racl->admit.hostnames); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit_hostnames: %s",
                StrList_At(racl->admit.hostnames, j));
        }
        for (j = 0; j < StrList_Len(racl->admit.keys); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit_keys: %s",
                StrList_At(racl->admit.keys, j));
        }
        for (j = 0; j < StrList_Len(racl->deny.ips); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny_ips: %s",
                StrList_At(racl->deny.ips, j));
        }
        for (j = 0; j < StrList_Len(racl->deny.hostnames); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny_hostnames: %s",
                StrList_At(racl->deny.hostnames, j));
        }
        for (j = 0; j < StrList_Len(racl->deny.keys); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny_keys: %s",
                StrList_At(racl->deny.keys, j));
        }
    }
}
