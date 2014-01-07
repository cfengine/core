/*
   Copyright (C) CFEngine AS

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

#include <evalfunction.h>

#include <eval_context.h>
#include <promises.h>
#include <dir.h>
#include <dbm_api.h>
#include <lastseen.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <vars.h>
#include <addr_lib.h>
#include <syntax.h>
#include <item_lib.h>
#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <keyring.h>
#include <matching.h>
#include <hashes.h>
#include <unix.h>
#include <string_lib.h>
#include <client_code.h>
#include <communication.h>
#include <classic.h>                                    /* SendSocketStream */
#include <pipes.h>
#include <exec_tools.h>
#include <policy.h>
#include <misc_lib.h>
#include <fncall.h>
#include <audit.h>
#include <sort.h>
#include <logging.h>
#include <set.h>
#include <buffer.h>
#include <files_lib.h>
#include <connection_info.h>

#include <math_eval.h>

#include <libgen.h>

#ifndef __MINGW32__
#include <glob.h>
#endif

#include <ctype.h>


static FnCallResult FilterInternal(EvalContext *ctx, FnCall *fp, char *regex, char *name, int do_regex, int invert, long max);

static char *StripPatterns(char *file_buffer, char *pattern, char *filename);
static void CloseStringHole(char *s, int start, int end);
static int BuildLineArray(EvalContext *ctx, const Bundle *bundle, char *array_lval, char *file_buffer, char *split, int maxent, DataType type, int intIndex);
static int ExecModule(EvalContext *ctx, char *command, const char *ns);
static int CheckID(char *id);
static const Rlist *GetListReferenceArgument(const EvalContext *ctx, const FnCall *fp, const char *lval_str, DataType *datatype_out);
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

/*
 * Return succesful FnCallResult with copy of str retained.
 */
static FnCallResult FnReturn(const char *str)
{
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(str), RVAL_TYPE_SCALAR } };
}

static FnCallResult FnReturnF(const char *fmt, ...) FUNC_ATTR_PRINTF(1, 2);

static FnCallResult FnReturnF(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *buffer;
    xvasprintf(&buffer, fmt, ap);
    va_end(ap);
    return (FnCallResult) { FNCALL_SUCCESS, { buffer, RVAL_TYPE_SCALAR } };
}

static FnCallResult FnReturnContext(bool result)
{
    return FnReturn(result ? "any" : "!any");
}

static FnCallResult FnFailure(void)
{
    return (FnCallResult) { FNCALL_FAILURE };
}

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
            Log(LOG_LEVEL_ERR, "Could not get host entry age");
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
            Log(LOG_LEVEL_DEBUG, "Old entry");

            if (RlistKeyIn(recent, address))
            {
                Log(LOG_LEVEL_DEBUG, "There is recent entry for this address. Do nothing.");
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Adding to list of aged hosts.");
                RlistPrependScalarIdemp(&aged, address);
            }
        }
        else
        {
            Rlist *r;

            Log(LOG_LEVEL_DEBUG, "Recent entry");

            if ((r = RlistKeyIn(aged, address)))
            {
                Log(LOG_LEVEL_DEBUG, "Purging from list of aged hosts.");
                RlistDestroyEntry(&aged, r);
            }

            Log(LOG_LEVEL_DEBUG, "Adding to list of recent hosts.");
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
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (!IsDefinedClass(ctx, RlistScalarValue(arg), PromiseGetNamespace(fp->caller)))
        {
            return FnReturnContext(false);
        }
    }

    return FnReturnContext(true);
}

/*******************************************************************/

static bool CallHostsSeenCallback(const char *hostkey, const char *address,
                                  ARG_UNUSED bool incoming, const KeyHostSeen *quality,
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

static FnCallResult FnCallHostsSeen(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    Item *addresses = NULL;

    int horizon = IntFromString(RlistScalarValue(finalargs)) * 3600;
    char *policy = RlistScalarValue(finalargs->next);
    char *format = RlistScalarValue(finalargs->next->next);

    Log(LOG_LEVEL_DEBUG, "Calling hostsseen(%d,%s,%s)", horizon, policy, format);

    if (!ScanLastSeenQuality(&CallHostsSeenCallback, &addresses))
    {
        return FnFailure();
    }

    Rlist *returnlist = GetHostsFromLastseenDB(addresses, horizon,
                                               strcmp(format, "address") == 0,
                                               strcmp(policy, "lastseen") == 0);

    DeleteItemList(addresses);

    {
        Writer *w = StringWriter();
        WriterWrite(w, "hostsseen return values:");
        for (Rlist *rp = returnlist; rp; rp = rp->next)
        {
            WriterWriteF(w, " '%s'", RlistScalarValue(rp));
        }
        Log(LOG_LEVEL_DEBUG, "%s", StringWriterData(w));
        WriterClose(w);
    }

    if (returnlist == NULL)
    {
        return FnFailure();
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
    }
}

/*********************************************************************/

static FnCallResult FnCallHostsWithClass(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    Rlist *returnlist = NULL;

    char *class_name = RlistScalarValue(finalargs);
    char *return_format = RlistScalarValue(finalargs->next);
    
    if(!ListHostsWithClass(ctx, &returnlist, class_name, return_format))
    {
        return FnFailure();
    }
    
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallRandomInt(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    int tmp, range, result;

    int from = IntFromString(RlistScalarValue(finalargs));
    int to = IntFromString(RlistScalarValue(finalargs->next));

    if (from == CF_NOINT || to == CF_NOINT)
    {
        return FnFailure();
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    range = fabs(to - from);
    result = from + (int) (drand48() * (double) range);

    return FnReturnF("%d", result);
}

/*********************************************************************/

static FnCallResult FnCallGetEnv(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE] = "", ctrlstr[CF_SMALLBUF];

    char *name = RlistScalarValue(finalargs);
    int limit = IntFromString(RlistScalarValue(finalargs->next));

    snprintf(ctrlstr, CF_SMALLBUF, "%%.%ds", limit);    // -> %45s

    if (getenv(name))
    {
        snprintf(buffer, CF_BUFSIZE - 1, ctrlstr, getenv(name));
    }

    return FnReturn(buffer);
}

/*********************************************************************/

#if defined(HAVE_GETPWENT)

static FnCallResult FnCallGetUsers(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        char *pw_uid_str = StringFromLong((int)pw->pw_uid);

        if (!RlistKeyIn(except_names, pw->pw_name) && !RlistKeyIn(except_uids, pw_uid_str))
        {
            RlistAppendScalarIdemp(&newlist, pw->pw_name);
        }

        free(pw_uid_str);
    }

    endpwent();

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

#else

static FnCallResult FnCallGetUsers(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    Log(LOG_LEVEL_ERR, "getusers is not implemented");
    return FnFailure();
}

#endif

/*********************************************************************/

static FnCallResult FnCallEscape(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

    char *name = RlistScalarValue(finalargs);

    EscapeSpecialChars(name, buffer, CF_BUFSIZE - 1, "", "");

    return FnReturn(buffer);
}

/*********************************************************************/

static FnCallResult FnCallHost2IP(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char *name = RlistScalarValue(finalargs);
    char ipaddr[CF_MAX_IP_LEN];

    if (Hostname2IPString(ipaddr, name, sizeof(ipaddr)) != -1)
    {
        return FnReturn(ipaddr);
    }
    else
    {
        /* Retain legacy behaviour,
           return hostname in case resolution fails. */
        return FnReturn(name);
    }

}

/*********************************************************************/

static FnCallResult FnCallIP2Host(ARG_UNUSED EvalContext *ctx, ARG_UNUSED  FnCall *fp, Rlist *finalargs)
{
    char hostname[MAXHOSTNAMELEN];
    char *ip = RlistScalarValue(finalargs);

    if (IPString2Hostname(hostname, ip, sizeof(hostname)) != -1)
    {
        return FnReturn(hostname);
    }
    else
    {
        /* Retain legacy behaviour,
           return ip address in case resolution fails. */
        return FnReturn(ip);
    }
}

/*********************************************************************/

#ifdef __MINGW32__

static FnCallResult FnCallGetUid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, ARG_UNUSED Rlist *finalargs)
{
    return FnFailure();
}

#else /* !__MINGW32__ */

static FnCallResult FnCallGetUid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    struct passwd *pw;

/* begin fn specific content */

    if ((pw = getpwnam(RlistScalarValue(finalargs))) == NULL)
    {
        return FnFailure();
    }
    else
    {
        return FnReturnF("%ju", (uintmax_t)pw->pw_uid);
    }
}

#endif /* !__MINGW32__ */

/*********************************************************************/

#ifdef __MINGW32__

static FnCallResult FnCallGetGid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, ARG_UNUSED Rlist *finalargs)
{
    return FnFailure();
}

#else /* !__MINGW32__ */

static FnCallResult FnCallGetGid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    struct group *gr;

/* begin fn specific content */

    if ((gr = getgrnam(RlistScalarValue(finalargs))) == NULL)
    {
        return FnFailure();
    }
    else
    {
        return FnReturnF("%ju", (uintmax_t)gr->gr_gid);
    }
}

#endif /* __MINGW32__ */

/*********************************************************************/

static FnCallResult FnCallHandlerHash(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        Log(LOG_LEVEL_ERR, "FIPS mode is enabled, and md5 is not an approved algorithm in call to hash()");
    }

    HashString(string, strlen(string), digest, type);

    char hashbuffer[EVP_MAX_MD_SIZE * 4];

    snprintf(buffer, CF_BUFSIZE - 1, "%s",
             HashPrintSafe(type, true, digest, hashbuffer));
    return FnReturn(SkipHashType(buffer));
}

/*********************************************************************/

static FnCallResult FnCallHashMatch(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
/* HashMatch(string,md5|sha1|crypt,"abdxy98edj") */
{
    char buffer[CF_BUFSIZE];
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
    snprintf(buffer, CF_BUFSIZE - 1, "%s",
             HashPrintSafe(type, true, digest, hashbuffer));
    Log(LOG_LEVEL_VERBOSE, "File '%s' hashes to '%s', compare to '%s'", string, buffer, compare);

    return FnReturnContext(strcmp(buffer + 4, compare) == 0);
}

/*********************************************************************/

static FnCallResult FnCallConcat(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    Rlist *arg = NULL;
    char id[CF_BUFSIZE];
    char result[CF_BUFSIZE] = "";

    snprintf(id, CF_BUFSIZE, "built-in FnCall concat-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, DATA_TYPE_STRING, "", 1);
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
            Log(LOG_LEVEL_ERR, "Unable to evaluate concat() function, arguments are too long");
            return FnFailure();
        }
    }

    return FnReturn(result);
}

/*********************************************************************/

static FnCallResult FnCallClassMatch(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    const char *regex = RlistScalarValue(finalargs);
    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            char *expr = ClassRefToString(cls->ns, cls->name);

            /* FIXME: review this strcmp. Moved out from StringMatch */
            if (!strcmp(regex, expr) || StringMatchFull(regex, expr))
            {
                free(expr);
                return FnReturnContext(true);
            }

            free(expr);
        }
        ClassTableIteratorDestroy(iter);
    }

    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewLocal(ctx);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            char *expr = ClassRefToString(cls->ns, cls->name);

            /* FIXME: review this strcmp. Moved out from StringMatch */
            if (!strcmp(regex,expr) || StringMatchFull(regex, expr))
            {
                free(expr);
                return FnReturnContext(true);
            }

            free(expr);
        }
        ClassTableIteratorDestroy(iter);
    }

    return FnReturnContext(false);
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
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, DATA_TYPE_STRING, "", 1);
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
            return FnReturn(RlistScalarValue(arg->next));
        }
    }

    /* If we get here, we've reached the last argument (arg->next is NULL). */
    return FnReturn(RlistScalarValue(arg));
}

/*********************************************************************/

static FnCallResult FnCallCountClassesMatching(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    unsigned count = 0;
    const char *regex = RlistScalarValue(finalargs);
    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            char *expr = ClassRefToString(cls->ns, cls->name);

            /* FIXME: review this strcmp. Moved out from StringMatch */
            if (!strcmp(regex, expr) || StringMatchFull(regex, expr))
            {
                count++;
            }

            free(expr);
        }
        ClassTableIteratorDestroy(iter);
    }

    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewLocal(ctx);
        Class *cls = NULL;
        while ((cls = ClassTableIteratorNext(iter)))
        {
            char *expr = ClassRefToString(cls->ns, cls->name);

            /* FIXME: review this strcmp. Moved out from StringMatch */
            if (!strcmp(regex, expr) || StringMatchFull(regex, expr))
            {
                count++;
            }

            free(expr);
        }
        ClassTableIteratorDestroy(iter);
    }

    return FnReturnF("%u", count);
}

/*********************************************************************/

static StringSet *ClassesMatching(const EvalContext *ctx, ClassTableIterator *iter, const Rlist *args)
{
    StringSet *matching = StringSetNew();

    const char *regex = RlistScalarValue(args);
    Class *cls = NULL;
    while ((cls = ClassTableIteratorNext(iter)))
    {
        char *expr = ClassRefToString(cls->ns, cls->name);

        /* FIXME: review this strcmp. Moved out from StringMatch */
        if (!strcmp(regex, expr) || StringMatchFull(regex, expr))
        {
            bool pass = false;
            StringSet *tagset = EvalContextClassTags(ctx, cls->ns, cls->name);

            if (args->next)
            {
                for (const Rlist *arg = args->next; arg; arg = arg->next)
                {
                    const char *tag_regex = RlistScalarValue(arg);
                    const char *element;
                    StringSetIterator it = StringSetIteratorInit(tagset);
                    while ((element = StringSetIteratorNext(&it)))
                    {
                        /* FIXME: review this strcmp. Moved out from StringMatch */
                        if (strcmp(tag_regex, element) == 0 ||
                            StringMatchFull(tag_regex, element))
                        {
                            pass = true;
                            break;
                        }
                    }
                }
            }
            else                        // without any tags queried, accept class
            {
                pass = true;
            }

            if (pass)
            {
                StringSetAdd(matching, expr);
            }
        }
        else
        {
            free(expr);
        }
    }

    return matching;
}

static FnCallResult FnCallClassesMatching(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    if (!finalargs)
    {
        FatalError(ctx, "Function '%s' requires at least one argument", fp->name);
    }

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(fp->name, arg->val, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in function '%s', '%s'", fp->name, SyntaxTypeMatchToString(err));
        }
    }

    Rlist *matches = NULL;

    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, PromiseGetNamespace(fp->caller), true, true);
        StringSet *global_matches = ClassesMatching(ctx, iter, finalargs);

        StringSetIterator it = StringSetIteratorInit(global_matches);
        const char *element = NULL;
        while ((element = StringSetIteratorNext(&it)))
        {
            RlistPrepend(&matches, element, RVAL_TYPE_SCALAR);
        }

        StringSetDestroy(global_matches);
        ClassTableIteratorDestroy(iter);
    }

    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewLocal(ctx);
        StringSet *local_matches = ClassesMatching(ctx, iter, finalargs);

        StringSetIterator it = StringSetIteratorInit(local_matches);
        const char *element = NULL;
        while ((element = StringSetIteratorNext(&it)))
        {
            RlistPrepend(&matches, element, RVAL_TYPE_SCALAR);
        }

        StringSetDestroy(local_matches);
        ClassTableIteratorDestroy(iter);
    }

    if (!matches)
    {
        RlistAppendScalarIdemp(&matches, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}


static StringSet *VariablesMatching(const EvalContext *ctx, VariableTableIterator *iter, const Rlist *args)
{
    StringSet *matching = StringSetNew();

    const char *regex = RlistScalarValue(args);
    Variable *v = NULL;
    while ((v = VariableTableIteratorNext(iter)))
    {
        char *expr = VarRefToString(v->ref, true);

        /* FIXME: review this strcmp. Moved out from StringMatch */
        if (!strcmp(regex, expr) || StringMatchFull(regex, expr))
        {
            StringSet *tagset = EvalContextVariableTags(ctx, v->ref);
            bool pass = false;

            if (args->next)
            {
                for (const Rlist *arg = args->next; arg; arg = arg->next)
                {
                    const char* tag_regex = RlistScalarValue(arg);
                    const char *element = NULL;
                    StringSetIterator it = StringSetIteratorInit(tagset);
                    while ((element = SetIteratorNext(&it)))
                    {
                        /* FIXME: review this strcmp. Moved out from StringMatch */
                        if (strcmp(tag_regex, element) == 0 ||
                            StringMatchFull(tag_regex, element))
                        {
                            pass = true;
                            break;
                        }
                    }
                }
            }
            else                        // without any tags queried, accept variable
            {
                pass = true;
            }

            if (pass)
            {
                StringSetAdd(matching, expr);
            }
        }
        else
        {
            free(expr);
        }
    }

    return matching;
}

static FnCallResult FnCallVariablesMatching(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    if (!finalargs)
    {
        FatalError(ctx, "Function '%s' requires at least one argument", fp->name);
    }

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(fp->name, arg->val, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "In function '%s', %s", fp->name, SyntaxTypeMatchToString(err));
        }
    }

    Rlist *matches = NULL;

    {
        VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, NULL, NULL, NULL);
        StringSet *global_matches = VariablesMatching(ctx, iter, finalargs);

        StringSetIterator it = StringSetIteratorInit(global_matches);
        const char *element = NULL;
        while ((element = StringSetIteratorNext(&it)))
        {
            RlistPrepend(&matches, element, RVAL_TYPE_SCALAR);
        }

        StringSetDestroy(global_matches);
        VariableTableIteratorDestroy(iter);
    }

    if (!matches)
    {
        RlistAppendScalarIdemp(&matches, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallBundlesmatching(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buf[CF_BUFSIZE];
    char *regex = RlistScalarValue(finalargs);
    Rlist *matches = NULL;

    if (!fp->caller)
    {
        FatalError(ctx, "Function '%s' had a null caller", fp->name);
    }

    const Policy *policy = PolicyFromPromise(fp->caller);

    if (!policy)
    {
        FatalError(ctx, "Function '%s' had a null policy", fp->name);
    }

    if (!policy->bundles)
    {
        FatalError(ctx, "Function '%s' had null policy bundles", fp->name);
    }

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        const Bundle *bp = SeqAt(policy->bundles, i);
        if (!bp)
        {
            FatalError(ctx, "Function '%s' found null bundle at %ld", fp->name, i);
        }

        snprintf(buf, CF_BUFSIZE, "%s:%s", bp->ns, bp->name);
        if (StringMatchFull(regex, buf))
        {
            RlistPrepend(&matches, xstrdup(buf), RVAL_TYPE_SCALAR);
        }
    }

    if (!matches)
    {
        RlistAppendScalarIdemp(&matches, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallPackagesMatching(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *regex_package = RlistScalarValue(finalargs);
    char *regex_version = RlistScalarValue(finalargs->next);
    char *regex_arch = RlistScalarValue(finalargs->next->next);
    char *regex_method = RlistScalarValue(finalargs->next->next->next);

    JsonElement *json = JsonArrayCreate(50);
    char filename[CF_MAXVARSIZE], line[CF_BUFSIZE], regex[CF_BUFSIZE];
    FILE *fin;

    GetSoftwareCacheFilename(filename);

    if ((fin = fopen(filename, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s cannot open the package inventory '%s' - you need to run a package discovery promise to create it in cf-agent. (fopen: %s)",
            fp->name, filename, GetErrorStr());
        JsonDestroy(json);
        return FnFailure();
    }
    
    int linenumber = 0;
    for(;;)
    {
        ssize_t res = CfReadLine(line, sizeof(line), fin);

        if (res == 0)
        {
            break;
        }

        if (res == -1)
        {
            Log(LOG_LEVEL_ERR, "Unable to read package inventory from '%s'. (fread: %s)", filename, GetErrorStr());
            fclose(fin);
            JsonDestroy(json);
            return FnFailure();
        }

        if (strlen(line) > CF_BUFSIZE - 80)
        {
            Log(LOG_LEVEL_ERR, "Line %d from package inventory '%s' is too long to be sensible", linenumber, filename);
            break;
        }

        memset(regex, 0, sizeof(regex));
        // Here we will truncate the regex if the parameters add up to over CF_BUFSIZE
        snprintf(regex, sizeof(regex)-1, "^%s,%s,%s,%s$", regex_package, regex_version, regex_arch, regex_method);

        if (StringMatchFull(regex, line))
        {
            char name[CF_MAXVARSIZE], version[CF_MAXVARSIZE], arch[CF_MAXVARSIZE], method[CF_MAXVARSIZE];
            JsonElement *line_obj = JsonObjectCreate(4);
            int scancount = sscanf(line, "%250[^,],%250[^,],%250[^,],%250[^\n]", name, version, arch, method);
            if (scancount != 4)
            {
                Log(LOG_LEVEL_ERR, "Line %d from package inventory '%s' did not yield 4 elements", linenumber, filename);
                JsonDestroy(line_obj);
                ++linenumber;
                continue;
            }

            JsonObjectAppendString(line_obj, "name", name);
            JsonObjectAppendString(line_obj, "version", version);
            JsonObjectAppendString(line_obj, "arch", arch);
            JsonObjectAppendString(line_obj, "method", method);
            JsonArrayAppendObject(json, line_obj);
        }

        ++linenumber;
    }

    fclose(fin);

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };

}

/*********************************************************************/

static FnCallResult FnCallCanonify(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buf[CF_BUFSIZE];
    char *string = RlistScalarValue(finalargs);

    buf[0] = '\0';
    
    if (!strcmp(fp->name, "canonifyuniquely"))
    {
        char hashbuffer[EVP_MAX_MD_SIZE * 4];
        unsigned char digest[EVP_MAX_MD_SIZE + 1];
        HashMethod type;

        type = HashMethodFromString("sha1");
        HashString(string, strlen(string), digest, type);
        snprintf(buf, CF_BUFSIZE, "%s_%s", string,
                 SkipHashType(HashPrintSafe(type, true, digest, hashbuffer)));
    }
    else
    {
        snprintf(buf, CF_BUFSIZE, "%s", string);
    }

    return FnReturn(CanonifyName(buf));
}

/*********************************************************************/

static FnCallResult FnCallTextXform(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buf[CF_BUFSIZE];
    char *string = RlistScalarValue(finalargs);
    int len = 0;

    memset(buf, 0, sizeof(buf));
    strncpy(buf, string, sizeof(buf) - 1);
    len = strlen(buf);

    if (!strcmp(fp->name, "downcase"))
    {
        int pos = 0;
        for (pos = 0; pos < len; pos++)
        {
            buf[pos] = tolower(buf[pos]);
        }
    }
    else if (!strcmp(fp->name, "upcase"))
    {
        int pos = 0;
        for (pos = 0; pos < len; pos++)
        {
            buf[pos] = toupper(buf[pos]);
        }
    }
    else if (!strcmp(fp->name, "reversestring"))
    {
        int c, i, j;
        for (i = 0, j = len - 1; i < j; i++, j--)
        {
            c = buf[i];
            buf[i] = buf[j];
            buf[j] = c;
        }
    }
    else if (!strcmp(fp->name, "strlen"))
    {
        sprintf(buf, "%d", len);
    }
    else if (!strcmp(fp->name, "head"))
    {
        long max = IntFromString(RlistScalarValue(finalargs->next));
        if (max < sizeof(buf))
        {
            buf[max] = '\0';
        }
    }
    else if (!strcmp(fp->name, "tail"))
    {
        long max = IntFromString(RlistScalarValue(finalargs->next));
        if (max < len)
        {
            strncpy(buf, string + len - max, sizeof(buf) - 1);
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "text xform with unknown call function %s, aborting", fp->name);
        return FnFailure();
    }

    return FnReturn(buf);
}

/*********************************************************************/

static FnCallResult FnCallLastNode(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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

    if (rp && rp->val.item)
    {
        char *res = xstrdup(RlistScalarValue(rp));
        RlistDestroy(newlist);
        return (FnCallResult) { FNCALL_SUCCESS, { res, RVAL_TYPE_SCALAR } };
    }
    else
    {
        RlistDestroy(newlist);
        return FnFailure();
    }
}

/*******************************************************************/

static FnCallResult FnCallDirname(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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

    return FnReturnContext(is_defined);
}

/*********************************************************************/
/* Executions                                                        */
/*********************************************************************/

static FnCallResult FnCallReturnsZero(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char comm[CF_BUFSIZE];
    const char *shell_option = RlistScalarValue(finalargs->next);
    ShellType shelltype = SHELL_TYPE_NONE;
    bool need_executable_check = false;

    if (strcmp(shell_option, "useshell") == 0)
    {
        shelltype = SHELL_TYPE_USE;
    }
    else if (strcmp(shell_option, "powershell") == 0)
    {
        shelltype = SHELL_TYPE_POWERSHELL;
    }

    if (IsAbsoluteFileName(RlistScalarValue(finalargs)))
    {
        need_executable_check = true;
    }
    else if (shelltype == SHELL_TYPE_NONE)
    {
        Log(LOG_LEVEL_ERR, "returnszero '%s' does not have an absolute path", RlistScalarValue(finalargs));
        return FnReturnContext(false);
    }

    if (need_executable_check && !IsExecutable(CommandArg0(RlistScalarValue(finalargs))))
    {
        Log(LOG_LEVEL_ERR, "returnszero '%s' is assumed to be executable but isn't", RlistScalarValue(finalargs));
        return FnReturnContext(false);
    }

    snprintf(comm, CF_BUFSIZE, "%s", RlistScalarValue(finalargs));

    if (ShellCommandReturnsZero(comm, shelltype))
    {
        Log(LOG_LEVEL_VERBOSE, "%s ran '%s' successfully and it returned zero", fp->name, RlistScalarValue(finalargs));
        return FnReturnContext(true);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s ran '%s' successfully and it did not return zero", fp->name, RlistScalarValue(finalargs));
        return FnReturnContext(false);
    }
}

/*********************************************************************/

static FnCallResult FnCallExecResult(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *shell_option = RlistScalarValue(finalargs->next);
    ShellType shelltype = SHELL_TYPE_NONE;
    bool need_executable_check = false;

    if (strcmp(shell_option, "useshell") == 0)
    {
        shelltype = SHELL_TYPE_USE;
    }
    else if (strcmp(shell_option, "powershell") == 0)
    {
        shelltype = SHELL_TYPE_POWERSHELL;
    }

    if (IsAbsoluteFileName(RlistScalarValue(finalargs)))
    {
        need_executable_check = true;
    }
    else if (shelltype == SHELL_TYPE_NONE)
    {
        Log(LOG_LEVEL_ERR, "%s '%s' does not have an absolute path", fp->name, RlistScalarValue(finalargs));
        return FnFailure();
    }

    if (need_executable_check && !IsExecutable(CommandArg0(RlistScalarValue(finalargs))))
    {
        Log(LOG_LEVEL_ERR, "%s '%s' is assumed to be executable but isn't", fp->name, RlistScalarValue(finalargs));
        return FnFailure();
    }

    char buffer[CF_EXPANDSIZE];

    if (GetExecOutput(RlistScalarValue(finalargs), buffer, shelltype))
    {
        Log(LOG_LEVEL_VERBOSE, "%s ran '%s' successfully", fp->name, RlistScalarValue(finalargs));
        return FnReturn(buffer);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s could not run '%s' successfully", fp->name, RlistScalarValue(finalargs));
        return FnFailure();
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

    snprintf(modulecmd, CF_BUFSIZE, "\"%s%cmodules%c%s\"", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, command);

    if (stat(CommandArg0(modulecmd), &statbuf) == -1)
    {
        Log(LOG_LEVEL_ERR, "Plug-in module '%s' not found", modulecmd);
        return FnFailure();
    }

    if ((statbuf.st_uid != 0) && (statbuf.st_uid != getuid()))
    {
        Log(LOG_LEVEL_ERR, "Module '%s' was not owned by uid %ju who is executing agent", modulecmd, (uintmax_t)getuid());
        return FnFailure();
    }

    if (!JoinPath(modulecmd, args))
    {
        Log(LOG_LEVEL_ERR, "Culprit: class list for module (shouldn't happen)");
        return FnFailure();
    }

    snprintf(modulecmd, CF_BUFSIZE, "\"%s%cmodules%c%s\" %s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, command, args);
    Log(LOG_LEVEL_VERBOSE, "Executing and using module [%s]", modulecmd);

    if (!ExecModule(ctx, modulecmd, PromiseGetNamespace(fp->caller)))
    {
        return FnFailure();
    }

    return FnReturnContext(true);
}

/*********************************************************************/
/* Misc                                                              */
/*********************************************************************/

static FnCallResult FnCallSplayClass(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char class[CF_MAXVARSIZE];

    Interval policy = IntervalFromString(RlistScalarValue(finalargs->next));

    if (policy == INTERVAL_HOURLY)
    {
        /* 12 5-minute slots in hour */
        int slot = StringHash(RlistScalarValue(finalargs), 0, CF_HASHTABLESIZE) * 12 / CF_HASHTABLESIZE;
        snprintf(class, CF_MAXVARSIZE, "Min%02d_%02d", slot * 5, ((slot + 1) * 5) % 60);
    }
    else
    {
        /* 12*24 5-minute slots in day */
        int dayslot = StringHash(RlistScalarValue(finalargs), 0, CF_HASHTABLESIZE) * 12 * 24 / CF_HASHTABLESIZE;
        int hour = dayslot / 12;
        int slot = dayslot % 12;

        snprintf(class, CF_MAXVARSIZE, "Min%02d_%02d.Hr%02d", slot * 5, ((slot + 1) * 5) % 60, hour);
    }

    return FnReturnContext(IsDefinedClass(ctx, class, PromiseGetNamespace(fp->caller)));
}

/*********************************************************************/

static FnCallResult FnCallReadTcp(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnFailure();
    }

    if (val > CF_BUFSIZE - 1)
    {
        Log(LOG_LEVEL_ERR, "Too many bytes to read from TCP port '%s@%s'", port, hostnameip);
        val = CF_BUFSIZE - CF_BUFFERMARGIN;
    }

    Log(LOG_LEVEL_DEBUG, "Want to read %d bytes from port %d at '%s'", val, portnum, hostnameip);

    conn = NewAgentConn(hostnameip, false);

    FileCopy fc = {
        .force_ipv4 = false,
        .portnumber = portnum,
    };

    /* TODO don't use ServerConnect, this is only for CFEngine connections! */

    if (!ServerConnect(conn, hostnameip, fc))
    {
        Log(LOG_LEVEL_INFO, "Couldn't open a tcp socket. (socket: %s)", GetErrorStr());
        DeleteAgentConn(conn, false);
        return FnFailure();
    }

    if (strlen(sendstring) > 0)
    {
        int sent = 0;
        int result = 0;
        size_t length = strlen(sendstring);
        do {
            result = send(ConnectionInfoSocket(conn->conn_info), sendstring, length, 0);
            if (result < 0)
            {
                cf_closesocket(ConnectionInfoSocket(conn->conn_info));
                DeleteAgentConn(conn, false);
                return FnFailure();
            }
            else
            {
                sent += result;
            }
        } while (sent < length);
    }

    if ((n_read = recv(ConnectionInfoSocket(conn->conn_info), buffer, val, 0)) == -1)
    {
    }

    if (n_read == -1)
    {
        cf_closesocket(ConnectionInfoSocket(conn->conn_info));
        DeleteAgentConn(conn, false);
        return FnFailure();
    }

    cf_closesocket(ConnectionInfoSocket(conn->conn_info));
    DeleteAgentConn(conn, false);

    return FnReturn(buffer);
}

/*********************************************************************/

static FnCallResult FnCallRegList(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    const char *listvar = RlistScalarValue(finalargs);
    const char *regex = RlistScalarValue(finalargs->next);

    if (!IsVarList(listvar))
    {
        Log(LOG_LEVEL_VERBOSE, "Function reglist was promised a list called '%s' but this was not found", listvar);
        return FnFailure();
    }

    char naked[CF_MAXVARSIZE] = "";
    GetNaked(naked, listvar);

    VarRef *ref = VarRefParse(naked);

    Rval retval;
    if (!EvalContextVariableGet(ctx, ref, &retval, NULL))
    {
        Log(LOG_LEVEL_VERBOSE, "Function REGLIST was promised a list called '%s' but this was not found", listvar);
        VarRefDestroy(ref);
        return FnFailure();
    }

    if (retval.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE, "Function reglist was promised a list called '%s' but this variable is not a list",
              listvar);
        return FnFailure();
    }

    const Rlist *list = retval.item;

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (strcmp(RlistScalarValue(rp), CF_NULL_VALUE) == 0)
        {
            continue;
        }

        if (StringMatchFull(regex, RlistScalarValue(rp)))
        {
            return FnReturnContext(true);
        }
    }

    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallRegArray(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char *arrayname = RlistScalarValue(finalargs);
    char *regex = RlistScalarValue(finalargs->next);

    VarRef *ref = VarRefParse(arrayname);
    bool found = false;

    VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, ref->ns, ref->scope, ref->lval);
    Variable *var = NULL;
    while ((var = VariableTableIteratorNext(iter)))
    {
        if (StringMatchFull(regex, RvalScalarValue(var->rval)))
        {
            found = true;
            break;
        }
    }
    VariableTableIteratorDestroy(iter);

    return FnReturnContext(found);
}


static FnCallResult FnCallGetIndices(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    VarRef *ref = VarRefParseFromBundle(RlistScalarValue(finalargs), PromiseGetBundle(fp->caller));

    DataType type = DATA_TYPE_NONE;
    Rval rval;
    EvalContextVariableGet(ctx, ref, &rval, &type);

    Rlist *keys = NULL;
    if (type == DATA_TYPE_CONTAINER)
    {
        if (JsonGetElementType(RvalContainerValue(rval)) == JSON_ELEMENT_TYPE_CONTAINER)
        {
            if (JsonGetContrainerType(RvalContainerValue(rval)) == JSON_CONTAINER_TYPE_OBJECT)
            {
                JsonIterator iter = JsonIteratorInit(RvalContainerValue(rval));
                const char *key = NULL;
                while ((key = JsonIteratorNextKey(&iter)))
                {
                    RlistAppendScalar(&keys, key);
                }
            }
            else
            {
                for (size_t i = 0; i < JsonLength(RvalContainerValue(rval)); i++)
                {
                    Rval key = (Rval) { StringFromLong(i), RVAL_TYPE_SCALAR };
                    RlistAppendRval(&keys, key);
                }
            }
        }
    }
    else
    {
        VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, ref->ns, ref->scope, ref->lval);
        const Variable *var = NULL;
        while ((var = VariableTableIteratorNext(iter)))
        {
            if (ref->num_indices < var->ref->num_indices)
            {
                RlistAppendScalarIdemp(&keys, var->ref->indices[ref->num_indices]);
            }
        }
        VariableTableIteratorDestroy(iter);
    }

    VarRefDestroy(ref);

    if (RlistLen(keys) == 0)
    {
        RlistAppendScalarIdemp(&keys, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { keys, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGetValues(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    VarRef *ref = VarRefParseFromBundle(RlistScalarValue(finalargs), PromiseGetBundle(fp->caller));

    DataType type = DATA_TYPE_NONE;
    Rval rval;
    EvalContextVariableGet(ctx, ref, &rval, &type);

    Rlist *values = NULL;
    if (type == DATA_TYPE_CONTAINER)
    {
        if (JsonGetElementType(RvalContainerValue(rval)) == JSON_ELEMENT_TYPE_CONTAINER)
        {
            JsonIterator iter = JsonIteratorInit(RvalContainerValue(rval));
            const JsonElement *el = NULL;
            while ((el = JsonIteratorNextValue(&iter)))
            {
                if (JsonGetElementType(el) != JSON_ELEMENT_TYPE_PRIMITIVE)
                {
                    continue;
                }

                switch (JsonGetPrimitiveType(el))
                {
                case JSON_PRIMITIVE_TYPE_BOOL:
                    RlistAppendScalar(&values, JsonPrimitiveGetAsBool(el) ? "true" : "false");
                    break;
                case JSON_PRIMITIVE_TYPE_INTEGER:
                    {
                        char *str = StringFromLong(JsonPrimitiveGetAsInteger(el));
                        RlistAppendScalar(&values, str);
                        free(str);
                    }
                    break;
                case JSON_PRIMITIVE_TYPE_REAL:
                    {
                        char *str = StringFromDouble(JsonPrimitiveGetAsReal(el));
                        RlistAppendScalar(&values, str);
                        free(str);
                    }
                    break;
                case JSON_PRIMITIVE_TYPE_STRING:
                    RlistAppendScalar(&values, JsonPrimitiveGetAsString(el));
                    break;

                case JSON_PRIMITIVE_TYPE_NULL:
                    break;
                }
            }
        }
    }
    else
    {
        VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, ref->ns, ref->scope, ref->lval);
        Variable *var = NULL;
        while ((var = VariableTableIteratorNext(iter)))
        {
            if (var->ref->num_indices != 1)
            {
                continue;
            }

            switch (var->rval.type)
            {
            case RVAL_TYPE_SCALAR:
                RlistAppendScalarIdemp(&values, var->rval.item);
                break;

            case RVAL_TYPE_LIST:
                for (const Rlist *rp = var->rval.item; rp != NULL; rp = rp->next)
                {
                    RlistAppendScalarIdemp(&values, RlistScalarValue(rp));
                }
                break;

            default:
                break;
            }
        }
    }

    VarRefDestroy(ref);

    if (RlistLen(values) == 0)
    {
        RlistAppendScalarIdemp(&values, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { values, RVAL_TYPE_LIST } };
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

static FnCallResult FnCallSum(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    Rval rval2;
    double sum = 0;

    VarRef *ref = VarRefParse(RlistScalarValue(finalargs));

    if (!EvalContextVariableGet(ctx, ref, &rval2, NULL))
    {
        Log(LOG_LEVEL_VERBOSE, "Function sum was promised a list called '%s' but this was not found", ref->lval);
        VarRefDestroy(ref);
        return FnFailure();
    }

    if (rval2.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE, "Function sum was promised a list called '%s' but this was not found", ref->lval);
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    for (const Rlist *rp = (const Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        double x;

        if (!DoubleFromString(RlistScalarValue(rp), &x))
        {
            return FnFailure();
        }
        else
        {
            sum += x;
        }
    }

    return FnReturnF("%lf", sum);
}

/*********************************************************************/

static FnCallResult FnCallProduct(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    Rval rval2;
    double product = 1.0;

    VarRef *ref = VarRefParse(RlistScalarValue(finalargs));

    if (!EvalContextVariableGet(ctx, ref, &rval2, NULL))
    {
        Log(LOG_LEVEL_VERBOSE, "Function 'product' was promised a list called '%s' but this was not found", ref->lval);
        VarRefDestroy(ref);
        return FnFailure();
    }

    if (rval2.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE, "Function 'product' was promised a list called '%s' but this was not found", ref->lval);
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    for (const Rlist *rp = (const Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        double x;
        if (!DoubleFromString(RlistScalarValue(rp), &x))
        {
            return FnFailure();
        }
        else
        {
            product *= x;
        }
    }

    return FnReturnF("%lf", product);
}

/*********************************************************************/

static FnCallResult FnCallJoin(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char *joined = NULL;
    Rval rval2;
    int size = 0;

    const char *join = RlistScalarValue(finalargs);
    VarRef *ref = VarRefParse(RlistScalarValue(finalargs->next));

    if (!EvalContextVariableGet(ctx, ref, &rval2, NULL))
    {
        Log(LOG_LEVEL_VERBOSE, "Function 'join' was promised a list called '%s.%s' but this was not (yet) found", ref->scope, ref->lval);
        VarRefDestroy(ref);
        return FnFailure();
    }

    if (rval2.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE, "Function 'join' was promised a list called '%s' but this was not (yet) found", ref->lval);
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    for (const Rlist *rp = RvalRlistValue(rval2); rp != NULL; rp = rp->next)
    {
        if (strcmp(RlistScalarValue(rp), CF_NULL_VALUE) == 0)
        {
            continue;
        }

        size += strlen(RlistScalarValue(rp)) + strlen(join);
    }

    joined = xcalloc(1, size + 1);
    size = 0;

    for (const Rlist *rp = (const Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        if (strcmp(RlistScalarValue(rp), CF_NULL_VALUE) == 0)
        {
            continue;
        }

        strcpy(joined + size, RlistScalarValue(rp));

        if (rp->next != NULL)
        {
            strcpy(joined + size + strlen(RlistScalarValue(rp)), join);
            size += strlen(RlistScalarValue(rp)) + strlen(join);
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { joined, RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetFields(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist;
    char name[CF_MAXVARSIZE];
    int lcount = 0, vcount = 0, nopurge = true;
    FILE *fin;

/* begin fn specific content */

    char *regex = RlistScalarValue(finalargs);
    char *filename = RlistScalarValue(finalargs->next);
    char *split = RlistScalarValue(finalargs->next->next);
    char *array_lval = RlistScalarValue(finalargs->next->next->next);

    if ((fin = safe_fopen(filename, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "File '%s' could not be read in getfields(). (fopen: %s)", filename, GetErrorStr());
        return FnFailure();
    }

    for (;;)
    {
        char line[CF_BUFSIZE];

        if (fgets(line, sizeof(line), fin) == NULL)
        {
            if (ferror(fin))
            {
                Log(LOG_LEVEL_ERR, "Unable to read data from file '%s'. (fgets: %s)", filename, GetErrorStr());
                fclose(fin);
                return FnFailure();
            }
            else /* feof */
            {
                break;
            }
        }

        if (Chop(line, CF_EXPANDSIZE) == -1)
        {
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }

        if (!StringMatchFull(regex, line))
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
                VarRef *ref = VarRefParseFromBundle(name, PromiseGetBundle(fp->caller));
                EvalContextVariablePut(ctx, ref, RlistScalarValue(rp), DATA_TYPE_STRING, "source=function,function=getfields");
                VarRefDestroy(ref);
                Log(LOG_LEVEL_VERBOSE, "getfields: defining '%s' => '%s'", name, RlistScalarValue(rp));
                vcount++;
            }
        }

        lcount++;
    }

    fclose(fin);

    return FnReturnF("%d", lcount);
}

/*********************************************************************/

static FnCallResult FnCallCountLinesMatching(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    int lcount = 0;
    FILE *fin;

/* begin fn specific content */

    char *regex = RlistScalarValue(finalargs);
    char *filename = RlistScalarValue(finalargs->next);

    if ((fin = safe_fopen(filename, "r")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "File '%s' could not be read in countlinesmatching(). (fopen: %s)", filename, GetErrorStr());
        return FnReturn("0");
    }

    for (;;)
    {
        char line[CF_BUFSIZE];
        if (fgets(line, sizeof(line), fin) == NULL)
        {
            if (ferror(fin))
            {
                Log(LOG_LEVEL_ERR, "Unable to read data from file '%s'. (fgets: %s)", filename, GetErrorStr());
                fclose(fin);
                return FnFailure();
            }
            else /* feof */
            {
                break;
            }
        }
        if (Chop(line, CF_EXPANDSIZE) == -1)
        {
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }

        if (StringMatchFull(regex, line))
        {
            lcount++;
            Log(LOG_LEVEL_VERBOSE, "countlinesmatching: matched '%s'", line);
            continue;
        }
    }

    fclose(fin);

    return FnReturnF("%d", lcount);
}

/*********************************************************************/

static FnCallResult FnCallLsDir(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char line[CF_BUFSIZE];
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
        Log(LOG_LEVEL_ERR, "Directory '%s' could not be accessed in lsdir(), (opendir: %s)", dirname, GetErrorStr());
        RlistPrepend(&newlist, CF_NULL_VALUE, RVAL_TYPE_SCALAR);
        return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
    }

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (strlen(regex) == 0 || StringMatchFull(regex, dirp->d_name))
        {
            if (includepath)
            {
                snprintf(line, CF_BUFSIZE, "%s/%s", dirname, dirp->d_name);
                MapName(line);
                RlistPrepend(&newlist, line, RVAL_TYPE_SCALAR);
            }
            else
            {
                RlistPrepend(&newlist, dirp->d_name, RVAL_TYPE_SCALAR);
            }
        }
    }

    DirClose(dirh);

    if (newlist == NULL)
    {
        RlistPrepend(&newlist, CF_NULL_VALUE, RVAL_TYPE_SCALAR);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallMapArray(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char expbuf[CF_EXPANDSIZE];
    Rlist *returnlist = NULL;

    char *map = RlistScalarValue(finalargs);

    VarRef *ref = VarRefParseFromBundle(RlistScalarValue(finalargs->next), PromiseGetBundle(fp->caller));

    VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, ref->ns, ref->scope, ref->lval);
    Variable *var = NULL;

    while ((var = VariableTableIteratorNext(iter)))
    {
        if (var->ref->num_indices != 1)
        {
            continue;
        }

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "k", var->ref->indices[0], DATA_TYPE_STRING, "source=function,function=maparray");

        switch (var->rval.type)
        {
        case RVAL_TYPE_SCALAR:
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "v", var->rval.item, DATA_TYPE_STRING, "source=function,function=maparray");
            ExpandScalar(ctx, PromiseGetBundle(fp->caller)->ns, PromiseGetBundle(fp->caller)->name, map, expbuf);

            if (strstr(expbuf, "$(this.k)") || strstr(expbuf, "${this.k}") ||
                strstr(expbuf, "$(this.v)") || strstr(expbuf, "${this.v}"))
            {
                RlistDestroy(returnlist);
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
                return FnFailure();
            }

            RlistAppendScalar(&returnlist, expbuf);
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
            break;

        case RVAL_TYPE_LIST:
            for (const Rlist *rp = var->rval.item; rp != NULL; rp = rp->next)
            {
                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "v", RlistScalarValue(rp), DATA_TYPE_STRING, "source=function,function=maparray");
                ExpandScalar(ctx, PromiseGetBundle(fp->caller)->ns, PromiseGetBundle(fp->caller)->name, map, expbuf);

                if (strstr(expbuf, "$(this.k)") || strstr(expbuf, "${this.k}") ||
                    strstr(expbuf, "$(this.v)") || strstr(expbuf, "${this.v}"))
                {
                    RlistDestroy(returnlist);
                    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
                    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
                    return FnFailure();
                }

                RlistAppendScalarIdemp(&returnlist, expbuf);
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
            }
            break;

        default:
            break;
        }
        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
    }

    VariableTableIteratorDestroy(iter);
    VarRefDestroy(ref);

    if (returnlist == NULL)
    {
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

static FnCallResult FnCallMapList(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char expbuf[CF_EXPANDSIZE];
    Rlist *newlist = NULL;
    Rval rval;
    DataType retype;

    const char *map = RlistScalarValue(finalargs);
    char *listvar = RlistScalarValue(finalargs->next);

    char naked[CF_MAXVARSIZE] = "";
    if (IsVarList(listvar))
    {
        GetNaked(naked, listvar);
    }
    else
    {
        strncpy(naked, listvar, CF_MAXVARSIZE - 1);
    }

    VarRef *ref = VarRefParse(naked);

    retype = DATA_TYPE_NONE;
    if (!EvalContextVariableGet(ctx, ref, &rval, &retype))
    {
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    if (retype != DATA_TYPE_STRING_LIST && retype != DATA_TYPE_INT_LIST && retype != DATA_TYPE_REAL_LIST)
    {
        return FnFailure();
    }

    for (const Rlist *rp = RvalRlistValue(rval); rp != NULL; rp = rp->next)
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "this", RlistScalarValue(rp), DATA_TYPE_STRING, "source=function,function=maplist");

        ExpandScalar(ctx, NULL, "this", map, expbuf);

        if (strstr(expbuf, "$(this)") || strstr(expbuf, "${this}"))
        {
            RlistDestroy(newlist);
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "this");
            return FnFailure();
        }

        RlistAppendScalar(&newlist, expbuf);
        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "this");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

static FnCallResult FnCallMergeData(EvalContext *ctx, FnCall *fp, Rlist *args)
{
    if (RlistLen(args) == 0)
    {
        Log(LOG_LEVEL_ERR, "Function mergedata needs at least one argument, a reference to a container variable");
        return FnFailure();
    }

    for (const Rlist *arg = args; arg; arg = arg->next)
    {
        if (args->val.type != RVAL_TYPE_SCALAR)
        {
            Log(LOG_LEVEL_ERR, "Function mergedata, argument '%s' is not a variable reference", RlistScalarValue(arg));
            return FnFailure();
        }
    }

    Seq *containers = SeqNew(10, NULL);
    for (const Rlist *arg = args; arg; arg = arg->next)
    {
        VarRef *ref = VarRefParseFromBundle(RlistScalarValue(arg), PromiseGetBundle(fp->caller));

        Rval rval;
        if (!EvalContextVariableGet(ctx, ref, &rval, NULL))
        {
            Log(LOG_LEVEL_ERR, "Function mergedata, argument '%s' does not resolve to a container", RlistScalarValue(arg));
            SeqDestroy(containers);
            VarRefDestroy(ref);
            return FnFailure();
        }

        SeqAppend(containers, RvalContainerValue(rval));

        VarRefDestroy(ref);
    }

    if (SeqLength(containers) == 1)
    {
        JsonElement *first = SeqAt(containers, 0);
        SeqDestroy(containers);
        return  (FnCallResult) { FNCALL_SUCCESS, (Rval) { JsonCopy(first), RVAL_TYPE_CONTAINER } };
    }
    else
    {
        JsonElement *first = SeqAt(containers, 0);
        JsonElement *second = SeqAt(containers, 1);
        JsonElement *result = JsonMerge(first, second);

        for (size_t i = 2; i < SeqLength(containers); i++)
        {
            JsonElement *cur = SeqAt(containers, i);
            JsonElement *tmp = JsonMerge(result, cur);
            JsonDestroy(result);
            result = tmp;
        }

        SeqDestroy(containers);
        return (FnCallResult) { FNCALL_SUCCESS, (Rval) { result, RVAL_TYPE_CONTAINER } };
    }

    assert(false);
}


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
        Log(LOG_LEVEL_VERBOSE, "Function selectservers was promised a list called '%s' but this was not found", listvar);
        return FnFailure();
    }

    VarRef *ref = VarRefParse(naked);

    if (!EvalContextVariableGet(ctx, ref, &retval, NULL))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function selectservers was promised a list called '%s' but this was not found from context '%s.%s'",
              listvar, ref->scope, naked);
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    if (retval.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function selectservers was promised a list called '%s' but this variable is not a list", listvar);
        return FnFailure();
    }

    hostnameip = RvalRlistValue(retval);
    val = IntFromString(maxbytes);
    portnum = (short) IntFromString(port);

    if (val < 0 || portnum < 0)
    {
        return FnFailure();
    }

    if (val > CF_BUFSIZE - 1)
    {
        Log(LOG_LEVEL_ERR, "Too many bytes specificed in selectservers");
        val = CF_BUFSIZE - CF_BUFFERMARGIN;
    }

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        return FnReturnF("%d", count);
    }

    Policy *select_server_policy = PolicyNew();
    {
        Bundle *bp = PolicyAppendBundle(select_server_policy, NamespaceDefault(), "select_server_bundle", "agent", NULL, NULL);
        PromiseType *tp = BundleAppendPromiseType(bp, "select_server");

        PromiseTypeAppendPromise(tp, "function", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, NULL);
    }

    for (Rlist *rp = hostnameip; rp != NULL; rp = rp->next)
    {
        Log(LOG_LEVEL_DEBUG, "Want to read %d bytes from port %d at '%s'", val, portnum, RlistScalarValue(rp));

        conn = NewAgentConn(RlistScalarValue(rp), false);

        FileCopy fc = {
            .force_ipv4 = false,
            .portnumber = portnum,
        };

        /* TODO don't use ServerConnect, this is only for CFEngine connections! */

        if (!ServerConnect(conn, RlistScalarValue(rp), fc))
        {
            Log(LOG_LEVEL_INFO, "Couldn't open a tcp socket. (socket %s)", GetErrorStr());
            DeleteAgentConn(conn, false);
            continue;
        }

        if (strlen(sendstring) > 0)
        {
            if (SendSocketStream(ConnectionInfoSocket(conn->conn_info), sendstring, strlen(sendstring)) == -1)
            {
                cf_closesocket(ConnectionInfoSocket(conn->conn_info));
                DeleteAgentConn(conn, false);
                continue;
            }

            if ((n_read = recv(ConnectionInfoSocket(conn->conn_info), buffer, val, 0)) == -1)
            {
            }

            if (n_read == -1)
            {
                cf_closesocket(ConnectionInfoSocket(conn->conn_info));
                DeleteAgentConn(conn, false);
                continue;
            }

            if (strlen(regex) == 0 || StringMatchFull(regex, buffer))
            {
                Log(LOG_LEVEL_VERBOSE, "Host '%s' is alive and responding correctly", RlistScalarValue(rp));
                snprintf(buffer, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, count);
                VarRef *ref = VarRefParseFromBundle(buffer, PromiseGetBundle(fp->caller));
                EvalContextVariablePut(ctx, ref, RvalScalarValue(rp->val), DATA_TYPE_STRING, "source=function,function=selectservers");
                VarRefDestroy(ref);
                count++;
            }
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Host '%s' is alive", RlistScalarValue(rp));
            snprintf(buffer, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, count);
            VarRef *ref = VarRefParseFromBundle(buffer, PromiseGetBundle(fp->caller));
            EvalContextVariablePut(ctx, ref, RvalScalarValue(rp->val), DATA_TYPE_STRING, "source=function,function=selectservers");
            VarRefDestroy(ref);

            if (IsDefinedClass(ctx, CanonifyName(RlistScalarValue(rp)), PromiseGetNamespace(fp->caller)))
            {
                Log(LOG_LEVEL_VERBOSE, "This host is in the list and has promised to join the class '%s' - joined",
                      array_lval);
                EvalContextClassPut(ctx, PromiseGetNamespace(fp->caller), array_lval, true, CONTEXT_SCOPE_NAMESPACE, "source=function,function=selectservers");
            }

            count++;
        }

        cf_closesocket(ConnectionInfoSocket(conn->conn_info));
        DeleteAgentConn(conn, false);
    }

    PolicyDestroy(select_server_policy);

/* Return the subset that is alive and responding correctly */

/* Return the number of lines in array */

    return FnReturnF("%d", count);
}


static FnCallResult FnCallShuffle(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *seed_str = RlistScalarValue(finalargs->next);

    DataType list_dtype = DATA_TYPE_NONE;
    const Rlist *list = GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), &list_dtype);
    if (!list)
    {
        return FnFailure();
    }

    if (list_dtype != DATA_TYPE_STRING_LIST)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' expected a variable that resolves to a string list, got '%s'", fp->name, DataTypeToString(list_dtype));
        return FnFailure();
    }

    Seq *seq = SeqNew(1000, NULL);
    for (const Rlist *rp = list; rp; rp = rp->next)
    {
        SeqAppend(seq, RlistScalarValue(rp));
    }

    SeqShuffle(seq, StringHash(seed_str, 0, RAND_MAX));

    Rlist *shuffled = NULL;
    for (size_t i = 0; i < SeqLength(seq); i++)
    {
        RlistPrepend(&shuffled, SeqAt(seq, i), RVAL_TYPE_SCALAR);
    }

    SeqDestroy(seq);
    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { shuffled, RVAL_TYPE_LIST } };
}


static FnCallResult FnCallIsNewerThan(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (stat(RlistScalarValue(finalargs), &frombuf) == -1)
    {
        return FnFailure();
    }

    if (stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return FnFailure();
    }

    return FnReturnContext(frombuf.st_mtime > tobuf.st_mtime);
}

/*********************************************************************/

static FnCallResult FnCallIsAccessedBefore(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (stat(RlistScalarValue(finalargs), &frombuf) == -1)
    {
        return FnFailure();
    }

    if (stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return FnFailure();
    }

    return FnReturnContext(frombuf.st_atime < tobuf.st_atime);
}

/*********************************************************************/

static FnCallResult FnCallIsChangedBefore(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (stat(RlistScalarValue(finalargs), &frombuf) == -1)
    {
        return FnFailure();
    }
    else if (stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return FnFailure();
    }

    return FnReturnContext(frombuf.st_ctime > tobuf.st_ctime);
}

/*********************************************************************/

static FnCallResult FnCallFileStat(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *path = RlistScalarValue(finalargs);
    struct stat statbuf;

/* begin fn specific content */

    if (lstat(path, &statbuf) == -1)
    {
        if (!strcmp(fp->name, "filesize"))
        {
            return FnFailure();
        }
        return FnReturnContext(false);
    }

    if (!strcmp(fp->name, "isexecutable"))
    {
        return FnReturnContext(IsExecutable(path));
    }
    if (!strcmp(fp->name, "isdir"))
    {
        return FnReturnContext(S_ISDIR(statbuf.st_mode));
    }
    if (!strcmp(fp->name, "islink"))
    {
        return FnReturnContext(S_ISLNK(statbuf.st_mode));
    }
    if (!strcmp(fp->name, "isplain"))
    {
        return FnReturnContext(S_ISREG(statbuf.st_mode));
    }
    if (!strcmp(fp->name, "fileexists"))
    {
        return FnReturnContext(true);
    }
    if (!strcmp(fp->name, "filesize"))
    {
        return FnReturnF("%jd", (uintmax_t) statbuf.st_size);
    }

    ProgrammingError("Unexpected function name in FnCallFileStat: %s", fp->name);
}

/*********************************************************************/

static FnCallResult FnCallFileStatDetails(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
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
        else if (!strcmp(detail, "linktarget") || !strcmp(detail, "linktarget_shallow"))
        {
#if !defined(__MINGW32__)
            char path_buffer[CF_BUFSIZE];
            bool recurse = !strcmp(detail, "linktarget");
            int cycles = 0;
            int max_cycles = 30; // This allows for up to 31 levels of indirection.

            snprintf(path_buffer, CF_MAXVARSIZE, "%s", path);

            // Iterate while we're looking at a link.
            while (S_ISLNK(statbuf.st_mode))
            {
                if (cycles > max_cycles)
                {
                    Log(LOG_LEVEL_INFO, "%s bailing on link '%s' (original '%s') because %d cycles were chased",
                        fp->name, path_buffer, path, cycles+1);
                    break;
                }

                Log(LOG_LEVEL_VERBOSE, "%s resolving link '%s', cycle %d", fp->name, path_buffer, cycles+1);
                // Prep buffer because readlink() doesn't terminate the path.
                memset(buffer, 0, CF_BUFSIZE);

                /* Note we subtract 1 since we may need an extra char for NULL. */
                if (readlink(path_buffer, buffer, CF_BUFSIZE-1) < 0)
                {
                    // An error happened.  Empty the buffer (don't keep the last target).
                    Log(LOG_LEVEL_ERR, "%s could not readlink '%s'", fp->name, path_buffer);
                    path_buffer[0] = '\0';
                    break;
                }

                Log(LOG_LEVEL_VERBOSE, "%s resolved link '%s' to %s", fp->name, path_buffer, buffer);
                // We got a good link target into buffer.  Copy it to path_buffer.
                snprintf(path_buffer, CF_MAXVARSIZE, "%s", buffer);

                if (!recurse || lstat(path_buffer, &statbuf) == -1)
                {
                    if (!recurse)
                    {
                        Log(LOG_LEVEL_VERBOSE, "%s bailing on link '%s' (original '%s') because linktarget_shallow was requested",
                            fp->name, path_buffer, path);
                    }
                    else // error from lstat
                    {
                        Log(LOG_LEVEL_INFO, "%s bailing on link '%s' (original '%s') because it could not be read",
                            fp->name, path_buffer, path);
                    }
                    break;
                }

                // At this point we haven't bailed, path_buffer has the link target
                cycles++;
            }

            // Get the path_buffer back into buffer.
            snprintf(buffer, CF_MAXVARSIZE, "%s", path_buffer);

#else
            // Always return the original path on W32.
            snprintf(buffer, CF_MAXVARSIZE, "%s", path);
#endif
        }
    }

    return FnReturn(buffer);
}

/*********************************************************************/

static FnCallResult FnCallFindfiles(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    Rlist *returnlist = NULL;
    Rlist *arg = NULL;
    int argcount = 0;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall findfiles-arg");

    /* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
        argcount++;
    }

    for (arg = finalargs;  /* Start with arg set to finalargs. */
         arg;              /* We must have arg to proceed. */
         arg = arg->next)  /* arg steps forward every time. */
    {
        char *pattern = RlistScalarValue(arg);
#ifdef __MINGW32__
        RlistAppendScalarIdemp(&returnlist, xstrdup(pattern));
#else /* !__MINGW32__ */
        glob_t globbuf;
        if (0 == glob(pattern, 0, NULL, &globbuf))
        {
            for (int i = 0; i < globbuf.gl_pathc; i++)
            {
                char* found = globbuf.gl_pathv[i];
                char fname[CF_BUFSIZE];
                snprintf(fname, CF_BUFSIZE, "%s", found);
                Log(LOG_LEVEL_VERBOSE, "%s pattern '%s' found match '%s'", fp->name, pattern, fname);
                RlistAppendScalarIdemp(&returnlist, xstrdup(fname));
            }

            globfree(&globbuf);
        }
#endif
    }

    // When no entries were found, mark the empty list
    if (NULL == returnlist)
    {
        RlistAppendScalar(&returnlist, CF_NULL_VALUE);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
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

static const Rlist *GetListReferenceArgument(const EvalContext *ctx, const FnCall *fp, const char *lval_str, DataType *datatype_out)
{
    VarRef *ref = VarRefParse(lval_str);
    Rval rval_out;

    if (!EvalContextVariableGet(ctx, ref, &rval_out, datatype_out))
    {
        Log(LOG_LEVEL_INFO,
            "Could not resolve expected list variable '%s' in function '%s'",
            lval_str,
            fp->name);
        VarRefDestroy(ref);
        return NULL;
    }

    VarRefDestroy(ref);

    if (rval_out.type != RVAL_TYPE_LIST)
    {
        if (datatype_out)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Function '%s' expected a list variable reference, got variable of type '%s'",
                fp->name,
                DataTypeToString(*datatype_out));
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE,
                "Function '%s' expected a list variable reference, got variable of a different type",
                fp->name);
        }
        return NULL;
    }

    return rval_out.item;
}

/*********************************************************************/

static FnCallResult FilterInternal(EvalContext *ctx, FnCall *fp, char *regex, char *name, int do_regex, int invert, long max)
{
    Rlist *returnlist = NULL;

    const Rlist *input_list = GetListReferenceArgument(ctx, fp, name, NULL);
    if (!input_list)
    {
        return FnFailure();
    }

    RlistAppendScalar(&returnlist, CF_NULL_VALUE);

    long match_count = 0;
    long total = 0;
    for (const Rlist *rp = input_list; rp != NULL && match_count < max; rp = rp->next)
    {
        bool found = do_regex ? StringMatchFull(regex, RlistScalarValue(rp)) : (0==strcmp(regex, RlistScalarValue(rp)));

        if (invert ? !found : found)
        {
            RlistAppendScalar(&returnlist, RlistScalarValue(rp));
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
        ProgrammingError("built-in FnCall %s: unhandled FilterInternal() contextmode", fp->name);
    }

    if (contextmode)
    {
        return FnReturnContext(ret);
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

    Rlist *returnlist = NULL;

    const Rlist *input_list = GetListReferenceArgument(ctx, fp, name, NULL);
    if (!input_list)
    {
        return FnFailure();
    }

    RlistAppendScalar(&returnlist, CF_NULL_VALUE);

    if (head)
    {
        long count = 0;
        for (const Rlist *rp = input_list; rp != NULL && count < max; rp = rp->next)
        {
            RlistAppendScalar(&returnlist, RlistScalarValue(rp));
            count++;
        }
    }
    else if (max > 0) // tail mode
    {
        const Rlist *rp = input_list;
        int length = RlistLen((const Rlist *) rp);

        int offset = max >= length ? 0 : length-max;

        for (; rp != NULL && offset--; rp = rp->next)
        {
            /* skip to offset */
        }

        for (; rp != NULL; rp = rp->next)
        {
            RlistAppendScalar(&returnlist, RlistScalarValue(rp));
        }

    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallSetop(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    bool difference = (0 == strcmp(fp->name, "difference"));

    const char *name_a = RlistScalarValue(finalargs);
    const char *name_b = RlistScalarValue(finalargs->next);

    const Rlist *input_list_a = GetListReferenceArgument(ctx, fp, name_a, NULL);
    if (!input_list_a)
    {
        return FnFailure();
    }

    const Rlist *input_list_b = GetListReferenceArgument(ctx, fp, name_b, NULL);
    if (!input_list_b)
    {
        return FnFailure();
    }

    Rlist *returnlist = NULL;
    RlistAppendScalar(&returnlist, CF_NULL_VALUE);

    StringSet *set_b = StringSetNew();
    for (const Rlist *rp_b = input_list_b; rp_b != NULL; rp_b = rp_b->next)
    {
        StringSetAdd(set_b, xstrdup(RlistScalarValue(rp_b)));
    }

    for (const Rlist *rp_a = input_list_a; rp_a != NULL; rp_a = rp_a->next)
    {
        if (strcmp(RlistScalarValue(rp_a), CF_NULL_VALUE) == 0)
        {
            continue;
        }

        // Yes, this is an XOR.  But it's more legible this way.
        if (difference && StringSetContains(set_b, RlistScalarValue(rp_a)))
        {
            continue;
        }

        if (!difference && !StringSetContains(set_b, RlistScalarValue(rp_a)))
        {
            continue;
        }
                
        RlistAppendScalarIdemp(&returnlist, RlistScalarValue(rp_a));
    }

    StringSetDestroy(set_b);

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/
static FnCallResult FnCallFold(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs);
    const char *sort_type = finalargs->next ? RlistScalarValue(finalargs->next) : NULL;

    int count = 0;
    double mean = 0;
    double M2 = 0;
    const Rlist *max = NULL;
    const Rlist *min = NULL;
    bool variance_mode = strcmp(fp->name, "variance") == 0;
    bool mean_mode = strcmp(fp->name, "mean") == 0;

    const Rlist *input_list = GetListReferenceArgument(ctx, fp, name, NULL);
    if (!input_list)
    {
        return FnFailure();
    }

    bool null_seen = false;
    for (const Rlist *rp = input_list; rp != NULL; rp = rp->next)
    {
        const char *cur = RlistScalarValue(rp);
        if (strcmp(cur, CF_NULL_VALUE) == 0)
        {
            null_seen = true;
        }
        else if (sort_type)
        {
            if (NULL == min || NULL == max)
            {
                min = max = rp;
            }
            else
            {
                if (!GenericItemLess(sort_type, (void*) min, (void*) rp))
                {
                    min = rp;
                }

                if (GenericItemLess(sort_type, (void*) max, (void*) rp))
                {
                    max = rp;
                }
            }
        }

        count++;

        // none of the following apply if CF_NULL_VALUE has been seen
        if (null_seen) continue;
        
        double x;
        if (mean_mode || variance_mode)
        {
            if (1 == sscanf(cur, "%lf", &x))
            {
                // Welford's algorithm
                double delta = x - mean;
                mean += delta/count;
                M2 += delta * (x - mean);
            }
        }
    }

    if (count == 1 && null_seen)
    {
        count = 0;
    }

    if (mean_mode)
    {
        return count == 0 ? FnFailure() : FnReturnF("%lf", mean);
    }
    else if (variance_mode)
    {
        double variance = 0;

        if (count == 0) return FnFailure();

        // if count is 1, variance is 0

        if (count > 1)
        {
            variance = M2/(count - 1);
        }

        return FnReturnF("%lf", variance);
    }
    else if (strcmp(fp->name, "length") == 0)
    {
        return FnReturnF("%d", count);
    }
    else if (strcmp(fp->name, "max") == 0)
    {
        return count == 0 ? FnFailure() : FnReturn(RlistScalarValue(max));
    }
    else if (strcmp(fp->name, "min") == 0)
    {
        return count == 0 ? FnFailure() : FnReturn(RlistScalarValue(min));
    }

    ProgrammingError("Unknown function call %s to FnCallFold", fp->name);
    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallUnique(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs);

    Rlist *returnlist = NULL;
    const Rlist *input_list = GetListReferenceArgument(ctx, fp, name, NULL);
    if (!input_list)
    {
        return FnFailure();
    }

    RlistAppendScalar(&returnlist, CF_NULL_VALUE);

    for (const Rlist *rp = input_list; rp != NULL; rp = rp->next)
    {
        RlistAppendScalarIdemp(&returnlist, RlistScalarValue(rp));
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallNth(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    const char* const varname = RlistScalarValue(finalargs);
    long index = IntFromString(RlistScalarValue(finalargs->next));

    VarRef *ref = VarRefParseFromBundle(varname, PromiseGetBundle(fp->caller));
    DataType type = DATA_TYPE_NONE;
    Rval rval;
    EvalContextVariableGet(ctx, ref, &rval, &type);
    VarRefDestroy(ref);

    if (type == DATA_TYPE_CONTAINER)
    {
        Rlist *return_list = NULL;

        const char* const jstring = RvalContainerPrimitiveAsString(rval, index);

        if (jstring != NULL)
        {
            Log(LOG_LEVEL_DEBUG, "%s: from data container %s, got JSON data '%s'", fp->name, varname, jstring);
            RlistAppendScalar(&return_list, jstring);
        }

        if (!return_list)
        {
            return FnFailure();
        }

        return FnReturn(RlistScalarValue(return_list));
    }
    else
    {
        const Rlist *input_list = GetListReferenceArgument(ctx, fp, varname, NULL);
        if (!input_list)
        {
            return FnFailure();
        }

        const Rlist *return_list = NULL;
        for (return_list = input_list; return_list && index--; return_list = return_list->next);

        if (!return_list)
        {
            return FnFailure();
        }

        return FnReturn(RlistScalarValue(return_list));
    }
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

static FnCallResult FnCallSort(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    VarRef *ref = VarRefParse(RlistScalarValue(finalargs));
    const char *sort_type = RlistScalarValue(finalargs->next); // list identifier
    Rval list_var_rval;
    DataType list_var_dtype = DATA_TYPE_NONE;

    if (!EvalContextVariableGet(ctx, ref, &list_var_rval, &list_var_dtype))
    {
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    if (list_var_dtype != DATA_TYPE_STRING_LIST)
    {
        return FnFailure();
    }

    Rlist *sorted;

    if (strcmp(sort_type, "int") == 0)
    {
        sorted = IntSortRListNames(RlistCopy(RvalRlistValue(list_var_rval)));
    }
    else if (strcmp(sort_type, "real") == 0)
    {
        sorted = RealSortRListNames(RlistCopy(RvalRlistValue(list_var_rval)));
    }
    else if (strcmp(sort_type, "IP") == 0 || strcmp(sort_type, "ip") == 0)
    {
        sorted = IPSortRListNames(RlistCopy(RvalRlistValue(list_var_rval)));
    }
    else if (strcmp(sort_type, "MAC") == 0 || strcmp(sort_type, "mac") == 0)
    {
        sorted = MACSortRListNames(RlistCopy(RvalRlistValue(list_var_rval)));
    }
    else // "lex"
    {
        sorted = AlphaSortRListNames(RlistCopy(RvalRlistValue(list_var_rval)));
    }

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { sorted, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFormat(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall %s-arg", fp->name);

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

/* begin fn specific content */
    if (!finalargs)
    {
        return FnFailure();
    }

    char *format = RlistScalarValue(finalargs);

    if (!format)
    {
        return FnFailure();
    }

    const Rlist *rp = finalargs->next;

    char *check = strchr(format, '%');
    char check_buffer[CF_BUFSIZE];
    Buffer *buf = BufferNew();

    if (check)
    {
        BufferAppend(buf, format, (check - format));
        Seq *s = NULL;

        while (check && (s = StringMatchCaptures("^(%%|%[^diouxXeEfFgGaAcsCSpnm%]*?[diouxXeEfFgGaAcsCSpnm])([^%]*)(.*)$", check)))
        {
            {
                if (SeqLength(s) >= 2)
                {
                    const char *format_piece = SeqAt(s, 1);
                    bool percent = (0 == strncmp(format_piece, "%%", 2));
                    char *data = NULL;

                    if (percent)
                    {
                    }
                    else if (rp)
                    {
                        data = RlistScalarValue(rp);
                        rp = rp->next;
                    }
                    else // not %% and no data
                    {
                        Log(LOG_LEVEL_ERR, "format() didn't have enough parameters");
                        BufferDestroy(&buf);
                        return FnFailure();
                    }

                    char piece[CF_BUFSIZE];
                    memset(piece, 0, CF_BUFSIZE);

                    // CfOut(OUTPUT_LEVEL_INFORM, "", "format: processing format piece = '%s' with data '%s'", format_piece, percent ? "%" : data);

                    char bad_modifiers[] = "hLqjzt";
                    for (int b = 0; b < strlen(bad_modifiers); b++)
                    {
                        if (NULL != strchr(format_piece, bad_modifiers[b]))
                        {
                            Log(LOG_LEVEL_ERR, "format() does not allow modifier character '%c' in format specifier '%s'.",
                                  bad_modifiers[b],
                                  format_piece);
                            BufferDestroy(&buf);
                            return FnFailure();
                        }
                    }

                    if (strrchr(format_piece, 'd') || strrchr(format_piece, 'o') || strrchr(format_piece, 'x'))
                    {
                        long x = 0;
                        sscanf(data, "%ld%s", &x, piece); // we don't care about the remainder and will overwrite it
                        snprintf(piece, CF_BUFSIZE, format_piece, x);
                        BufferAppend(buf, piece, strlen(piece));
                        // CfOut(OUTPUT_LEVEL_INFORM, "", "format: appending int format piece = '%s' with data '%s'", format_piece, data);
                    }
                    else if (percent)
                    {
                        BufferAppend(buf, "%", 1);
                        // CfOut(OUTPUT_LEVEL_INFORM, "", "format: appending int format piece = '%s' with data '%s'", format_piece, data);
                    }
                    else if (strrchr(format_piece, 'f'))
                    {
                        double x = 0;
                        sscanf(data, "%lf%s", &x, piece); // we don't care about the remainder and will overwrite it
                        snprintf(piece, CF_BUFSIZE, format_piece, x);
                        BufferAppend(buf, piece, strlen(piece));
                        // CfOut(OUTPUT_LEVEL_INFORM, "", "format: appending float format piece = '%s' with data '%s'", format_piece, data);
                    }
                    else if (strrchr(format_piece, 's'))
                    {
                        snprintf(piece, CF_BUFSIZE, format_piece, data);
                        BufferAppend(buf, piece, strlen(piece));
                        // CfOut(OUTPUT_LEVEL_INFORM, "", "format: appending string format piece = '%s' with data '%s'", format_piece, data);
                    }
                    else
                    {
                        char error[] = "(unhandled format)";
                        BufferAppend(buf, error, strlen(error));
                        // CfOut(OUTPUT_LEVEL_INFORM, "", "format: error appending unhandled format piece = '%s' with data '%s'", format_piece, data);
                    }
                }
                else
                {
                    check = NULL;
                }
            }

            {
                if (SeqLength(s) >= 3)
                {
                    const char* static_piece = SeqAt(s, 2);
                    BufferAppend(buf, static_piece, strlen(static_piece));
                    // CfOut(OUTPUT_LEVEL_INFORM, "", "format: appending static piece = '%s'", static_piece);
                }
                else
                {
                    check = NULL;
                }
            }

            {
                if (SeqLength(s) >= 4)
                {
                    strncpy(check_buffer, SeqAt(s, 3), CF_BUFSIZE);
                    check = check_buffer;
                }
                else
                {
                    check = NULL;
                }
            }

            SeqDestroy(s);
        }
    }
    else
    {
        BufferAppend(buf, format, strlen(format));
    }

    char *result = xstrdup(BufferData(buf));
    BufferDestroy(&buf);

    return (FnCallResult) { FNCALL_SUCCESS, { result, RVAL_TYPE_SCALAR } };

}

/*********************************************************************/

static FnCallResult FnCallIPRange(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char *range = RlistScalarValue(finalargs);
    Item *ip;

    if (!FuzzyMatchParse(range))
    {
        return FnFailure();
    }

    for (ip = EvalContextGetIpAddresses(ctx); ip != NULL; ip = ip->next)
    {
        if (FuzzySetMatch(range, VIPADDRESS) == 0)
        {
            return FnReturnContext(true);
        }
        else
        {
            if (FuzzySetMatch(range, ip->name) == 0)
            {
                return FnReturnContext(true);
            }
        }
    }

    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallHostRange(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *prefix = RlistScalarValue(finalargs);
    char *range = RlistScalarValue(finalargs->next);

    strcpy(buffer, "!any");

    if (!FuzzyHostParse(range))
    {
        return FnFailure();
    }

    return FnReturnContext(FuzzyHostMatch(prefix, range, VUQNAME) == 0);
}

/*********************************************************************/

FnCallResult FnCallHostInNetgroup(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char *host, *user, *domain;

    setnetgrent(RlistScalarValue(finalargs));

    while (getnetgrent(&host, &user, &domain))
    {
        if (host == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "Matched '%s' in netgroup '%s'", VFQNAME, RlistScalarValue(finalargs));
            endnetgrent();
            return FnReturnContext(true);
        }

        if (strcmp(host, VFQNAME) == 0 || strcmp(host, VUQNAME) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Matched '%s' in netgroup '%s'", host, RlistScalarValue(finalargs));
            endnetgrent();
            return FnReturnContext(true);
        }
    }

    endnetgrent();

    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallIsVariable(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    const char *lval = RlistScalarValue(finalargs);
    bool found = false;

    if (!lval)
    {
        found = false;
    }
    else
    {
        VarRef *ref = VarRefParse(lval);
        Rval rval = { 0 };
        found = EvalContextVariableGet(ctx, ref, &rval, NULL);
        VarRefDestroy(ref);
    }

    return FnReturnContext(found);
}

/*********************************************************************/

static FnCallResult FnCallStrCmp(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    return FnReturnContext(strcmp(RlistScalarValue(finalargs), RlistScalarValue(finalargs->next)) == 0);
}

/*********************************************************************/

static FnCallResult FnCallTranslatePath(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[MAX_FILENAME];

    snprintf(buffer, sizeof(buffer), "%s", RlistScalarValue(finalargs));
    MapName(buffer);

    return FnReturn(buffer);
}

/*********************************************************************/

#if defined(__MINGW32__)

static FnCallResult FnCallRegistryValue(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE] = "";

    if (GetRegistryValue(RlistScalarValue(finalargs), RlistScalarValue(finalargs->next), buffer, sizeof(buffer)))
    {
        return FnReturn(buffer);
    }
    else
    {
        return FnFailure();
    }
    return FnFailure();
}

#else /* !__MINGW32__ */

static FnCallResult FnCallRegistryValue(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, ARG_UNUSED Rlist *finalargs)
{
    return FnFailure();
}

#endif /* !__MINGW32__ */

/*********************************************************************/

static FnCallResult FnCallRemoteScalar(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnReturn("<remote scalar>");
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

        return FnReturn(buffer);
    }
}

/*********************************************************************/

static FnCallResult FnCallHubKnowledge(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

    char *handle = RlistScalarValue(finalargs);

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        return FnReturn("<inaccessible remote scalar>");
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Accessing hub knowledge base for '%s'", handle);
        GetRemoteScalar(ctx, "VAR", handle, POLICY_SERVER, true, buffer);

        // This should always be successful - and this one doesn't cache

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            return FnReturn("0");
        }

        return FnReturn(buffer);
    }
}

/*********************************************************************/

static FnCallResult FnCallRemoteClassesMatching(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnReturn("remote_classes");
    }
    else
    {
        GetRemoteScalar(ctx, "CONTEXT", regex, server, encrypted, buffer);

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            return FnFailure();
        }

        if ((classlist = RlistFromSplitString(buffer, ',')))
        {
            for (rp = classlist; rp != NULL; rp = rp->next)
            {
                snprintf(class, CF_MAXVARSIZE - 1, "%s_%s", prefix, RlistScalarValue(rp));
                EvalContextClassPut(ctx, NULL, class, true, CONTEXT_SCOPE_BUNDLE, "source=function,function=remoteclassesmatching");
            }
            RlistDestroy(classlist);
        }

        return FnReturnContext(true);
    }
}

/*********************************************************************/

static FnCallResult FnCallPeers(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnFailure();
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

        if (EmptyString(RlistScalarValue(rp)))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(RlistScalarValue(rp), "%s", s);

        if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
        {
            found = true;
        }
        else
        {
            RlistPrepend(&pruned, s, RVAL_TYPE_SCALAR);
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
        return FnFailure();
    }
}

/*********************************************************************/

static FnCallResult FnCallPeerLeader(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnFailure();
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

        if (EmptyString(RlistScalarValue(rp)))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(RlistScalarValue(rp), "%s", s);

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
        return FnReturn(buffer);
    }
    else
    {
        return FnFailure();
    }

}

/*********************************************************************/

static FnCallResult FnCallPeerLeaders(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnFailure();
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

        if (EmptyString(RlistScalarValue(rp)))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(RlistScalarValue(rp), "%s", s);

        if (i % groupsize == 0)
        {
            if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
            {
                RlistPrepend(&pruned, "localhost", RVAL_TYPE_SCALAR);
            }
            else
            {
                RlistPrepend(&pruned, s, RVAL_TYPE_SCALAR);
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
        return FnFailure();
    }

}

/*********************************************************************/

static FnCallResult FnCallRegCmp(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);
    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);

    return FnReturnContext(StringMatchFull(argv0, argv1));
}

/*********************************************************************/

static FnCallResult FnCallRegExtract(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);
    char *regex = RlistScalarValue(finalargs);
    char *data = RlistScalarValue(finalargs->next);
    char *arrayname = RlistScalarValue(finalargs->next->next);

    Seq *s = StringMatchCaptures(regex, data);

    if (!s || SeqLength(s) == 0)
    {
        SeqDestroy(s);
        return FnReturnContext(false);
    }

    for (int i = 0; i < SeqLength(s); ++i)
    {
        char var[CF_MAXVARSIZE] = "";
        snprintf(var, CF_MAXVARSIZE - 1, "%s[%d]", arrayname, i);
        VarRef *new_ref = VarRefParseFromBundle(var, PromiseGetBundle(fp->caller));
        EvalContextVariablePut(ctx, new_ref, SeqAt(s, i), DATA_TYPE_STRING, "source=function,function=regextract");
        VarRefDestroy(new_ref);
    }

    SeqDestroy(s);
    return FnReturnContext(true);
}

/*********************************************************************/

static FnCallResult FnCallRegLine(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    FILE *fin;

    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);

    if ((fin = safe_fopen(argv1, "r")) == NULL)
    {
        return FnReturnContext(false);
    }

    for (;;)
    {
        char line[CF_BUFSIZE];

        if (fgets(line, sizeof(line), fin) == NULL)
        {
            if (ferror(fin))
            {
                Log(LOG_LEVEL_ERR, "Function regline, unable to read from the file '%s'", argv1);
                fclose(fin);
                return FnFailure();
            }
            else /* feof */
            {
                fclose(fin);
                return FnReturnContext(false);
            }
        }

        if (Chop(line, CF_EXPANDSIZE) == -1)
        {
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }

        if (StringMatchFull(argv0, line))
        {
            fclose(fin);
            return FnReturnContext(true);
        }
    }
}

/*********************************************************************/

static FnCallResult FnCallIsLessGreaterThan(ARG_UNUSED EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);

    if (IsRealNumber(argv0) && IsRealNumber(argv1))
    {
        double a = 0;
        if (!DoubleFromString(argv0, &a))
        {
            return FnFailure();
        }
        double b = 0;
        if (!DoubleFromString(argv1, &b))
        {
            return FnFailure();
        }

        if (!strcmp(fp->name, "isgreaterthan"))
        {
            return FnReturnContext(a > b);
        }
        else
        {
            return FnReturnContext(a < b);
        }
    }

    if (!strcmp(fp->name, "isgreaterthan"))
    {
        return FnReturnContext(strcmp(argv0, argv1) > 0);
    }
    else
    {
        return FnReturnContext(strcmp(argv0, argv1) < 0);
    }
}

/*********************************************************************/

static FnCallResult FnCallIRange(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    long tmp;

/* begin fn specific content */

    long from = IntFromString(RlistScalarValue(finalargs));
    long to = IntFromString(RlistScalarValue(finalargs->next));

    if (from == CF_NOINT || to == CF_NOINT)
    {
        return FnFailure();
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    return FnReturnF("%ld,%ld", from, to);
}

/*********************************************************************/

static FnCallResult FnCallRRange(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    int tmp;

/* begin fn specific content */

    double from = 0;
    if (!DoubleFromString(RlistScalarValue(finalargs), &from))
    {
        Log(LOG_LEVEL_ERR, "Function rrange, error reading assumed real value '%s' => %lf", RlistScalarValue(finalargs), from);
        return FnFailure();
    }

    double to = 0;
    if (!DoubleFromString(RlistScalarValue(finalargs), &to))
    {
        Log(LOG_LEVEL_ERR, "Function rrange, error reading assumed real value '%s' => %lf", RlistScalarValue(finalargs->next), from);
        return FnFailure();
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    return FnReturnF("%lf,%lf", from, to);
}

static FnCallResult FnCallReverse(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    DataType list_dtype = DATA_TYPE_NONE;
    const Rlist *input_list = GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), &list_dtype);
    if (!input_list)
    {
        return FnFailure();
    }

    if (list_dtype != DATA_TYPE_STRING_LIST)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' expected a variable that resolves to a string list, got '%s'", fp->name, DataTypeToString(list_dtype));
        return FnFailure();
    }

    Rlist *copy = RlistCopy(input_list);
    RlistReverse(&copy);

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { copy, RVAL_TYPE_LIST } };
}


/* Convert y/m/d/h/m/s 6-tuple */
static struct tm FnArgsToTm(const Rlist *rp)
{
    struct tm ret = { .tm_isdst = -1 };
    ret.tm_year = IntFromString(RlistScalarValue(rp)) - 1900; /* tm.tm_year stores year - 1900 */
    rp = rp->next;
    ret.tm_mon = IntFromString(RlistScalarValue(rp));
    rp = rp->next;
    ret.tm_mday = IntFromString(RlistScalarValue(rp)) + 1;
    rp = rp->next;
    ret.tm_hour = IntFromString(RlistScalarValue(rp));
    rp = rp->next;
    ret.tm_min = IntFromString(RlistScalarValue(rp));
    rp = rp->next;
    ret.tm_sec = IntFromString(RlistScalarValue(rp));
    return ret;
}

static FnCallResult FnCallOn(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    struct tm tmv = FnArgsToTm(finalargs);
    time_t cftime = mktime(&tmv);

    if (cftime == -1)
    {
        Log(LOG_LEVEL_INFO, "Illegal time value");
    }

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
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (IsDefinedClass(ctx, RlistScalarValue(arg), PromiseGetNamespace(fp->caller)))
        {
            return FnReturnContext(true);
        }
    }

    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallLaterThan(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    time_t now = time(NULL);
    struct tm tmv = FnArgsToTm(finalargs);
    time_t cftime = mktime(&tmv);

    if (cftime == -1)
    {
        Log(LOG_LEVEL_INFO, "Illegal time value");
    }

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

static FnCallResult FnCallAgoDate(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    struct tm ago = FnArgsToTm(finalargs);
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    t.tm_year -= ago.tm_year + 1900; /* tm.tm_year stores year - 1900 */
    t.tm_mon -= ago.tm_mon;
    t.tm_mday -= ago.tm_mday - 1;
    t.tm_hour -= ago.tm_hour;
    t.tm_min -= ago.tm_min;
    t.tm_sec -= ago.tm_sec;

    time_t cftime = mktime(&t);

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    if (cftime < 0)
    {
        strcpy(buffer, "0");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallAccumulatedDate(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    struct tm tmv = FnArgsToTm(finalargs);

    time_t cftime = 0;
    cftime = 0;
    cftime += tmv.tm_sec;
    cftime += tmv.tm_min * 60;
    cftime += tmv.tm_hour * 3600;
    cftime += (tmv.tm_mday -1) * 24 * 3600;
    cftime += tmv.tm_mon * 30 * 24 * 3600;
    cftime += (tmv.tm_year + 1900) * 365 * 24 * 3600;

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), RVAL_TYPE_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallNot(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    return FnReturnContext(!IsDefinedClass(ctx, RlistScalarValue(finalargs), PromiseGetNamespace(fp->caller)));
}

/*********************************************************************/

static FnCallResult FnCallNow(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, ARG_UNUSED Rlist *finalargs)
{
    return FnReturnF("%ld", (long)CFSTARTTIME);
}

/*********************************************************************/

static FnCallResult FnCallStrftime(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        Log(LOG_LEVEL_WARNING, "Function strftime, the given time stamp '%ld' was invalid. (strftime: %s)", when, GetErrorStr());
    }

    return FnReturn(buffer);
}

/*********************************************************************/

static FnCallResult FnCallEval(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char *input = RlistScalarValue(finalargs);
    char *type = RlistScalarValue(finalargs->next);
    char *options = RlistScalarValue(finalargs->next->next);
    if (0 != strcmp(type, "math") || 0 != strcmp(options, "infix"))
    {
        Log(LOG_LEVEL_ERR, "Unknown %s evaluation type %s or options %s", fp->name, type, options);
        return FnFailure();
    }

    char failure[CF_BUFSIZE];
    memset(failure, 0, sizeof(failure));

    double result = EvaluateMathInfix(ctx, input, failure);
    if (strlen(failure) > 0)
    {
        Log(LOG_LEVEL_INFO, "%s error: %s (input '%s')", fp->name, failure, input);
        return FnReturn("");
    }
    else
    {
        return FnReturnF("%lf", result);
    }
}

/*********************************************************************/
/* Read functions                                                    */
/*********************************************************************/

static FnCallResult FnCallReadFile(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    char *contents;

    char *filename = RlistScalarValue(finalargs);
    int maxsize = IntFromString(RlistScalarValue(finalargs->next));

    if (maxsize == 0 || maxsize > CF_BUFSIZE)
    {
        Log(LOG_LEVEL_INFO, "readfile(): max_size is more than internal limit " TOSTRING(CF_BUFSIZE));
        maxsize = CF_BUFSIZE;
    }

    // Read once to validate structure of file in itemlist
    contents = CfReadFile(filename, maxsize);

    if (contents)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { contents, RVAL_TYPE_SCALAR } };
    }
    else
    {
        return FnFailure();
    }
}

/*********************************************************************/

static FnCallResult ReadList(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs, DataType type)
{
    Rlist *rp, *newlist = NULL;
    char fnname[CF_MAXVARSIZE], *file_buffer = NULL;
    int noerrors = true, blanks = false;

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    char *split = RlistScalarValue(finalargs->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next));

    // Read once to validate structure of file in itemlist
    snprintf(fnname, CF_MAXVARSIZE - 1, "read%slist", DataTypeToString(type));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return FnFailure();
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
                Log(LOG_LEVEL_ERR, "Presumed int value '%s' read from file '%s' has no recognizable value",
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
                Log(LOG_LEVEL_ERR, "Presumed real value '%s' read from file '%s' has no recognizable value",
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
        return FnFailure();
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

static FnCallResult FnCallReadJson(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *args)
{
    const char *input_path = RlistScalarValue(args);
    size_t size_max = IntFromString(RlistScalarValue(args->next));

    /* FIXME: fail if truncated? */
    Writer *contents = FileRead(input_path, size_max, NULL);
    if (!contents)
    {
        Log(LOG_LEVEL_ERR, "Error reading JSON input file '%s'", input_path);
        return FnFailure();
    }
    JsonElement *json = NULL;
    const char *data = StringWriterData(contents);
    if (JsonParse(&data, &json) != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_ERR, "Error parsing JSON file '%s'", input_path);
        WriterClose(contents);
        return FnFailure();
    }

    WriterClose(contents);
    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
}

static FnCallResult FnCallParseJson(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *args)
{
    const char *data = RlistScalarValue(args);
    JsonElement *json = NULL;
    if (JsonParse(&data, &json) != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_ERR, "Error parsing JSON expression '%s'", data);
        return FnFailure();
    }

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
}

/*********************************************************************/

static FnCallResult FnCallStoreJson(EvalContext *ctx, FnCall *fp, Rlist *finalargs)
{
    char buf[CF_BUFSIZE];
    char *varname = RlistScalarValue(finalargs);
    VarRef *ref = VarRefParseFromBundle(varname, PromiseGetBundle(fp->caller));

    DataType type = DATA_TYPE_NONE;
    Rval rval;
    EvalContextVariableGet(ctx, ref, &rval, &type);

    if (type == DATA_TYPE_CONTAINER)
    {
        Writer *w = StringWriter();
        int length;

        JsonWrite(w, RvalContainerValue(rval), 0);
        Log(LOG_LEVEL_DEBUG, "%s: from data container %s, got JSON data '%s'", fp->name, varname, StringWriterData(w));

        length = strlen(StringWriterData(w));
        if (length >= CF_BUFSIZE)
        {
            Log(LOG_LEVEL_INFO, "%s: truncating data container %s JSON data from %d bytes to %d", fp->name, varname, length, CF_BUFSIZE);
        }

        snprintf(buf, CF_BUFSIZE, "%s", StringWriterData(w));
        WriterClose(w);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s: data container %s could not be found or has an invalid type", fp->name, varname);
        return FnFailure();
    }

    return FnReturn(buf);
}


/*********************************************************************/

static FnCallResult ReadArray(EvalContext *ctx, FnCall *fp, Rlist *finalargs, DataType type, int intIndex)
/* lval,filename,separator,comment,Max number of bytes  */
{
    char *file_buffer = NULL;
    int entries = 0;

/* begin fn specific content */

    /* 6 args: array_lval,filename,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *array_lval = RlistScalarValue(finalargs);
    char *filename = RlistScalarValue(finalargs->next);
    char *comment = RlistScalarValue(finalargs->next->next);
    char *split = RlistScalarValue(finalargs->next->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next->next));

// Read once to validate structure of file in itemlist
    file_buffer = CfReadFile(filename, maxsize);
    if (!file_buffer)
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

    return FnReturnF("%d", entries);
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
    int entries = 0;

/* begin fn specific content */

    /* 6 args: array_lval,instring,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *array_lval = RlistScalarValue(finalargs);
    char *instring = xstrdup(RlistScalarValue(finalargs->next));
    char *comment = RlistScalarValue(finalargs->next->next);
    char *split = RlistScalarValue(finalargs->next->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next->next));

// Read once to validate structure of file in itemlist

    Log(LOG_LEVEL_DEBUG, "Parse string data from string '%s' - , maxent %d, maxsize %d", instring, maxent, maxsize);

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

    return FnReturnF("%d", entries);
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

static FnCallResult FnCallSplitString(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        RlistPrepend(&newlist, CF_NULL_VALUE, RVAL_TYPE_SCALAR);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFileSexist(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *files;
    char naked[CF_MAXVARSIZE];
    Rval retval;
    struct stat sb;

/* begin fn specific content */

    char *listvar = RlistScalarValue(finalargs);

    if (IsVarList(listvar))
    {
        GetNaked(naked, listvar);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Function filesexist was promised a list called '%s' but this was not found", listvar);
        return FnFailure();
    }

    VarRef *ref = VarRefParse(naked);

    if (!EvalContextVariableGet(ctx, ref, &retval, NULL))
    {
        Log(LOG_LEVEL_VERBOSE, "Function filesexist was promised a list called '%s' but this was not found", listvar);
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    if (retval.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE, "Function filesexist was promised a list called '%s' but this variable is not a list",
              listvar);
        return FnFailure();
    }

    files = (Rlist *) retval.item;

    for (rp = files; rp != NULL; rp = rp->next)
    {
        if (stat(RlistScalarValue(rp), &sb) == -1)
        {
            return FnReturnContext(false);
        }
    }

    return FnReturnContext(true);
}

/*********************************************************************/
/* LDAP Nova features                                                */
/*********************************************************************/

static FnCallResult FnCallLDAPValue(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnFailure();
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
        return FnFailure();
    }

}

/*********************************************************************/

static FnCallResult FnCallLDAPList(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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
        return FnFailure();
    }

}

/*********************************************************************/

static FnCallResult FnCallRegLDAP(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
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

    if ((newval = CfRegLDAP(ctx, uri, dn, filter, name, scope, regex, sec)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, RVAL_TYPE_SCALAR } };
    }
    else
    {
        return FnFailure();
    }

}

/*********************************************************************/

#define KILOBYTE 1024

static FnCallResult FnCallDiskFree(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    off_t df;

    df = GetDiskUsage(RlistScalarValue(finalargs), cfabs);

    if (df == CF_INFINITY)
    {
        df = 0;
    }

    return FnReturnF("%jd", ((intmax_t) df) / KILOBYTE);
}


static FnCallResult FnCallMakerule(EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    const char *target = RlistScalarValue(finalargs);
    const char *listvar = RlistScalarValue(finalargs->next);
    Rlist *list = NULL;
    bool stale = false;
    time_t target_time = 0;

    // TODO: replace IsVarList with GetListReferenceArgument
    if (!IsVarList(listvar))
    {
        RlistPrepend(&list, listvar, RVAL_TYPE_SCALAR);
    }
    else
    {
        char naked[CF_MAXVARSIZE] = "";
        GetNaked(naked, listvar);

        VarRef *ref = VarRefParse(naked);

        Rval retval;
        if (!EvalContextVariableGet(ctx, ref, &retval, NULL))
        {
            Log(LOG_LEVEL_VERBOSE, "Function 'makerule' was promised a list called '%s' but this was not found", listvar);
            VarRefDestroy(ref);
            return FnFailure();
        }

       if (retval.type != RVAL_TYPE_LIST)
       {
           Log(LOG_LEVEL_WARNING, "Function 'makerule' was promised a list called '%s' but this variable is not a list", listvar);
           return FnFailure();
       }

       list = retval.item;
    }

    struct stat statbuf;
    if (lstat(target, &statbuf) == -1)
    {
        stale = true;
    }
    else
    {
        if (!S_ISREG(statbuf.st_mode))
        {
            Log(LOG_LEVEL_WARNING, "Function 'makerule' target-file '%s' exists and is not a plain file", target);
            // Not a probe's responsibility to fix - but have this for debugging
        }

        target_time = statbuf.st_mtime;
    }

    // For each file in sources, check they exist and are older than target

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (lstat(RvalScalarValue(rp->val), &statbuf) == -1)
        {
            Log(LOG_LEVEL_INFO, "Function MAKERULE, source dependency %s was not readable",  RvalScalarValue(rp->val));
            return FnFailure();
        }
        else
        {
            if (statbuf.st_mtime > target_time)
            {
                stale = true;
            }
        }
    }

    return stale ? FnReturnContext(true) : FnFailure();
}


#if !defined(__MINGW32__)

FnCallResult FnCallUserExists(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    struct passwd *pw;
    uid_t uid = CF_SAME_OWNER;
    char *arg = RlistScalarValue(finalargs);

    if (StringIsNumeric(arg))
    {
        uid = Str2Uid(arg, NULL, NULL);

        if (uid == CF_SAME_OWNER || uid == CF_UNKNOWN_OWNER)
        {
            return FnFailure();
        }

        if ((pw = getpwuid(uid)) == NULL)
        {
            return FnReturnContext(false);
        }
    }
    else if ((pw = getpwnam(arg)) == NULL)
    {
        return FnReturnContext(false);
    }

    return FnReturnContext(true);
}

/*********************************************************************/

FnCallResult FnCallGroupExists(ARG_UNUSED EvalContext *ctx, ARG_UNUSED FnCall *fp, Rlist *finalargs)
{
    struct group *gr;
    gid_t gid = CF_SAME_GROUP;
    char *arg = RlistScalarValue(finalargs);

    if (isdigit((int) *arg))
    {
        gid = Str2Gid(arg, NULL, NULL);

        if (gid == CF_SAME_GROUP || gid == CF_UNKNOWN_GROUP)
        {
            return FnFailure();
        }

        if ((gr = getgrgid(gid)) == NULL)
        {
            return FnReturnContext(false);
        }
    }
    else if ((gr = getgrnam(arg)) == NULL)
    {
        return FnReturnContext(false);
    }

    return FnReturnContext(true);
}

#endif /* !defined(__MINGW32__) */

static bool SingleLine(const char *s)
{
    size_t length = strcspn(s, "\n\r");
    /* [\n\r] followed by EOF */
    return s[length] && !s[length+1];
}

static void *CfReadFile(char *filename, int maxsize)
{
    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
        {
            Log(LOG_LEVEL_INFO, "readfile: Could not examine file '%s'", filename);
        }
        else
        {
            if (IsCf3VarString(filename))
            {
                Log(LOG_LEVEL_VERBOSE, "readfile: Cannot converge/reduce variable '%s' yet .. assuming it will resolve later",
                      filename);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "readfile: Could not examine file '%s' (stat: %s)",
                      filename, GetErrorStr());
            }
        }
        return NULL;
    }

    /* 0 means 'read until the end of file' */
    size_t limit = maxsize ? maxsize : SIZE_MAX;
    bool truncated = false;
    Writer *w = FileRead(filename, limit, &truncated);

    if (!w)
    {
        Log(LOG_LEVEL_INFO, "CfReadFile: Error while reading file '%s' (%s)",
            filename, GetErrorStr());
        return NULL;
    }

    if (truncated)
    {
        Log(LOG_LEVEL_INFO, "CfReadFile: Truncating file '%s' to %d bytes as "
            "requested by maxsize parameter", filename, maxsize);
    }

    size_t size = StringWriterLength(w);
    char *result = StringWriterClose(w);

    /* FIXME: Is it necessary here? Move to caller(s) */
    if (SingleLine(result))
        StripTrailingNewline(result, size);

    return result;
}

/*********************************************************************/

static char *StripPatterns(char *file_buffer, char *pattern, char *filename)
{
    int start, end;
    int count = 0;

    if (!NULL_OR_EMPTY(pattern))
    {
        while (StringMatch(pattern, file_buffer, &start, &end))
        {
            CloseStringHole(file_buffer, start, end);

            if (count++ > strlen(file_buffer))
            {
                Log(LOG_LEVEL_ERR,
                    "Comment regex '%s' was irreconcilable reading input '%s' probably because it legally matches nothing",
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
    int allowblanks = true, vcount, hcount;
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
                strncpy(this_rval, RlistScalarValue(rp), CF_MAXVARSIZE - 1);
                break;

            case DATA_TYPE_INT:
                ival = IntFromString(RlistScalarValue(rp));
                snprintf(this_rval, CF_MAXVARSIZE, "%d", (int) ival);
                break;

            case DATA_TYPE_REAL:
                {
                    double real_value = 0;
                    if (!DoubleFromString(RlistScalarValue(rp), &real_value))
                    {
                        FatalError(ctx, "Could not convert rval to double");
                    }
                }
                sscanf(RlistScalarValue(rp), "%255s", this_rval);
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

            VarRef *ref = VarRefParseFromBundle(name, bundle);
            EvalContextVariablePut(ctx, ref, this_rval, type, "source=function,function=buildlinearray");
            VarRefDestroy(ref);
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
    char context[CF_BUFSIZE];
    int print = false;

    context[0] = '\0';

    if ((pp = cf_popen(command, "rt", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe from '%s'. (cf_popen: %s)", command, GetErrorStr());
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
            Log(LOG_LEVEL_ERR, "Unable to read output from '%s'. (fread: %s)", command, GetErrorStr());
            cf_pclose(pp);
            return false;
        }

        if (strlen(line) > CF_BUFSIZE - 80)
        {
            Log(LOG_LEVEL_ERR, "Line from module '%s' is too long to be sensible", command);
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

        ModuleProtocol(ctx, command, line, print, ns, context);
    }

    cf_pclose(pp);
    return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void ModuleProtocol(EvalContext *ctx, char *command, char *line, int print, const char *ns, char* context)
{
    char name[CF_BUFSIZE], content[CF_BUFSIZE];
    char arg0[CF_BUFSIZE];
    char *filename;

    if (*context == '\0')
    {
/* Infer namespace from script name */

        snprintf(arg0, CF_BUFSIZE, "%s", CommandArg0(command));
        filename = basename(arg0);

/* Canonicalize filename into acceptable namespace name*/

        CanonifyNameInPlace(filename);
        strcpy(context, filename);
        Log(LOG_LEVEL_VERBOSE, "Module context '%s'", context);
    }

    name[0] = '\0';
    content[0] = '\0';

    switch (*line)
    {
    case '^':
        content[0] = '\0';

        // Allow modules to set their variable context (up to 50 characters)
        if (1 == sscanf(line + 1, "context=%50[a-z]", content) && strlen(content) > 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Module changed variable context from '%s' to '%s'", context, content);
            strcpy(context, content);
        }
        break;

    case '+':
        Log(LOG_LEVEL_VERBOSE, "Activated classes '%s'", line + 1);
        if (CheckID(line + 1))
        {
             EvalContextClassPut(ctx, ns, line + 1, true, CONTEXT_SCOPE_NAMESPACE, "source=module");
        }
        break;
    case '-':
        Log(LOG_LEVEL_VERBOSE, "Deactivated classes '%s'", line + 1);
        if (CheckID(line + 1))
        {
            if (line[1] != '\0')
            {
                StringSet *negated = StringSetFromString(line + 1, ',');
                StringSetIterator it = StringSetIteratorInit(negated);
                const char *negated_context = NULL;
                while ((negated_context = StringSetIteratorNext(&it)))
                {
                    Class *cls = EvalContextClassGet(ctx, NULL, negated_context);
                    if (cls && !cls->is_soft)
                    {
                        FatalError(ctx, "Cannot negate the reserved class '%s'", negated_context);
                    }

                    ClassRef ref = ClassRefParse(negated_context);
                    EvalContextClassRemove(ctx, ref.ns, ref.name);
                    ClassRefDestroy(ref);
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
            Log(LOG_LEVEL_VERBOSE, "Defined variable '%s' in context '%s' with value '%s'", name, context, content);
            VarRef *ref = VarRefParseFromScope(name, context);
            EvalContextVariablePut(ctx, ref, content, DATA_TYPE_STRING, "source=module");
            VarRefDestroy(ref);
        }
        break;

    case '@':
        content[0] = '\0';
        sscanf(line + 1, "%[^=]=%[^\n]", name, content);

        if (CheckID(name))
        {
            Rlist *list = NULL;

            list = RlistParseString(content);
            Log(LOG_LEVEL_VERBOSE, "Defined variable '%s' in context '%s' with value '%s'", name, context, content);

            VarRef *ref = VarRefParseFromScope(name, context);
            EvalContextVariablePut(ctx, ref, list, DATA_TYPE_STRING_LIST, "source=module");
            VarRefDestroy(ref);
        }
        break;

    case '\0':
        break;

    default:
        if (print)
        {
            Log(LOG_LEVEL_INFO, "M '%s': %s", command, line);
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
            Log(LOG_LEVEL_ERR,
                  "Module protocol contained an illegal character '%c' in class/variable identifier '%s'.", *sp,
                  id);
            return false;
        }
    }

    return true;
}

/*********************************************************/
/* Function prototypes                                   */
/*********************************************************/

static const FnCallArg ACCESSEDBEFORE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Newer filename"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Older filename"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg ACCUM_ARGS[] =
{
    {"0,1000", DATA_TYPE_INT, "Years"},
    {"0,1000", DATA_TYPE_INT, "Months"},
    {"0,1000", DATA_TYPE_INT, "Days"},
    {"0,1000", DATA_TYPE_INT, "Hours"},
    {"0,1000", DATA_TYPE_INT, "Minutes"},
    {"0,40000", DATA_TYPE_INT, "Seconds"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg AND_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg AGO_ARGS[] =
{
    {"0,1000", DATA_TYPE_INT, "Years"},
    {"0,1000", DATA_TYPE_INT, "Months"},
    {"0,1000", DATA_TYPE_INT, "Days"},
    {"0,1000", DATA_TYPE_INT, "Hours"},
    {"0,1000", DATA_TYPE_INT, "Minutes"},
    {"0,40000", DATA_TYPE_INT, "Seconds"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg LATERTHAN_ARGS[] =
{
    {"0,1000", DATA_TYPE_INT, "Years"},
    {"0,1000", DATA_TYPE_INT, "Months"},
    {"0,1000", DATA_TYPE_INT, "Days"},
    {"0,1000", DATA_TYPE_INT, "Hours"},
    {"0,1000", DATA_TYPE_INT, "Minutes"},
    {"0,40000", DATA_TYPE_INT, "Seconds"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg CANONIFY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "String containing non-identifier characters"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg CHANGEDBEFORE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Newer filename"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Older filename"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg CLASSIFY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg CLASSMATCH_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg CONCAT_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg COUNTCLASSESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg COUNTLINESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Filename"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg DIRNAME_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "File path"},
    {NULL, DATA_TYPE_NONE, NULL},
};

static const FnCallArg DISKFREE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File system directory"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg ESCAPE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "IP address or string to escape"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg EXECRESULT_ARGS[] =
{
    {CF_PATHRANGE, DATA_TYPE_STRING, "Fully qualified command path"},
    {"noshell,useshell,powershell", DATA_TYPE_OPTION, "Shell encapsulation option"},
    {NULL, DATA_TYPE_NONE, NULL}
};

// fileexists, isdir,isplain,islink

static const FnCallArg FILESTAT_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File object name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILESTAT_DETAIL_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File object name"},
    {"size,gid,uid,ino,nlink,ctime,atime,mtime,mode,modeoct,permstr,permoct,type,devno,dev_minor,dev_major,basename,dirname,linktarget,linktarget_shallow", DATA_TYPE_OPTION, "stat() field to get"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILESEXIST_ARGS[] =
{
    {CF_NAKEDLRANGE, DATA_TYPE_STRING, "Array identifier containing list"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg FINDFILES_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILTER_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression or string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_BOOL, DATA_TYPE_OPTION, "Match as regular expression if true, as exact string otherwise"},
    {CF_BOOL, DATA_TYPE_OPTION, "Invert matches"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of matches to return"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETFIELDS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression to match line"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Filename to read"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression to split fields"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Return array name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETINDICES_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine array or data container identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETUSERS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comma separated list of User names"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comma separated list of UserID numbers"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETENV_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Name of environment variable"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of characters to read "},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETGID_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Group name in text"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETUID_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "User name in text"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GREP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg GROUPEXISTS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Group name or identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HASH_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input text"},
    {"md5,sha1,sha256,sha512,sha384,crypt", DATA_TYPE_OPTION, "Hash or digest algorithm"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HASHMATCH_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Filename to hash"},
    {"md5,sha1,crypt,cf_sha224,cf_sha256,cf_sha384,cf_sha512", DATA_TYPE_OPTION, "Hash or digest algorithm"},
    {CF_IDRANGE, DATA_TYPE_STRING, "ASCII representation of hash for comparison"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOST2IP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Host name in ascii"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg IP2HOST_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "IP address (IPv4 or IPv6)"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTINNETGROUP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Netgroup name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTRANGE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Hostname prefix"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Enumerated range"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTSSEEN_ARGS[] =
{
    {CF_VALRANGE, DATA_TYPE_INT, "Horizon since last seen in hours"},
    {"lastseen,notseen", DATA_TYPE_OPTION, "Complements for selection policy"},
    {"name,address", DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTSWITHCLASS_ARGS[] =
{
    {"[a-zA-Z0-9_]+", DATA_TYPE_STRING, "Class name to look for"},
    {"name,address", DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg IFELSE_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg IPRANGE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "IP address range syntax"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg IRANGE_ARGS[] =
{
    {CF_INTRANGE, DATA_TYPE_INT, "Integer"},
    {CF_INTRANGE, DATA_TYPE_INT, "Integer"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISGREATERTHAN_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Larger string or value"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Smaller string or value"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISLESSTHAN_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Smaller string or value"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Larger string or value"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISNEWERTHAN_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Newer file name"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Older file name"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISVARIABLE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Variable identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg JOIN_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Join glue-string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg LASTNODE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Link separator, e.g. /,:"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg LDAPARRAY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Array name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filter"},
    {"subtree,onelevel,base", DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", DATA_TYPE_OPTION, "Security level"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg LDAPLIST_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", DATA_TYPE_OPTION, "Security level"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg LDAPVALUE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", DATA_TYPE_OPTION, "Security level"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg LSDIRLIST_ARGS[] =
{
    {CF_PATHRANGE, DATA_TYPE_STRING, "Path to base directory"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression to match files or blank"},
    {CF_BOOL, DATA_TYPE_OPTION, "Include the base path in the list"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAPLIST_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Pattern based on $(this) as original text"},
    {CF_IDRANGE, DATA_TYPE_STRING, "The name of the list variable to map"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAPARRAY_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Pattern based on $(this.k) and $(this.v) as original text"},
    {CF_IDRANGE, DATA_TYPE_STRING, "The name of the array variable to map"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg MERGEDATA_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg NOT_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Class value"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg NOW_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg OR_ARGS[] =
{
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SUM_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "A list of arbitrary real values"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PRODUCT_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "A list of arbitrary real values"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATE_ARGS[] =
{
    {"1970,3000", DATA_TYPE_INT, "Year"},
    {"1,12", DATA_TYPE_INT, "Month"},
    {"1,31", DATA_TYPE_INT, "Day"},
    {"0,23", DATA_TYPE_INT, "Hour"},
    {"0,59", DATA_TYPE_INT, "Minute"},
    {"0,59", DATA_TYPE_INT, "Second"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PACKAGESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression (unanchored) to match package name"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression (unanchored) to match package version"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression (unanchored) to match package architecture"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression (unanchored) to match package method"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PEERS_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comment regex pattern"},
    {CF_VALRANGE, DATA_TYPE_INT, "Peer group size"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PEERLEADER_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comment regex pattern"},
    {CF_VALRANGE, DATA_TYPE_INT, "Peer group size"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PEERLEADERS_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Comment regex pattern"},
    {CF_VALRANGE, DATA_TYPE_INT, "Peer group size"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg RANDOMINT_ARGS[] =
{
    {CF_INTRANGE, DATA_TYPE_INT, "Lower inclusive bound"},
    {CF_INTRANGE, DATA_TYPE_INT, "Upper exclusive bound"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg READFILE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg READSTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name to read"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PARSESTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "A string to parse for input data"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg READSTRINGARRAYIDX_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "A string to parse for input data"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PARSESTRINGARRAYIDX_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "A string to parse for input data"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg READSTRINGLIST_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name to read"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg READJSON_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "File name to read"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg PARSEJSON_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "JSON string to parse"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg STOREJSON_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine data container identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg READTCP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Host name or IP address of server socket"},
    {CF_VALRANGE, DATA_TYPE_INT, "Port number"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Protocol query string"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGARRAY_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine array identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGCMP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Match string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGEXTRACT_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Match string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "Identifier for back-references"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGISTRYVALUE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Windows registry key"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Windows registry value-id"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGLINE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Filename to search"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGLIST_ARGS[] =
{
    {CF_NAKEDLRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAKERULE_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Target filename"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Source filename or CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGLDAP_ARGS[] =
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

static const FnCallArg REMOTESCALAR_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Variable identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Hostname or IP address of server"},
    {CF_BOOL, DATA_TYPE_OPTION, "Use enryption"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg HUB_KNOWLEDGE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "Variable identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REMOTECLASSESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Server name or address"},
    {CF_BOOL, DATA_TYPE_OPTION, "Use encryption"},
    {CF_IDRANGE, DATA_TYPE_STRING, "Return class prefix"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg RETURNSZERO_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Fully qualified command path"},
    {"noshell,useshell,powershell", DATA_TYPE_OPTION, "Shell encapsulation option"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg RRANGE_ARGS[] =
{
    {CF_REALRANGE, DATA_TYPE_REAL, "Real number"},
    {CF_REALRANGE, DATA_TYPE_REAL, "Real number"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SELECTSERVERS_ARGS[] =
{
    {CF_NAKEDLRANGE, DATA_TYPE_STRING, "The identifier of a cfengine list of hosts or addresses to contact"},
    {CF_VALRANGE, DATA_TYPE_INT, "The port number"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "A query string"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "A regular expression to match success"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of bytes to read from server"},
    {CF_IDRANGE, DATA_TYPE_STRING, "Name for array of results"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SPLAYCLASS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string for classification"},
    {"daily,hourly", DATA_TYPE_OPTION, "Splay time policy"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SPLITSTRING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "A data string"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regex to split on"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of pieces"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRCMP_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "String"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "String"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRFTIME_ARGS[] =
{
    {"gmtime,localtime", DATA_TYPE_OPTION, "Use GMT or local time"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "A format string"},
    {CF_VALRANGE, DATA_TYPE_INT, "The time as a Unix epoch offset"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SUBLIST_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {"head,tail", DATA_TYPE_OPTION, "Whether to return elements from the head or from the tail of the list"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of elements to return"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg TRANSLATEPATH_ARGS[] =
{
    {CF_ABSPATHRANGE, DATA_TYPE_STRING, "Unix style path"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg USEMODULE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Name of module command"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Argument string for the module"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg UNIQUE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg NTH_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list or data container identifier"},
    {CF_VALRANGE, DATA_TYPE_INT, "Offset of element to return"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg EVERY_SOME_NONE_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression or string"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg USEREXISTS_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "User name or identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SORT_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {"lex,int,real,IP,ip,MAC,mac", DATA_TYPE_OPTION, "Sorting method: lex or int or real (floating point) or IPv4/IPv6 or MAC address"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg REVERSE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SHUFFLE_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_ANYSTRING, DATA_TYPE_STRING, "Any seed string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg STAT_FOLD_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg SETOP_ARGS[] =
{
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine base list identifier"},
    {CF_IDRANGE, DATA_TYPE_STRING, "CFEngine filter list identifier"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg FORMAT_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "CFEngine format string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg EVAL_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string"},
    {"math", DATA_TYPE_OPTION, "Evaluation type"},
    {"infix", DATA_TYPE_OPTION, "Evaluation options"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg BUNDLESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Regular expression"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg XFORM_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string"},
    {NULL, DATA_TYPE_NONE, NULL}
};

static const FnCallArg XFORM_SUBSTR_ARGS[] =
{
    {CF_ANYSTRING, DATA_TYPE_STRING, "Input string"},
    {CF_VALRANGE, DATA_TYPE_INT, "Maximum number of characters to return"},
    {NULL, DATA_TYPE_NONE, NULL}
};

/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

const FnCallType CF_FNCALL_TYPES[] =
{
    FnCallTypeNew("accessedbefore", DATA_TYPE_CONTEXT, ACCESSEDBEFORE_ARGS, &FnCallIsAccessedBefore, "True if arg1 was accessed before arg2 (atime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("accumulated", DATA_TYPE_INT, ACCUM_ARGS, &FnCallAccumulatedDate, "Convert an accumulated amount of time into a system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ago", DATA_TYPE_INT, AGO_ARGS, &FnCallAgoDate, "Convert a time relative to now to an integer system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("and", DATA_TYPE_STRING, AND_ARGS, &FnCallAnd, "Calculate whether all arguments evaluate to true",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("bundlesmatching", DATA_TYPE_STRING_LIST, BUNDLESMATCHING_ARGS, &FnCallBundlesmatching, "Find all the bundles that match a regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("canonify", DATA_TYPE_STRING, CANONIFY_ARGS, &FnCallCanonify, "Convert an abitrary string into a legal class name",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("canonifyuniquely", DATA_TYPE_STRING, CANONIFY_ARGS, &FnCallCanonify, "Convert an abitrary string into a unique legal class name",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("concat", DATA_TYPE_STRING, CONCAT_ARGS, &FnCallConcat, "Concatenate all arguments into string",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("changedbefore", DATA_TYPE_CONTEXT, CHANGEDBEFORE_ARGS, &FnCallIsChangedBefore, "True if arg1 was changed before arg2 (ctime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classify", DATA_TYPE_CONTEXT, CLASSIFY_ARGS, &FnCallClassify, "True if the canonicalization of the argument is a currently defined class",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classmatch", DATA_TYPE_CONTEXT, CLASSMATCH_ARGS, &FnCallClassMatch, "True if the regular expression matches any currently defined class",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classesmatching", DATA_TYPE_STRING_LIST, CLASSMATCH_ARGS, &FnCallClassesMatching, "List the defined classes matching regex arg1 and tag regexes arg2,arg3,...",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countclassesmatching", DATA_TYPE_INT, COUNTCLASSESMATCHING_ARGS, &FnCallCountClassesMatching, "Count the number of defined classes matching regex arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countlinesmatching", DATA_TYPE_INT, COUNTLINESMATCHING_ARGS, &FnCallCountLinesMatching, "Count the number of lines matching regex arg1 in file arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("difference", DATA_TYPE_STRING_LIST, SETOP_ARGS, &FnCallSetop, "Returns all the unique elements of list arg1 that are not in list arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("dirname", DATA_TYPE_STRING, DIRNAME_ARGS, &FnCallDirname, "Return the parent directory name for given path",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("diskfree", DATA_TYPE_INT, DISKFREE_ARGS, &FnCallDiskFree, "Return the free space (in KB) available on the directory's current partition (0 if not found)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("escape", DATA_TYPE_STRING, ESCAPE_ARGS, &FnCallEscape, "Escape regular expression characters in a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("eval", DATA_TYPE_STRING, EVAL_ARGS, &FnCallEval, "Evaluate a mathematical expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("every", DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if every element in the named list matches the given regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("execresult", DATA_TYPE_STRING, EXECRESULT_ARGS, &FnCallExecResult, "Execute named command and assign output to variable",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("fileexists", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named file can be accessed",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesexist", DATA_TYPE_CONTEXT, FILESEXIST_ARGS, &FnCallFileSexist, "True if the named list of files can ALL be accessed",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesize", DATA_TYPE_INT, FILESTAT_ARGS, &FnCallFileStat, "Returns the size in bytes of the file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filestat", DATA_TYPE_STRING, FILESTAT_DETAIL_ARGS, &FnCallFileStatDetails, "Returns stat() details of the file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filter", DATA_TYPE_STRING_LIST, FILTER_ARGS, &FnCallFilter, "Similarly to grep(), filter the list arg2 for matches to arg2.  The matching can be as a regular expression or exactly depending on arg3.  The matching can be inverted with arg4.  A maximum on the number of matches returned can be set with arg5.",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("findfiles", DATA_TYPE_STRING_LIST, FINDFILES_ARGS, &FnCallFindfiles, "Find files matching a shell glob pattern",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("format", DATA_TYPE_STRING, FORMAT_ARGS, &FnCallFormat, "Applies a list of string values in arg2,arg3... to a string format in arg1 with sprintf() rules",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getenv", DATA_TYPE_STRING, GETENV_ARGS, &FnCallGetEnv, "Return the environment variable named arg1, truncated at arg2 characters",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getfields", DATA_TYPE_INT, GETFIELDS_ARGS, &FnCallGetFields, "Get an array of fields in the lines matching regex arg1 in file arg2, split on regex arg3 as array name arg4",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getgid", DATA_TYPE_INT, GETGID_ARGS, &FnCallGetGid, "Return the integer group id of the named group on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getindices", DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetIndices, "Get a list of keys to the array whose id is the argument and assign to variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getuid", DATA_TYPE_INT, GETUID_ARGS, &FnCallGetUid, "Return the integer user id of the named user on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getusers", DATA_TYPE_STRING_LIST, GETUSERS_ARGS, &FnCallGetUsers, "Get a list of all system users defined, minus those names defined in arg1 and uids in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getvalues", DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetValues, "Get a list of values corresponding to the right hand sides in an array whose id is the argument and assign to variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("grep", DATA_TYPE_STRING_LIST, GREP_ARGS, &FnCallGrep, "Extract the sub-list if items matching the regular expression in arg1 of the list named in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("groupexists", DATA_TYPE_CONTEXT, GROUPEXISTS_ARGS, &FnCallGroupExists, "True if group or numerical id exists on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hash", DATA_TYPE_STRING, HASH_ARGS, &FnCallHandlerHash, "Return the hash of arg1, type arg2 and assign to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hashmatch", DATA_TYPE_CONTEXT, HASHMATCH_ARGS, &FnCallHashMatch, "Compute the hash of arg1, of type arg2 and test if it matches the value in arg3",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("host2ip", DATA_TYPE_STRING, HOST2IP_ARGS, &FnCallHost2IP, "Returns the primary name-service IP address for the named host",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ip2host", DATA_TYPE_STRING, IP2HOST_ARGS, &FnCallIP2Host, "Returns the primary name-service host name for the IP address",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostinnetgroup", DATA_TYPE_CONTEXT, HOSTINNETGROUP_ARGS, &FnCallHostInNetgroup, "True if the current host is in the named netgroup",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostrange", DATA_TYPE_CONTEXT, HOSTRANGE_ARGS, &FnCallHostRange, "True if the current host lies in the range of enumerated hostnames specified",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostsseen", DATA_TYPE_STRING_LIST, HOSTSSEEN_ARGS, &FnCallHostsSeen, "Extract the list of hosts last seen/not seen within the last arg1 hours",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostswithclass", DATA_TYPE_STRING_LIST, HOSTSWITHCLASS_ARGS, &FnCallHostsWithClass, "Extract the list of hosts with the given class set from the hub database (enterprise extension)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hubknowledge", DATA_TYPE_STRING, HUB_KNOWLEDGE_ARGS, &FnCallHubKnowledge, "Read global knowledge from the hub host by id (enterprise extension)",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ifelse", DATA_TYPE_STRING, IFELSE_ARGS, &FnCallIfElse, "Do If-ElseIf-ElseIf-...-Else evaluation of arguments",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("intersection", DATA_TYPE_STRING_LIST, SETOP_ARGS, &FnCallSetop, "Returns all the unique elements of list arg1 that are also in list arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("iprange", DATA_TYPE_CONTEXT, IPRANGE_ARGS, &FnCallIPRange, "True if the current host lies in the range of IP addresses specified",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("irange", DATA_TYPE_INT_RANGE, IRANGE_ARGS, &FnCallIRange, "Define a range of integer values for cfengine internal use",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isdir", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a directory",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isexecutable", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object has execution rights for the current user",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isgreaterthan", DATA_TYPE_CONTEXT, ISGREATERTHAN_ARGS, &FnCallIsLessGreaterThan, "True if arg1 is numerically greater than arg2, else compare strings like strcmp",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("islessthan", DATA_TYPE_CONTEXT, ISLESSTHAN_ARGS, &FnCallIsLessGreaterThan, "True if arg1 is numerically less than arg2, else compare strings like NOT strcmp",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("islink", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a symbolic link",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isnewerthan", DATA_TYPE_CONTEXT, ISNEWERTHAN_ARGS, &FnCallIsNewerThan, "True if arg1 is newer (modified later) than arg2 (mtime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isplain", DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a plain/regular file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isvariable", DATA_TYPE_CONTEXT, ISVARIABLE_ARGS, &FnCallIsVariable, "True if the named variable is defined",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("join", DATA_TYPE_STRING, JOIN_ARGS, &FnCallJoin, "Join the items of arg2 into a string, using the conjunction in arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("lastnode", DATA_TYPE_STRING, LASTNODE_ARGS, &FnCallLastNode, "Extract the last of a separated string, e.g. filename from a path",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("laterthan", DATA_TYPE_CONTEXT, LATERTHAN_ARGS, &FnCallLaterThan, "True if the current time is later than the given date",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldaparray", DATA_TYPE_CONTEXT, LDAPARRAY_ARGS, &FnCallLDAPArray, "Extract all values from an ldap record",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldaplist", DATA_TYPE_STRING_LIST, LDAPLIST_ARGS, &FnCallLDAPList, "Extract all named values from multiple ldap records",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldapvalue", DATA_TYPE_STRING, LDAPVALUE_ARGS, &FnCallLDAPValue, "Extract the first matching named value from ldap",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("lsdir", DATA_TYPE_STRING_LIST, LSDIRLIST_ARGS, &FnCallLsDir, "Return a list of files in a directory matching a regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("makerule", DATA_TYPE_CONTEXT, MAKERULE_ARGS, &FnCallMakerule, "True if the target file arg1 does not exist or a source file in arg2 is newer",
                      FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maparray", DATA_TYPE_STRING_LIST, MAPARRAY_ARGS, &FnCallMapArray, "Return a list with each element modified by a pattern based $(this.k) and $(this.v)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maplist", DATA_TYPE_STRING_LIST, MAPLIST_ARGS, &FnCallMapList, "Return a list with each element modified by a pattern based $(this)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mergedata", DATA_TYPE_CONTAINER, MERGEDATA_ARGS, &FnCallMergeData, "",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("none", DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if no element in the named list matches the given regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("not", DATA_TYPE_STRING, NOT_ARGS, &FnCallNot, "Calculate whether argument is false",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("now", DATA_TYPE_INT, NOW_ARGS, &FnCallNow, "Convert the current time into system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("nth", DATA_TYPE_STRING, NTH_ARGS, &FnCallNth, "Get the element at arg2 in list or data container arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("on", DATA_TYPE_INT, DATE_ARGS, &FnCallOn, "Convert an exact date/time to an integer system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("or", DATA_TYPE_STRING, OR_ARGS, &FnCallOr, "Calculate whether any argument evaluates to true",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("packagesmatching", DATA_TYPE_CONTAINER, PACKAGESMATCHING_ARGS, &FnCallPackagesMatching, "List the defined packages (\"name,version,arch,manager\") matching regex arg1=name,arg2=version,arg3=arch,arg4=method",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parseintarray", DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseIntArray, "Read an array of integers from a file and assign the dimension to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsejson", DATA_TYPE_CONTAINER, PARSEJSON_ARGS, &FnCallParseJson, "",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parserealarray", DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseRealArray, "Read an array of real numbers from a file and assign the dimension to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsestringarray", DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseStringArray, "Read an array of strings from a file and assign the dimension to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsestringarrayidx", DATA_TYPE_INT, PARSESTRINGARRAYIDX_ARGS, &FnCallParseStringArrayIndex, "Read an array of strings from a file and assign the dimension to a variable with integer indeces",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peers", DATA_TYPE_STRING_LIST, PEERS_ARGS, &FnCallPeers, "Get a list of peers (not including ourself) from the partition to which we belong",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peerleader", DATA_TYPE_STRING, PEERLEADER_ARGS, &FnCallPeerLeader, "Get the assigned peer-leader of the partition to which we belong",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peerleaders", DATA_TYPE_STRING_LIST, PEERLEADERS_ARGS, &FnCallPeerLeaders, "Get a list of peer leaders from the named partitioning",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("product", DATA_TYPE_REAL, PRODUCT_ARGS, &FnCallProduct, "Return the product of a list of reals",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("randomint", DATA_TYPE_INT, RANDOMINT_ARGS, &FnCallRandomInt, "Generate a random integer between the given limits, excluding the upper",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readfile", DATA_TYPE_STRING, READFILE_ARGS, &FnCallReadFile, "Read max number of bytes from named file and assign to variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintarray", DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadIntArray, "Read an array of integers from a file and assign the dimension to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintlist", DATA_TYPE_INT_LIST, READSTRINGLIST_ARGS, &FnCallReadIntList, "Read and assign a list variable from a file of separated ints",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readjson", DATA_TYPE_CONTAINER, READJSON_ARGS, &FnCallReadJson, "",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readrealarray", DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadRealArray, "Read an array of real numbers from a file and assign the dimension to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readreallist", DATA_TYPE_REAL_LIST, READSTRINGLIST_ARGS, &FnCallReadRealList, "Read and assign a list variable from a file of separated real numbers",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringarray", DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadStringArray, "Read an array of strings from a file and assign the dimension to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringarrayidx", DATA_TYPE_INT, READSTRINGARRAYIDX_ARGS, &FnCallReadStringArrayIndex, "Read an array of strings from a file and assign the dimension to a variable with integer indeces",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringlist", DATA_TYPE_STRING_LIST, READSTRINGLIST_ARGS, &FnCallReadStringList, "Read and assign a list variable from a file of separated strings",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readtcp", DATA_TYPE_STRING, READTCP_ARGS, &FnCallReadTcp, "Connect to tcp port, send string and assign result to variable",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regarray", DATA_TYPE_CONTEXT, REGARRAY_ARGS, &FnCallRegArray, "True if arg1 matches any item in the associative array with id=arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regcmp", DATA_TYPE_CONTEXT, REGCMP_ARGS, &FnCallRegCmp, "True if arg1 is a regular expression matching that matches string arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regextract", DATA_TYPE_CONTEXT, REGEXTRACT_ARGS, &FnCallRegExtract, "True if the regular expression in arg 1 matches the string in arg2 and sets a non-empty array of backreferences named arg3",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("registryvalue", DATA_TYPE_STRING, REGISTRYVALUE_ARGS, &FnCallRegistryValue, "Returns a value for an MS-Win registry key,value pair",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regline", DATA_TYPE_CONTEXT, REGLINE_ARGS, &FnCallRegLine, "True if the regular expression in arg1 matches a line in file arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reglist", DATA_TYPE_CONTEXT, REGLIST_ARGS, &FnCallRegList, "True if the regular expression in arg2 matches any item in the list whose id is arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regldap", DATA_TYPE_CONTEXT, REGLDAP_ARGS, &FnCallRegLDAP, "True if the regular expression in arg6 matches a value item in an ldap search",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("remotescalar", DATA_TYPE_STRING, REMOTESCALAR_ARGS, &FnCallRemoteScalar, "Read a scalar value from a remote cfengine server",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("remoteclassesmatching", DATA_TYPE_CONTEXT, REMOTECLASSESMATCHING_ARGS, &FnCallRemoteClassesMatching, "Read persistent classes matching a regular expression from a remote cfengine server and add them into local context with prefix",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("returnszero", DATA_TYPE_CONTEXT, RETURNSZERO_ARGS, &FnCallReturnsZero, "True if named shell command has exit status zero",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("rrange", DATA_TYPE_REAL_RANGE, RRANGE_ARGS, &FnCallRRange, "Define a range of real numbers for cfengine internal use",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reverse", DATA_TYPE_STRING_LIST, REVERSE_ARGS, &FnCallReverse, "Reverse a string list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("selectservers", DATA_TYPE_INT, SELECTSERVERS_ARGS, &FnCallSelectServers, "Select tcp servers which respond correctly to a query and return their number, set array of names",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("shuffle", DATA_TYPE_STRING_LIST, SHUFFLE_ARGS, &FnCallShuffle, "Shuffle a string list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("some", DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if an element in the named list matches the given regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sort", DATA_TYPE_STRING_LIST, SORT_ARGS, &FnCallSort, "Sort a string list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splayclass", DATA_TYPE_CONTEXT, SPLAYCLASS_ARGS, &FnCallSplayClass, "True if the first argument's time-slot has arrived, according to a policy in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splitstring", DATA_TYPE_STRING_LIST, SPLITSTRING_ARGS, &FnCallSplitString, "Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("storejson", DATA_TYPE_STRING, STOREJSON_ARGS, &FnCallStoreJson, "Convert a data container to a JSON string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strcmp", DATA_TYPE_CONTEXT, STRCMP_ARGS, &FnCallStrCmp, "True if the two strings match exactly",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strftime", DATA_TYPE_STRING, STRFTIME_ARGS, &FnCallStrftime, "Format a date and time string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sublist", DATA_TYPE_STRING_LIST, SUBLIST_ARGS, &FnCallSublist, "Returns arg3 element from either the head or the tail (according to arg2) of list arg1.",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sum", DATA_TYPE_REAL, SUM_ARGS, &FnCallSum, "Return the sum of a list of reals",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("translatepath", DATA_TYPE_STRING, TRANSLATEPATH_ARGS, &FnCallTranslatePath, "Translate path separators from Unix style to the host's native",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("unique", DATA_TYPE_STRING_LIST, UNIQUE_ARGS, &FnCallUnique, "Returns all the unique elements of list arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("usemodule", DATA_TYPE_CONTEXT, USEMODULE_ARGS, &FnCallUseModule, "Execute cfengine module script and set class if successful",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("userexists", DATA_TYPE_CONTEXT, USEREXISTS_ARGS, &FnCallUserExists, "True if user name or numerical id exists on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("variablesmatching", DATA_TYPE_STRING_LIST, CLASSMATCH_ARGS, &FnCallVariablesMatching, "List the variables matching regex arg1 and tag regexes arg2,arg3,...",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),

    // Text xform functions
    FnCallTypeNew("downcase", DATA_TYPE_STRING, XFORM_ARGS, &FnCallTextXform, "Convert a string to lowercase",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("head", DATA_TYPE_STRING, XFORM_SUBSTR_ARGS, &FnCallTextXform, "Extract characters from the head of the string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reversestring", DATA_TYPE_STRING, XFORM_ARGS, &FnCallTextXform, "Reverse a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strlen", DATA_TYPE_INT, XFORM_ARGS, &FnCallTextXform, "Return the length of a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("tail", DATA_TYPE_STRING, XFORM_SUBSTR_ARGS, &FnCallTextXform, "Extract characters from the tail of the string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("upcase", DATA_TYPE_STRING, XFORM_ARGS, &FnCallTextXform, "Convert a string to UPPERCASE",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // List folding functions
    FnCallTypeNew("length", DATA_TYPE_INT, STAT_FOLD_ARGS, &FnCallFold, "Return the length of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("max", DATA_TYPE_STRING, SORT_ARGS, &FnCallFold, "Return the maximum of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mean", DATA_TYPE_REAL, STAT_FOLD_ARGS, &FnCallFold, "Return the mean (average) of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("min", DATA_TYPE_STRING, SORT_ARGS, &FnCallFold, "Return the minimum of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("variance", DATA_TYPE_REAL, STAT_FOLD_ARGS, &FnCallFold, "Return the variance of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    
    FnCallTypeNewNull()
};
