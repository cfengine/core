#include "access.h"
#include "strlist.h"
#include "server.h"

#include <cf3.defs.h>                                     /* FILE_SEPARATOR */
#include <addr_lib.h>                                     /* FuzzySetMatch */
#include <conversion.h>                                   /* MapAddress */
#include <misc_lib.h>


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
    size_t pos = (size_t) -1;
    bool access = false;                                 /* DENY by default */

    /* First we check for admission, secondly for denial, so that denial takes
     * precedence. */

    /* TODO acl->{admit,deny}_subnets */
    /* TODO acl->{admit,deny}_domains */

    char *rule;
    if (acl->admit_ips)
    {
        /* TODO still using legacy code here, doing linear search over all IPs
         * in textual representation... too CPU intensive! TODO store the ACL
         * list as one IPv4 list and as one IPv6 list, both in binary (in_addr
         * or in6_addr), together with subnet lengths, and binary search
         * it. Hmmm, NO I must not differentiate the two IP families! Just
         * store struct sockaddr_storage pointers. But how do I binary search
         * that?  Easy since all IPv4 addresses go on top.

           struct addrlist {
             size_t len;
             size_t alloc_len;
             struct {
               int masklen;
               struct sockaddr_storage *ss;
             } list[];
           };

         * Naah it make much more sense to reuse the existing strlist but
         * store arbitrary data (sockaddr_storage) as long as that custom
         * struct has a "size_t len" as a first parameter with the total size
         * of the struct:

           struct addr {
             size_t len;
             struct sockaddr_storage ss[];
           };

         * Yes, *not* a pointer but a sockaddr_storage itself, but make sure
         * allocation is *not* for the whole sockaddr_storage, but depending
         * on its address family - thus the empty array [], so that we can
         * allocate as much we want.
         * This ss field should then be cast'able to sockaddr_in etc.
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
    if (access == false && acl->admit_keys)
    {
        bool ret = strlist_BinarySearch(acl->admit_keys, key, &pos);
        if (ret)
        {
            rule = acl->admit_keys->list[pos]->str;
            Log(LOG_LEVEL_DEBUG, "access_Check: admit key: %s", rule);
            access = true;
        }
    }
    if (access == false && acl->admit_hostnames && hostname)
    {
        size_t hostname_len = strlen(hostname);
        size_t pos = strlist_SearchShortestPrefix(acl->admit_hostnames,
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

    if (access == true && acl->deny_ips)
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
    if (access == true && acl->deny_keys)
    {
        bool ret = strlist_BinarySearch(acl->deny_keys, key, &pos);
        if (ret)
        {
            rule = acl->deny_keys->list[pos]->str;
            Log(LOG_LEVEL_DEBUG, "access_Check: deny key: %s", rule);
            access = false;
        }
    }
    if (access == true && acl->deny_hostnames && hostname)
    {
        size_t hostname_len = strlen(hostname);
        size_t pos = strlist_SearchShortestPrefix(acl->deny_hostnames,
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
                access = true;
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

    /* TODO write TEST: if paths_acl has both dirs and files,
       whether it will properly match entries. */
    size_t pos = strlist_SearchLongestPrefix(acl->resource_names,
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
    char mangled_path[(reqpath_len + 40) * 2];         /* leave enough room */
    memcpy(mangled_path, reqpath, reqpath_len + 1);
    size_t mangled_path_len =
        ReplaceSpecialVariables(mangled_path, sizeof(mangled_path),
                                ipaddr,   "$(connection.ip)",
                                hostname, "$(connection.fqdn)",
                                key,      "$(connection.key)");

    /* If there were special variables replaced */
    if (mangled_path_len != 0 &&
        mangled_path_len != (size_t) -1)
    {
        size_t pos2 = strlist_SearchLongestPrefix(acl->resource_names,
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
    bool found = strlist_BinarySearch(acl->resource_names, req_string, &pos);
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
 *  Search in a sorted list of unique paths.
 *
 *  @brief Search for string s, of s_len length (not necessarily
 *         '\0'-terminated), in-between the [min,max) positions of pathlist.
 *
 *  @param min, inputs the min boundary of the search, outputs the minimum
 *         boundary for a future search for a superstring of s.
 *  @param max, inputs the max boundary (not inclusive) of the search, outputs
 *         the maximum boundary for a future search for a superstring of s.
 *
 *  @return If an exact match was found return the address of the exact
 *          match. Return NULL in all other cases. In addition, if returned
 *          (*min >= *max) then there can be no match even for longer s,
 *          i.e. pathlist contains no strings prefixed with s.
 */
static char *PathBinarySearch(const char *s, size_t s_len,
                                    const struct strlist *pathlist,
                                    size_t *min, size_t *max)
{
    char *found = NULL;

    size_t priv_min = *min;
    size_t priv_max = *max;

    assert(priv_min < priv_max);
    assert(priv_max <= pathlist->len);

    while (priv_min < priv_max)
    {
        size_t pos = (priv_max - priv_min) / 2 + priv_min;

        /* Even though we compare s_len bytes, we'll never cross the
         * boundaries of the second string since it's NULL terminated and we
         * know that s_len contains no '\0' in the first s_len bytes - it's a
         * C string anyway. */
        int match = memcmp(s, pathlist->list[pos]->str, s_len); /* TODO TEST */

        if (match == 0)
        {
            if (pathlist->list[pos]->len == s_len)
            {
                /* Exact match, we know the list has no duplicates so it's
                 * the first match. */
                found = pathlist->list[pos]->str;
                /* We might as well not search here later, when s_len is
                 * longer, because longer means it won't match this. */
                *min = pos + 1;
                break;
            }
            else
            {
                /* Prefix match, pathlist[pos] is superstring of s. That means
                 * that the exact match may be before. */
                priv_max = pos;
                /* However we keep this as a different case because we must
                 * not change *max, since that position might match a search
                 * later, with bigger s_len. */
            }
        }
        else if (match < 0)                            /* s < pathlist[pos] */
        {
            priv_max = pos;
            /* This position will never match, no matter how big s_len grows
             * in later searches. Thus we change *max to avoid searching
             * here. */
            *max = priv_max;
        }
        else                                           /* s > pathlist[pos] */
        {
            priv_min = pos + 1;
            /* Same here, this position will never match even for longer s. */
            *min = priv_min;
        }
    }

    return found;
}

/* Find the longest directory in dirlist, that is a parent directory of s. s
 * does not have to be '\0'-terminated. s can be a file or a directory,
 * i.e. it can end or not end with '/'. */
static char *FindLongestParentDir(const char *s, size_t s_len,
                                  struct strlist *dirlist)
{
    /* Remember, NULL strlist is equivalent to empty strlist. */
    if (dirlist == NULL)
    {
        return NULL;
    }

    char *found = NULL;
    char *old_found = NULL;
    size_t s_prefix_len = 0;
    size_t min = 0;
    size_t max = dirlist->len;
    bool longer_match_possible = true;

    /* Keep searching until we've searched the whole length, or until there is
     * no reason to keep going. */
    while (longer_match_possible && (s_prefix_len < s_len))
    {
        s_prefix_len =
            (char *) memchr(&s[s_prefix_len],
                            FILE_SEPARATOR, s_len - s_prefix_len)
            - &s[0];

        if (found != NULL)
        {
            /* Keep the smaller string match in case we don't match. */
            old_found = found;
        }

        found = PathBinarySearch(s, s_prefix_len, dirlist, &min, &max);

        /* If not even a superstring was found then don't keep trying. */
        longer_match_possible = (min < max);
    }

    if (found == NULL)
    {
        return old_found;
    }
    else
    {
        return found;
    }
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
    bool found = strlist_BinarySearch(acl->resource_names,
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
    size_t ret = strlist_Insert(&acl->resource_names,
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
    strlist_Free(&a->resource_names);

    size_t i;
    for (i = 0; i < a->len; i++)
    {
        strlist_Free(&a->acls[i].admit_ips);
        strlist_Free(&a->acls[i].admit_hostnames);
        strlist_Free(&a->acls[i].admit_keys);
        strlist_Free(&a->acls[i].deny_ips);
        strlist_Free(&a->acls[i].deny_hostnames);
        strlist_Free(&a->acls[i].deny_keys);
    }

    free(a);
}
