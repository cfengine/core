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
#ifndef CFENGINE_ACCESS_H
#define CFENGINE_ACCESS_H


#include <platform.h>

#include <map.h>                                         /* StringMap */
#include "strlist.h"                                     /* StrList */


/**
 * Access control list referring to one resource, e.g. path, class, variable,
 * literal, bundle.
 *
 * @note: Each strlist might be NULL, which is equivalent to having 0
 *        elements.
 *
 * @note: Currently these lists are binary searched, so after filling them up
 *        make sure you call StrList_Sort() to sort them.
 */
struct admitdeny_acl
{
    StrList *ips;                        /* admit_ips, deny_ips */
    StrList *hostnames;                  /* admit_hostnames, deny_hostnames */
    StrList *keys;                       /* admit_keys, deny_keys */
    StrList *usernames;      /* currently used only in roles access promise */
};

/**
 * This is a list of all resorce ACLs for one resource. E.g. for resource_type
 * == path, this should contain a list of all paths together with a list of
 * ACLs (struct resource_acl) referring to the relevant path.
 *
 * @note Currently this list of resource_names may be binary searched so it
 *       must be sorted once populated.
 *
 * @WARNING Remember to store directories *always* with traling '/', else they
 *          won't match for children dirs (on purpose, and this functionality
 *          was built into StrList_SearchLongestPrefix()).
 */
struct acl
{
//    enum acl_type resource_type;
    size_t len;                        /* Length of resource_names,acls[] */
    size_t alloc_len;                  /* Used for realloc() economy  */
    StrList *resource_names;           /* paths, class names, variables etc */
    struct resource_acl
    {
        struct admitdeny_acl admit;
        struct admitdeny_acl deny;
    } acls[];
};


/* These acls are set on server startup or when promises change, and are
 * read-only for the rest of their life, thus are thread-safe. */

/* The paths_acl should be populated with directories having a trailing '/'
 * to be able to tell apart from files. */
extern struct acl *paths_acl;
extern struct acl *classes_acl;                    /* remoteclassesmatching */
extern struct acl *vars_acl;                       /* remotescalar */
extern struct acl *literals_acl;
extern struct acl *query_acl;                     /* reporting */
extern struct acl *bundles_acl;                   /* cf-runagent connections*/
/* Roles ACL contains classes regexes under resource_names, but currently only
 * lists of admit usernames under the admitdeny_acl, no
 * ips,hostnames,keys. It's used for the "roles" access promise. TODO convert
 * to a common access promise with resource_type=>"role". */
extern struct acl *roles_acl;                    /* cf-runagent connections */


size_t ReplaceSpecialVariables(char *buf, size_t buf_size,
                               const char *find1, const char *repl1,
                               const char *find2, const char *repl2,
                               const char *find3, const char *repl3);

size_t acl_SortedInsert(struct acl **a, const char *handle);
void   acl_Free(struct acl *a);
void   acl_Summarise(const struct acl *acl, const char *title);

/* TODO instead of getting all kind of different parameters like
 * ipaddr,hostname,key, the following functions should get a
 * "struct peer_id" with all this plus more. */

bool acl_CheckExact(const struct acl *acl, const char *req_string,
                    const char *ipaddr, const char *hostname,
                    const char *key);
bool acl_CheckPath(const struct acl *acl, const char *reqpath,
                   const char *ipaddr, const char *hostname,
                   const char *key);
bool acl_CheckRegex(const struct acl *acl, const char *req_string,
                    const char *ipaddr, const char *hostname,
                    const char *key, const char *username);

#endif
