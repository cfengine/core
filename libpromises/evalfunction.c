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

#include "evalfunction.h"

#include "env_context.h"
#include "promises.h"
#include "dir.h"
#include "dbm_api.h"
#include "lastseen.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "vars.h"
#include "addr_lib.h"
#include "syntax.h"
#include "item_lib.h"
#include "conversion.h"
#include "expand.h"
#include "scope.h"
#include "keyring.h"
#include "matching.h"
#include "hashes.h"
#include "unix.h"
#include "logging_old.h"
#include "string_lib.h"
#include "args.h"
#include "client_code.h"
#include "communication.h"
#include "net.h"
#include "pipes.h"
#include "exec_tools.h"
#include "policy.h"
#include "misc_lib.h"
#include "fncall.h"
#include "audit.h"
#include "sort.h"
#include "logging.h"

#include <libgen.h>
#include <assert.h>

/*
 * This module contains numeruous functions which don't use all their parameters
 * (e.g. language-function calls which don't use EvalContext or
 * language-function calls which don't use arguments as language-function does
 * not accept any).
 *
 * Temporarily, in order to avoid cluttering output with thousands of warnings,
 * this module is excempted from producing warnings about unused function
 * parameters.
 *
 * Please remove this #pragma ASAP and provide ARG_UNUSED declarations for
 * unused parameters.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

typedef enum
{
    DATE_TEMPLATE_YEAR,
    DATE_TEMPLATE_MONTH,
    DATE_TEMPLATE_DAY,
    DATE_TEMPLATE_HOUR,
    DATE_TEMPLATE_MIN,
    DATE_TEMPLATE_SEC
} DateTemplate;

static FnCallResult FilterInternal(EvalContext *ctx, FnCall *fp, char *regex, char *name, int do_regex, int invert, long max);

static char *StripPatterns(char *file_buffer, char *pattern, char *filename);
static void CloseStringHole(char *s, int start, int end);
static int BuildLineArray(EvalContext *ctx, const Bundle *bundle, char *array_lval, char *file_buffer, char *split, int maxent, DataType type, int intIndex);
static int ExecModule(EvalContext *ctx, char *command, const char *ns);
static int CheckID(char *id);
static bool GetListReferenceArgument(const EvalContext *ctx, const FnCall *fp, const char *lval_str, Rval *rval_out, DataType *datatype_out);
static void *CfReadFile(char *filename, int maxsize);

/*******************************************************************/

int FnNumArgs(const FnCallType *call_type)
{
    for (int i = 0;; i++)
    {
        if (call_type->args[i].pattern == NULL)
        {
            return i;
        }
    }
}

/*******************************************************************/

/* assume args are all scalar literals by the time we get here
     and each handler allocates the memory it returns. There is
     a protocol to be followed here:
     Set args,
     Eval Content,
     Set rtype,
     ErrorFlags

     returnval = FnCallXXXResult(fp)

  */

/*******************************************************************/
/* End FnCall API                                                  */
/*******************************************************************/

static Rlist *GetHostsFromLastseenDB(Item *addresses, time_t horizon, bool return_address, bool return_recent)
{
    Rlist *recent = NULL, *aged = NULL;
    Item *ip;
    time_t now = time(NULL);
    double entrytime;
    char address[CF_MAXVARSIZE];

    for (ip = addresses; ip != NULL; ip = ip->next)
    {
        if (sscanf(ip->classes, "%lf", &entrytime) != 1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not get host entry age");
            continue;
        }

        if (return_address)
        {
            snprintf(address, sizeof(address), "%s", ip->name);
        }
        else
        {
            char hostname[MAXHOSTNAMELEN];
            if (IPString2Hostname(hostname, ip->name, sizeof(hostname)) != -1)
            {
                snprintf(address, sizeof(address), "%s", hostname);
            }
            else
            {
                /* Not numeric address was requested, but IP was unresolvable. */
                snprintf(address, sizeof(address), "%s", ip->name);
            }
        }

        if (entrytime < now - horizon)
        {
            CfDebug("Old entry.\n");

            if (RlistKeyIn(recent, address))
            {
                CfDebug("There is recent entry for this address. Do nothing.\n");
            }
            else
            {
                CfDebug("Adding to list of aged hosts.\n");
                RlistPrependScalarIdemp(&aged, address);
            }
        }
        else
        {
            Rlist *r;

            CfDebug("Recent entry.\n");

            if ((r = RlistKeyIn(aged, address)))
            {
                CfDebug("Purging from list of aged hosts.\n");
                RlistDestroyEntry(&aged, r);
            }

            CfDebug("Adding to list of recent hosts.\n");
            RlistPrependScalarIdemp(&recent, address);
        }
    }

    if (return_recent)
    {
        RlistDestroy(aged);
        if (recent == NULL)
        {
            RlistAppendScalarIdemp(&recent, CF_NULL_VALUE);
        }
        return recent;
    }
    else
    {
        RlistDestroy(recent);
        if (aged == NULL)
        {
            RlistAppendScalarIdemp(&aged, CF_NULL_VALUE);
        }
        return aged;
    }
}

/*********************************************************************/

static FnCallResult FnCallAnd(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *arg;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall and-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, (Rval) {arg->item, arg->type}, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (!IsDefinedClass(ctx, RlistScalarValue(arg), PromiseGetNamespace(fp->caller)))
        {
            return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
}

/*******************************************************************/

static bool CallHostsSeenCallback(const char *hostkey, const char *address,
                                  bool incoming, const KeyHostSeen *quality,
                                  void *ctx)
{
    Item **addresses = ctx;

    if (HostKeyAddressUnknown(hostkey))
    {
        return true;
    }

    char buf[CF_BUFSIZE];
    snprintf(buf, sizeof(buf), "%ju", (uintmax_t)quality->lastseen);

    PrependItem(addresses, address, buf);

    return true;
}

/*******************************************************************/

static FnCallResult FnCallHostsSeen(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Item *addresses = NULL;

    int horizon = IntFromString(RlistScalarValue(finalargs)) * 3600;
    char *policy = RlistScalarValue(finalargs->next);
    char *format = RlistScalarValue(finalargs->next->next);

    CfDebug("Calling hostsseen(%d,%s,%s)\n", horizon, policy, format);

    if (!ScanLastSeenQuality(&CallHostsSeenCallback, &addresses))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    Rlist *returnlist = GetHostsFromLastseenDB(addresses, horizon,
                                               strcmp(format, "address") == 0,
                                               strcmp(policy, "lastseen") == 0);

    DeleteItemList(addresses);

    CfDebug(" | Return value:\n");
    for (Rlist *rp = returnlist; rp; rp = rp->next)
    {
        CfDebug(" |  %s\n", (char *) rp->item);
    }

    if (returnlist == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
    }
}

/*********************************************************************/

static FnCallResult FnCallHostsWithClass(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *returnlist = NULL;

    char *class_name = RlistScalarValue(finalargs);
    char *return_format = RlistScalarValue(finalargs->next);
    
    if(!CFDB_HostsWithClass(ctx, &returnlist, class_name, return_format))
    {
        return (FnCallResult){ FNCALL_FAILURE };
    }
    
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallRandomInt(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    int tmp, range, result;

    buffer[0] = '\0';

/* begin fn specific content */

    int from = IntFromString(RlistScalarValue(finalargs));
    int to = IntFromString(RlistScalarValue(finalargs->next));

    if (from == CF_NOINT || to == CF_NOINT)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    range = fabs(to - from);
    result = from + (int) (drand48() * (double) range);
    snprintf(buffer, CF_BUFSIZE - 1, "%d", result);

/* end fn specific content */

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetEnv(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE] = "", ctrlstr[CF_SMALLBUF];

/* begin fn specific content */

    char *name = RlistScalarValue(finalargs);
    int limit = IntFromString(RlistScalarValue(finalargs->next));

    snprintf(ctrlstr, CF_SMALLBUF, "%%.%ds", limit);    // -> %45s

    if (getenv(name))
    {
        snprintf(buffer, CF_BUFSIZE - 1, ctrlstr, getenv(name));
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

#if defined(HAVE_GETPWENT)

static FnCallResult FnCallGetUsers(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *newlist = NULL, *except_names, *except_uids;
    struct passwd *pw;

/* begin fn specific content */

    char *except_name = RlistScalarValue(finalargs);
    char *except_uid = RlistScalarValue(finalargs->next);

    except_names = RlistFromSplitString(except_name, ',');
    except_uids = RlistFromSplitString(except_uid, ',');

    setpwent();

    while ((pw = getpwent()))
    {
        if (!RlistIsStringIn(except_names, pw->pw_name) && !RlistIsIntIn(except_uids, (int) pw->pw_uid))
        {
            RlistAppendScalarIdemp(&newlist, pw->pw_name);
        }
    }

    endpwent();

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

#else

static FnCallResult FnCallGetUsers(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    CfOut(OUTPUT_LEVEL_ERROR, "", " -> getusers is not implemented");
    return (FnCallResult) { FNCALL_FAILURE };
}

#endif

/*********************************************************************/

static FnCallResult FnCallEscape(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *name = RlistScalarValue(finalargs);

    EscapeSpecialChars(name, buffer, CF_BUFSIZE - 1, "", "");

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallHost2IP(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *name = RlistScalarValue(finalargs);
    char ipaddr[CF_MAX_IP_LEN];

    if (Hostname2IPString(ipaddr, name, sizeof(ipaddr)) != -1)
    {
        return (FnCallResult) {
            FNCALL_SUCCESS, { xstrdup(ipaddr), RVAL_TYPE_SCALAR }
        };
    }
    else
    {
        /* Retain legacy behaviour,
           return hostname in case resolution fails. */
        return (FnCallResult) {
            FNCALL_SUCCESS, { xstrdup(name), RVAL_TYPE_SCALAR }
        };
    }

}

/*********************************************************************/

static FnCallResult FnCallIP2Host(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char hostname[MAXHOSTNAMELEN];
    char *ip = RlistScalarValue(finalargs);

    if (IPString2Hostname(hostname, ip, sizeof(hostname)) != -1)
    {
        return (FnCallResult) {
            FNCALL_SUCCESS, { xstrdup(hostname), RVAL_TYPE_SCALAR }
        };
    }
    else
    {
        /* Retain legacy behaviour,
           return ip address in case resolution fails. */
        return (FnCallResult) {
            FNCALL_SUCCESS, { xstrdup(ip), RVAL_TYPE_SCALAR }
        };
    }
}

/*********************************************************************/

#ifdef __MINGW32__

static FnCallResult FnCallGetUid(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return (FnCallResult) { FNCALL_FAILURE };
}

#else /* !__MINGW32__ */

static FnCallResult FnCallGetUid(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    struct passwd *pw;

/* begin fn specific content */

    if ((pw = getpwnam(RlistScalarValue(finalargs))) == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        char buffer[CF_BUFSIZE];

        snprintf(buffer, CF_BUFSIZE - 1, "%ju", (uintmax_t)pw->pw_uid);
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
}

#endif /* !__MINGW32__ */

/*********************************************************************/

#ifdef __MINGW32__

static FnCallResult FnCallGetGid(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return (FnCallResult) { FNCALL_FAILURE };
}

#else /* !__MINGW32__ */

static FnCallResult FnCallGetGid(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    struct group *gr;

/* begin fn specific content */

    if ((gr = getgrnam(RlistScalarValue(finalargs))) == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        char buffer[CF_BUFSIZE];

        snprintf(buffer, CF_BUFSIZE - 1, "%ju", (uintmax_t)gr->gr_gid);
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
}

#endif /* __MINGW32__ */

/*********************************************************************/

static FnCallResult FnCallHash(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
/* Hash(string,md5|sha1|crypt) */
{
    char buffer[CF_BUFSIZE];
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    HashMethod type;

    buffer[0] = '\0';

/* begin fn specific content */

    char *string = RlistScalarValue(finalargs);
    char *typestring = RlistScalarValue(finalargs->next);

    type = HashMethodFromString(typestring);

    if (FIPS_MODE && type == HASH_METHOD_MD5)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! FIPS mode is enabled, and md5 is not an approved algorithm in call to hash()");
    }

    HashString(string, strlen(string), digest, type);

    char hashbuffer[EVP_MAX_MD_SIZE * 4];

    snprintf(buffer, CF_BUFSIZE - 1, "%s", HashPrintSafe(type, digest, hashbuffer));

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(SkipHashType(buffer)), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallHashMatch(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
/* HashMatch(string,md5|sha1|crypt,"abdxy98edj") */
{
    char buffer[CF_BUFSIZE], ret[CF_BUFSIZE];
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    HashMethod type;

    buffer[0] = '\0';

/* begin fn specific content */

    char *string = RlistScalarValue(finalargs);
    char *typestring = RlistScalarValue(finalargs->next);
    char *compare = RlistScalarValue(finalargs->next->next);

    type = HashMethodFromString(typestring);
    HashFile(string, digest, type);

    char hashbuffer[EVP_MAX_MD_SIZE * 4];
    snprintf(buffer, CF_BUFSIZE - 1, "%s", HashPrintSafe(type, digest, hashbuffer));
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> File \"%s\" hashes to \"%s\", compare to \"%s\"\n", string, buffer, compare);

    if (strcmp(buffer + 4, compare) == 0)
    {
        strcpy(ret, "any");
    }
    else
    {
        strcpy(ret, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(ret), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallConcat(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *arg = NULL;
    char id[CF_BUFSIZE];
    char result[CF_BUFSIZE] = "";

    snprintf(id, CF_BUFSIZE, "built-in FnCall concat-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, (Rval) {arg->item, arg->type}, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (strlcat(result, RlistScalarValue(arg), CF_BUFSIZE) >= CF_BUFSIZE)
        {
            /* Complain */
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unable to evaluate concat() function, arguments are too long");
            return (FnCallResult) { FNCALL_FAILURE};
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(result), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallClassMatch(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    if (EvalContextHeapMatchCountHard(ctx, RlistScalarValue(finalargs))
        || EvalContextHeapMatchCountSoft(ctx, RlistScalarValue(finalargs))
        || EvalContextStackFrameMatchCountSoft(ctx, RlistScalarValue(finalargs)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallIfElse(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *arg = NULL;
    int argcount = 0;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall ifelse-arg");

    /* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, (Rval) {arg->item, arg->type}, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
        argcount++;
    }

    /* Require an odd number of arguments. We will always return something. */
    if (argcount%2 != 1)
    {
        FatalError(ctx, "in built-in FnCall ifelse: even number of arguments");
    }

    for (arg = finalargs;        /* Start with arg set to finalargs. */
         arg && arg->next;       /* We must have arg and arg->next to proceed. */
         arg = arg->next->next)  /* arg steps forward *twice* every time. */
    {
        /* Similar to classmatch(), we evaluate the first of the two
         * arguments as a class. */
        if (IsDefinedClass(ctx, RlistScalarValue(arg), PromiseGetNamespace(fp->caller)))
        {
            /* If the evaluation returned true in the current context,
             * return the second of the two arguments. */
            return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(RlistScalarValue(arg->next)), RVAL_TYPE_SCALAR } };
        }
    }

    /* If we get here, we've reached the last argument (arg->next is NULL). */
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(RlistScalarValue(arg)), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallCountClassesMatching(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *string = RlistScalarValue(finalargs);
    int count = 0;

    count += EvalContextHeapMatchCountSoft(ctx, string);
    count += EvalContextHeapMatchCountHard(ctx, string);
    count += EvalContextStackFrameMatchCountSoft(ctx, string);

    snprintf(buffer, CF_MAXVARSIZE, "%d", count);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallClassesMatching(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *string = RlistScalarValue(finalargs);
    StringSet* base = StringSetNew();

    EvalContextHeapAddMatchingSoft(ctx, base, string);
    EvalContextHeapAddMatchingHard(ctx, base, string);
    EvalContextStackFrameAddMatchingSoft(ctx, base, string);

    Rlist *returnlist = NULL;
    StringSetIterator it = StringSetIteratorInit(base);
    char *element = NULL;
    while ((element = StringSetIteratorNext(&it)))
    {
        RlistPrependScalar(&returnlist, element);
    }

    if (returnlist == NULL)
    {
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
    }

    StringSetDestroy(base);

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallCanonify(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(CanonifyName(RlistScalarValue(finalargs))), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallLastNode(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist;

/* begin fn specific content */

    char *name = RlistScalarValue(finalargs);
    char *split = RlistScalarValue(finalargs->next);

    newlist = RlistFromSplitRegex(name, split, 100, true);

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        if (rp->next == NULL)
        {
            break;
        }
    }

    if (rp && rp->item)
    {
        char *res = xstrdup(rp->item);

        RlistDestroy(newlist);
        return (FnCallResult) { FNCALL_SUCCESS, { res, RVAL_TYPE_SCALAR } };
    }
    else
    {
        RlistDestroy(newlist);
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*******************************************************************/

static FnCallResult FnCallDirname(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *dir = xstrdup(RlistScalarValue(finalargs));

    DeleteSlash(dir);
    ChopLastNode(dir);

    return (FnCallResult) { FNCALL_SUCCESS, { dir, RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallClassify(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    bool is_defined = IsDefinedClass(ctx, CanonifyName(RlistScalarValue(finalargs)), PromiseGetNamespace(fp->caller));

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(is_defined ? "any" : "!any"), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/
/* Executions                                                        */
/*********************************************************************/

static FnCallResult FnCallReturnsZero(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    if (!IsAbsoluteFileName(RlistScalarValue(finalargs)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "execresult \"%s\" does not have an absolute path\n", RlistScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!IsExecutable(CommandArg0(RlistScalarValue(finalargs))))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "execresult \"%s\" is assumed to be executable but isn't\n", RlistScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    struct stat statbuf;
    char comm[CF_BUFSIZE];
    int useshell = strcmp(RlistScalarValue(finalargs->next), "useshell") == 0;

    snprintf(comm, CF_BUFSIZE, "%s", RlistScalarValue(finalargs));

    if (stat(CommandArg0(RlistScalarValue(finalargs)), &statbuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (ShellCommandReturnsZero(comm, useshell))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallExecResult(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
  /* execresult("/programpath",useshell|noshell) */
{
    if (!IsAbsoluteFileName(RlistScalarValue(finalargs)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "execresult \"%s\" does not have an absolute path\n", RlistScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!IsExecutable(CommandArg0(RlistScalarValue(finalargs))))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "execresult \"%s\" is assumed to be executable but isn't\n", RlistScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    bool useshell = strcmp(RlistScalarValue(finalargs->next), "useshell") == 0;
    char buffer[CF_EXPANDSIZE];

    if (GetExecOutput(RlistScalarValue(finalargs), buffer, useshell))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult FnCallUseModule(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
  /* usemodule("/programpath",varargs) */
{
    char modulecmd[CF_BUFSIZE];
    struct stat statbuf;

/* begin fn specific content */

    char *command = RlistScalarValue(finalargs);
    char *args = RlistScalarValue(finalargs->next);

    snprintf(modulecmd, CF_BUFSIZE, "%s%cmodules%c%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, command);

    if (stat(CommandArg0(modulecmd), &statbuf) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "(Plug-in module %s not found)", modulecmd);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if ((statbuf.st_uid != 0) && (statbuf.st_uid != getuid()))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Module %s was not owned by uid=%ju who is executing agent\n", modulecmd, (uintmax_t)getuid());
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!JoinPath(modulecmd, args))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Culprit: class list for module (shouldn't happen)\n");
        return (FnCallResult) { FNCALL_FAILURE };
    }

    snprintf(modulecmd, CF_BUFSIZE, "%s%cmodules%c%s %s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, command, args);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Executing and using module [%s]\n", modulecmd);

    if (!ExecModule(ctx, modulecmd, PromiseGetNamespace(fp->caller)))
    {
        return (FnCallResult) { FNCALL_FAILURE};
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/
/* Misc                                                              */
/*********************************************************************/

static FnCallResult FnCallSplayClass(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], class[CF_MAXVARSIZE];

    Interval policy = IntervalFromString(RlistScalarValue(finalargs->next));

    if (policy == INTERVAL_HOURLY)
    {
        /* 12 5-minute slots in hour */
        int slot = OatHash(RlistScalarValue(finalargs), CF_HASHTABLESIZE) * 12 / CF_HASHTABLESIZE;
        snprintf(class, CF_MAXVARSIZE, "Min%02d_%02d", slot * 5, ((slot + 1) * 5) % 60);
    }
    else
    {
        /* 12*24 5-minute slots in day */
        int dayslot = OatHash(RlistScalarValue(finalargs), CF_HASHTABLESIZE) * 12 * 24 / CF_HASHTABLESIZE;
        int hour = dayslot / 12;
        int slot = dayslot % 12;

        snprintf(class, CF_MAXVARSIZE, "Min%02d_%02d.Hr%02d", slot * 5, ((slot + 1) * 5) % 60, hour);
    }

    if (IsDefinedClass(ctx, class, PromiseGetNamespace(fp->caller)))
    {
        strcpy(buffer, "any");
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallReadTcp(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
 /* ReadTCP(localhost,80,'GET index.html',1000) */
{
    AgentConnection *conn = NULL;
    char buffer[CF_BUFSIZE];
    int val = 0, n_read = 0;
    short portnum;

    memset(buffer, 0, sizeof(buffer));

/* begin fn specific content */

    char *hostnameip = RlistScalarValue(finalargs);
    char *port = RlistScalarValue(finalargs->next);
    char *sendstring = RlistScalarValue(finalargs->next->next);
    char *maxbytes = RlistScalarValue(finalargs->next->next->next);

    val = IntFromString(maxbytes);
    portnum = (short) IntFromString(port);

    if (val < 0 || portnum < 0 || THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (val > CF_BUFSIZE - 1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Too many bytes to read from TCP port %s@%s", port, hostnameip);
        val = CF_BUFSIZE - CF_BUFFERMARGIN;
    }

    CfDebug("Want to read %d bytes from port %d at %s\n", val, portnum, hostnameip);

    conn = NewAgentConn(hostnameip);

    FileCopy fc = {
        .force_ipv4 = false,
        .portnumber = portnum,
    };

    if (!ServerConnect(conn, hostnameip, fc))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "socket", "Couldn't open a tcp socket");
        DeleteAgentConn(conn);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (strlen(sendstring) > 0)
    {
        if (SendSocketStream(conn->sd, sendstring, strlen(sendstring), 0) == -1)
        {
            cf_closesocket(conn->sd);
            DeleteAgentConn(conn);
            return (FnCallResult) { FNCALL_FAILURE };
        }
    }

    if ((n_read = recv(conn->sd, buffer, val, 0)) == -1)
    {
    }

    if (n_read == -1)
    {
        cf_closesocket(conn->sd);
        DeleteAgentConn(conn);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    cf_closesocket(conn->sd);
    DeleteAgentConn(conn);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegList(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *list;
    char buffer[CF_BUFSIZE], naked[CF_MAXVARSIZE];
    Rval retval;

    buffer[0] = '\0';

/* begin fn specific content */

    char *listvar = RlistScalarValue(finalargs);
    char *regex = RlistScalarValue(finalargs->next);

    if (IsVarList(listvar))
    {
        GetNaked(naked, listvar);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function reglist was promised a list called \"%s\" but this was not found\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!EvalContextVariableGet(ctx, (VarRef) { NULL, PromiseGetBundle(fp->caller)->name, naked }, &retval, NULL))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function REGLIST was promised a list called \"%s\" but this was not found\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (retval.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function reglist was promised a list called \"%s\" but this variable is not a list\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    list = (Rlist *) retval.item;

    strcpy(buffer, "!any");

    for (rp = list; rp != NULL; rp = rp->next)
    {
        if (strcmp(rp->item, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        if (FullTextMatch(regex, rp->item))
        {
            strcpy(buffer, "any");
            break;
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegArray(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char match[CF_MAXVARSIZE], buffer[CF_BUFSIZE];
    Scope *ptr;
    AssocHashTableIterator i;
    CfAssoc *assoc;

/* begin fn specific content */

    char *arrayname = RlistScalarValue(finalargs);
    char *regex = RlistScalarValue(finalargs->next);

/* Locate the array */

    VarRef var = VarRefParse(arrayname);

    if ((ptr = ScopeGet(var.scope)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function regarray was promised an array called \"%s\" but this was not found\n",
              arrayname);
        VarRefDestroy(var);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    strcpy(buffer, "!any");

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        snprintf(match, CF_MAXVARSIZE, "%s[", var.lval);
        if (strncmp(match, assoc->lval, strlen(match)) == 0)
        {
            if (FullTextMatch(regex, assoc->rval.item))
            {
                strcpy(buffer, "any");
                break;
            }
        }
    }

    VarRefDestroy(var);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetIndices(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    char index[CF_MAXVARSIZE], match[CF_MAXVARSIZE];
    Scope *ptr;
    Rlist *returnlist = NULL;
    AssocHashTableIterator i;
    CfAssoc *assoc;

/* begin fn specific content */

    char *arrayname = RlistScalarValue(finalargs);

/* Locate the array */

    if (strstr(arrayname, "."))
    {
        scopeid[0] = '\0';
        sscanf(arrayname, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, arrayname);
        strcpy(scopeid, PromiseGetBundle(fp->caller)->name);
    }

    if ((ptr = ScopeGet(scopeid)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "Function getindices was promised an array called \"%s\" in scope \"%s\" but this was not found\n", lval,
              scopeid);
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
    }

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        snprintf(match, CF_MAXVARSIZE - 1, "%.127s[", lval);

        if (strncmp(match, assoc->lval, strlen(match)) == 0)
        {
            char *sp;

            index[0] = '\0';
            sscanf(assoc->lval + strlen(match), "%127[^\n]", index);
            if ((sp = strchr(index, ']')))
            {
                *sp = '\0';
            }
            else
            {
                index[strlen(index) - 1] = '\0';
            }

            if (strlen(index) > 0)
            {
                RlistAppendScalarIdemp(&returnlist, index);
            }
        }
    }

    if (returnlist == NULL)
    {
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGetValues(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    char match[CF_MAXVARSIZE];
    Scope *ptr;
    Rlist *rp, *returnlist = NULL;
    AssocHashTableIterator i;
    CfAssoc *assoc;

/* begin fn specific content */

    char *arrayname = RlistScalarValue(finalargs);

/* Locate the array */

    if (strstr(arrayname, "."))
    {
        scopeid[0] = '\0';
        sscanf(arrayname, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, arrayname);
        strcpy(scopeid, PromiseGetBundle(fp->caller)->name);
    }

    if ((ptr = ScopeGet(scopeid)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "Function getvalues was promised an array called \"%s\" in scope \"%s\" but this was not found\n", lval,
              scopeid);
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
    }

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        snprintf(match, CF_MAXVARSIZE - 1, "%.127s[", lval);

        if (strncmp(match, assoc->lval, strlen(match)) == 0)
        {
            switch (assoc->rval.type)
            {
            case RVAL_TYPE_SCALAR:
                RlistAppendScalarIdemp(&returnlist, assoc->rval.item);
                break;

            case RVAL_TYPE_LIST:
                for (rp = assoc->rval.item; rp != NULL; rp = rp->next)
                {
                    RlistAppendScalarIdemp(&returnlist, rp->item);
                }
                break;

            default:
                break;
            }
        }
    }

    if (returnlist == NULL)
    {
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGrep(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex
                          RlistScalarValue(finalargs->next), // list identifier
                          1, // regex match = TRUE
                          0, // invert matches = FALSE
                          99999999999); // max results = max int
}

/*********************************************************************/

static FnCallResult FnCallSum(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], buffer[CF_MAXVARSIZE];
    char scopeid[CF_MAXVARSIZE];
    Rval rval2;
    Rlist *rp;
    double sum = 0;

/* begin fn specific content */

    char *name = RlistScalarValue(finalargs);

/* Locate the array */

    if (strstr(name, "."))
    {
        scopeid[0] = '\0';
        sscanf(name, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, name);
        strcpy(scopeid, PromiseGetBundle(fp->caller)->name);
    }

    if (!ScopeExists(scopeid))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"sum\" was promised a list in scope \"%s\" but this was not found\n", scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!EvalContextVariableGet(ctx, (VarRef) { NULL, scopeid, lval }, &rval2, NULL))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"sum\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (rval2.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"sum\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        double x;

        if (!DoubleFromString(rp->item, &x))
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }
        else
        {
            sum += x;
        }
    }

    snprintf(buffer, CF_MAXVARSIZE, "%lf", sum);
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallProduct(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], buffer[CF_MAXVARSIZE];
    char scopeid[CF_MAXVARSIZE];
    Rval rval2;
    Rlist *rp;
    double product = 1.0;

/* begin fn specific content */

    char *name = RlistScalarValue(finalargs);

/* Locate the array */

    if (strstr(name, "."))
    {
        scopeid[0] = '\0';
        sscanf(name, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, name);
        strcpy(scopeid, PromiseGetBundle(fp->caller)->name);
    }

    if (!ScopeExists(scopeid))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"product\" was promised a list in scope \"%s\" but this was not found\n",
              scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!EvalContextVariableGet(ctx, (VarRef) { NULL, scopeid, lval }, &rval2, NULL))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"product\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (rval2.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"product\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        double x;
        if (!DoubleFromString(rp->item, &x))
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }
        else
        {
            product *= x;
        }
    }

    snprintf(buffer, CF_MAXVARSIZE, "%lf", product);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallJoin(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], *joined;
    char scopeid[CF_MAXVARSIZE];
    Rval rval2;
    Rlist *rp;
    int size = 0;

/* begin fn specific content */

    char *join = RlistScalarValue(finalargs);
    char *name = RlistScalarValue(finalargs->next);

/* Locate the array */

    if (strstr(name, "."))
    {
        scopeid[0] = '\0';
        sscanf(name, "%[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, name);
        strcpy(scopeid, "this");
    }

    if (!ScopeExists(scopeid))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"join\" was promised an array in scope \"%s\" but this was not found\n",
              scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!EvalContextVariableGet(ctx, (VarRef) { NULL, scopeid, lval }, &rval2, NULL))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"join\" was promised a list called \"%s.%s\" but this was not (yet) found\n",
              scopeid, name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (rval2.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"join\" was promised a list called \"%s\" but this was not (yet) found\n",
              name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        if (strcmp(rp->item, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        size += strlen(rp->item) + strlen(join);
    }

    joined = xcalloc(1, size + 1);
    size = 0;

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        if (strcmp(rp->item, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        strcpy(joined + size, rp->item);

        if (rp->next != NULL)
        {
            strcpy(joined + size + strlen(rp->item), join);
            size += strlen(rp->item) + strlen(join);
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { joined, RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetFields(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist;
    char name[CF_MAXVARSIZE], retval[CF_SMALLBUF];
    int lcount = 0, vcount = 0, nopurge = true;
    FILE *fin;

/* begin fn specific content */

    char *regex = RlistScalarValue(finalargs);
    char *filename = RlistScalarValue(finalargs->next);
    char *split = RlistScalarValue(finalargs->next->next);
    char *array_lval = RlistScalarValue(finalargs->next->next->next);

    if ((fin = fopen(filename, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", " !! File \"%s\" could not be read in getfields()", filename);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (;;)
    {
        char line[CF_BUFSIZE];

        if (fgets(line, CF_BUFSIZE, fin) == NULL)
        {
            if (ferror(fin))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "fgets", "Unable to read data from file %s", filename);
                fclose(fin);
                return (FnCallResult) { FNCALL_FAILURE };
            }
            else /* feof */
            {
                break;
            }
        }

        if (Chop(line, CF_EXPANDSIZE) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
        }

        if (!FullTextMatch(regex, line))
        {
            continue;
        }

        if (lcount == 0)
        {
            newlist = RlistFromSplitRegex(line, split, 31, nopurge);

            vcount = 1;

            for (rp = newlist; rp != NULL; rp = rp->next)
            {
                snprintf(name, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, vcount);
                ScopeNewScalar(ctx, (VarRef) { NULL, PromiseGetBundle(fp->caller)->name, name }, RlistScalarValue(rp), DATA_TYPE_STRING);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> getfields: defining %s = %s\n", name, RlistScalarValue(rp));
                vcount++;
            }
        }

        lcount++;
    }

    fclose(fin);

    snprintf(retval, CF_SMALLBUF - 1, "%d", lcount);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallCountLinesMatching(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char retval[CF_SMALLBUF];
    int lcount = 0;
    FILE *fin;

/* begin fn specific content */

    char *regex = RlistScalarValue(finalargs);
    char *filename = RlistScalarValue(finalargs->next);

    if ((fin = fopen(filename, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "fopen", " !! File \"%s\" could not be read in countlinesmatching()", filename);
        snprintf(retval, CF_SMALLBUF - 1, "0");
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), RVAL_TYPE_SCALAR } };
    }

    for (;;)
    {
        char line[CF_BUFSIZE];
        if (fgets(line, CF_BUFSIZE, fin) == NULL)
        {
            if (ferror(fin))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "fgets", "Unable to read data from file %s", filename);
                fclose(fin);
                return (FnCallResult) { FNCALL_FAILURE };
            }
            else /* feof */
            {
                break;
            }
        }
        if (Chop(line, CF_EXPANDSIZE) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
        }

        if (FullTextMatch(regex, line))
        {
            lcount++;
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> countlinesmatching: matched \"%s\"", line);
            continue;
        }
    }

    fclose(fin);

    snprintf(retval, CF_SMALLBUF - 1, "%d", lcount);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallLsDir(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char line[CF_BUFSIZE], retval[CF_SMALLBUF];
    Dir *dirh = NULL;
    const struct dirent *dirp;
    Rlist *newlist = NULL;

/* begin fn specific content */

    char *dirname = RlistScalarValue(finalargs);
    char *regex = RlistScalarValue(finalargs->next);
    int includepath = BooleanFromString(RlistScalarValue(finalargs->next->next));

    dirh = DirOpen(dirname);

    if (dirh == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "opendir", " !! Directory \"%s\" could not be accessed in lsdir()", dirname);
        snprintf(retval, CF_SMALLBUF - 1, "0");
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), RVAL_TYPE_SCALAR } };
    }

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (strlen(regex) == 0 || FullTextMatch(regex, dirp->d_name))
        {
            if (includepath)
            {
                snprintf(line, CF_BUFSIZE, "%s/%s", dirname, dirp->d_name);
                MapName(line);
                RlistPrependScalar(&newlist, line);
            }
            else
            {
                RlistPrependScalar(&newlist, (char *) dirp->d_name);
            }
        }
    }

    DirClose(dirh);

    if (newlist == NULL)
    {
        RlistPrependScalar(&newlist, "cf_null");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallMapArray(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char expbuf[CF_EXPANDSIZE], lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    char index[CF_MAXVARSIZE], match[CF_MAXVARSIZE];
    Scope *ptr;
    Rlist *rp, *returnlist = NULL;
    AssocHashTableIterator i;
    CfAssoc *assoc;

/* begin fn specific content */

    char *map = RlistScalarValue(finalargs);
    char *arrayname = RlistScalarValue(finalargs->next);

/* Locate the array */

    if (strstr(arrayname, "."))
    {
        scopeid[0] = '\0';
        sscanf(arrayname, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, arrayname);
        strcpy(scopeid, ScopeGetCurrent()->scope);
    }

    if ((ptr = ScopeGet(scopeid)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "Function maparray was promised an array called \"%s\" in scope \"%s\" but this was not found\n", lval,
              scopeid);
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
        return (FnCallResult) { FNCALL_FAILURE, { returnlist, RVAL_TYPE_LIST } };
    }

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        snprintf(match, CF_MAXVARSIZE - 1, "%.127s[", lval);

        if (strncmp(match, assoc->lval, strlen(match)) == 0)
        {
            char *sp;

            index[0] = '\0';
            sscanf(assoc->lval + strlen(match), "%127[^\n]", index);
            if ((sp = strchr(index, ']')))
            {
                *sp = '\0';
            }
            else
            {
                index[strlen(index) - 1] = '\0';
            }

            if (strlen(index) > 0)
            {
                ScopeNewSpecialScalar(ctx, "this", "k", index, DATA_TYPE_STRING);

                switch (assoc->rval.type)
                {
                case RVAL_TYPE_SCALAR:
                    ScopeNewSpecialScalar(ctx, "this", "v", assoc->rval.item, DATA_TYPE_STRING);
                    ExpandScalar(ctx, PromiseGetBundle(fp->caller)->name, map, expbuf);

                    if (strstr(expbuf, "$(this.k)") || strstr(expbuf, "${this.k}") ||
                        strstr(expbuf, "$(this.v)") || strstr(expbuf, "${this.v}"))
                    {
                        RlistDestroy(returnlist);
                        ScopeDeleteSpecialScalar("this", "k");
                        ScopeDeleteSpecialScalar("this", "v");
                        return (FnCallResult) { FNCALL_FAILURE };
                    }

                    RlistAppendScalar(&returnlist, expbuf);
                    ScopeDeleteSpecialScalar("this", "v");
                    break;

                case RVAL_TYPE_LIST:
                    for (rp = assoc->rval.item; rp != NULL; rp = rp->next)
                    {
                        ScopeNewSpecialScalar(ctx, "this", "v", rp->item, DATA_TYPE_STRING);
                        ExpandScalar(ctx, PromiseGetBundle(fp->caller)->name, map, expbuf);

                        if (strstr(expbuf, "$(this.k)") || strstr(expbuf, "${this.k}") ||
                            strstr(expbuf, "$(this.v)") || strstr(expbuf, "${this.v}"))
                        {
                            RlistDestroy(returnlist);
                            ScopeDeleteSpecialScalar("this", "k");
                            ScopeDeleteSpecialScalar("this", "v");
                            return (FnCallResult) { FNCALL_FAILURE };
                        }

                        RlistAppendScalarIdemp(&returnlist, expbuf);
                        ScopeDeleteSpecialScalar("this", "v");
                    }
                    break;

                default:
                    break;
                }
                ScopeDeleteSpecialScalar("this", "k");
            }
        }
    }

    if (returnlist == NULL)
    {
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallMapList(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char expbuf[CF_EXPANDSIZE], lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    Rlist *rp, *newlist = NULL;
    Rval rval;
    DataType retype;

/* begin fn specific content */

    char *map = RlistScalarValue(finalargs);
    char *listvar = RlistScalarValue(finalargs->next);

/* Locate the array */

    if (*listvar == '@')        // Handle use of @(list) as well as raw name
    {
        listvar += 2;
    }

    if (strstr(listvar, "."))
    {
        scopeid[0] = '\0';
        sscanf(listvar, "%127[^.].%127[^)}]", scopeid, lval);
    }
    else
    {
        strcpy(lval, listvar);

        if (*(lval + strlen(lval) - 1) == ')' || *(lval + strlen(lval) - 1) == '}')
        {
            *(lval + strlen(lval) - 1) = '\0';
        }

        strcpy(scopeid, PromiseGetBundle(fp->caller)->name);
    }

    if (!ScopeExists(scopeid))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function \"maplist\" was promised an list in scope \"%s\" but this was not found\n",
              scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    retype = DATA_TYPE_NONE;
    if (!EvalContextVariableGet(ctx, (VarRef) { NULL, scopeid, lval }, &rval, &retype))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (retype != DATA_TYPE_STRING_LIST && retype != DATA_TYPE_INT_LIST && retype != DATA_TYPE_REAL_LIST)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
    {
        ScopeNewSpecialScalar(ctx, "this", "this", (char *) rp->item, DATA_TYPE_STRING);

        ExpandScalar(ctx, PromiseGetBundle(fp->caller)->name, map, expbuf);

        if (strstr(expbuf, "$(this)") || strstr(expbuf, "${this}"))
        {
            RlistDestroy(newlist);
            return (FnCallResult) { FNCALL_FAILURE };
        }

        RlistAppendScalar(&newlist, expbuf);
        ScopeDeleteSpecialScalar("this", "this");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallSelectServers(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
 /* ReadTCP(localhost,80,'GET index.html',1000) */
{
    AgentConnection *conn = NULL;
    Rlist *hostnameip;
    char buffer[CF_BUFSIZE], naked[CF_MAXVARSIZE];
    int val = 0, n_read = 0, count = 0;
    short portnum;
    Rval retval;
    buffer[0] = '\0';

/* begin fn specific content */

    char *listvar = RlistScalarValue(finalargs);
    char *port = RlistScalarValue(finalargs->next);
    char *sendstring = RlistScalarValue(finalargs->next->next);
    char *regex = RlistScalarValue(finalargs->next->next->next);
    char *maxbytes = RlistScalarValue(finalargs->next->next->next->next);
    char *array_lval = RlistScalarValue(finalargs->next->next->next->next->next);

    if (IsVarList(listvar))
    {
        GetNaked(naked, listvar);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function selectservers was promised a list called \"%s\" but this was not found\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!EvalContextVariableGet(ctx, (VarRef) { NULL, PromiseGetBundle(fp->caller)->name, naked }, &retval, NULL))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "Function selectservers was promised a list called \"%s\" but this was not found from context %s.%s\n",
              listvar, PromiseGetBundle(fp->caller)->name, naked);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (retval.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "Function selectservers was promised a list called \"%s\" but this variable is not a list\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    hostnameip = RvalRlistValue(retval);
    val = IntFromString(maxbytes);
    portnum = (short) IntFromString(port);

    if (val < 0 || portnum < 0)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (val > CF_BUFSIZE - 1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Too many bytes specificed in selectservers");
        val = CF_BUFSIZE - CF_BUFFERMARGIN;
    }

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        snprintf(buffer, CF_MAXVARSIZE - 1, "%d", count);
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }

    Policy *select_server_policy = PolicyNew();
    Promise *pp = NULL;
    {
        Bundle *bp = PolicyAppendBundle(select_server_policy, NamespaceDefault(), "select_server_bundle", "agent", NULL, NULL);
        PromiseType *tp = BundleAppendPromiseType(bp, "select_server");

        pp = PromiseTypeAppendPromise(tp, "function", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, NULL);
    }

    assert(pp);

    for (Rlist *rp = hostnameip; rp != NULL; rp = rp->next)
    {
        CfDebug("Want to read %d bytes from port %d at %s\n", val, portnum, (char *) rp->item);

        conn = NewAgentConn(RlistScalarValue(rp));

        FileCopy fc = {
            .force_ipv4 = false,
            .portnumber = portnum,
        };

        if (!ServerConnect(conn, rp->item, fc))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "socket", "Couldn't open a tcp socket");
            DeleteAgentConn(conn);
            continue;
        }

        if (strlen(sendstring) > 0)
        {
            if (SendSocketStream(conn->sd, sendstring, strlen(sendstring), 0) == -1)
            {
                cf_closesocket(conn->sd);
                DeleteAgentConn(conn);
                continue;
            }

            if ((n_read = recv(conn->sd, buffer, val, 0)) == -1)
            {
            }

            if (n_read == -1)
            {
                cf_closesocket(conn->sd);
                DeleteAgentConn(conn);
                continue;
            }

            if (strlen(regex) == 0 || FullTextMatch(regex, buffer))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s is alive and responding correctly\n", RlistScalarValue(rp));
                snprintf(buffer, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, count);
                ScopeNewScalar(ctx, (VarRef) { NULL, PromiseGetBundle(fp->caller)->name, buffer }, rp->item, DATA_TYPE_STRING);
                count++;
            }
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host %s is alive\n", RlistScalarValue(rp));
            snprintf(buffer, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, count);
            ScopeNewScalar(ctx, (VarRef) { NULL, PromiseGetBundle(fp->caller)->name, buffer }, rp->item, DATA_TYPE_STRING);

            if (IsDefinedClass(ctx, CanonifyName(rp->item), PromiseGetNamespace(fp->caller)))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "This host is in the list and has promised to join the class %s - joined\n",
                      array_lval);
                EvalContextHeapAddSoft(ctx, array_lval, PromiseGetNamespace(fp->caller));
            }

            count++;
        }

        cf_closesocket(conn->sd);
        DeleteAgentConn(conn);
    }

    PolicyDestroy(select_server_policy);

/* Return the subset that is alive and responding correctly */

/* Return the number of lines in array */

    snprintf(buffer, CF_MAXVARSIZE - 1, "%d", count);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}


static FnCallResult FnCallShuffle(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *seed_str = RlistScalarValue(finalargs->next);

    Rval list_rval;
    DataType list_dtype = DATA_TYPE_NONE;

    if (!GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), &list_rval, &list_dtype))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (list_dtype != DATA_TYPE_STRING_LIST)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' expected a variable that resolves to a string list, got '%s'", fp->name, DataTypeToString(list_dtype));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    Seq *seq = SeqNew(1000, NULL);
    for (const Rlist *rp = list_rval.item; rp; rp = rp->next)
    {
        SeqAppend(seq, rp->item);
    }

    SeqShuffle(seq, OatHash(seed_str, RAND_MAX));

    Rlist *shuffled = NULL;
    for (size_t i = 0; i < SeqLength(seq); i++)
    {
        RlistPrependScalar(&shuffled, xstrdup(SeqAt(seq, i)));
    }

    SeqDestroy(seq);
    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { shuffled, RVAL_TYPE_LIST } };
}


static FnCallResult FnCallIsNewerThan(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (stat(RlistScalarValue(finalargs), &frombuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE};
    }

    if (frombuf.st_mtime > tobuf.st_mtime)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallIsAccessedBefore(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (stat(RlistScalarValue(finalargs), &frombuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE};
    }

    if (frombuf.st_atime < tobuf.st_atime)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallIsChangedBefore(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (stat(RlistScalarValue(finalargs), &frombuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else if (stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (frombuf.st_ctime > tobuf.st_ctime)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallFileStat(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *path = RlistScalarValue(finalargs);
    struct stat statbuf;

    buffer[0] = '\0';

/* begin fn specific content */

    if (lstat(path, &statbuf) == -1)
    {
        if (!strcmp(fp->name, "filesize"))
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        strcpy(buffer, "!any");
    }
    else
    {
        strcpy(buffer, "!any");

        if (!strcmp(fp->name, "isexecutable"))
        {
            if (IsExecutable(path))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "isdir"))
        {
            if (S_ISDIR(statbuf.st_mode))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "islink"))
        {
            if (S_ISLNK(statbuf.st_mode))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "isplain"))
        {
            if (S_ISREG(statbuf.st_mode))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "fileexists"))
        {
            strcpy(buffer, "any");
        }
        else if (!strcmp(fp->name, "filesize"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_size);
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallFileStatDetails(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *path = RlistScalarValue(finalargs);
    char *detail = RlistScalarValue(finalargs->next);
    struct stat statbuf;

    buffer[0] = '\0';

/* begin fn specific content */

    if (lstat(path, &statbuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
    else
    {
        if (!strcmp(detail, "size"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_size);
        }
        else if (!strcmp(detail, "gid"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_gid);
        }
        else if (!strcmp(detail, "uid"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_uid);
        }
        else if (!strcmp(detail, "ino"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_ino);
        }
        else if (!strcmp(detail, "nlink"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_nlink);
        }
        else if (!strcmp(detail, "ctime"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_ctime);
        }
        else if (!strcmp(detail, "mtime"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_mtime);
        }
        else if (!strcmp(detail, "atime"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_atime);
        }
        else if (!strcmp(detail, "permstr"))
        {
        #if !defined(__MINGW32__)
            snprintf(buffer, CF_MAXVARSIZE,
                     "%c%c%c%c%c%c%c%c%c%c",
                     S_ISDIR(statbuf.st_mode) ? 'd' : '-',
                     (statbuf.st_mode & S_IRUSR) ? 'r' : '-',
                     (statbuf.st_mode & S_IWUSR) ? 'w' : '-',
                     (statbuf.st_mode & S_IXUSR) ? 'x' : '-',
                     (statbuf.st_mode & S_IRGRP) ? 'r' : '-',
                     (statbuf.st_mode & S_IWGRP) ? 'w' : '-',
                     (statbuf.st_mode & S_IXGRP) ? 'x' : '-',
                     (statbuf.st_mode & S_IROTH) ? 'r' : '-',
                     (statbuf.st_mode & S_IWOTH) ? 'w' : '-',
                     (statbuf.st_mode & S_IXOTH) ? 'x' : '-');
        #else
            snprintf(buffer, CF_MAXVARSIZE, "Not available on Windows");
        #endif
        }
        else if (!strcmp(detail, "permoct"))
        {
        #if !defined(__MINGW32__)
            snprintf(buffer, CF_MAXVARSIZE, "%jo", (uintmax_t) (statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)));
        #else
            snprintf(buffer, CF_MAXVARSIZE, "Not available on Windows");
        #endif
        }
        else if (!strcmp(detail, "modeoct"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jo", (uintmax_t) statbuf.st_mode);
        }
        else if (!strcmp(detail, "mode"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_mode);
        }
        else if (!strcmp(detail, "type"))
        {
        #if !defined(__MINGW32__)
          switch (statbuf.st_mode & S_IFMT)
          {
          case S_IFBLK:  snprintf(buffer, CF_MAXVARSIZE, "%s", "block device");     break;
          case S_IFCHR:  snprintf(buffer, CF_MAXVARSIZE, "%s", "character device"); break;
          case S_IFDIR:  snprintf(buffer, CF_MAXVARSIZE, "%s", "directory");        break;
          case S_IFIFO:  snprintf(buffer, CF_MAXVARSIZE, "%s", "FIFO/pipe");        break;
          case S_IFLNK:  snprintf(buffer, CF_MAXVARSIZE, "%s", "symlink");          break;
          case S_IFREG:  snprintf(buffer, CF_MAXVARSIZE, "%s", "regular file");     break;
          case S_IFSOCK: snprintf(buffer, CF_MAXVARSIZE, "%s", "socket");           break;
          default:       snprintf(buffer, CF_MAXVARSIZE, "%s", "unknown");          break;
          }
        #else
            snprintf(buffer, CF_MAXVARSIZE, "Not available on Windows");
        #endif
        }
        else if (!strcmp(detail, "dev_minor"))
        {
        #if !defined(__MINGW32__)
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) minor(statbuf.st_dev) );
        #else
            snprintf(buffer, CF_MAXVARSIZE, "Not available on Windows");
        #endif
        }
        else if (!strcmp(detail, "dev_major"))
        {
        #if !defined(__MINGW32__)
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) major(statbuf.st_dev) );
        #else
            snprintf(buffer, CF_MAXVARSIZE, "Not available on Windows");
        #endif
        }
        else if (!strcmp(detail, "devno"))
        {
        #if !defined(__MINGW32__)
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_dev );
        #else
            snprintf(buffer, CF_MAXVARSIZE, "%c:", statbuf.st_dev + 'A');
        #endif
        }
        else if (!strcmp(detail, "dirname"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%s", path);
            ChopLastNode(buffer);
        }
        else if (!strcmp(detail, "basename"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%s", ReadLastNode(path));
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallFilter(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex or string
                          RlistScalarValue(finalargs->next), // list identifier
                          BooleanFromString(RlistScalarValue(finalargs->next->next)), // match as regex or exactly
                          BooleanFromString(RlistScalarValue(finalargs->next->next->next)), // invert matches
                          IntFromString(RlistScalarValue(finalargs->next->next->next->next))); // max results
}

/*********************************************************************/

static bool GetListReferenceArgument(const EvalContext *ctx, const FnCall *fp, const char *lval_str, Rval *rval_out, DataType *datatype_out)
{
    VarRef list_var_lval = VarRefParseFromBundle(lval_str, PromiseGetBundle(fp->caller));

    if (!EvalContextVariableGet(ctx, list_var_lval, rval_out, datatype_out))
    {
        Log(LOG_LEVEL_ERR, "Could not resolve expected list variable '%s' in function '%s'", lval_str, fp->name);
        return false;
    }

    if (rval_out->type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function '%s' expected a list variable reference, got variable of type '%s'", fp->name, DataTypeToString(*datatype_out));
        return false;
    }

    return true;
}

/*********************************************************************/

static FnCallResult FilterInternal(EvalContext *ctx, FnCall *fp, char *regex, char *name, int do_regex, int invert, long max)
{
    Rval rval2;
    Rlist *rp, *returnlist = NULL;

    if (!GetListReferenceArgument(ctx, fp, name, &rval2, NULL))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    RlistAppendScalar(&returnlist, CF_NULL_VALUE);

    long match_count = 0;
    long total = 0;
    for (rp = (Rlist *) rval2.item; rp != NULL && match_count < max; rp = rp->next)
    {
        int found = do_regex ? FullTextMatch(regex, rp->item) : (0==strcmp(regex, rp->item));

        CfDebug("%s() called: %s %s %s\n", fp->name, (char*) rp->item, found ? "matches" : "does not match", regex);

        if (invert ? !found : found)
        {
            RlistAppendScalar(&returnlist, rp->item);
            match_count++;

            // exit early in case "some" is being called
            if (0 == strcmp(fp->name, "some"))
            {
                break;
            }
        }
        // exit early in case "none" is being called
        else if (0 == strcmp(fp->name, "every"))
        {
            total++; // we just 
            break;
        }

        total++;
    }

    bool contextmode = 0;
    bool ret;
    if (0 == strcmp(fp->name, "every"))
    {
        contextmode = 1;
        ret = (match_count == total);
    }
    else if (0 == strcmp(fp->name, "none"))
    {
        contextmode = 1;
        ret = (match_count == 0);
    }
    else if (0 == strcmp(fp->name, "some"))
    {
        contextmode = 1;
        ret = (match_count > 0);
    }
    else if (0 != strcmp(fp->name, "grep") && 0 != strcmp(fp->name, "filter"))
    {
        contextmode = -1;
        ret = -1;
        FatalError(ctx, "in built-in FnCall %s: unhandled FilterInternal() contextmode", fp->name);
    }

    if (contextmode)
    {
        CfDebug("%s() called: found %ld matches for %s; tested %ld\n", fp->name, match_count, regex, total);
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(ret ? "any" : "!any"), RVAL_TYPE_SCALAR } };
    }

    // else, return the list itself
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallSublist(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs); // list identifier
    bool head = 0 == strcmp(RlistScalarValue(finalargs->next), "head"); // heads or tails
    long max = IntFromString(RlistScalarValue(finalargs->next->next)); // max results

    Rval rval2;
    Rlist *rp, *returnlist = NULL;

    if (!GetListReferenceArgument(ctx, fp, name, &rval2, NULL))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    RlistAppendScalar(&returnlist, CF_NULL_VALUE);

    if (head)
    {
        long count = 0;
        for (rp = (Rlist *) rval2.item; rp != NULL && count < max; rp = rp->next)
        {
            RlistAppendScalar(&returnlist, rp->item);
            count++;
        }
    }
    else if (max > 0) // tail mode
    {
        rp = (Rlist *) rval2.item;
        int length = RlistLen((const Rlist *) rp);

        int offset = max >= length ? 0 : length-max;

        for (; rp != NULL && offset--; rp = rp->next);

        for (; rp != NULL; rp = rp->next)
        {
            RlistAppendScalar(&returnlist, rp->item);
        }

    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallUniq(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs);

    Rval rval2;
    Rlist *rp, *returnlist = NULL;

    if (!GetListReferenceArgument(ctx, fp, name, &rval2, NULL))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    RlistAppendScalar(&returnlist, CF_NULL_VALUE);

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        RlistAppendScalarIdemp(&returnlist, rp->item);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallNth(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs);
    long offset = IntFromString(RlistScalarValue(finalargs->next)); // offset

    Rval rval2;
    Rlist *rp = NULL;

    if (!GetListReferenceArgument(ctx, fp, name, &rval2, NULL))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval2.item; rp != NULL && offset--; rp = rp->next);

    if (NULL == rp) return (FnCallResult) { FNCALL_FAILURE };

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(rp->item), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallEverySomeNone(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex or string
                          RlistScalarValue(finalargs->next), // list identifier
                          1,
                          0,
                          99999999999);
}

static FnCallResult FnCallSort(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    VarRef list_var_lval = VarRefParseFromBundle(RlistScalarValue(finalargs), PromiseGetBundle(fp->caller));
    Rval list_var_rval;
    DataType list_var_dtype = DATA_TYPE_NONE;

    if (!EvalContextVariableGet(ctx, list_var_lval, &list_var_rval, &list_var_dtype))
    {
        VarRefDestroy(list_var_lval);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    VarRefDestroy(list_var_lval);

    if (list_var_dtype != DATA_TYPE_STRING_LIST)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    Rlist *sorted = AlphaSortRListNames(RlistCopy(RvalRlistValue(list_var_rval)));

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { sorted, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallIPRange(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *range = RlistScalarValue(finalargs);
    Item *ip;

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, "!any");

    if (!FuzzyMatchParse(range))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (ip = IPADDRESSES; ip != NULL; ip = ip->next)
    {
        CfDebug("Checking IP Range against RDNS %s\n", VIPADDRESS);

        if (FuzzySetMatch(range, VIPADDRESS) == 0)
        {
            CfDebug("IPRange Matched\n");
            strcpy(buffer, "any");
            break;
        }
        else
        {
            CfDebug("Checking IP Range against iface %s\n", ip->name);

            if (FuzzySetMatch(range, ip->name) == 0)
            {
                CfDebug("IPRange Matched\n");
                strcpy(buffer, "any");
                break;
            }
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallHostRange(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *prefix = RlistScalarValue(finalargs);
    char *range = RlistScalarValue(finalargs->next);

    strcpy(buffer, "!any");

    if (!FuzzyHostParse(range))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (FuzzyHostMatch(prefix, range, VUQNAME) == 0)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

FnCallResult FnCallHostInNetgroup(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    char *host, *user, *domain;

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, "!any");

    setnetgrent(RlistScalarValue(finalargs));

    while (getnetgrent(&host, &user, &domain))
    {
        if (host == NULL)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Matched %s in netgroup %s\n", VFQNAME, RlistScalarValue(finalargs));
            strcpy(buffer, "any");
            break;
        }

        if (strcmp(host, VFQNAME) == 0 || strcmp(host, VUQNAME) == 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Matched %s in netgroup %s\n", host, RlistScalarValue(finalargs));
            strcpy(buffer, "any");
            break;
        }
    }

    endnetgrent();

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIsVariable(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *lval = RlistScalarValue(finalargs);
    Rval rval = { 0 };
    bool found = false;

    if (lval == NULL)
    {
        found = false;
    }
    else
    {
        found = EvalContextVariableGet(ctx, (VarRef) { NULL, "this", lval }, &rval, NULL);
    }

    if (found)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallStrCmp(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    if (strcmp(RlistScalarValue(finalargs), RlistScalarValue(finalargs->next)) == 0)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallTranslatePath(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[MAX_FILENAME];

    buffer[0] = '\0';

/* begin fn specific content */

    snprintf(buffer, sizeof(buffer), "%s", RlistScalarValue(finalargs));
    MapName(buffer);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

#if defined(__MINGW32__)

static FnCallResult FnCallRegistryValue(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE] = "";

    if (GetRegistryValue(RlistScalarValue(finalargs), RlistScalarValue(finalargs->next), buffer, sizeof(buffer)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    return (FnCallResult) { FNCALL_FAILURE };
}

#else /* !__MINGW32__ */

static FnCallResult FnCallRegistryValue(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return (FnCallResult) { FNCALL_FAILURE };
}

#endif /* !__MINGW32__ */

/*********************************************************************/

static FnCallResult FnCallRemoteScalar(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *handle = RlistScalarValue(finalargs);
    char *server = RlistScalarValue(finalargs->next);
    int encrypted = BooleanFromString(RlistScalarValue(finalargs->next->next));

    if (strcmp(server, "localhost") == 0)
    {
        /* The only reason for this is testing... */
        server = "127.0.0.1";
    }

    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("<remote scalar>"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        GetRemoteScalar(ctx, "VAR", handle, server, encrypted, buffer);

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            if (!RetrieveUnreliableValue("remotescalar", handle, buffer))
            {
                // This function should never fail
                buffer[0] = '\0';
            }
        }
        else
        {
            CacheUnreliableValue("remotescalar", handle, buffer);
        }

        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallHubKnowledge(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *handle = RlistScalarValue(finalargs);

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("<inaccessible remote scalar>"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Accessing hub knowledge bank for \"%s\"", handle);
        GetRemoteScalar(ctx, "VAR", handle, POLICY_SERVER, true, buffer);

        // This should always be successful - and this one doesn't cache

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            snprintf(buffer, CF_MAXVARSIZE, "0");
        }

        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallRemoteClassesMatching(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *classlist;
    char buffer[CF_BUFSIZE], class[CF_MAXVARSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *regex = RlistScalarValue(finalargs);
    char *server = RlistScalarValue(finalargs->next);
    int encrypted = BooleanFromString(RlistScalarValue(finalargs->next->next));
    char *prefix = RlistScalarValue(finalargs->next->next->next);

    if (strcmp(server, "localhost") == 0)
    {
        /* The only reason for this is testing... */
        server = "127.0.0.1";
    }

    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("remote_classes"), RVAL_TYPE_SCALAR } };
    }
    else
    {
        GetRemoteScalar(ctx, "CONTEXT", regex, server, encrypted, buffer);

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        if ((classlist = RlistFromSplitString(buffer, ',')))
        {
            for (rp = classlist; rp != NULL; rp = rp->next)
            {
                snprintf(class, CF_MAXVARSIZE - 1, "%s_%s", prefix, (char *) rp->item);
                EvalContextStackFrameAddSoft(ctx, class);
            }
            RlistDestroy(classlist);
        }

        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallPeers(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist, *pruned;
    char *split = "\n";
    char *file_buffer = NULL;
    int i, found, maxent = 100000, maxsize = 100000;

/* begin fn specific content */

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    int groupsize = IntFromString(RlistScalarValue(finalargs->next->next));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    file_buffer = StripPatterns(file_buffer, comment, filename);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { NULL, RVAL_TYPE_LIST } };
    }
    else
    {
        newlist = RlistFromSplitRegex(file_buffer, split, maxent, true);
    }

/* Slice up the list and discard everything except our slice */

    i = 0;
    found = false;
    pruned = NULL;

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        char s[CF_MAXVARSIZE];

        if (EmptyString(rp->item))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(rp->item, "%s", s);

        if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
        {
            found = true;
        }
        else
        {
            RlistPrependScalar(&pruned, s);
        }

        if (i++ % groupsize == groupsize - 1)
        {
            if (found)
            {
                break;
            }
            else
            {
                RlistDestroy(pruned);
                pruned = NULL;
            }
        }
    }

    RlistDestroy(newlist);
    free(file_buffer);

    if (pruned)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { pruned, RVAL_TYPE_LIST } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult FnCallPeerLeader(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist;
    char *split = "\n";
    char *file_buffer = NULL, buffer[CF_MAXVARSIZE];
    int i, found, maxent = 100000, maxsize = 100000;

    buffer[0] = '\0';

/* begin fn specific content */

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    int groupsize = IntFromString(RlistScalarValue(finalargs->next->next));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer == NULL)
        {
            return (FnCallResult) { FNCALL_SUCCESS, { NULL, RVAL_TYPE_LIST } };
        }
        else
        {
            newlist = RlistFromSplitRegex(file_buffer, split, maxent, true);
        }
    }

/* Slice up the list and discard everything except our slice */

    i = 0;
    found = false;
    buffer[0] = '\0';

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        char s[CF_MAXVARSIZE];

        if (EmptyString(rp->item))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(rp->item, "%s", s);

        if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
        {
            found = true;
        }

        if (i % groupsize == 0)
        {
            if (found)
            {
                if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
                {
                    strncpy(buffer, "localhost", CF_MAXVARSIZE - 1);
                }
                else
                {
                    strncpy(buffer, s, CF_MAXVARSIZE - 1);
                }
                break;
            }
        }

        i++;
    }

    RlistDestroy(newlist);
    free(file_buffer);

    if (strlen(buffer) > 0)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallPeerLeaders(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist, *pruned;
    char *split = "\n";
    char *file_buffer = NULL;
    int i, maxent = 100000, maxsize = 100000;

/* begin fn specific content */

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    int groupsize = IntFromString(RlistScalarValue(finalargs->next->next));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    file_buffer = StripPatterns(file_buffer, comment, filename);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { NULL, RVAL_TYPE_LIST } };
    }

    newlist = RlistFromSplitRegex(file_buffer, split, maxent, true);

/* Slice up the list and discard everything except our slice */

    i = 0;
    pruned = NULL;

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        char s[CF_MAXVARSIZE];

        if (EmptyString(rp->item))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(rp->item, "%s", s);

        if (i % groupsize == 0)
        {
            if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
            {
                RlistPrependScalar(&pruned, "localhost");
            }
            else
            {
                RlistPrependScalar(&pruned, s);
            }
        }

        i++;
    }

    RlistDestroy(newlist);
    free(file_buffer);

    if (pruned)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { pruned, RVAL_TYPE_LIST } };
    }
    else
    {
        free(file_buffer);
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallRegCmp(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);
    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);

    if (FullTextMatch(argv0, argv1))
    {
        strcpy(buffer, "any");
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegExtract(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    Scope *ptr;

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);
    char *regex = RlistScalarValue(finalargs);
    char *data = RlistScalarValue(finalargs->next);
    char *arrayname = RlistScalarValue(finalargs->next->next);

    if (FullTextMatch(regex, data))
    {
        strcpy(buffer, "any");
    }
    else
    {
        strcpy(buffer, "!any");
    }

    ptr = ScopeGet("match");

    if (ptr && ptr->hashtable)
    {
        AssocHashTableIterator i = HashIteratorInit(ptr->hashtable);
        CfAssoc *assoc;

        while ((assoc = HashIteratorNext(&i)))
        {
            char var[CF_MAXVARSIZE];

            if (assoc->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      " !! Software error: pattern match was non-scalar in regextract (shouldn't happen)");
                return (FnCallResult) { FNCALL_FAILURE };
            }
            else
            {
                snprintf(var, CF_MAXVARSIZE - 1, "%s[%s]", arrayname, assoc->lval);
                ScopeNewScalar(ctx, (VarRef) { NULL, PromiseGetBundle(fp->caller)->name, var }, assoc->rval.item, DATA_TYPE_STRING);
            }
        }
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegLine(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    FILE *fin;

    buffer[0] = '\0';

/* begin fn specific content */

    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);

    strcpy(buffer, "!any");

    if ((fin = fopen(argv1, "r")) != NULL)
    {
        for (;;)
        {
            char line[CF_BUFSIZE];

            if (fgets(line, CF_BUFSIZE, fin) == NULL)
            {
                if (ferror(fin))
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to read from the file %s", argv1);
                    fclose(fin);
                    return (FnCallResult) { FNCALL_FAILURE };
                }
                else /* feof */
                {
                    break;
                }
            }

            if (Chop(line, CF_EXPANDSIZE) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
            }

            if (FullTextMatch(argv0, line))
            {
                strcpy(buffer, "any");
                break;
            }
        }

        fclose(fin);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIsLessGreaterThan(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);

    if (IsRealNumber(argv0) && IsRealNumber(argv1))
    {
        double a = 0;
        if (!DoubleFromString(argv0, &a))
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }
        double b = 0;
        if (!DoubleFromString(argv1, &b))
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        CfDebug("%s and %s are numerical\n", argv0, argv1);

        if (!strcmp(fp->name, "isgreaterthan"))
        {
            if (a > b)
            {
                strcpy(buffer, "any");
            }
            else
            {
                strcpy(buffer, "!any");
            }
        }
        else
        {
            if (a < b)
            {
                strcpy(buffer, "any");
            }
            else
            {
                strcpy(buffer, "!any");
            }
        }
    }
    else if (strcmp(argv0, argv1) > 0)
    {
        CfDebug("%s and %s are NOT numerical\n", argv0, argv1);

        if (!strcmp(fp->name, "isgreaterthan"))
        {
            strcpy(buffer, "any");
        }
        else
        {
            strcpy(buffer, "!any");
        }
    }
    else
    {
        if (!strcmp(fp->name, "isgreaterthan"))
        {
            strcpy(buffer, "!any");
        }
        else
        {
            strcpy(buffer, "any");
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIRange(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    long tmp;

    buffer[0] = '\0';

/* begin fn specific content */

    long from = IntFromString(RlistScalarValue(finalargs));
    long to = IntFromString(RlistScalarValue(finalargs->next));

    if (from == CF_NOINT || to == CF_NOINT)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    snprintf(buffer, CF_BUFSIZE - 1, "%ld,%ld", from, to);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRRange(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    int tmp;

    buffer[0] = '\0';

/* begin fn specific content */

    double from = 0;
    if (!DoubleFromString(RlistScalarValue(finalargs), &from))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Error reading assumed real value %s => %lf\n", (char *) (finalargs->item), from);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    double to = 0;
    if (!DoubleFromString(RlistScalarValue(finalargs), &to))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Error reading assumed real value %s => %lf\n", (char *) (finalargs->next->item), from);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    snprintf(buffer, CF_BUFSIZE - 1, "%lf,%lf", from, to);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

static FnCallResult FnCallReverse(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rval list_rval;
    DataType list_dtype = DATA_TYPE_NONE;

    if (!GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), &list_rval, &list_dtype))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (list_dtype != DATA_TYPE_STRING_LIST)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' expected a variable that resolves to a string list, got '%s'", fp->name, DataTypeToString(list_dtype));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    Rlist *copy = RlistCopy(RvalRlistValue(list_rval));
    RlistReverse(&copy);

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { copy, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallOn(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    long d[6];
    time_t cftime;
    struct tm tmv;
    DateTemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = IntFromString(RlistScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    tmv.tm_year = d[DATE_TEMPLATE_YEAR] - 1900;
    tmv.tm_mon = d[DATE_TEMPLATE_MONTH] - 1;
    tmv.tm_mday = d[DATE_TEMPLATE_DAY];
    tmv.tm_hour = d[DATE_TEMPLATE_HOUR];
    tmv.tm_min = d[DATE_TEMPLATE_MIN];
    tmv.tm_sec = d[DATE_TEMPLATE_SEC];
    tmv.tm_isdst = -1;

    if ((cftime = mktime(&tmv)) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Illegal time value");
    }

    CfDebug("Time computed from input was: %s\n", ctime(&cftime));

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallOr(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *arg;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall or-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, (Rval) {arg->item, arg->type}, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (IsDefinedClass(ctx, RlistScalarValue(arg), PromiseGetNamespace(fp->caller)))
        {
            return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), RVAL_TYPE_SCALAR } };
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallLaterThan(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    long d[6];
    time_t cftime, now = time(NULL);
    struct tm tmv;
    DateTemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = IntFromString(RlistScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    tmv.tm_year = d[DATE_TEMPLATE_YEAR] - 1900;
    tmv.tm_mon = d[DATE_TEMPLATE_MONTH] - 1;
    tmv.tm_mday = d[DATE_TEMPLATE_DAY];
    tmv.tm_hour = d[DATE_TEMPLATE_HOUR];
    tmv.tm_min = d[DATE_TEMPLATE_MIN];
    tmv.tm_sec = d[DATE_TEMPLATE_SEC];
    tmv.tm_isdst = -1;

    if ((cftime = mktime(&tmv)) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Illegal time value");
    }

    CfDebug("Time computed from input was: %s\n", ctime(&cftime));

    if (now > cftime)
    {
        strcpy(buffer, CF_ANYCLASS);
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallAgoDate(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    time_t cftime;
    long d[6];
    DateTemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = IntFromString(RlistScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    cftime = CFSTARTTIME;
    cftime -= d[DATE_TEMPLATE_SEC];
    cftime -= d[DATE_TEMPLATE_MIN] * 60;
    cftime -= d[DATE_TEMPLATE_HOUR] * 3600;
    cftime -= d[DATE_TEMPLATE_DAY] * 24 * 3600;
    cftime -= Months2Seconds(d[DATE_TEMPLATE_MONTH]);
    cftime -= d[DATE_TEMPLATE_YEAR] * 365 * 24 * 3600;

    CfDebug("Total negative offset = %.1f minutes\n", (double) (CFSTARTTIME - cftime) / 60.0);
    CfDebug("Time computed from input was: %s\n", ctime(&cftime));

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    if (cftime < 0)
    {
        CfDebug("AGO overflowed, truncating at zero\n");
        strcpy(buffer, "0");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallAccumulatedDate(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    long d[6], cftime;
    DateTemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = IntFromString(RlistScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    cftime = 0;
    cftime += d[DATE_TEMPLATE_SEC];
    cftime += d[DATE_TEMPLATE_MIN] * 60;
    cftime += d[DATE_TEMPLATE_HOUR] * 3600;
    cftime += d[DATE_TEMPLATE_DAY] * 24 * 3600;
    cftime += d[DATE_TEMPLATE_MONTH] * 30 * 24 * 3600;
    cftime += d[DATE_TEMPLATE_YEAR] * 365 * 24 * 3600;

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallNot(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(IsDefinedClass(ctx, RlistScalarValue(finalargs), PromiseGetNamespace(fp->caller)) ? "!any" : "any"), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallNow(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    time_t cftime;

    buffer[0] = '\0';

/* begin fn specific content */

    cftime = CFSTARTTIME;

    CfDebug("Time computed from input was: %s\n", ctime(&cftime));

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", (long) cftime);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallStrftime(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    /* begin fn specific content */

    char *mode = RlistScalarValue(finalargs);
    char *format_string = RlistScalarValue(finalargs->next);
    // this will be a problem on 32-bit systems...
    const time_t when = IntFromString(RlistScalarValue(finalargs->next->next));

    char buffer[CF_BUFSIZE];
    buffer[0] = '\0';

    struct tm* tm;

    if (0 == strcmp("gmtime", mode))
    {
        tm = gmtime(&when);
    }
    else
    {
        tm = localtime(&when);
    }

    if(tm != NULL)
    {
        strftime(buffer, sizeof buffer, format_string, tm);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_INFORM, "strftime", "The given time stamp %ld was invalid", when);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/
/* Read functions                                                    */
/*********************************************************************/

static FnCallResult FnCallReadFile(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *contents;

/* begin fn specific content */

    char *filename = RlistScalarValue(finalargs);
    int maxsize = IntFromString(RlistScalarValue(finalargs->next));

// Read once to validate structure of file in itemlist

    CfDebug("Read string data from file %s (up to %d)\n", filename, maxsize);

    contents = CfReadFile(filename, maxsize);

    if (contents)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { contents, RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult ReadList(EvalContext *ctx, FnCall *fp, Rlist *finalargs, DataType type)
{
    Rlist *rp, *newlist = NULL;
    char fnname[CF_MAXVARSIZE], *file_buffer = NULL;
    int noerrors = true, blanks = false;

/* begin fn specific content */

    /* 5args: filename,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    char *split = RlistScalarValue(finalargs->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next));

// Read once to validate structure of file in itemlist

    CfDebug("Read string data from file %s\n", filename);
    snprintf(fnname, CF_MAXVARSIZE - 1, "read%slist", DataTypeToString(type));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer == NULL)
        {
            return (FnCallResult) { FNCALL_SUCCESS, { NULL, RVAL_TYPE_LIST } };
        }
        else
        {
            newlist = RlistFromSplitRegex(file_buffer, split, maxent, blanks);
        }
    }

    switch (type)
    {
    case DATA_TYPE_STRING:
        break;

    case DATA_TYPE_INT:
        for (rp = newlist; rp != NULL; rp = rp->next)
        {
            if (IntFromString(RlistScalarValue(rp)) == CF_NOINT)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Presumed int value \"%s\" read from file %s has no recognizable value",
                      RlistScalarValue(rp), filename);
                noerrors = false;
            }
        }
        break;

    case DATA_TYPE_REAL:
        for (rp = newlist; rp != NULL; rp = rp->next)
        {
            double real_value = 0;
            if (!DoubleFromString(RlistScalarValue(rp), &real_value))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Presumed real value \"%s\" read from file %s has no recognizable value",
                      RlistScalarValue(rp), filename);
                noerrors = false;
            }
        }
        break;

    default:
        ProgrammingError("Unhandled type in switch: %d", type);
    }

    free(file_buffer);

    if (newlist && noerrors)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

static FnCallResult FnCallReadStringList(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ReadList(ctx, fp, args, DATA_TYPE_STRING);
}

static FnCallResult FnCallReadIntList(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ReadList(ctx, fp, args, DATA_TYPE_INT);
}

static FnCallResult FnCallReadRealList(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ReadList(ctx, fp, args, DATA_TYPE_REAL);
}

/*********************************************************************/

static FnCallResult ReadArray(EvalContext *ctx, FnCall *fp, Rlist *finalargs, DataType type, int intIndex)
/* lval,filename,separator,comment,Max number of bytes  */
{
    char fnname[CF_MAXVARSIZE], *file_buffer = NULL;
    int entries = 0;

    /* Arg validation */

    if (intIndex)
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarrayidx", DataTypeToString(type));
    }
    else
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarray", DataTypeToString(type));
    }

/* begin fn specific content */

    /* 6 args: array_lval,filename,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *array_lval = RlistScalarValue(finalargs);
    char *filename = RlistScalarValue(finalargs->next);
    char *comment = RlistScalarValue(finalargs->next->next);
    char *split = RlistScalarValue(finalargs->next->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next->next));

// Read once to validate structure of file in itemlist

    CfDebug("Read string data from file %s - , maxent %d, maxsize %d\n", filename, maxent, maxsize);

    file_buffer = (char *) CfReadFile(filename, maxsize);

    CfDebug("FILE: %s\n", file_buffer);

    if (file_buffer == NULL)
    {
        entries = 0;
    }
    else
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer == NULL)
        {
            entries = 0;
        }
        else
        {
            entries = BuildLineArray(ctx, PromiseGetBundle(fp->caller), array_lval, file_buffer, split, maxent, type, intIndex);
        }
    }

    switch (type)
    {
    case DATA_TYPE_STRING:
    case DATA_TYPE_INT:
    case DATA_TYPE_REAL:
        break;

    default:
        ProgrammingError("Unhandled type in switch: %d", type);
    }

    free(file_buffer);

/* Return the number of lines in array */

    snprintf(fnname, CF_MAXVARSIZE - 1, "%d", entries);
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(fnname), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallReadStringArray(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ReadArray(ctx, fp, args, DATA_TYPE_STRING, false);
}

/*********************************************************************/

static FnCallResult FnCallReadStringArrayIndex(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ReadArray(ctx, fp, args, DATA_TYPE_STRING, true);
}

/*********************************************************************/

static FnCallResult FnCallReadIntArray(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ReadArray(ctx, fp, args, DATA_TYPE_INT, false);
}

/*********************************************************************/

static FnCallResult FnCallReadRealArray(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ReadArray(ctx, fp, args, DATA_TYPE_REAL, false);
}

/*********************************************************************/

static FnCallResult ParseArray(EvalContext *ctx, FnCall *fp, Rlist *finalargs, DataType type, int intIndex)
/* lval,filename,separator,comment,Max number of bytes  */
{
    char fnname[CF_MAXVARSIZE];
    int entries = 0;

    /* Arg validation */

    if (intIndex)
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarrayidx", DataTypeToString(type));
    }
    else
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarray", DataTypeToString(type));
    }

/* begin fn specific content */

    /* 6 args: array_lval,instring,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *array_lval = RlistScalarValue(finalargs);
    char *instring = xstrdup(RlistScalarValue(finalargs->next));
    char *comment = RlistScalarValue(finalargs->next->next);
    char *split = RlistScalarValue(finalargs->next->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next->next));

// Read once to validate structure of file in itemlist

    CfDebug("Parse string data from string %s - , maxent %d, maxsize %d\n", instring, maxent, maxsize);

    if (instring == NULL)
    {
        entries = 0;
    }
    else
    {
        instring = StripPatterns(instring, comment, "string argument 2");

        if (instring == NULL)
        {
            entries = 0;
        }
        else
        {
            entries = BuildLineArray(ctx, PromiseGetBundle(fp->caller), array_lval, instring, split, maxent, type, intIndex);
        }
    }

    switch (type)
    {
    case DATA_TYPE_STRING:
    case DATA_TYPE_INT:
    case DATA_TYPE_REAL:
        break;

    default:
        ProgrammingError("Unhandled type in switch: %d", type);
    }

    free(instring);

/* Return the number of lines in array */

    snprintf(fnname, CF_MAXVARSIZE - 1, "%d", entries);
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(fnname), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallParseStringArray(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ParseArray(ctx, fp, args, DATA_TYPE_STRING, false);
}

/*********************************************************************/

static FnCallResult FnCallParseStringArrayIndex(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ParseArray(ctx, fp, args, DATA_TYPE_STRING, true);
}

/*********************************************************************/

static FnCallResult FnCallParseIntArray(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ParseArray(ctx, fp, args, DATA_TYPE_INT, false);
}

/*********************************************************************/

static FnCallResult FnCallParseRealArray(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    return ParseArray(ctx, fp, args, DATA_TYPE_REAL, false);
}

/*********************************************************************/

static FnCallResult FnCallSplitString(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *newlist = NULL;

/* begin fn specific content */

    /* 2args: string,split_regex,max  */

    char *string = RlistScalarValue(finalargs);
    char *split = RlistScalarValue(finalargs->next);
    int max = IntFromString(RlistScalarValue(finalargs->next->next));

// Read once to validate structure of file in itemlist

    newlist = RlistFromSplitRegex(string, split, max, true);

    if (newlist == NULL)
    {
        RlistPrependScalar(&newlist, "cf_null");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFileSexist(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *files;
    char buffer[CF_BUFSIZE], naked[CF_MAXVARSIZE];
    Rval retval;
    struct stat sb;

    buffer[0] = '\0';

/* begin fn specific content */

    char *listvar = RlistScalarValue(finalargs);

    if (IsVarList(listvar))
    {
        GetNaked(naked, listvar);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function filesexist was promised a list called \"%s\" but this was not found\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!EvalContextVariableGet(ctx, (VarRef) { NULL, PromiseGetBundle(fp->caller)->name, naked }, &retval, NULL))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function filesexist was promised a list called \"%s\" but this was not found\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (retval.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Function filesexist was promised a list called \"%s\" but this variable is not a list\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    files = (Rlist *) retval.item;

    strcpy(buffer, "any");

    for (rp = files; rp != NULL; rp = rp->next)
    {
        if (stat(rp->item, &sb) == -1)
        {
            strcpy(buffer, "!any");
            break;
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/
/* LDAP Nova features                                                */
/*********************************************************************/

static FnCallResult FnCallLDAPValue(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], handle[CF_BUFSIZE];
    void *newval = NULL;

/* begin fn specific content */

    char *uri = RlistScalarValue(finalargs);
    char *dn = RlistScalarValue(finalargs->next);
    char *filter = RlistScalarValue(finalargs->next->next);
    char *name = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next);

    snprintf(handle, CF_BUFSIZE, "%s_%s_%s_%s", dn, filter, name, scope);

    if ((newval = CfLDAPValue(uri, dn, filter, name, scope, sec)))
    {
        CacheUnreliableValue("ldapvalue", handle, newval);
    }
    else
    {
        if (RetrieveUnreliableValue("ldapvalue", handle, buffer))
        {
            newval = xstrdup(buffer);
        }
    }

    if (newval)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult FnCallLDAPArray(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    void *newval;

/* begin fn specific content */

    char *array = RlistScalarValue(finalargs);
    char *uri = RlistScalarValue(finalargs->next);
    char *dn = RlistScalarValue(finalargs->next->next);
    char *filter = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next);

    if ((newval = CfLDAPArray(ctx, PromiseGetBundle(fp->caller), array, uri, dn, filter, scope, sec)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallLDAPList(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    void *newval;

/* begin fn specific content */

    char *uri = RlistScalarValue(finalargs);
    char *dn = RlistScalarValue(finalargs->next);
    char *filter = RlistScalarValue(finalargs->next->next);
    char *name = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next);

    if ((newval = CfLDAPList(uri, dn, filter, name, scope, sec)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, RVAL_TYPE_LIST } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallRegLDAP(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    void *newval;

/* begin fn specific content */

    char *uri = RlistScalarValue(finalargs);
    char *dn = RlistScalarValue(finalargs->next);
    char *filter = RlistScalarValue(finalargs->next->next);
    char *name = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *regex = RlistScalarValue(finalargs->next->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next->next);

    if ((newval = CfRegLDAP(uri, dn, filter, name, scope, regex, sec)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, RVAL_TYPE_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

#define KILOBYTE 1024

static FnCallResult FnCallDiskFree(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    off_t df;

    buffer[0] = '\0';

    df = GetDiskUsage(RlistScalarValue(finalargs), cfabs);

    if (df == CF_INFINITY)
    {
        df = 0;
    }

    /* Result is in kilobytes */
    snprintf(buffer, CF_BUFSIZE - 1, "%jd", ((intmax_t) df) / KILOBYTE);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

#if !defined(__MINGW32__)

FnCallResult FnCallUserExists(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    struct passwd *pw;
    uid_t uid = CF_SAME_OWNER;
    char *arg = RlistScalarValue(finalargs);

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);

    if (IsNumber(arg))
    {
        uid = Str2Uid(arg, NULL, NULL);

        if (uid == CF_SAME_OWNER || uid == CF_UNKNOWN_OWNER)
        {
            return (FnCallResult){ FNCALL_FAILURE };
        }

        if ((pw = getpwuid(uid)) == NULL)
        {
            strcpy(buffer, "!any");
        }
    }
    else if ((pw = getpwnam(arg)) == NULL)
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

FnCallResult FnCallGroupExists(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    struct group *gr;
    gid_t gid = CF_SAME_GROUP;
    char *arg = RlistScalarValue(finalargs);

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);

    if (isdigit((int) *arg))
    {
        gid = Str2Gid(arg, NULL, NULL);

        if (gid == CF_SAME_GROUP || gid == CF_UNKNOWN_GROUP)
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        if ((gr = getgrgid(gid)) == NULL)
        {
            strcpy(buffer, "!any");
        }
    }
    else if ((gr = getgrnam(arg)) == NULL)
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

#endif /* !defined(__MINGW32__) */

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void *CfReadFile(char *filename, int maxsize)
{
    struct stat sb;
    char *result = NULL;
    FILE *fp;
    size_t size;
    int i, newlines = 0;

    if (stat(filename, &sb) == -1)
    {
        if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
        {
            CfDebug("Could not examine file %s in readfile on this system", filename);
        }
        else
        {
            if (IsCf3VarString(filename))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Cannot converge/reduce variable \"%s\" yet .. assuming it will resolve later",
                      filename);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_INFORM, "stat", " !! Could not examine file \"%s\" in readfile", filename);
            }
        }
        return NULL;
    }

    if (sb.st_size > maxsize)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Truncating long file %s in readfile to max limit %d", filename, maxsize);
        size = maxsize;
    }
    else
    {
        size = sb.st_size;
    }

    result = xmalloc(size + 1);

    if ((fp = fopen(filename, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "fopen", "Could not open file \"%s\" in readfile", filename);
        free(result);
        return NULL;
    }

    result[size] = '\0';

    if (size > 0)
    {
        if (fread(result, size, 1, fp) != 1)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "fread", "Could not read expected amount from file %s in readfile", filename);
            fclose(fp);
            free(result);
            return NULL;
        }

        for (i = 0; i < size - 1; i++)
        {
            if (result[i] == '\n' || result[i] == '\r')
            {
                newlines++;
            }
        }

        if (newlines == 0 && (result[size - 1] == '\n' || result[size - 1] == '\r'))
        {
            result[size - 1] = '\0';
        }
    }

    fclose(fp);
    return (void *) result;
}

/*********************************************************************/

static char *StripPatterns(char *file_buffer, char *pattern, char *filename)
{
    int start, end;
    int count = 0;

    if (!NULL_OR_EMPTY(pattern))
    {
        while (BlockTextMatch(pattern, file_buffer, &start, &end))
        {
            CloseStringHole(file_buffer, start, end);

            if (count++ > strlen(file_buffer))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      " !! Comment regex \"%s\" was irreconcilable reading input \"%s\" probably because it legally matches nothing",
                      pattern, filename);
                return file_buffer;
            }
        }
    }

    return file_buffer;
}

/*********************************************************************/

static void CloseStringHole(char *s, int start, int end)
{
    int off = end - start;
    char *sp;

    if (off <= 0)
    {
        return;
    }

    for (sp = s + start; *(sp + off) != '\0'; sp++)
    {
        *sp = *(sp + off);
    }

    *sp = '\0';
}

/*********************************************************************/

static int BuildLineArray(EvalContext *ctx, const Bundle *bundle, char *array_lval, char *file_buffer, char *split, int maxent, DataType type,
                          int intIndex)
{
    char *sp, linebuf[CF_BUFSIZE], name[CF_MAXVARSIZE], first_one[CF_MAXVARSIZE];
    Rlist *rp, *newlist = NULL;
    int allowblanks = true, vcount, hcount, lcount = 0;
    int lineLen;

    memset(linebuf, 0, CF_BUFSIZE);
    hcount = 0;

    for (sp = file_buffer; hcount < maxent && *sp != '\0'; sp++)
    {
        linebuf[0] = '\0';
        sscanf(sp, "%1023[^\n]", linebuf);

        lineLen = strlen(linebuf);

        if (lineLen == 0)
        {
            continue;
        }
        else if (lineLen == 1 && linebuf[0] == '\r')
        {
            continue;
        }

        if (linebuf[lineLen - 1] == '\r')
        {
            linebuf[lineLen - 1] = '\0';
        }

        if (lcount++ > CF_HASHTABLESIZE)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Array is too big to be read into Cfengine (max 4000)");
            break;
        }

        newlist = RlistFromSplitRegex(linebuf, split, maxent, allowblanks);

        vcount = 0;
        first_one[0] = '\0';

        for (rp = newlist; rp != NULL; rp = rp->next)
        {
            char this_rval[CF_MAXVARSIZE];
            long ival;

            switch (type)
            {
            case DATA_TYPE_STRING:
                strncpy(this_rval, rp->item, CF_MAXVARSIZE - 1);
                break;

            case DATA_TYPE_INT:
                ival = IntFromString(rp->item);
                snprintf(this_rval, CF_MAXVARSIZE, "%d", (int) ival);
                break;

            case DATA_TYPE_REAL:
                {
                    double real_value = 0;
                    if (!DoubleFromString(rp->item, &real_value))
                    {
                        FatalError(ctx, "Could not convert rval to double");
                    }
                }
                sscanf(rp->item, "%255s", this_rval);
                break;

            default:
                ProgrammingError("Unhandled type in switch: %d", type);
            }

            if (strlen(first_one) == 0)
            {
                strncpy(first_one, this_rval, CF_MAXVARSIZE - 1);
            }

            if (intIndex)
            {
                snprintf(name, CF_MAXVARSIZE, "%s[%d][%d]", array_lval, hcount, vcount);
            }
            else
            {
                snprintf(name, CF_MAXVARSIZE, "%s[%s][%d]", array_lval, first_one, vcount);
            }

            ScopeNewScalar(ctx, (VarRef) { NULL, bundle->name, name }, this_rval, type);
            vcount++;
        }

        RlistDestroy(newlist);

        hcount++;
        sp += lineLen;

        if (*sp == '\0')        // either \n or \0
        {
            break;
        }
    }

/* Don't free data - goes into vars */

    return hcount;
}

/*********************************************************************/

static int ExecModule(EvalContext *ctx, char *command, const char *ns)
{
    FILE *pp;
    char *sp, line[CF_BUFSIZE];
    int print = false;

    if ((pp = cf_popen(command, "r", true)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "Couldn't open pipe from %s\n", command);
        return false;
    }

    for (;;)
    {
        ssize_t res = CfReadLine(line, CF_BUFSIZE, pp);

        if (res == 0)
        {
            break;
        }

        if (res == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fread", "Unable to read output from %s", command);
            cf_pclose(pp);
            return false;
        }

        if (strlen(line) > CF_BUFSIZE - 80)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Line from module %s is too long to be sensible\n", command);
            break;
        }

        print = false;

        for (sp = line; *sp != '\0'; sp++)
        {
            if (!isspace((int) *sp))
            {
                print = true;
                break;
            }
        }

        ModuleProtocol(ctx, command, line, print, ns);
    }

    cf_pclose(pp);
    return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void ModuleProtocol(EvalContext *ctx, char *command, char *line, int print, const char *ns)
{
    char name[CF_BUFSIZE], content[CF_BUFSIZE], context[CF_BUFSIZE];
    char arg0[CF_BUFSIZE];
    char *filename;

/* Infer namespace from script name */

    snprintf(arg0, CF_BUFSIZE, "%s", CommandArg0(command));
    filename = basename(arg0);

/* Canonicalize filename into acceptable namespace name*/

    CanonifyNameInPlace(filename);
    strcpy(context, filename);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Module context: %s\n", context);

    name[0] = '\0';
    content[0] = '\0';

    switch (*line)
    {
    case '+':
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Activated classes: %s\n", line + 1);
        if (CheckID(line + 1))
        {
             EvalContextHeapAddSoft(ctx, line + 1, ns);
        }
        break;
    case '-':
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Deactivated classes: %s\n", line + 1);
        if (CheckID(line + 1))
        {
            if (line[1] != '\0')
            {
                StringSet *negated = StringSetFromString(line + 1, ',');
                StringSetIterator it = StringSetIteratorInit(negated);
                const char *negated_context = NULL;
                while ((negated_context = StringSetIteratorNext(&it)))
                {
                    if (EvalContextHeapContainsHard(ctx, negated_context))
                    {
                        FatalError(ctx, "Cannot negate the reserved class [%s]\n", negated_context);
                    }

                    EvalContextHeapAddNegated(ctx, negated_context);
                }
                StringSetDestroy(negated);
            }
        }
        break;
    case '=':
        content[0] = '\0';
        sscanf(line + 1, "%[^=]=%[^\n]", name, content);

        if (CheckID(name))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Defined variable: %s in context %s with value: %s\n", name, context, content);
            ScopeNewScalar(ctx, (VarRef) { NULL, context, name }, content, DATA_TYPE_STRING);
        }
        break;

    case '@':
        content[0] = '\0';
        sscanf(line + 1, "%[^=]=%[^\n]", name, content);

        if (CheckID(name))
        {
            Rlist *list = NULL;

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Defined variable: %s in context %s with value: %s\n", name, context, content);
            list = RlistParseShown(content);
            ScopeNewList(ctx, (VarRef) { NULL, context, name }, list, DATA_TYPE_STRING_LIST);
        }
        break;

    case '\0':
        break;

    default:
        if (print)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "M \"%s\": %s\n", command, line);
        }
        break;
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static int CheckID(char *id)
{
    char *sp;

    for (sp = id; *sp != '\0'; sp++)
    {
        if (!isalnum((int) *sp) && (*sp != '.') && (*sp != '-') && (*sp != '_') && (*sp != '[') && (*sp != ']'))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "",
                  "Module protocol contained an illegal character \'%c\' in class/variable identifier \'%s\'.", *sp,
                  id);
            return false;
        }
    }

    return true;
}

/*********************************************************************/

FnCallResult CallFunction(EvalContext *ctx, const FnCallType *function, FnCall *fp, Rlist *expargs)
{
    ArgTemplate(ctx, fp, function->args, expargs);
    return (*function->impl) (ctx, fp, expargs);
}

/*********************************************************/
/* Function prototypes                                   */
/*********************************************************/

FnCallArg ACCESSEDBEFORE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Newer filename"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Older filename"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg ACCUM_ARGS[] =
{
    {"0,1000", DATA_TYPE_INT, "Years"},
    {"0,1000", DATA_TYPE_INT, "Months"},
    {"0,1000", DATA_TYPE_INT, "Days"},
    {"0,1000", DATA_TYPE_INT, "Hours"},
    {"0,1000", DATA_TYPE_INT, "Minutes"},
    {"0,40000", DATA_TYPE_INT, "Seconds"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg AND_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg AGO_ARGS[] =
{
    {"0,1000", DATA_TYPE_INT, "Years"},
    {"0,1000", DATA_TYPE_INT, "Months"},
    {"0,1000", DATA_TYPE_INT, "Days"},
    {"0,1000", DATA_TYPE_INT, "Hours"},
    {"0,1000", DATA_TYPE_INT, "Minutes"},
    {"0,40000", DATA_TYPE_INT, "Seconds"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg LATERTHAN_ARGS[] =
{
    {"0,1000", DATA_TYPE_INT, "Years"},
    {"0,1000", DATA_TYPE_INT, "Months"},
    {"0,1000", DATA_TYPE_INT, "Days"},
    {"0,1000", DATA_TYPE_INT, "Hours"},
    {"0,1000", DATA_TYPE_INT, "Minutes"},
    {"0,40000", DATA_TYPE_INT, "Seconds"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg CANONIFY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "String containing non-identifier characters"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg CHANGEDBEFORE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Newer filename"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Older filename"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg CLASSIFY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg CLASSMATCH_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg CONCAT_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg COUNTCLASSESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg COUNTLINESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Filename"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg DIRNAME_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "File path"},
    {NULL, DATA_TYPE_NONE, NULL},
};

FnCallArg DISKFREE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File system directory"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg ESCAPE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "IP address or string to escape"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg EXECRESULT_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Fully qualified command path"},
    {"useshell,noshell", DATA_TYPE_OPTION, "Shell encapsulation option"},
    {NULL, DATA_TYPE_NONE, NULL}
};

// fileexists, isdir,isplain,islink

FnCallArg FILESTAT_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File object name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg FILESTAT_DETAIL_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File object name"},
    {"size,gid,uid,ino,nlink,ctime,atime,mtime,mode,modeoct,permstr,permoct,type,devno,dev_minor,dev_major,basename,dirname", DATA_TYPE_OPTION, "stat() field to get"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg FILESEXIST_ARGS[] =
{
    {CF_NAKEDLRANGE, DATA_TYPE_STRING, "Array identifier containing list"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg FILTER_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression or string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_BOOL, DATA_TYPE_OPTION, "Match as regular expression if true, as exact string otherwise"},
    {CF_BOOL, DATA_TYPE_OPTION, "Invert matches"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of matches to return"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GETFIELDS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression to match line"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Filename to read"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression to split fields"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Return array name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GETINDICES_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Cfengine array identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GETUSERS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comma separated list of User names"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comma separated list of UserID numbers"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GETENV_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Name of environment variable"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of characters to read "},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GETGID_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Group name in text"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GETUID_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "User name in text"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GREP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg GROUPEXISTS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Group name or identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HASH_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input text"},
    {"md5,sha1,sha256,sha512,sha384,crypt", DATA_TYPE_OPTION, "Hash or digest algorithm"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HASHMATCH_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Filename to hash"},
    {"md5,sha1,crypt,cf_sha224,cf_sha256,cf_sha384,cf_sha512", DATA_TYPE_OPTION, "Hash or digest algorithm"},
    {CF_IDRANGE, DATA_TYPE_STRING, "ASCII representation of hash for comparison"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HOST2IP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Host name in ascii"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg IP2HOST_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "IP address (IPv4 or IPv6)"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HOSTINNETGROUP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Netgroup name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HOSTRANGE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Hostname prefix"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Enumerated range"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HOSTSSEEN_ARGS[] =
{
    {CF_VALRANGE, DATA_TYPE_INT, "Horizon since last seen in hours"},
    {"lastseen,notseen", DATA_TYPE_OPTION, "Complements for selection policy"},
    {"name,address", DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HOSTSWITHCLASS_ARGS[] =
{
    {"[a-zA-Z0-9_]+", DATA_TYPE_STRING, "Class name to look for"},
    {"name,address", DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg IFELSE_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg IPRANGE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "IP address range syntax"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg IRANGE_ARGS[] =
{
    {CF_INTRANGE, DATA_TYPE_INT, "Integer"},
    {CF_INTRANGE, DATA_TYPE_INT, "Integer"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg ISGREATERTHAN_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Larger string or value"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Smaller string or value"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg ISLESSTHAN_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Smaller string or value"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Larger string or value"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg ISNEWERTHAN_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Newer file name"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Older file name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg ISVARIABLE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Variable identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg JOIN_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Join glue-string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg LASTNODE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Link separator, e.g. /,:"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg LDAPARRAY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Array name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filter"},
    {"subtree,onelevel,base", DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", DATA_TYPE_OPTION, "Security level"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg LDAPLIST_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", DATA_TYPE_OPTION, "Security level"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg LDAPVALUE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", DATA_TYPE_OPTION, "Security level"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg LSDIRLIST_ARGS[] =
{
    {CF_PATHRANGE, DATA_TYPE_STRING, "Path to base directory"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression to match files or blank"},
    {CF_BOOL, DATA_TYPE_OPTION, "Include the base path in the list"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg MAPLIST_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Pattern based on $(this) as original text"},
    {CF_IDRANGE, DATA_TYPE_STRING, "The name of the list variable to map"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg MAPARRAY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Pattern based on $(this.k) and $(this.v) as original text"},
    {CF_IDRANGE, DATA_TYPE_STRING, "The name of the array variable to map"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg NOT_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Class value"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg NOW_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg OR_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg SUM_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "A list of arbitrary real values"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg PRODUCT_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "A list of arbitrary real values"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg DATE_ARGS[] =
{
    {"1970,3000", DATA_TYPE_INT, "Year"},
    {"1,12", DATA_TYPE_INT, "Month"},
    {"1,31", DATA_TYPE_INT, "Day"},
    {"0,23", DATA_TYPE_INT, "Hour"},
    {"0,59", DATA_TYPE_INT, "Minute"},
    {"0,59", DATA_TYPE_INT, "Second"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg PEERS_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comment regex pattern"},
    {CF_VALRANGE, DATA_TYPE_INT, "Peer group size"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg PEERLEADER_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comment regex pattern"},
    {CF_VALRANGE, DATA_TYPE_INT, "Peer group size"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg PEERLEADERS_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comment regex pattern"},
    {CF_VALRANGE, DATA_TYPE_INT, "Peer group size"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg RANDOMINT_ARGS[] =
{
    {CF_INTRANGE, DATA_TYPE_INT, "Lower inclusive bound"},
    {CF_INTRANGE, DATA_TYPE_INT, "Upper inclusive bound"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg READFILE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg READSTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name to read"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg PARSESTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "A string to parse for input data"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg READSTRINGARRAYIDX_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "A string to parse for input data"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg PARSESTRINGARRAYIDX_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "A string to parse for input data"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg READSTRINGLIST_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name to read"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg READTCP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Host name or IP address of server socket"},
    {CF_VALRANGE, DATA_TYPE_INT, "Port number"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Protocol query string"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REGARRAY_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Cfengine array identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REGCMP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Match string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REGEXTRACT_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Match string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "Identifier for back-references"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REGISTRYVALUE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Windows registry key"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Windows registry value-id"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REGLINE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filename to search"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REGLIST_ARGS[] =
{
    {CF_NAKEDLRANGE, DATA_TYPE_STRING, "Cfengine list identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REGLDAP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", DATA_TYPE_OPTION, "Search scope policy"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to match results"},
    {"none,ssl,sasl", DATA_TYPE_OPTION, "Security level"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REMOTESCALAR_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Variable identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Hostname or IP address of server"},
    {CF_BOOL, DATA_TYPE_OPTION, "Use enryption"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg HUB_KNOWLEDGE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Variable identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REMOTECLASSESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Server name or address"},
    {CF_BOOL, DATA_TYPE_OPTION, "Use encryption"},
    {CF_IDRANGE, DATA_TYPE_STRING, "Return class prefix"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg RETURNSZERO_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Fully qualified command path"},
    {"useshell,noshell", DATA_TYPE_OPTION, "Shell encapsulation option"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg RRANGE_ARGS[] =
{
    {CF_REALRANGE, DATA_TYPE_REAL, "Real number"},
    {CF_REALRANGE, DATA_TYPE_REAL, "Real number"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg SELECTSERVERS_ARGS[] =
{
    {CF_NAKEDLRANGE, DATA_TYPE_STRING, "The identifier of a cfengine list of hosts or addresses to contact"},
    {CF_VALRANGE, DATA_TYPE_INT, "The port number"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "A query string"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "A regular expression to match success"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of bytes to read from server"},
    {CF_IDRANGE, DATA_TYPE_STRING, "Name for array of results"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg SPLAYCLASS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string for classification"},
    {"daily,hourly", DATA_TYPE_OPTION, "Splay time policy"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg SPLITSTRING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "A data string"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split on"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of pieces"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg STRCMP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "String"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "String"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg STRFTIME_ARGS[] =
{
    {"gmtime,localtime", DATA_TYPE_OPTION, "Use GMT or local time"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "A format string"},
    {CF_VALRANGE, DATA_TYPE_INT, "The time as a Unix epoch offset"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg SUBLIST_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {"head,tail", DATA_TYPE_OPTION, "Whether to return elements from the head or from the tail of the list"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of elements to return"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg TRANSLATEPATH_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Unix style path"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg USEMODULE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Name of module command"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Argument string for the module"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg UNIQ_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg NTH_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_VALRANGE, DATA_TYPE_INT, "Offset of element to return"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg EVERY_SOME_NONE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression or string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg USEREXISTS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "User name or identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg SORT_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {"lex", DATA_TYPE_STRING, "Sorting method: lex"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg REVERSE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

FnCallArg SHUFFLE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Any seed string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

const FnCallType CF_FNCALL_TYPES[] =
{
    FnCallTypeNew("accessedbefore", DATA_TYPE_CONTEXT, ACCESSEDBEFORE_ARGS, &FnCallIsAccessedBefore, "True if arg1 was accessed before arg2 (atime)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("accumulated", DATA_TYPE_INT, ACCUM_ARGS, &FnCallAccumulatedDate, "Convert an accumulated amount of time into a system representation", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ago", DATA_TYPE_INT, AGO_ARGS, &FnCallAgoDate, "Convert a time relative to now to an integer system representation", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("and", DATA_TYPE_STRING, AND_ARGS, &FnCallAnd, "Calculate whether all arguments evaluate to true", true, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("canonify", DATA_TYPE_STRING, CANONIFY_ARGS, &FnCallCanonify, "Convert an abitrary string into a legal class name", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("concat", DATA_TYPE_STRING, CONCAT_ARGS, &FnCallConcat, "Concatenate all arguments into string", true, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("changedbefore", DATA_TYPE_CONTEXT, CHANGEDBEFORE_ARGS, &FnCallIsChangedBefore, "True if arg1 was changed before arg2 (ctime)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classify", DATA_TYPE_CONTEXT, CLASSIFY_ARGS, &FnCallClassify, "True if the canonicalization of the argument is a currently defined class", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classmatch", DATA_TYPE_CONTEXT, CLASSMATCH_ARGS, &FnCallClassMatch, "True if the regular expression matches any currently defined class", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classesmatching", DATA_TYPE_STRING_LIST, CLASSMATCH_ARGS, &FnCallClassesMatching, "List the defined classes matching regex arg1", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countclassesmatching", DATA_TYPE_INT, COUNTCLASSESMATCHING_ARGS, &FnCallCountClassesMatching, "Count the number of defined classes matching regex arg1", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countlinesmatching", DATA_TYPE_INT, COUNTLINESMATCHING_ARGS, &FnCallCountLinesMatching, "Count the number of lines matching regex arg1 in file arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("dirname", DATA_TYPE_STRING, DIRNAME_ARGS, &FnCallDirname, "Return the parent directory name for given path", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("diskfree", DATA_TYPE_INT, DISKFREE_ARGS, &FnCallDiskFree, "Return the free space (in KB) available on the directory's current partition (0 if not found)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("escape", DATA_TYPE_STRING, ESCAPE_ARGS, &FnCallEscape, "Escape regular expression characters in a string", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("every", DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if every element in the named list matches the given regular expression", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("execresult", DATA_TYPE_STRING, EXECRESULT_ARGS, &FnCallExecResult, "Execute named command and assign output to variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("fileexists", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named file can be accessed", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesexist", DATA_TYPE_CONTEXT, FILESEXIST_ARGS, &FnCallFileSexist, "True if the named list of files can ALL be accessed", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesize", DATA_TYPE_INT, FILESTAT_ARGS, &FnCallFileStat, "Returns the size in bytes of the file", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filestat", DATA_TYPE_STRING, FILESTAT_DETAIL_ARGS, &FnCallFileStatDetails, "Returns stat() details of the file", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filter", DATA_TYPE_STRING_LIST, FILTER_ARGS, &FnCallFilter, "Similarly to grep(, SYNTAX_STATUS_NORMAL), filter the list arg2 for matches to arg2.  The matching can be as a regular expression or exactly depending on arg3.  The matching can be inverted with arg4.  A maximum on the number of matches returned can be set with arg5.", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getenv", DATA_TYPE_STRING, GETENV_ARGS, &FnCallGetEnv, "Return the environment variable named arg1, truncated at arg2 characters", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getfields", DATA_TYPE_INT, GETFIELDS_ARGS, &FnCallGetFields, "Get an array of fields in the lines matching regex arg1 in file arg2, split on regex arg3 as array name arg4", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getgid", DATA_TYPE_INT, GETGID_ARGS, &FnCallGetGid, "Return the integer group id of the named group on this host", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getindices", DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetIndices, "Get a list of keys to the array whose id is the argument and assign to variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getuid", DATA_TYPE_INT, GETUID_ARGS, &FnCallGetUid, "Return the integer user id of the named user on this host", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getusers", DATA_TYPE_STRING_LIST, GETUSERS_ARGS, &FnCallGetUsers, "Get a list of all system users defined, minus those names defined in arg1 and uids in arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getvalues", DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetValues, "Get a list of values corresponding to the right hand sides in an array whose id is the argument and assign to variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("grep", DATA_TYPE_STRING_LIST, GREP_ARGS, &FnCallGrep, "Extract the sub-list if items matching the regular expression in arg1 of the list named in arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("groupexists", DATA_TYPE_CONTEXT, GROUPEXISTS_ARGS, &FnCallGroupExists, "True if group or numerical id exists on this host", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hash", DATA_TYPE_STRING, HASH_ARGS, &FnCallHash, "Return the hash of arg1, type arg2 and assign to a variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hashmatch", DATA_TYPE_CONTEXT, HASHMATCH_ARGS, &FnCallHashMatch, "Compute the hash of arg1, of type arg2 and test if it matches the value in arg3", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("host2ip", DATA_TYPE_STRING, HOST2IP_ARGS, &FnCallHost2IP, "Returns the primary name-service IP address for the named host", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ip2host", DATA_TYPE_STRING, IP2HOST_ARGS, &FnCallIP2Host, "Returns the primary name-service host name for the IP address", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostinnetgroup", DATA_TYPE_CONTEXT, HOSTINNETGROUP_ARGS, &FnCallHostInNetgroup, "True if the current host is in the named netgroup", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostrange", DATA_TYPE_CONTEXT, HOSTRANGE_ARGS, &FnCallHostRange, "True if the current host lies in the range of enumerated hostnames specified", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostsseen", DATA_TYPE_STRING_LIST, HOSTSSEEN_ARGS, &FnCallHostsSeen, "Extract the list of hosts last seen/not seen within the last arg1 hours", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostswithclass", DATA_TYPE_STRING_LIST, HOSTSWITHCLASS_ARGS, &FnCallHostsWithClass, "Extract the list of hosts with the given class set from the hub database (commercial extension)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hubknowledge", DATA_TYPE_STRING, HUB_KNOWLEDGE_ARGS, &FnCallHubKnowledge, "Read global knowledge from the hub host by id (commercial extension)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ifelse", DATA_TYPE_STRING, IFELSE_ARGS, &FnCallIfElse, "Do If-ElseIf-ElseIf-...-Else evaluation of arguments", true, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("iprange", DATA_TYPE_CONTEXT, IPRANGE_ARGS, &FnCallIPRange, "True if the current host lies in the range of IP addresses specified", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("irange", DATA_TYPE_INT_RANGE, IRANGE_ARGS, &FnCallIRange, "Define a range of integer values for cfengine internal use", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isdir", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a directory", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isexecutable", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object has execution rights for the current user", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isgreaterthan", DATA_TYPE_CONTEXT, ISGREATERTHAN_ARGS, &FnCallIsLessGreaterThan, "True if arg1 is numerically greater than arg2, else compare strings like strcmp", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("islessthan", DATA_TYPE_CONTEXT, ISLESSTHAN_ARGS, &FnCallIsLessGreaterThan, "True if arg1 is numerically less than arg2, else compare strings like NOT strcmp", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("islink", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a symbolic link", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isnewerthan", DATA_TYPE_CONTEXT, ISNEWERTHAN_ARGS, &FnCallIsNewerThan, "True if arg1 is newer (modified later) than arg2 (mtime)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isplain", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a plain/regular file", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isvariable", DATA_TYPE_CONTEXT, ISVARIABLE_ARGS, &FnCallIsVariable, "True if the named variable is defined", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("join", DATA_TYPE_STRING, JOIN_ARGS, &FnCallJoin, "Join the items of arg2 into a string, using the conjunction in arg1", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("lastnode", DATA_TYPE_STRING, LASTNODE_ARGS, &FnCallLastNode, "Extract the last of a separated string, e.g. filename from a path", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("laterthan", DATA_TYPE_CONTEXT, LATERTHAN_ARGS, &FnCallLaterThan, "True if the current time is later than the given date", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldaparray", DATA_TYPE_CONTEXT, LDAPARRAY_ARGS, &FnCallLDAPArray, "Extract all values from an ldap record", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldaplist", DATA_TYPE_STRING_LIST, LDAPLIST_ARGS, &FnCallLDAPList, "Extract all named values from multiple ldap records", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldapvalue", DATA_TYPE_STRING, LDAPVALUE_ARGS, &FnCallLDAPValue, "Extract the first matching named value from ldap", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("lsdir", DATA_TYPE_STRING_LIST, LSDIRLIST_ARGS, &FnCallLsDir, "Return a list of files in a directory matching a regular expression", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maparray", DATA_TYPE_STRING_LIST, MAPARRAY_ARGS, &FnCallMapArray, "Return a list with each element modified by a pattern based $(this.k) and $(this.v)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maplist", DATA_TYPE_STRING_LIST, MAPLIST_ARGS, &FnCallMapList, "Return a list with each element modified by a pattern based $(this)", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("none", DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if no element in the named list matches the given regular expression", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("not", DATA_TYPE_STRING, NOT_ARGS, &FnCallNot, "Calculate whether argument is false", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("now", DATA_TYPE_INT, NOW_ARGS, &FnCallNow, "Convert the current time into system representation", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("nth", DATA_TYPE_STRING, NTH_ARGS, &FnCallNth, "Get the element at arg2 in list arg1", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("on", DATA_TYPE_INT, DATE_ARGS, &FnCallOn, "Convert an exact date/time to an integer system representation", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("or", DATA_TYPE_STRING, OR_ARGS, &FnCallOr, "Calculate whether any argument evaluates to true", true, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parseintarray", DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseIntArray, "Read an array of integers from a file and assign the dimension to a variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parserealarray", DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseRealArray, "Read an array of real numbers from a file and assign the dimension to a variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsestringarray", DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseStringArray, "Read an array of strings from a file and assign the dimension to a variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsestringarrayidx", DATA_TYPE_INT, PARSESTRINGARRAYIDX_ARGS, &FnCallParseStringArrayIndex, "Read an array of strings from a file and assign the dimension to a variable with integer indeces", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peers", DATA_TYPE_STRING_LIST, PEERS_ARGS, &FnCallPeers, "Get a list of peers (not including ourself) from the partition to which we belong", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peerleader", DATA_TYPE_STRING, PEERLEADER_ARGS, &FnCallPeerLeader, "Get the assigned peer-leader of the partition to which we belong", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peerleaders", DATA_TYPE_STRING_LIST, PEERLEADERS_ARGS, &FnCallPeerLeaders, "Get a list of peer leaders from the named partitioning", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("product", DATA_TYPE_REAL, PRODUCT_ARGS, &FnCallProduct, "Return the product of a list of reals", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("randomint", DATA_TYPE_INT, RANDOMINT_ARGS, &FnCallRandomInt, "Generate a random integer between the given limits", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readfile", DATA_TYPE_STRING, READFILE_ARGS, &FnCallReadFile, "Read max number of bytes from named file and assign to variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintarray", DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadIntArray, "Read an array of integers from a file and assign the dimension to a variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintlist", DATA_TYPE_INT_LIST, READSTRINGLIST_ARGS, &FnCallReadIntList, "Read and assign a list variable from a file of separated ints", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readrealarray", DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadRealArray, "Read an array of real numbers from a file and assign the dimension to a variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readreallist", DATA_TYPE_REAL_LIST, READSTRINGLIST_ARGS, &FnCallReadRealList, "Read and assign a list variable from a file of separated real numbers", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringarray", DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadStringArray, "Read an array of strings from a file and assign the dimension to a variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringarrayidx", DATA_TYPE_INT, READSTRINGARRAYIDX_ARGS, &FnCallReadStringArrayIndex, "Read an array of strings from a file and assign the dimension to a variable with integer indeces", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringlist", DATA_TYPE_STRING_LIST, READSTRINGLIST_ARGS, &FnCallReadStringList, "Read and assign a list variable from a file of separated strings", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readtcp", DATA_TYPE_STRING, READTCP_ARGS, &FnCallReadTcp, "Connect to tcp port, send string and assign result to variable", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regarray", DATA_TYPE_CONTEXT, REGARRAY_ARGS, &FnCallRegArray, "True if arg1 matches any item in the associative array with id=arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regcmp", DATA_TYPE_CONTEXT, REGCMP_ARGS, &FnCallRegCmp, "True if arg1 is a regular expression matching that matches string arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regextract", DATA_TYPE_CONTEXT, REGEXTRACT_ARGS, &FnCallRegExtract, "True if the regular expression in arg 1 matches the string in arg2 and sets a non-empty array of backreferences named arg3", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("registryvalue", DATA_TYPE_STRING, REGISTRYVALUE_ARGS, &FnCallRegistryValue, "Returns a value for an MS-Win registry key,value pair", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regline", DATA_TYPE_CONTEXT, REGLINE_ARGS, &FnCallRegLine, "True if the regular expression in arg1 matches a line in file arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reglist", DATA_TYPE_CONTEXT, REGLIST_ARGS, &FnCallRegList, "True if the regular expression in arg2 matches any item in the list whose id is arg1", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regldap", DATA_TYPE_CONTEXT, REGLDAP_ARGS, &FnCallRegLDAP, "True if the regular expression in arg6 matches a value item in an ldap search", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("remotescalar", DATA_TYPE_STRING, REMOTESCALAR_ARGS, &FnCallRemoteScalar, "Read a scalar value from a remote cfengine server", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("remoteclassesmatching", DATA_TYPE_CONTEXT, REMOTECLASSESMATCHING_ARGS, &FnCallRemoteClassesMatching, "Read persistent classes matching a regular expression from a remote cfengine server and add them into local context with prefix", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("returnszero", DATA_TYPE_CONTEXT, RETURNSZERO_ARGS, &FnCallReturnsZero, "True if named shell command has exit status zero", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("rrange", DATA_TYPE_REAL_RANGE, RRANGE_ARGS, &FnCallRRange, "Define a range of real numbers for cfengine internal use", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reverse", DATA_TYPE_STRING_LIST, REVERSE_ARGS, &FnCallReverse, "Reverse a string list", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("selectservers", DATA_TYPE_INT, SELECTSERVERS_ARGS, &FnCallSelectServers, "Select tcp servers which respond correctly to a query and return their number, set array of names", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("shuffle", DATA_TYPE_STRING_LIST, SHUFFLE_ARGS, &FnCallShuffle, "Shuffle a string list", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("some", DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if an element in the named list matches the given regular expression", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sort", DATA_TYPE_STRING_LIST, SORT_ARGS, &FnCallSort, "Sort a string list", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splayclass", DATA_TYPE_CONTEXT, SPLAYCLASS_ARGS, &FnCallSplayClass, "True if the first argument's time-slot has arrived, according to a policy in arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splitstring", DATA_TYPE_STRING_LIST, SPLITSTRING_ARGS, &FnCallSplitString, "Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strcmp", DATA_TYPE_CONTEXT, STRCMP_ARGS, &FnCallStrCmp, "True if the two strings match exactly", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strftime", DATA_TYPE_STRING, STRFTIME_ARGS, &FnCallStrftime, "Format a date and time string", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sublist", DATA_TYPE_STRING_LIST, SUBLIST_ARGS, &FnCallSublist, "Returns arg3 element from either the head or the tail (according to arg2) of list arg1.", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sum", DATA_TYPE_REAL, SUM_ARGS, &FnCallSum, "Return the sum of a list of reals", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("translatepath", DATA_TYPE_STRING, TRANSLATEPATH_ARGS, &FnCallTranslatePath, "Translate path separators from Unix style to the host's native", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("uniq", DATA_TYPE_STRING_LIST, UNIQ_ARGS, &FnCallUniq, "Returns all the unique elements of list arg1", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("usemodule", DATA_TYPE_CONTEXT, USEMODULE_ARGS, &FnCallUseModule, "Execute cfengine module script and set class if successful", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("userexists", DATA_TYPE_CONTEXT, USEREXISTS_ARGS, &FnCallUserExists, "True if user name or numerical id exists on this host", false, SYNTAX_STATUS_NORMAL),
    FnCallTypeNewNull()
};
