#include "server_access.h"
#include "strlist.h"
#include "server.h"

#include <cf3.defs.h>                                     /* FILE_SEPARATOR */
#include <addr_lib.h>                                     /* FuzzySetMatch */
#include <conversion.h>                                   /* MapAddress */
#include <misc_lib.h>

struct acl *paths_acl;
struct acl *classes_acl, *vars_acl, *literals_acl;
struct acl *query_acl;

/**
 * Run this function on every resource (file, class, var etc) access to
 * grant/deny rights. Currently it checks if:
 *  1. #ipaddr matches the subnet expression in {admit,deny}_ips
 *  2. #hostname matches the subdomain expression in {admit,deny}_hostnames
 *  3. #key is found exactly as it is in {admit,deny}_keys
 *
 * @return Default is deny. If a match is found in #acl->admit_* then return
 *         true, unless a match is also found in #acl->deny_* in which case
 *         return false.
 *
 * @TODO preprocess our global ACL the moment a client connects, and store in
 *       ServerConnectionState a list of objects that he can access. That way
 *       only his relevant resources will be stored in e.g. {admit,deny}_paths
 *       lists, and running through these two lists on every file request will
 *       be much faster. */
bool access_CheckResource(const struct resource_acl *acl,
                          const char *ipaddr, const char *hostname,
                          const char *key)
{
    /* Only hostname may be NULL in case of resolution failure. */
    assert(ipaddr != NULL);
    assert(key != NULL);

    size_t pos = (size_t) -1;
    bool access = false;                                 /* DENY by default */

    /* First we check for admission, secondly for denial, so that denial takes
     * precedence. */

    char *rule;
    if (acl->admit_ips)
    {
        /* TODO still using legacy code here, doing linear search over all IPs
         * in textual representation... too CPU intensive! TODO store the ACL
         * as one list of struct sockaddr_storage, together with CIDR
         * subnet length.
         */

        bool found_rule = false;
        for (int i = 0; i < acl->admit_ips->len; i++)
        {
            if (FuzzySetMatch(acl->admit_ips->list[i]->str,
                              MapAddress(ipaddr))
                == 0)
            {
                found_rule = true;
                rule = acl->admit_ips->list[i]->str;
                break;
            }
        }

        if (found_rule)
        {
            Log(LOG_LEVEL_DEBUG, "access_Check: admit IP: %s", rule);
            access = true;
        }
    }
    if (!access && acl->admit_keys != NULL)
    {
        bool ret = StrList_BinarySearch(acl->admit_keys, key, &pos);
        if (ret)
        {
            rule = acl->admit_keys->list[pos]->str;
            Log(LOG_LEVEL_DEBUG, "access_Check: admit key: %s", rule);
            access = true;
        }
    }
    if (!access && acl->admit_hostnames != NULL && hostname != NULL)
    {
        size_t hostname_len = strlen(hostname);
        size_t pos = StrList_SearchShortestPrefix(acl->admit_hostnames,
                                                  hostname, hostname_len,
                                                  false);
        if (pos != (size_t) -1)
        {
            const char *rule = acl->admit_hostnames->list[pos]->str;
            size_t rule_len  = acl->admit_hostnames->list[pos]->len;
            /* The rule in the access list has to be an exact match, or be a
             * subdomain match (i.e. the rule begins with '.'). */
            if (rule_len == hostname_len || rule[0] == '.')
            {
                Log(LOG_LEVEL_DEBUG, "access_Check: admit hostname: %s", rule);
                access = true;
            }
        }
    }

    /* If access has been granted, we might need to deny it based on ACL. */

    if (access && acl->deny_ips != NULL)
    {
        bool found_rule = false;
        for (int i = 0; i < acl->deny_ips->len; i++)
        {
            if (FuzzySetMatch(acl->deny_ips->list[i]->str,
                              MapAddress(ipaddr))
                == 0)
            {
                found_rule = true;
                rule = acl->deny_ips->list[i]->str;
                break;
            }
        }

        if (found_rule)
        {
            Log(LOG_LEVEL_DEBUG, "access_Check: deny IP: %s", rule);
            access = false;
        }
    }
    if (access && acl->deny_keys != NULL)
    {
        bool ret = StrList_BinarySearch(acl->deny_keys, key, &pos);
        if (ret)
        {
            rule = acl->deny_keys->list[pos]->str;
            Log(LOG_LEVEL_DEBUG, "access_Check: deny key: %s", rule);
            access = false;
        }
    }
    if (access && acl->deny_hostnames != NULL && hostname != NULL)
    {
        size_t hostname_len = strlen(hostname);
        size_t pos = StrList_SearchShortestPrefix(acl->deny_hostnames,
                                                  hostname, hostname_len,
                                                  false);
        if (pos != (size_t) -1)
        {
            const char *rule = acl->deny_hostnames->list[pos]->str;
            size_t rule_len  = acl->deny_hostnames->list[pos]->len;
            /* The rule in the access list has to be an exact match, or be a
             * subdomain match (i.e. the rule begins with '.'). */
            if (rule_len == hostname_len || rule[0] == '.')
            {
                Log(LOG_LEVEL_DEBUG, "access_Check: deny hostname: %s", rule);
                access = false;
            }
        }
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

    size_t pos = StrList_SearchLongestPrefix(acl->resource_names,
                                             reqpath, reqpath_len,
                                             FILE_SEPARATOR, true);

    if (pos != (size_t) -1)                          /* acl entry was found */
    {
        const struct resource_acl *racl = &acl->acls[pos];
        bool ret = access_CheckResource(racl, ipaddr, hostname, key);
        if (ret == true)                  /* entry found that grants access */
        {
            access = true;
        }
        Log(LOG_LEVEL_DEBUG,
            "acl_CheckPath: '%s' found in ACL entry '%s', admit=%s",
            reqpath, acl->resource_names->list[pos]->str,
            ret == true ? "true" : "false");
    }

    /* Put back special variables, so that we can find the entry in the ACL,
     * e.g. turn "/path/to/192.168.1.1.json"
     *      to   "/path/to/$(connection.ip).json" */
    char mangled_path[PATH_MAX];
    memcpy(mangled_path, reqpath, reqpath_len + 1);
    size_t mangled_path_len =
        ReplaceSpecialVariables(mangled_path, sizeof(mangled_path),
                                ipaddr,   "$(connection.ip)",
                                hostname, "$(connection.fqdn)",
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
                access_CheckResource(racl, "$(connection.ip)", "$(connection.fqdn)",
                                     "$(connection.key)");
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
        bool ret = access_CheckResource(racl, ipaddr, hostname, key);
        if (ret == true)                  /* entry found that grants access */
        {
            access = true;
        }
    }

    return access;
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
    acl->acls[position] = (struct resource_acl) { 0 }; /*  NULL acls <=> empty */

    Log(LOG_LEVEL_DEBUG, "Inserted in ACL position %zu: %s",
        position, handle);

    return position;
}

void acl_Free(struct acl *a)
{
    StrList_Free(&a->resource_names);

    size_t i;
    for (i = 0; i < a->len; i++)
    {
        StrList_Free(&a->acls[i].admit_ips);
        StrList_Free(&a->acls[i].admit_hostnames);
        StrList_Free(&a->acls[i].admit_keys);
        StrList_Free(&a->acls[i].deny_ips);
        StrList_Free(&a->acls[i].deny_hostnames);
        StrList_Free(&a->acls[i].deny_keys);
    }

    free(a);
}
