


#ifndef CFENGINE_ACCESS_H
#define CFENGINE_ACCESS_H


#include <platform.h>

#include <map.h>                                          /* StringMap */
#include "strlist.h"                                      /* struct strlist */


/**
 * Access control list referring to one resource, e.g. path, class, variable,
 * literal.
 *
 * @note: Each strlist might be NULL, which is equivalent to having 0
 *        elements.
 *
 * @note: Currently these lists are binary searched, so after filling them up
 *        make sure you call strlist_Sort() to sort them.
 */
struct resource_acl
{
    struct strlist *admit_ips;
    struct strlist *admit_hostnames;
    struct strlist *admit_keys;
    struct strlist *deny_ips;
    struct strlist *deny_hostnames;
    struct strlist *deny_keys;
};

enum acl_type
{
    ACL_TYPE_PATH,
    ACL_TYPE_CONTEXT,
    ACL_TYPE_VARIABLE,
    ACL_TYPE_LITERAL,
    ACL_TYPE_QUERY
//    ACL_TYPE_ROLES
};

/**
 * This is a list of all resorce ACLs for one resource. E.g. for resource_type
 * == path, this should contain a list of all paths together with a list of
 * ACLs (struct resource_acl) referring to the relevant path.
 *
 * @note Currently this list of resource_names may binary searched so it must
 *       be sorted before being searched.
 *
 * @WARNING Remember to store directories *always* with traling '/', else they
 *          won't match for children dirs (on purpose, and this functionality
 *          was built into strlist_SearchLongestPrefix()).
 */
struct acl
{
//TODO    enum acl_type resource_type;
    size_t len;                        /* Length of the following arrays */
    size_t alloc_len;                  /* Used for realloc() economy  */
    struct strlist *resource_names;    /* paths, class names, variables etc */
    struct resource_acl acls[];
};



/* These acls are set on server startup or when promises change, and are
 * read-only for the rest of their life, thus thread-safe. */
extern struct acl *paths_acl;
extern struct acl *classes_acl, *vars_acl, *literals_acl;
extern struct acl *query_acl;                                  /* reporting */
//extern struct acl *roles_acl;                                /* cf-runagent */

StringMap *path_shortcuts;


size_t ReplaceSpecialVariables(char *buf, size_t buf_size,
                               const char *find1, const char *repl1,
                               const char *find2, const char *repl2,
                               const char *find3, const char *repl3);


bool access_CheckResource(const struct resource_acl *acl,
                          const char *ipaddr, const char *hostname,
                          const char *key);


size_t acl_SortedInsert(struct acl **a, const char *handle);
void acl_Free(struct acl *a);
bool acl_CheckExact(const struct acl *acl, const char *req_string,
                    const char *ipaddr, const char *hostname,
                    const char *key);
bool acl_CheckPath(const struct acl *acl, const char *reqpath,
                   const char *ipaddr, const char *hostname,
                   const char *key);


#endif
