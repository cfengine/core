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

#include <evalfunction.h>

#include <eval_context.h>
#include <promises.h>
#include <dir.h>
#include <dbm_api.h>
#include <lastseen.h>
#include <files_copy.h>
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
#include <regex.h>          /* CompileRegex,StringMatchWithPrecompiledRegex */
#include <net.h>                                           /* SocketConnect */
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
#include <printsize.h>
#include <csv_parser.h>
#include <json-yaml.h>
#include <known_dirs.h>
#include <mustache.h>

#include <math_eval.h>

#include <libgen.h>

#ifndef __MINGW32__
#include <glob.h>
#endif

#include <ctype.h>


static FnCallResult FilterInternal(EvalContext *ctx, const FnCall *fp, const char *regex, const char *name, bool do_regex, bool invert, long max);

static char *StripPatterns(char *file_buffer, const char *pattern, const char *filename);
static void CloseStringHole(char *s, int start, int end);
static int BuildLineArray(EvalContext *ctx, const Bundle *bundle, const char *array_lval, const char *file_buffer,
                          const char *split, int maxent, DataType type, bool int_index);
static JsonElement* BuildData(EvalContext *ctx, const char *file_buffer,  const char *split, int maxent, bool make_array);
static int ExecModule(EvalContext *ctx, char *command);

static bool CheckID(const char *id);
static const Rlist *GetListReferenceArgument(const EvalContext *ctx, const const FnCall *fp, const char *lval_str, DataType *datatype_out);
static char *CfReadFile(const char *filename, int maxsize);

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
 * Return successful FnCallResult with copy of str retained.
 */
static FnCallResult FnReturn(const char *str)
{
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(str), RVAL_TYPE_SCALAR } };
}

/*
 * Return successful FnCallResult with str as is.
 */
static FnCallResult FnReturnNoCopy(char *str)
{
    return (FnCallResult) { FNCALL_SUCCESS, { str, RVAL_TYPE_SCALAR } };
}

static FnCallResult FnReturnBuffer(Buffer *buf)
{
    return (FnCallResult) { FNCALL_SUCCESS, { BufferClose(buf), RVAL_TYPE_SCALAR } };
}

static FnCallResult FnReturnF(const char *fmt, ...) FUNC_ATTR_PRINTF(1, 2);

static FnCallResult FnReturnF(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *buffer;
    xvasprintf(&buffer, fmt, ap);
    va_end(ap);
    return FnReturnNoCopy(buffer);
}

static FnCallResult FnReturnContext(bool result)
{
    return FnReturn(result ? "any" : "!any");
}

static FnCallResult FnFailure(void)
{
    return (FnCallResult) { FNCALL_FAILURE };
}

static VarRef* ResolveAndQualifyVarName(const FnCall *fp, const char *varname)
{
    VarRef *ref = VarRefParse(varname);
    if (!VarRefIsQualified(ref))
    {
        if (fp->caller)
        {
            const Bundle *caller_bundle = PromiseGetBundle(fp->caller);
            VarRefQualify(ref, caller_bundle->ns, caller_bundle->name);
        }
        else
        {
            Log(LOG_LEVEL_WARNING,
                "Function '%s' was not called from a promise; "
                "the unqualified variable reference %s cannot be qualified automatically.",
                fp->name,
                varname);
            VarRefDestroy(ref);
            return NULL;
        }
    }

    return ref;
}

static JsonElement* VarRefValueToJson(EvalContext *ctx, const FnCall *fp, const VarRef *ref,
                                      const DataType disallowed_datatypes[], size_t disallowed_count)
{
    assert(ref);

    DataType value_type = CF_DATA_TYPE_NONE;
    const void *value = EvalContextVariableGet(ctx, ref, &value_type);
    bool want_type = true;

    for (int di = 0; di < disallowed_count; di++)
    {
        if (disallowed_datatypes[di] == value_type)
        {
            want_type = false;
            break;
        }
    }

    JsonElement *convert = NULL;
    if (want_type)
    {
        switch (DataTypeToRvalType(value_type))
        {
        case RVAL_TYPE_LIST:
            convert = JsonArrayCreate(RlistLen(value));
            for (const Rlist *rp = value; rp != NULL; rp = rp->next)
            {
                if (rp->val.type == RVAL_TYPE_SCALAR &&
                    strcmp(RlistScalarValue(value), CF_NULL_VALUE) != 0)
                {
                    JsonArrayAppendString(convert, RlistScalarValue(rp));
                }
            }
            break;

        case RVAL_TYPE_CONTAINER:
            convert = JsonCopy(value);
            break;

        default:
            {
                VariableTableIterator *iter = EvalContextVariableTableFromRefIteratorNew(ctx, ref);
                convert = JsonObjectCreate(10);
                Variable *var;

                char *last_key = NULL;
                while ((var = VariableTableIteratorNext(iter)))
                {
                    size_t index_offset = ref->name_index_count;
                    JsonElement *holder = convert;
                    if (var->ref->num_indices - index_offset == 1)
                    {
                        last_key = var->ref->indices[index_offset];
                    }
                    else if (var->ref->num_indices - index_offset > 1)
                    {
                        Log(LOG_LEVEL_DEBUG, "%s: got ref with starting depth %zd and index count %zd", fp->name, index_offset, var->ref->num_indices);
                        for (int index = index_offset; index < var->ref->num_indices-1; index++)
                        {
                            JsonElement *local = JsonObjectGet(holder, var->ref->indices[index]);
                            if (NULL == local)
                            {
                                local = JsonObjectCreate(1);
                                JsonObjectAppendObject(holder, var->ref->indices[index], local);
                            }

                            last_key = var->ref->indices[index+1];
                            holder = local;
                        }
                    }

                    if (NULL != last_key && NULL != holder)
                    {
                        switch (var->rval.type)
                        {
                        case RVAL_TYPE_SCALAR:
                            JsonObjectAppendString(holder, last_key, var->rval.item);
                            break;

                        case RVAL_TYPE_LIST:
                        {
                            JsonElement *array = JsonArrayCreate(10);
                            for (const Rlist *rp = RvalRlistValue(var->rval); rp != NULL; rp = rp->next)
                            {
                                if (rp->val.type == RVAL_TYPE_SCALAR &&
                                    strcmp(RlistScalarValue(rp), CF_NULL_VALUE) != 0)
                                {
                                    JsonArrayAppendString(array, RlistScalarValue(rp));
                                }
                            }
                            JsonObjectAppendArray(holder, last_key, array);
                        }
                        break;

                        default:
                            break;
                        }
                    }
                }

                VariableTableIteratorDestroy(iter);

                if (JsonLength(convert) < 1)
                {
                    char *varname = VarRefToString(ref, true);
                    Log(LOG_LEVEL_VERBOSE, "%s: argument '%s' does not resolve to a container or a list or a CFEngine array", fp->name, varname);
                    free(varname);
                    JsonDestroy(convert);
                    return NULL;
                }

                break;
            } // end of default case
        } // end of data type switch
    }
    else // !wanted_type
    {
        char *varname = VarRefToString(ref, true);
        Log(LOG_LEVEL_DEBUG, "%s: argument '%s' resolved to an undesired data type", fp->name, varname);
        free(varname);
    }

    return convert;
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
            Log(LOG_LEVEL_DEBUG, "Recent entry");

            Rlist *r = RlistKeyIn(aged, address);
            if (r)
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
        return recent;
    }
    else
    {
        RlistDestroy(recent);
        return aged;
    }
}

/*********************************************************************/

static FnCallResult FnCallAnd(EvalContext *ctx,
                              ARG_UNUSED const Policy *policy,
                              ARG_UNUSED const FnCall *fp,
                              const Rlist *finalargs)
{

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(fp->name, arg->val, CF_DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "Function '%s', %s", fp->name, SyntaxTypeMatchToString(err));
        }
    }

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        if (!IsDefinedClass(ctx, RlistScalarValue(arg)))
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

    char buf[PRINTSIZE(uintmax_t)];
    xsnprintf(buf, sizeof(buf), "%ju", (uintmax_t) quality->lastseen);

    PrependItem(addresses, address, buf);

    return true;
}

/*******************************************************************/

static FnCallResult FnCallHostsSeen(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    Item *addresses = NULL;

    int horizon = IntFromString(RlistScalarValue(finalargs)) * 3600;
    char *hostseen_policy = RlistScalarValue(finalargs->next);
    char *format = RlistScalarValue(finalargs->next->next);

    Log(LOG_LEVEL_DEBUG, "Calling hostsseen(%d,%s,%s)",
        horizon, hostseen_policy, format);

    if (!ScanLastSeenQuality(&CallHostsSeenCallback, &addresses))
    {
        return FnFailure();
    }

    Rlist *returnlist = GetHostsFromLastseenDB(addresses, horizon,
                                               strcmp(format, "address") == 0,
                                               strcmp(hostseen_policy, "lastseen") == 0);

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

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallHostsWithClass(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    Rlist *returnlist = NULL;

    char *class_name = RlistScalarValue(finalargs);
    char *return_format = RlistScalarValue(finalargs->next);

    if (!ListHostsWithClass(ctx, &returnlist, class_name, return_format))
    {
        return FnFailure();
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallRandomInt(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
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

static FnCallResult FnCallGetEnv(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
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

static FnCallResult FnCallGetUsers(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const char *except_name = RlistScalarValue(finalargs);
    const char *except_uid = RlistScalarValue(finalargs->next);

    Rlist *except_names = RlistFromSplitString(except_name, ',');
    Rlist *except_uids = RlistFromSplitString(except_uid, ',');

    setpwent();

    Rlist *newlist = NULL;
    struct passwd *pw;
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

    RlistDestroy(except_names);
    RlistDestroy(except_uids);

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

#else

static FnCallResult FnCallGetUsers(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    Log(LOG_LEVEL_ERR, "getusers is not implemented");
    return FnFailure();
}

#endif

/*********************************************************************/

static FnCallResult FnCallEscape(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

    char *name = RlistScalarValue(finalargs);

    EscapeSpecialChars(name, buffer, CF_BUFSIZE - 1, "", "");

    return FnReturn(buffer);
}

/*********************************************************************/

static FnCallResult FnCallHost2IP(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
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

static FnCallResult FnCallIP2Host(ARG_UNUSED EvalContext *ctx,
                                  ARG_UNUSED ARG_UNUSED const Policy *policy,
                                  ARG_UNUSED const FnCall *fp,
                                  const Rlist *finalargs)
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

static FnCallResult FnCallGetUid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    return FnFailure();
}

#else /* !__MINGW32__ */

static FnCallResult FnCallGetUid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    struct passwd *pw = getpwnam(RlistScalarValue(finalargs));

    if (pw == NULL)
    {
        return FnFailure();
    }

    return FnReturnF("%ju", (uintmax_t)pw->pw_uid);
}

#endif /* !__MINGW32__ */

/*********************************************************************/

#ifdef __MINGW32__

static FnCallResult FnCallGetGid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    return FnFailure();
}

#else /* !__MINGW32__ */

static FnCallResult FnCallGetGid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    struct group *gr = getgrnam(RlistScalarValue(finalargs));

    if (gr == NULL)
    {
        return FnFailure();
    }

    return FnReturnF("%ju", (uintmax_t)gr->gr_gid);
}

#endif /* __MINGW32__ */

/*********************************************************************/

static FnCallResult FnCallHandlerHash(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
/* Hash(string,md5|sha1|crypt) */
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    HashMethod type;

    char *string_or_filename = RlistScalarValue(finalargs);
    char *typestring = RlistScalarValue(finalargs->next);
    const bool filehash_mode = strcmp(fp->name, "file_hash") == 0;

    type = HashIdFromName(typestring);

    if (FIPS_MODE && type == HASH_METHOD_MD5)
    {
        Log(LOG_LEVEL_ERR, "FIPS mode is enabled, and md5 is not an approved algorithm in call to %s()", fp->name);
    }

    if (filehash_mode)
    {
        HashFile(string_or_filename, digest, type);
    }
    else
    {
        HashString(string_or_filename, strlen(string_or_filename), digest, type);
    }

    char hashbuffer[CF_HOSTKEY_STRING_SIZE];
    HashPrintSafe(hashbuffer, sizeof(hashbuffer),
                  digest, type, true);

    return FnReturn(SkipHashType(hashbuffer));
}

/*********************************************************************/

static FnCallResult FnCallHashMatch(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
/* HashMatch(string,md5|sha1|crypt,"abdxy98edj") */
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    HashMethod type;

    char *string = RlistScalarValue(finalargs);
    char *typestring = RlistScalarValue(finalargs->next);
    char *compare = RlistScalarValue(finalargs->next->next);

    type = HashIdFromName(typestring);
    HashFile(string, digest, type);

    char hashbuffer[CF_HOSTKEY_STRING_SIZE];
    HashPrintSafe(hashbuffer, sizeof(hashbuffer),
                  digest, type, true);

    Log(LOG_LEVEL_VERBOSE,
        "File '%s' hashes to '%s', compare to '%s'",
        string, hashbuffer, compare);

    return FnReturnContext(strcmp(hashbuffer + 4, compare) == 0);
}

/*********************************************************************/

static FnCallResult FnCallConcat(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char id[CF_BUFSIZE];
    char result[CF_BUFSIZE] = "";

    snprintf(id, CF_BUFSIZE, "built-in FnCall concat-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, CF_DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
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

static FnCallResult FnCallIfElse(EvalContext *ctx,
                                 ARG_UNUSED const Policy *policy,
                                 ARG_UNUSED const FnCall *fp,
                                 const Rlist *finalargs)
{
    int argcount = 0;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall ifelse-arg");

    /* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, CF_DATA_TYPE_STRING, "", 1);
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

    const Rlist *arg;
    for (arg = finalargs;        /* Start with arg set to finalargs. */
         arg && arg->next;       /* We must have arg and arg->next to proceed. */
         arg = arg->next->next)  /* arg steps forward *twice* every time. */
    {
        /* Similar to classmatch(), we evaluate the first of the two
         * arguments as a class. */
        if (IsDefinedClass(ctx, RlistScalarValue(arg)))
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

static FnCallResult FnCallClassesMatching(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    bool count_only = false;
    bool check_only = false;
    unsigned count = 0;

    if (0 == strcmp(fp->name, "classmatch"))
    {
        check_only = true;
    }
    else if (0 == strcmp(fp->name, "countclassesmatching"))
    {
        count_only = true;
    }
    else if (0 == strcmp(fp->name, "classesmatching"))
    {
    }
    else
    {
        FatalError(ctx, "FnCallClassesMatching: got unknown function name '%s', aborting", fp->name);
    }

    if (!finalargs)
    {
        FatalError(ctx, "Function '%s' requires at least one argument", fp->name);
    }

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(fp->name, arg->val, CF_DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in function '%s', '%s'", fp->name, SyntaxTypeMatchToString(err));
        }
    }

    Rlist *matches = NULL;

    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
        StringSet *global_matches = ClassesMatching(ctx, iter, RlistScalarValue(finalargs), finalargs->next, check_only);

        StringSetIterator it = StringSetIteratorInit(global_matches);
        const char *element = NULL;
        while ((element = StringSetIteratorNext(&it)))
        {
            if (count_only || check_only)
            {
                count++;
            }
            else
            {
                RlistPrepend(&matches, element, RVAL_TYPE_SCALAR);
            }
        }

        StringSetDestroy(global_matches);
        ClassTableIteratorDestroy(iter);
    }

    if (check_only && count >= 1)
    {
        return FnReturnContext(true);
    }

    {
        ClassTableIterator *iter = EvalContextClassTableIteratorNewLocal(ctx);
        StringSet *local_matches = ClassesMatching(ctx, iter, RlistScalarValue(finalargs), finalargs->next, check_only);

        StringSetIterator it = StringSetIteratorInit(local_matches);
        const char *element = NULL;
        while ((element = StringSetIteratorNext(&it)))
        {
            if (count_only || check_only)
            {
                count++;
            }
            else
            {
                RlistPrepend(&matches, element, RVAL_TYPE_SCALAR);
            }
        }

        StringSetDestroy(local_matches);
        ClassTableIteratorDestroy(iter);
    }

    if (check_only)
    {
        return FnReturnContext(count >= 1);
    }
    else if (count_only)
    {
        return FnReturnF("%u", count);
    }

    // else, this is classesmatching()
    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}


static StringSet *VariablesMatching(const EvalContext *ctx, VariableTableIterator *iter, const Rlist *args)
{
    StringSet *matching = StringSetNew();

    const char *regex = RlistScalarValue(args);
    pcre *rx = CompileRegex(regex);

    Variable *v = NULL;
    while ((v = VariableTableIteratorNext(iter)))
    {
        char *expr = VarRefToString(v->ref, true);

        /* FIXME: review this strcmp. Moved out from StringMatch */
        if (!strcmp(regex, expr) ||
            (rx && StringMatchFullWithPrecompiledRegex(rx, expr)))
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
            else
            {
                free(expr);
            }
        }
        else
        {
            free(expr);
        }
    }

    if (rx)
    {
        pcre_free(rx);
    }

    return matching;
}

static FnCallResult FnCallVariablesMatching(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (!finalargs)
    {
        FatalError(ctx, "Function '%s' requires at least one argument", fp->name);
    }

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(fp->name, arg->val, CF_DATA_TYPE_STRING, "", 1);
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

    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGetMetaTags(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    Rlist *tags = NULL;
    StringSet *tagset = NULL;

    if (0 == strcmp(fp->name, "getvariablemetatags"))
    {
        VarRef *ref = VarRefParse(RlistScalarValue(finalargs));
        tagset = EvalContextVariableTags(ctx, ref);
        VarRefDestroy(ref);
    }
    else if (0 == strcmp(fp->name, "getclassmetatags"))
    {
        ClassRef ref = ClassRefParse(RlistScalarValue(finalargs));
        tagset = EvalContextClassTags(ctx, ref.ns, ref.name);
        ClassRefDestroy(ref);
    }
    else
    {
        FatalError(ctx, "FnCallGetMetaTags: got unknown function name '%s', aborting", fp->name);
    }

    if (NULL == tagset)
    {
        Log(LOG_LEVEL_VERBOSE, "%s found variable or class %s without a tagset", fp->name, RlistScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    char* element;
    StringSetIterator it = StringSetIteratorInit(tagset);
    while ((element = SetIteratorNext(&it)))
    {
        RlistAppendScalar(&tags, element);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { tags, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallBundlesMatching(EvalContext *ctx, const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (!finalargs)
    {
        return FnFailure();
    }

    const char *regex = RlistScalarValue(finalargs);
    pcre *rx = CompileRegex(regex);
    if (!rx)
    {
        return FnFailure();
    }

    const Rlist *tag_args = finalargs->next;

    Rlist *matches = NULL;
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        const Bundle *bp = SeqAt(policy->bundles, i);

        char *bundle_name = BundleQualifiedName(bp);
        if (StringMatchFullWithPrecompiledRegex(rx, bundle_name))
        {
            VarRef *ref = VarRefParseFromBundle("tags", bp);
            VarRefSetMeta(ref, true);
            DataType type = CF_DATA_TYPE_NONE;
            const void *bundle_tags = EvalContextVariableGet(ctx, ref, &type);
            VarRefDestroy(ref);

            bool found = false; // case where tag_args are given and the bundle has no tags

            if (NULL == tag_args)
            {
                // we declare it found if no tags were requested
                found = true;
            }
            else if (NULL != bundle_tags)
            {

                switch (DataTypeToRvalType(type))
                {
                case RVAL_TYPE_SCALAR:
                    {
                        Rlist *searched = RlistFromSplitString(bundle_tags, ',');
                        found = RlistMatchesRegexRlist(searched, tag_args);
                        RlistDestroy(searched);
                    }
                    break;

                case RVAL_TYPE_LIST:
                    found = RlistMatchesRegexRlist(bundle_tags, tag_args);
                    break;

                default:
                    Log(LOG_LEVEL_WARNING, "Function '%s' only matches tags defined as a scalar or a list.  "
                        "Bundle '%s' had meta defined as '%s'", fp->name, bundle_name, DataTypeToString(type));
                    found = false;
                    break;
                }
            }

            if (found)
            {
                RlistPrepend(&matches, bundle_name, RVAL_TYPE_SCALAR);
            }
        }

        free(bundle_name);
    }

    pcre_free(rx);

    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static bool AddPackagesMatchingJsonLine(pcre *matcher, JsonElement *json, char *line)
{
    
    if (strlen(line) > CF_BUFSIZE - 80)
    {
        Log(LOG_LEVEL_ERR,
            "Line from package inventory is too long (%zd) to be sensible",
            strlen(line));
        return false;
    }
    

    if (StringMatchFullWithPrecompiledRegex(matcher, line))
    {
        Seq *list = SeqParseCsvString(line);
        if (SeqLength(list) != 4)
        {
            Log(LOG_LEVEL_ERR,
                "Line from package inventory '%s' did not yield correct number of elements.",
                line);
            SeqDestroy(list);
            return true;
        }

        JsonElement *line_obj = JsonObjectCreate(4);
        JsonObjectAppendString(line_obj, "name",    SeqAt(list, 0));
        JsonObjectAppendString(line_obj, "version", SeqAt(list, 1));
        JsonObjectAppendString(line_obj, "arch",    SeqAt(list, 2));
        JsonObjectAppendString(line_obj, "method",  SeqAt(list, 3));
                
        SeqDestroy(list);
        JsonArrayAppendObject(json, line_obj);
    }

    return true;
}

static bool GetLegacyPackagesMatching(pcre *matcher, JsonElement *json, const bool installed_mode)
{
    char filename[CF_MAXVARSIZE];
    if (installed_mode)
    {
        GetSoftwareCacheFilename(filename);
    }
    else
    {
        GetSoftwarePatchesFilename(filename);
    }

    Log(LOG_LEVEL_DEBUG, "Reading inventory from '%s'", filename);
    
    FILE *const fin = fopen(filename, "r");
    if (fin == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Cannot open the %s packages inventory '%s' - "
            "This is not necessarily an error. "
            "Either the inventory policy has not been included, "
            "or it has not had time to have an effect yet or you are using"
            "new package promise and check for legacy promise is made."
            "A future call may still succeed. (fopen: %s)",
            installed_mode ? "installed" : "available",
            filename,
            GetErrorStr());

        return true;
    }
    
    char *line;
    while (NULL != (line = GetCsvLineNext(fin)))
    {
        if (!AddPackagesMatchingJsonLine(matcher, json, line))
        {
            free(line);
            break;
        }
        free(line);
    }

    bool ret = (feof(fin) != 0);
    fclose(fin);
    
    return ret;
}

static bool GetPackagesMatching(pcre *matcher, JsonElement *json, const bool installed_mode, Rlist *default_inventory)
{
    dbid database = (installed_mode == true ? dbid_packages_installed : dbid_packages_updates);

    for (const Rlist *rp = default_inventory; rp != NULL; rp = rp->next)
    {
        const char *pm_name =  RlistScalarValue(rp);
        size_t pm_name_size = strlen(pm_name);
        
        Log(LOG_LEVEL_DEBUG, "Reading packages (%d) for package module [%s]", 
                database, pm_name);
        
        if (StringSafeEqual(pm_name, "cf_null"))
        {
            continue;
        }
        
        CF_DB *db_cached;
        if (!OpenSubDB(&db_cached, database, pm_name))
        {
            Log(LOG_LEVEL_ERR, "Can not open database %d to get packages data.", database);
            return false;
        }

        char *key = "<inventory>";
        int data_size = ValueSizeDB(db_cached, key, strlen(key) + 1);
        
        Log(LOG_LEVEL_DEBUG, "Reading inventory from database: %d", data_size);
        
        /* For empty list we are storing one byte value in database. */
        if (data_size > 1)
        {
            char *buff = xmalloc(data_size + 1);
            buff[data_size] = '\0';
            if (!ReadDB(db_cached, key, buff, data_size))
            {
                Log(LOG_LEVEL_WARNING, "Can not read installed packages database "
                    "for '%s' package module.", pm_name);
                continue;
            }
            
            Seq *packages_from_module = SeqStringFromString(buff, '\n');
            free(buff);
            
            if (packages_from_module)
            {
                // Iterate over and see where match is.
                for (int i = 0; i < SeqLength(packages_from_module); i++)
                {
                    // With the new package promise we are storing inventory 
                    // information it the database. This set of lines ('\n' separated)
                    // containing packages information. Each line is comma 
                    // separated set of data containing name, version and architecture.
                    //
                    // Legacy package promise is using 4 values, where the last one
                    // is package method. In our case, method is simply package 
                    // module name. To make sure regex matching is working as
                    // expected (we are comparing whole lines, containing package
                    // method) we need to extend the line to contain package
                    // module before regex match is taking place.
                    char *line = SeqAt(packages_from_module, i);
                    size_t new_line_size = strlen(line) + pm_name_size + 2; // we need coma and terminator
                    char new_line[new_line_size];
                    strcpy(new_line, line);
                    strcat(new_line, ",");
                    strcat(new_line, pm_name);
                    
                    if (!AddPackagesMatchingJsonLine(matcher, json, new_line))
                    {
                        break;
                    }
                }
                SeqDestroy(packages_from_module);
            }
            else
            {
                 Log(LOG_LEVEL_WARNING, "Can not parse packages database for '%s' "
                     "package module.", pm_name);
                 
            }
        }
        CloseDB(db_cached);
    }
    return true;
}

static FnCallResult FnCallPackagesMatching(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const bool installed_mode = (0 == strcmp(fp->name, "packagesmatching"));
    pcre *matcher;
    {
        const char *regex_package = RlistScalarValue(finalargs);
        const char *regex_version = RlistScalarValue(finalargs->next);
        const char *regex_arch = RlistScalarValue(finalargs->next->next);
        const char *regex_method = RlistScalarValue(finalargs->next->next->next);
        char regex[CF_BUFSIZE];

        // Here we will truncate the regex if the parameters add up to over CF_BUFSIZE
        snprintf(regex, sizeof(regex), "^%s,%s,%s,%s$",
                 regex_package, regex_version, regex_arch, regex_method);
        matcher = CompileRegex(regex);
        if (matcher == NULL)
        {
            return FnFailure();
        }
    }
    
    JsonElement *json = JsonArrayCreate(50);
    bool ret = false;
    
    Rlist *default_inventory = GetDefaultInventoryFromContext(ctx);
    if (!default_inventory)
    {
        // Legacy package promise
        ret = GetLegacyPackagesMatching(matcher, json, installed_mode);
    }
    else
    {
        // We are using package modules.
        ret = GetPackagesMatching(matcher, json, installed_mode, default_inventory);
    }

    pcre_free(matcher);

    if (ret == false)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Unable to read package inventory.", fp->name);
        JsonDestroy(json);
        return FnFailure();
    }

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
}

/*********************************************************************/

static FnCallResult FnCallCanonify(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    char buf[CF_BUFSIZE];
    char *string = RlistScalarValue(finalargs);

    buf[0] = '\0';

    if (!strcmp(fp->name, "canonifyuniquely"))
    {
        char hashbuffer[CF_HOSTKEY_STRING_SIZE];
        unsigned char digest[EVP_MAX_MD_SIZE + 1];
        HashMethod type;

        type = HashIdFromName("sha1");
        HashString(string, strlen(string), digest, type);
        snprintf(buf, CF_BUFSIZE, "%s_%s", string,
                 SkipHashType(HashPrintSafe(hashbuffer, sizeof(hashbuffer),
                                            digest, type, true)));
    }
    else
    {
        snprintf(buf, CF_BUFSIZE, "%s", string);
    }

    return FnReturn(CanonifyName(buf));
}

/*********************************************************************/

static FnCallResult FnCallTextXform(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    char *string = RlistScalarValue(finalargs);
    size_t len = strlen(string);
    /* In case of string_length(), buf needs enough space to hold a number. */
    size_t bufsiz = MAX(len + 1, PRINTSIZE(len));
    char *buf = xcalloc(bufsiz, sizeof(char));
    memcpy(buf, string, len + 1);

    if (!strcmp(fp->name, "string_downcase"))
    {
        int pos = 0;
        for (pos = 0; pos < len; pos++)
        {
            buf[pos] = tolower(buf[pos]);
        }
    }
    else if (!strcmp(fp->name, "string_upcase"))
    {
        int pos = 0;
        for (pos = 0; pos < len; pos++)
        {
            buf[pos] = toupper(buf[pos]);
        }
    }
    else if (!strcmp(fp->name, "string_reverse"))
    {
        int c, i, j;
        for (i = 0, j = len - 1; i < j; i++, j--)
        {
            c = buf[i];
            buf[i] = buf[j];
            buf[j] = c;
        }
    }
    else if (!strcmp(fp->name, "string_length"))
    {
        xsnprintf(buf, bufsiz, "%d", len);
    }
    else if (!strcmp(fp->name, "string_head"))
    {
        const long max = IntFromString(RlistScalarValue(finalargs->next));
        if (max < 0)
        {
            Log(LOG_LEVEL_ERR, "string_head called with negative value %ld", max);
            free(buf);
            return FnFailure();
        }
        else if (max < bufsiz)
        {
            buf[max] = '\0';
        }
    }
    else if (!strcmp(fp->name, "string_tail"))
    {
        const long max = IntFromString(RlistScalarValue(finalargs->next));
        if (max < 0)
        {
            Log(LOG_LEVEL_ERR, "string_tail called with negative value %ld", max);
            free(buf);
            return FnFailure();
        }
        else if (max < len)
        {
            memcpy(buf, string + len - max, max + 1);
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "text xform with unknown call function %s, aborting", fp->name);
        free(buf);
        return FnFailure();
    }

    return FnReturnNoCopy(buf);
}

/*********************************************************************/

static FnCallResult FnCallLastNode(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *name = RlistScalarValue(finalargs);
    char *split = RlistScalarValue(finalargs->next);

    Rlist *newlist = RlistFromSplitRegex(name, split, 100, true);
    if (newlist != NULL)
    {
        char *res = NULL;
        const Rlist *rp = newlist;
        while (rp->next != NULL)
        {
            rp = rp->next;
        }
        assert(rp && !rp->next);

        if (rp->val.item)
        {
            res = xstrdup(RlistScalarValue(rp));
        }

        RlistDestroy(newlist);
        if (res)
        {
            return FnReturnNoCopy(res);
        }
    }
    return FnFailure();
}

/*******************************************************************/

static FnCallResult FnCallDirname(ARG_UNUSED EvalContext *ctx,
                                  ARG_UNUSED const Policy *policy,
                                  ARG_UNUSED const FnCall *fp,
                                  const Rlist *finalargs)
{
    char dir[PATH_MAX];
    strlcpy(dir, RlistScalarValue(finalargs), PATH_MAX);

    DeleteSlash(dir);
    ChopLastNode(dir);

    return FnReturn(dir);
}

/*********************************************************************/

static FnCallResult FnCallClassify(EvalContext *ctx,
                                   ARG_UNUSED const Policy *policy,
                                   ARG_UNUSED const FnCall *fp,
                                   const Rlist *finalargs)
{
    bool is_defined = IsDefinedClass(ctx, CanonifyName(RlistScalarValue(finalargs)));

    return FnReturnContext(is_defined);
}

/*********************************************************************/
/* Executions                                                        */
/*********************************************************************/

static FnCallResult FnCallReturnsZero(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
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

static FnCallResult FnCallExecResult(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
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

    size_t buffer_size = CF_EXPANDSIZE;
    char *buffer = xcalloc(1, buffer_size);

    if (GetExecOutput(RlistScalarValue(finalargs), &buffer, &buffer_size, shelltype))
    {
        Log(LOG_LEVEL_VERBOSE, "%s ran '%s' successfully", fp->name, RlistScalarValue(finalargs));
        FnCallResult res = FnReturn(buffer);
        free(buffer);
        return res;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s could not run '%s' successfully", fp->name, RlistScalarValue(finalargs));
        free(buffer);
        return FnFailure();
    }
}

/*********************************************************************/

static FnCallResult FnCallUseModule(EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    ARG_UNUSED const FnCall *fp,
                                    const Rlist *finalargs)
  /* usemodule("/programpath",varargs) */
{
    char modulecmd[CF_BUFSIZE];
    struct stat statbuf;

    char *command = RlistScalarValue(finalargs);
    char *args = RlistScalarValue(finalargs->next);
    const char* const workdir = GetWorkDir();

    snprintf(modulecmd, CF_BUFSIZE, "\"%s%cmodules%c%s\"",
             workdir, FILE_SEPARATOR, FILE_SEPARATOR, command);

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

    snprintf(modulecmd, CF_BUFSIZE, "\"%s%cmodules%c%s\" %s",
             workdir, FILE_SEPARATOR, FILE_SEPARATOR, command, args);

    Log(LOG_LEVEL_VERBOSE, "Executing and using module [%s]", modulecmd);

    if (!ExecModule(ctx, modulecmd))
    {
        return FnFailure();
    }

    return FnReturnContext(true);
}

/*********************************************************************/
/* Misc                                                              */
/*********************************************************************/

static FnCallResult FnCallSplayClass(EvalContext *ctx,
                                     ARG_UNUSED const Policy *policy,
                                     ARG_UNUSED const FnCall *fp,
                                     const Rlist *finalargs)
{
    char class_name[CF_MAXVARSIZE];

    Interval splay_policy = IntervalFromString(RlistScalarValue(finalargs->next));

    if (splay_policy == INTERVAL_HOURLY)
    {
        /* 12 5-minute slots in hour */
        int slot = StringHash(RlistScalarValue(finalargs), 0, CF_HASHTABLESIZE) * 12 / CF_HASHTABLESIZE;
        snprintf(class_name, CF_MAXVARSIZE, "Min%02d_%02d", slot * 5, ((slot + 1) * 5) % 60);
    }
    else
    {
        /* 12*24 5-minute slots in day */
        int dayslot = StringHash(RlistScalarValue(finalargs), 0, CF_HASHTABLESIZE) * 12 * 24 / CF_HASHTABLESIZE;
        int hour = dayslot / 12;
        int slot = dayslot % 12;

        snprintf(class_name, CF_MAXVARSIZE, "Min%02d_%02d.Hr%02d", slot * 5, ((slot + 1) * 5) % 60, hour);
    }

    return FnReturnContext(IsDefinedClass(ctx, class_name));
}

/*********************************************************************/

/* ReadTCP(localhost,80,'GET index.html',1000) */
static FnCallResult FnCallReadTcp(ARG_UNUSED EvalContext *ctx,
                                  ARG_UNUSED const Policy *policy,
                                  ARG_UNUSED const FnCall *fp,
                                  const Rlist *finalargs)
{
    char *hostnameip = RlistScalarValue(finalargs);
    char *port = RlistScalarValue(finalargs->next);
    char *sendstring = RlistScalarValue(finalargs->next->next);
    ssize_t maxbytes = IntFromString(RlistScalarValue(finalargs->next->next->next));

    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        return FnFailure();
    }

    if (maxbytes < 0 || maxbytes > CF_BUFSIZE - 1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "readtcp: invalid number of bytes %zd to read, defaulting to %d",
            maxbytes, CF_BUFSIZE - 1);
        maxbytes = CF_BUFSIZE - 1;
    }

    char txtaddr[CF_MAX_IP_LEN] = "";
    int sd = SocketConnect(hostnameip, port, CONNTIMEOUT, false,
                           txtaddr, sizeof(txtaddr));
    if (sd == -1)
    {
        Log(LOG_LEVEL_INFO, "readtcp: Couldn't connect. (socket: %s)",
            GetErrorStr());
        return FnFailure();
    }

    if (strlen(sendstring) > 0)
    {
        int sent = 0;
        int result = 0;
        size_t length = strlen(sendstring);
        do
        {
            result = send(sd, sendstring, length, 0);
            if (result < 0)
            {
                cf_closesocket(sd);
                return FnFailure();
            }
            else
            {
                sent += result;
            }
        } while (sent < length);
    }

    char recvbuf[CF_BUFSIZE];
    ssize_t n_read = recv(sd, recvbuf, maxbytes, 0);
    cf_closesocket(sd);

    if (n_read == -1)
    {
        Log(LOG_LEVEL_INFO, "readtcp: Error while receiving (%s)",
            GetErrorStr());
        return FnFailure();
    }

    assert(n_read < sizeof(recvbuf));
    recvbuf[n_read] = '\0';

    Log(LOG_LEVEL_VERBOSE,
        "readtcp: requested %zd maxbytes, got %zd bytes from %s",
        maxbytes, n_read, txtaddr);

    return FnReturn(recvbuf);
}

/*********************************************************************/

static FnCallResult FnCallRegList(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const char *listvar = RlistScalarValue(finalargs);

    if (!IsVarList(listvar))
    {
        Log(LOG_LEVEL_VERBOSE, "Function reglist was promised a list called '%s' but this was not found", listvar);
        return FnFailure();
    }

    char naked[CF_MAXVARSIZE] = "";
    GetNaked(naked, listvar);

    VarRef *ref = VarRefParse(naked);

    DataType value_type = CF_DATA_TYPE_NONE;
    const Rlist *value = EvalContextVariableGet(ctx, ref, &value_type);
    VarRefDestroy(ref);

    if (!value)
    {
        Log(LOG_LEVEL_VERBOSE, "Function REGLIST was promised a list called '%s' but this was not found", listvar);
        return FnFailure();
    }

    if (DataTypeToRvalType(value_type) != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE, "Function reglist was promised a list called '%s' but this variable is not a list",
            listvar);
        return FnFailure();
    }

    pcre *rx = CompileRegex(RlistScalarValue(finalargs->next));
    if (!rx)
    {
        return FnFailure();
    }

    for (const Rlist *rp = value; rp != NULL; rp = rp->next)
    {
        if (strcmp(RlistScalarValue(rp), CF_NULL_VALUE) == 0)
        {
            continue;
        }

        if (StringMatchFullWithPrecompiledRegex(rx, RlistScalarValue(rp)))
        {
            pcre_free(rx);
            return FnReturnContext(true);
        }
    }

    pcre_free(rx);
    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallRegArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *arrayname = RlistScalarValue(finalargs);
    pcre *rx = CompileRegex(RlistScalarValue(finalargs->next));
    if (!rx)
    {
        return FnFailure();
    }

    VarRef *ref = VarRefParse(arrayname);
    bool found = false;

    VariableTableIterator *iter = EvalContextVariableTableIteratorNew(ctx, ref->ns, ref->scope, ref->lval);
    VarRefDestroy(ref);
    Variable *var;
    while ((var = VariableTableIteratorNext(iter)))
    {
        if (StringMatchFullWithPrecompiledRegex(rx, RvalScalarValue(var->rval)))
        {
            found = true;
            break;
        }
    }
    VariableTableIteratorDestroy(iter);
    pcre_free(rx);

    return FnReturnContext(found);
}

/*********************************************************************/

static FnCallResult FnCallGetIndices(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    VarRef *ref = ResolveAndQualifyVarName(fp, RlistScalarValue(finalargs));

    DataType type = CF_DATA_TYPE_NONE;
    EvalContextVariableGet(ctx, ref, &type);

    if (type != CF_DATA_TYPE_CONTAINER)
    {
        VarRefDestroy(ref);
        ref = VarRefParse(RlistScalarValue(finalargs));
        if (!VarRefIsQualified(ref))
        {
            if (fp->caller)
            {
                const Bundle *caller_bundle = PromiseGetBundle(fp->caller);
                VarRefQualify(ref, caller_bundle->ns, caller_bundle->name);
            }
            else
            {
                Log(LOG_LEVEL_WARNING,
                    "Function '%s' was given an unqualified variable reference, "
                    "and it was not called from a promise. No way to automatically qualify the reference '%s'.",
                    fp->name, RlistScalarValue(finalargs));
                VarRefDestroy(ref);
                return FnFailure();
            }
        }
    }

    type = CF_DATA_TYPE_NONE;
    const void *var_value = EvalContextVariableGet(ctx, ref, &type);

    Rlist *keys = NULL;
    if (type == CF_DATA_TYPE_CONTAINER)
    {
        if (JsonGetElementType(var_value) == JSON_ELEMENT_TYPE_CONTAINER)
        {
            if (JsonGetContainerType(var_value) == JSON_CONTAINER_TYPE_OBJECT)
            {
                JsonIterator iter = JsonIteratorInit(var_value);
                const char *key;
                while ((key = JsonIteratorNextKey(&iter)))
                {
                    RlistAppendScalar(&keys, key);
                }
            }
            else
            {
                for (size_t i = 0; i < JsonLength(var_value); i++)
                {
                    Rval key = (Rval) { StringFromLong(i), RVAL_TYPE_SCALAR };
                    RlistAppendRval(&keys, key);
                }
            }
        }
    }
    else
    {
        VariableTableIterator *iter =
                EvalContextVariableTableFromRefIteratorNew(ctx, ref);
        const Variable *var;
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

    return (FnCallResult) { FNCALL_SUCCESS, { keys, RVAL_TYPE_LIST } };
}

/*********************************************************************/

void CollectContainerValues(EvalContext *ctx, Rlist **values, const JsonElement *container)
{
    if (JsonGetElementType(container) == JSON_ELEMENT_TYPE_CONTAINER)
    {
        JsonIterator iter = JsonIteratorInit(container);
        const JsonElement *el;
        while ((el = JsonIteratorNextValue(&iter)))
        {
            if (JsonGetElementType(el) == JSON_ELEMENT_TYPE_CONTAINER)
            {
                CollectContainerValues(ctx, values, el);
            }
            else
            {
                char *value = JsonPrimitiveToString(el);
                if (NULL != value)
                {
                    RlistAppendScalar(values, value);
                    free(value);
                }
            }
        }
    }
    else if (JsonGetElementType(container) == JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        char *value = JsonPrimitiveToString(container);
        if (NULL != value)
        {
            RlistAppendScalar(values, value);
            free(value);
        }
    }
}

/*********************************************************************/

static FnCallResult FnCallGetValues(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    VarRef *ref = ResolveAndQualifyVarName(fp, RlistScalarValue(finalargs));

    DataType type = CF_DATA_TYPE_NONE;
    EvalContextVariableGet(ctx, ref, &type);

    if (type != CF_DATA_TYPE_CONTAINER)
    {
        VarRefDestroy(ref);
        ref = VarRefParse(RlistScalarValue(finalargs));
        if (!VarRefIsQualified(ref))
        {
            if (fp->caller)
            {
                const Bundle *caller_bundle = PromiseGetBundle(fp->caller);
                VarRefQualify(ref, caller_bundle->ns, caller_bundle->name);
            }
            else
            {
                Log(LOG_LEVEL_WARNING,
                    "Function '%s' was given an unqualified variable reference, "
                    "and it was not called from a promise. No way to automatically qualify the reference '%s'.",
                    fp->name, RlistScalarValue(finalargs));
                VarRefDestroy(ref);
                return FnFailure();
            }
        }
    }

    type = CF_DATA_TYPE_NONE;
    const void *var_value = EvalContextVariableGet(ctx, ref, &type);

    Rlist *values = NULL;

    if (type == CF_DATA_TYPE_CONTAINER)
    {
        CollectContainerValues(ctx, &values, var_value);
    }
    else
    {
        VariableTableIterator *iter = EvalContextVariableTableFromRefIteratorNew(ctx, ref);
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
        VariableTableIteratorDestroy(iter);
    }

    VarRefDestroy(ref);

    return (FnCallResult) { FNCALL_SUCCESS, { values, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGrep(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex
                          RlistScalarValue(finalargs->next), // list identifier
                          1, // regex match = TRUE
                          0, // invert matches = FALSE
                          LONG_MAX); // max results = max int
}

/*********************************************************************/

static FnCallResult FnCallSum(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    double sum = 0;

    const Rlist *input_list = GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), NULL);

    for (const Rlist *rp = input_list; rp; rp = rp->next)
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

static FnCallResult FnCallProduct(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    double product = 1.0;

    const Rlist *input_list = GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), NULL);

    for (const Rlist *rp = input_list; rp; rp = rp->next)
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

static FnCallResult JoinRlist(const Rlist *input_list, const char *delimiter)
{
    if (RlistIsNullList(input_list))
    {
        return FnReturn("");
    }

    bool more = false;
    Buffer *result = BufferNew();
    for (const Rlist *rp = input_list; rp; rp = rp->next)
    {
        if (!more && strcmp(RlistScalarValue(rp), CF_NULL_VALUE) == 0)
        {
            continue;
        }

        BufferAppendString(result, RlistScalarValue(rp));

        more = false;
        // skip all following cf_null values
        while (rp->next)
        {
            if (strcmp(RlistScalarValue(rp->next), CF_NULL_VALUE) == 0)
            {
                rp = rp->next;
            }
            else
            {
                more = true;
                break;
            }
        }
        if (more)
        {
            BufferAppendString(result, delimiter);
        }
    }

    return FnReturnBuffer(result);
}

static FnCallResult JoinContainer(const JsonElement *container, const char *delimiter)
{
    if (JsonGetElementType(container) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        return FnReturn("");
    }

    JsonIterator iter = JsonIteratorInit(container);
    const JsonElement *e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true);
    if (!e)
    {
        return FnReturn("");
    }

    Buffer *result = BufferNew();
    BufferAppendString(result, JsonPrimitiveGetAsString(e));

    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        BufferAppendString(result, delimiter);
        BufferAppendString(result, JsonPrimitiveGetAsString(e));
    }

    return FnReturnBuffer(result);
}

static FnCallResult FnCallJoin(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const char *delimiter = RlistScalarValue(finalargs);
    const char *name = RlistScalarValue(finalargs->next);

    DataType type = CF_DATA_TYPE_NONE;
    VarRef *ref = VarRefParse(name);
    const void *value = EvalContextVariableGet(ctx, ref, &type);
    VarRefDestroy(ref);

    if (!value)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' did not resolve to a variable",
            fp->name, name);
        return FnFailure();
    }

    switch (DataTypeToRvalType(type))
    {
    case RVAL_TYPE_LIST:
        return JoinRlist(value, delimiter);

    case RVAL_TYPE_CONTAINER:
        return JoinContainer(value, delimiter);

    default:
        Log(LOG_LEVEL_ERR, "Function '%s', argument '%s' resolved to unsupported datatype '%s'",
            fp->name, name, DataTypeToString(type));
        return FnFailure();
    }

    assert(false && "never reach");
}

/*********************************************************************/

static FnCallResult FnCallGetFields(EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    const FnCall *fp,
                                    const Rlist *finalargs)
{
    pcre *rx = CompileRegex(RlistScalarValue(finalargs));
    if (!rx)
    {
        return FnFailure();
    }

    const char *filename = RlistScalarValue(finalargs->next);
    const char *split = RlistScalarValue(finalargs->next->next);
    const char *array_lval = RlistScalarValue(finalargs->next->next->next);

    FILE *fin = safe_fopen(filename, "rt");
    if (!fin)
    {
        Log(LOG_LEVEL_ERR, "File '%s' could not be read in getfields(). (fopen: %s)", filename, GetErrorStr());
        pcre_free(rx);
        return FnFailure();
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(CF_BUFSIZE);

    int line_count = 0;

    while (CfReadLine(&line, &line_size, fin) != -1)
    {
        if (!StringMatchFullWithPrecompiledRegex(rx, line))
        {
            continue;
        }

        if (line_count == 0)
        {
            Rlist *newlist = RlistFromSplitRegex(line, split, 31, true);
            int vcount = 1;

            for (const Rlist *rp = newlist; rp != NULL; rp = rp->next)
            {
                char name[CF_MAXVARSIZE];
                snprintf(name, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, vcount);
                VarRef *ref = VarRefParse(name);
                if (!VarRefIsQualified(ref))
                {
                    if (fp->caller)
                    {
                        const Bundle *caller_bundle = PromiseGetBundle(fp->caller);
                        VarRefQualify(ref, caller_bundle->ns, caller_bundle->name);
                    }
                    else
                    {
                        Log(LOG_LEVEL_WARNING,
                            "Function '%s' was given an unqualified variable reference, "
                            "and it was not called from a promise. No way to automatically qualify the reference '%s'.",
                            fp->name, RlistScalarValue(finalargs));
                        VarRefDestroy(ref);
                        free(line);
                        RlistDestroy(newlist);
                        pcre_free(rx);
                        return FnFailure();
                    }
                }

                EvalContextVariablePut(ctx, ref, RlistScalarValue(rp), CF_DATA_TYPE_STRING, "source=function,function=getfields");
                VarRefDestroy(ref);
                Log(LOG_LEVEL_VERBOSE, "getfields: defining '%s' => '%s'", name, RlistScalarValue(rp));
                vcount++;
            }

            RlistDestroy(newlist);
        }

        line_count++;
    }

    pcre_free(rx);
    free(line);

    if (!feof(fin))
    {
        Log(LOG_LEVEL_ERR, "Unable to read data from file '%s'. (fgets: %s)", filename, GetErrorStr());
        fclose(fin);
        return FnFailure();
    }

    fclose(fin);

    return FnReturnF("%d", line_count);
}

/*********************************************************************/

static FnCallResult FnCallCountLinesMatching(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    pcre *rx = CompileRegex(RlistScalarValue(finalargs));
    if (!rx)
    {
        return FnFailure();
    }

    char *filename = RlistScalarValue(finalargs->next);

    FILE *fin = safe_fopen(filename, "rt");
    if (!fin)
    {
        Log(LOG_LEVEL_VERBOSE, "File '%s' could not be read in countlinesmatching(). (fopen: %s)", filename, GetErrorStr());
        pcre_free(rx);
        return FnReturn("0");
    }

    int lcount = 0;
    {
        size_t line_size = CF_BUFSIZE;
        char *line = xmalloc(line_size);

        while (CfReadLine(&line, &line_size, fin) != -1)
        {
            if (StringMatchFullWithPrecompiledRegex(rx, line))
            {
                lcount++;
                Log(LOG_LEVEL_VERBOSE, "countlinesmatching: matched '%s'", line);
                continue;
            }
        }

        free(line);
    }

    pcre_free(rx);

    if (!feof(fin))
    {
        Log(LOG_LEVEL_ERR, "Unable to read data from file '%s'. (fgets: %s)", filename, GetErrorStr());
        fclose(fin);
        return FnFailure();
    }

    fclose(fin);

    return FnReturnF("%d", lcount);
}

/*********************************************************************/

static FnCallResult FnCallLsDir(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    Rlist *newlist = NULL;

    char *dirname = RlistScalarValue(finalargs);
    char *regex = RlistScalarValue(finalargs->next);
    int includepath = BooleanFromString(RlistScalarValue(finalargs->next->next));

    Dir *dirh = DirOpen(dirname);
    if (dirh == NULL)
    {
        Log(LOG_LEVEL_ERR, "Directory '%s' could not be accessed in lsdir(), (opendir: %s)", dirname, GetErrorStr());
        return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
    }

    const struct dirent *dirp;
    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (strlen(regex) == 0 || StringMatchFull(regex, dirp->d_name))
        {
            if (includepath)
            {
                char line[CF_BUFSIZE];
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

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

bool EvalContextVariablePutSpecialEscaped(EvalContext *ctx, SpecialScope scope, const char *lval, const void *value, DataType type, const char *tags, bool escape)
{
    if (escape)
    {
        char *escaped = EscapeCharCopy(value, '"', '\\');
        bool ret = EvalContextVariablePutSpecial(ctx, scope, lval, escaped, type, tags);
        free(escaped);
        return ret;
    }

    return EvalContextVariablePutSpecial(ctx, scope, lval, value, type, tags);
}

static FnCallResult FnCallMapData(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (!fp->caller)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' must be called from a promise", fp->name);
        return FnFailure();
    }

    bool mapdatamode = 0 == strcmp(fp->name, "mapdata");
    Rlist *returnlist = NULL;

    const char *conversion;
    const char *arg_map;
    const char *varname;

    if (mapdatamode)
    {
        conversion = RlistScalarValue(finalargs);
        arg_map = RlistScalarValue(finalargs->next);
        varname = RlistScalarValue(finalargs->next->next);
    }
    else
    {
        conversion = "none";
        arg_map = RlistScalarValue(finalargs);
        varname = RlistScalarValue(finalargs->next);
    }

    bool jsonmode = 0 == strcmp(conversion, "json");

    VarRef *ref = ResolveAndQualifyVarName(fp, varname);
    if (!ref)
    {
        return FnFailure();
    }

    JsonElement *container = VarRefValueToJson(ctx, fp, ref, NULL, 0);

    if (NULL == container)
    {
        return FnFailure();
    }

    if (JsonGetElementType(container) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' got an unexpected non-container from argument '%s'", fp->name, varname);
        JsonDestroy(container);

        return FnFailure();
    }

    Buffer *expbuf = BufferNew();

    if (JsonGetContainerType(container) != JSON_CONTAINER_TYPE_OBJECT)
    {
        JsonElement *temp = JsonObjectCreate(0);
        JsonElement *temp2 = JsonMerge(temp, container);
        JsonDestroy(temp);
        JsonDestroy(container);

        container = temp2;
    }

    JsonIterator iter = JsonIteratorInit(container);
    const JsonElement *e;

    while (NULL != (e = JsonIteratorNextValue(&iter)))
    {
        EvalContextVariablePutSpecialEscaped(ctx, SPECIAL_SCOPE_THIS, "k", JsonGetPropertyAsString(e),
                                             CF_DATA_TYPE_STRING, "source=function,function=maparray",
                                             jsonmode);

        switch (JsonGetElementType(e))
        {
        case JSON_ELEMENT_TYPE_PRIMITIVE:
            BufferClear(expbuf);
            EvalContextVariablePutSpecialEscaped(ctx, SPECIAL_SCOPE_THIS, "v", JsonPrimitiveGetAsString(e),
                                                 CF_DATA_TYPE_STRING, "source=function,function=maparray",
                                                 jsonmode);

            ExpandScalar(ctx, PromiseGetBundle(fp->caller)->ns, PromiseGetBundle(fp->caller)->name, arg_map, expbuf);

            if (strstr(BufferData(expbuf), "$(this.k)") || strstr(BufferData(expbuf), "${this.k}") ||
                strstr(BufferData(expbuf), "$(this.v)") || strstr(BufferData(expbuf), "${this.v}"))
            {
                RlistDestroy(returnlist);
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
                BufferDestroy(expbuf);
                JsonDestroy(container);
                return FnFailure();
            }

            RlistAppendScalar(&returnlist, BufferData(expbuf));
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");

            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
        {
            const JsonElement *e2;
            JsonIterator iter2 = JsonIteratorInit(e);
            int position = 0;
            while (NULL != (e2 = JsonIteratorNextValueByType(&iter2, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
            {
                char *key = (char*) JsonGetPropertyAsString(e2);
                bool havekey = NULL != key;
                if (havekey)
                {
                    EvalContextVariablePutSpecialEscaped(ctx, SPECIAL_SCOPE_THIS, "k[1]", key,
                                                         CF_DATA_TYPE_STRING, "source=function,function=maparray",
                                                         jsonmode);
                }

                BufferClear(expbuf);

                EvalContextVariablePutSpecialEscaped(ctx, SPECIAL_SCOPE_THIS, "v", JsonPrimitiveGetAsString(e2),
                                                     CF_DATA_TYPE_STRING, "source=function,function=maparray",
                                                     jsonmode);

                ExpandScalar(ctx, PromiseGetBundle(fp->caller)->ns, PromiseGetBundle(fp->caller)->name, arg_map, expbuf);

                if (strstr(BufferData(expbuf), "$(this.k)") || strstr(BufferData(expbuf), "${this.k}") ||
                    (havekey && (strstr(BufferData(expbuf), "$(this.k[1])") || strstr(BufferData(expbuf), "${this.k[1]}"))) ||
                    strstr(BufferData(expbuf), "$(this.v)") || strstr(BufferData(expbuf), "${this.v}"))
                {
                    RlistDestroy(returnlist);
                    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
                    if (havekey)
                    {
                        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k[1]");
                    }
                    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
                    BufferDestroy(expbuf);
                    JsonDestroy(container);
                    return FnFailure();
                }

                RlistAppendScalarIdemp(&returnlist, BufferData(expbuf));
                if (havekey)
                {
                    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k[1]");
                }
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
                position++;
            }
        }
        break;

        default:
            break;
        }
        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
    }

    BufferDestroy(expbuf);
    JsonDestroy(container);

    JsonElement *returnjson = NULL;

    if (returnlist == NULL && !mapdatamode)
    {
        RlistAppendScalarIdemp(&returnlist, CF_NULL_VALUE);
    }

    // this is mapdata()
    if (mapdatamode)
    {
        returnjson = JsonArrayCreate(RlistLen(returnlist));
        for (const Rlist *rp = returnlist; rp != NULL; rp = rp->next)
        {
            const char *data = RlistScalarValue(rp);
            if (jsonmode)
            {
                JsonElement *parsed = NULL;
                if (JsonParse(&data, &parsed) == JSON_PARSE_OK)
                {
                    JsonArrayAppendElement(returnjson, parsed);
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Function '%s' could not parse dynamic JSON '%s', skipping it", fp->name, data);
                }
            }
            else
            {
                JsonArrayAppendString(returnjson, data);
            }
        }

        RlistDestroy(returnlist);
        return (FnCallResult) { FNCALL_SUCCESS, { returnjson, RVAL_TYPE_CONTAINER } };
    }

    // this is maparray()
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallMapList(EvalContext *ctx,
                                  ARG_UNUSED const Policy *policy,
                                  ARG_UNUSED const FnCall *fp,
                                  const Rlist *finalargs)
{
    Rlist *newlist = NULL;

    const char *arg_map = RlistScalarValue(finalargs);
    char *listvar = RlistScalarValue(finalargs->next);

    char naked[CF_MAXVARSIZE] = "";
    if (IsVarList(listvar))
    {
        GetNaked(naked, listvar);
    }
    else
    {
        strlcpy(naked, listvar, CF_MAXVARSIZE);
    }

    VarRef *ref = VarRefParse(naked);

    DataType retype = CF_DATA_TYPE_NONE;
    const Rlist *list = EvalContextVariableGet(ctx, ref, &retype);
    if (!list)
    {
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    if (retype != CF_DATA_TYPE_STRING_LIST && retype != CF_DATA_TYPE_INT_LIST && retype != CF_DATA_TYPE_REAL_LIST)
    {
        return FnFailure();
    }

    Buffer *expbuf = BufferNew();
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (strcmp(RlistScalarValue(rp), CF_NULL_VALUE) == 0)
        {
            continue;
        }
        BufferClear(expbuf);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "this", RlistScalarValue(rp), CF_DATA_TYPE_STRING, "source=function,function=maplist");

        ExpandScalar(ctx, NULL, "this", arg_map, expbuf);

        if (strstr(BufferData(expbuf), "$(this)") || strstr(BufferData(expbuf), "${this}"))
        {
            RlistDestroy(newlist);
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "this");
            BufferDestroy(expbuf);
            return FnFailure();
        }

        RlistAppendScalar(&newlist, BufferData(expbuf));
        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "this");
    }
    BufferDestroy(expbuf);

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/******************************************************************************/

static FnCallResult FnCallExpandRange(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    Rlist *newlist = NULL;
    const char *template = RlistScalarValue(finalargs);
    char *step = RlistScalarValue(finalargs->next);
    size_t template_size = strlen(template) + 1;
    char *before = xstrdup(template);
    char *after = xcalloc(template_size, 1);
    char *work = xstrdup(template);
    int from = CF_NOINT, to = CF_NOINT, step_size = atoi(step);

    if (*template == '[')
    {
        *before = '\0';
        sscanf(template, "[%d-%d]%[^\n]", &from, &to, after);
    }
    else
    {
        sscanf(template, "%[^[\[][%d-%d]%[^\n]", before, &from, &to, after);
    }

    if (step_size < 1 || fabs(from-to) < step_size)
    {
        FatalError(ctx, "EXPANDRANGE Step size cannot be less than 1 or greater than the interval");
    }

    if (from == CF_NOINT || to == CF_NOINT)
    {
        FatalError(ctx, "EXPANDRANGE malformed range expression");
    }

    if (from > to)
    {
        for (int i = from; i >= to; i -= step_size)
        {
            xsnprintf(work, template_size, "%s%d%s",before,i,after);;
            RlistAppendScalar(&newlist, work);
        }
    }
    else
    {
        for (int i = from; i <= to; i += step_size)
        {
            xsnprintf(work, template_size, "%s%d%s",before,i,after);;
            RlistAppendScalar(&newlist, work);
        }
    }

    free(before);
    free(after);

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*****************************************************************************/

static FnCallResult FnCallMergeData(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    if (RlistLen(args) == 0)
    {
        Log(LOG_LEVEL_ERR, "%s needs at least one argument, a reference to a container variable", fp->name);
        return FnFailure();
    }

    for (const Rlist *arg = args; arg; arg = arg->next)
    {
        if (args->val.type != RVAL_TYPE_SCALAR)
        {
            Log(LOG_LEVEL_ERR, "%s: argument '%s' is not a variable reference", fp->name, RlistScalarValue(arg));
            return FnFailure();
        }
    }

    Seq *containers = SeqNew(10, &JsonDestroy);

    for (const Rlist *arg = args; arg; arg = arg->next)
    {
        const char *name_str = RlistScalarValue(arg);
        int name_len = strlen(name_str);
        bool wrap_array_mode = false;
        Buffer *wrap_map_key = NULL;
        Buffer *name = NULL;

        if (name_len > 2 && name_str[0] == '[')
        {
            Seq *s = StringMatchCaptures("^\\[ *([^ ]+) *\\]$", name_str, false);

            if (s && SeqLength(s) == 2)
            {
                wrap_array_mode = true;
                name = BufferCopy((const Buffer*) SeqAt(s, 1));
            }

            SeqDestroy(s);
        }
        else if (name_len > 0 && name_str[0] == '{' && name_str[name_len-1] == '}')
        {
            Seq *s = StringMatchCaptures("^\\{ *\"([^\"]+)\" *: *([^ ]+) *\\}$", name_str, false);

            if (s && SeqLength(s) == 3)
            {
                wrap_map_key = BufferCopy((const Buffer*) SeqAt(s, 1));
                name = BufferCopy((const Buffer*) SeqAt(s, 2));
            }

            SeqDestroy(s);
        }
        else
        {
            name = BufferNewFrom(name_str, name_len);
        }

        VarRef *ref = NULL;

        if (NULL != name)
        {
            ref = ResolveAndQualifyVarName(fp, BufferData(name));
            BufferDestroy(name);
        }

        if (!ref)
        {
            SeqDestroy(containers);

            if (NULL != wrap_map_key) BufferDestroy(wrap_map_key);

            return FnFailure();
        }

        JsonElement *json = VarRefValueToJson(ctx, fp, ref, NULL, 0);

        VarRefDestroy(ref);

        if (!json)
        {
            SeqDestroy(containers);

            if (NULL != wrap_map_key) BufferDestroy(wrap_map_key);

            return FnFailure();
        }

        if (wrap_array_mode)
        {
            JsonElement *parent = JsonArrayCreate(1);
            JsonArrayAppendElement(parent, json);
            json = parent;
        }
        else if (NULL != wrap_map_key)
        {
            JsonElement *parent = JsonObjectCreate(1);
            JsonObjectAppendElement(parent, BufferData(wrap_map_key), json);
            json = parent;
        }
        else
        {
            // do nothing, no wrapping
        }

        SeqAppend(containers, json);

        if (NULL != wrap_map_key) BufferDestroy(wrap_map_key);

    } // end of args loop

    if (SeqLength(containers) == 1)
    {
        JsonElement *first = JsonCopy(SeqAt(containers, 0));
        SeqDestroy(containers);
        return  (FnCallResult) { FNCALL_SUCCESS, (Rval) { first, RVAL_TYPE_CONTAINER } };
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

JsonElement *DefaultTemplateData(const EvalContext *ctx, const char *wantbundle)
{
    JsonElement *hash = JsonObjectCreate(30);
    JsonElement *classes = NULL;
    JsonElement *bundles = NULL;

    bool want_all_bundles = (NULL == wantbundle);

    if (want_all_bundles) // no specific bundle
    {
        classes = JsonObjectCreate(50);
        bundles = JsonObjectCreate(50);
        JsonObjectAppendObject(hash, "classes", classes);
        JsonObjectAppendObject(hash, "vars", bundles);

        ClassTableIterator *it = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
        Class *cls;
        while ((cls = ClassTableIteratorNext(it)))
        {
            char *key = ClassRefToString(cls->ns, cls->name);
            JsonObjectAppendBool(classes, key, true);
            free(key);
        }
        ClassTableIteratorDestroy(it);

        it = EvalContextClassTableIteratorNewLocal(ctx);
        while ((cls = ClassTableIteratorNext(it)))
        {
            char *key = ClassRefToString(cls->ns, cls->name);
            JsonObjectAppendBool(classes, key, true);
            free(key);
        }
        ClassTableIteratorDestroy(it);
    }

    {
        VariableTableIterator *it = EvalContextVariableTableIteratorNew(ctx, NULL, NULL, NULL);
        Variable *var;
        while ((var = VariableTableIteratorNext(it)))
        {
            // TODO: need to get a CallRef, this is bad
            char *scope_key = ClassRefToString(var->ref->ns, var->ref->scope);

            JsonElement *scope_obj = NULL;
            if (want_all_bundles)
            {
                scope_obj = JsonObjectGetAsObject(bundles, scope_key);
                if (!scope_obj)
                {
                    scope_obj = JsonObjectCreate(50);
                    JsonObjectAppendObject(bundles, scope_key, scope_obj);
                }
            }
            else if (0 == strcmp(scope_key, wantbundle))
            {
                scope_obj = hash;
            }

            free(scope_key);

            if (NULL != scope_obj)
            {
                char *lval_key = VarRefToString(var->ref, false);
                if (NULL == strchr(lval_key, '#')) // don't collect mangled refs
                {
                    JsonObjectAppendElement(scope_obj, lval_key, RvalToJson(var->rval));
                }
                free(lval_key);
            }
        }
        VariableTableIteratorDestroy(it);
    }

    Writer *w = StringWriter();
    JsonWrite(w, hash, 0);
    Log(LOG_LEVEL_DEBUG, "Generated DefaultTemplateData '%s'", StringWriterData(w));
    WriterClose(w);

    return hash;
}

static FnCallResult FnCallDatastate(EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    ARG_UNUSED const FnCall *fp,
                                    ARG_UNUSED const Rlist *args)
{
    JsonElement *state = DefaultTemplateData(ctx, NULL);
    return  (FnCallResult) { FNCALL_SUCCESS, (Rval) { state, RVAL_TYPE_CONTAINER } };
}

static FnCallResult FnCallBundlestate(EvalContext *ctx,
                                      ARG_UNUSED const Policy *policy,
                                      ARG_UNUSED const FnCall *fp,
                                      ARG_UNUSED const Rlist *args)
{
    JsonElement *state = DefaultTemplateData(ctx, RlistScalarValue(args));

    if (NULL == state ||
        JsonGetElementType(state) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonLength(state) < 1)
    {
        if (NULL != state)
        {
            JsonDestroy(state);
        }

        return FnFailure();
    }
    else
    {
        return  (FnCallResult) { FNCALL_SUCCESS, (Rval) { state, RVAL_TYPE_CONTAINER } };
    }
}


static FnCallResult FnCallSelectServers(EvalContext *ctx,
                                        ARG_UNUSED const Policy *policy,
                                        const FnCall *fp,
                                        const Rlist *finalargs)
{
    const char *listvar = RlistScalarValue(finalargs);
    const char *port = RlistScalarValue(finalargs->next);
    const char *sendstring = RlistScalarValue(finalargs->next->next);
    const char *regex = RlistScalarValue(finalargs->next->next->next);
    ssize_t maxbytes = IntFromString(RlistScalarValue(finalargs->next->next->next->next));
    char *array_lval = xstrdup(RlistScalarValue(finalargs->next->next->next->next->next));

    if (!IsQualifiedVariable(array_lval))
    {
        if (fp->caller)
        {
            VarRef *ref = VarRefParseFromBundle(array_lval, PromiseGetBundle(fp->caller));
            free(array_lval);
            array_lval = VarRefToString(ref, true);
            VarRefDestroy(ref);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Function '%s' called with an unqualifed array reference '%s', "
                "and the reference could not be automatically qualified as the function was not called from a promise.",
                fp->name, array_lval);
            free(array_lval);
            return FnFailure();
        }
    }

    char naked[CF_MAXVARSIZE] = "";

    if (IsVarList(listvar))
    {
        GetNaked(naked, listvar);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function selectservers was promised a list called '%s' but this was not found",
            listvar);
        return FnFailure();
    }

    VarRef *ref = VarRefParse(naked);
    DataType value_type;
    const Rlist *hostnameip = EvalContextVariableGet(ctx, ref, &value_type);
    if (!hostnameip)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function selectservers was promised a list called '%s' but this was not found from context '%s.%s'",
              listvar, ref->scope, naked);
        VarRefDestroy(ref);
        free(array_lval);
        return FnFailure();
    }

    VarRefDestroy(ref);

    if (DataTypeToRvalType(value_type) != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function selectservers was promised a list called '%s' but this variable is not a list",
            listvar);
        free(array_lval);
        return FnFailure();
    }

    if (maxbytes < 0 || maxbytes > CF_BUFSIZE - 1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "selectservers: invalid number of bytes %zd to read, defaulting to %d",
            maxbytes, CF_BUFSIZE - 1);
        maxbytes = CF_BUFSIZE - 1;
    }

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        free(array_lval);
        return FnReturnF("%d", 0);
    }

    Policy *select_server_policy = PolicyNew();
    {
        Bundle *bp = PolicyAppendBundle(select_server_policy, NamespaceDefault(),
                                        "select_server_bundle", "agent", NULL, NULL);
        PromiseType *tp = BundleAppendPromiseType(bp, "select_server");

        PromiseTypeAppendPromise(tp, "function", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, NULL, NULL);
    }

    size_t count = 0;
    for (const Rlist *rp = hostnameip; rp != NULL; rp = rp->next)
    {
        const char *host = RlistScalarValue(rp);
        Log(LOG_LEVEL_DEBUG, "Want to read %zd bytes from %s port %s",
            maxbytes, host, port);

        char txtaddr[CF_MAX_IP_LEN] = "";
        int sd = SocketConnect(host, port, CONNTIMEOUT, false,
                               txtaddr, sizeof(txtaddr));
        if (sd == -1)
        {
            continue;
        }

        if (strlen(sendstring) > 0)
        {
            if (SendSocketStream(sd, sendstring, strlen(sendstring)) != -1)
            {
                char recvbuf[CF_BUFSIZE];
                ssize_t n_read = recv(sd, recvbuf, maxbytes, 0);

                if (n_read != -1)
                {
                    /* maxbytes was checked earlier, but just make sure... */
                    assert(n_read < sizeof(recvbuf));
                    recvbuf[n_read] = '\0';

                    if (strlen(regex) == 0 || StringMatchFull(regex, recvbuf))
                    {
                        Log(LOG_LEVEL_VERBOSE,
                            "selectservers: Got matching reply from host %s address %s",
                            host, txtaddr);

                        char buffer[CF_MAXVARSIZE] = "";
                        snprintf(buffer, sizeof(buffer), "%s[%zu]", array_lval, count);
                        VarRef *ref = VarRefParse(buffer);
                        EvalContextVariablePut(ctx, ref, host, CF_DATA_TYPE_STRING,
                                               "source=function,function=selectservers");
                        VarRefDestroy(ref);

                        count++;
                    }
                }
            }
        }
        else                      /* If query is empty, all hosts are added */
        {
            Log(LOG_LEVEL_VERBOSE,
                "selectservers: Got reply from host %s address %s",
                host, txtaddr);

            char buffer[CF_MAXVARSIZE] = "";
            snprintf(buffer, sizeof(buffer), "%s[%zu]", array_lval, count);
            VarRef *ref = VarRefParse(buffer);
            EvalContextVariablePut(ctx, ref, host, CF_DATA_TYPE_STRING,
                                   "source=function,function=selectservers");
            VarRefDestroy(ref);

            count++;
        }

        cf_closesocket(sd);
    }

    PolicyDestroy(select_server_policy);
    free(array_lval);

    Log(LOG_LEVEL_VERBOSE, "selectservers: found %zu servers", count);
    return FnReturnF("%zu", count);
}


static FnCallResult FnCallShuffle(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char *seed_str = RlistScalarValue(finalargs->next);

    DataType list_dtype = CF_DATA_TYPE_NONE;
    const Rlist *list = GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), &list_dtype);

    if (list_dtype != CF_DATA_TYPE_STRING_LIST)
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


static FnCallResult FnCallIsNewerThan(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    struct stat frombuf, tobuf;

    if (stat(RlistScalarValue(finalargs),     &frombuf) == -1 ||
        stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return FnFailure();
    }

    return FnReturnContext(frombuf.st_mtime > tobuf.st_mtime);
}

/*********************************************************************/

static FnCallResult FnCallIsAccessedBefore(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    struct stat frombuf, tobuf;

    if (stat(RlistScalarValue(finalargs),     &frombuf) == -1 ||
        stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return FnFailure();
    }

    return FnReturnContext(frombuf.st_atime < tobuf.st_atime);
}

/*********************************************************************/

static FnCallResult FnCallIsChangedBefore(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    struct stat frombuf, tobuf;

    if (stat(RlistScalarValue(finalargs),     &frombuf) == -1 ||
        stat(RlistScalarValue(finalargs->next), &tobuf) == -1)
    {
        return FnFailure();
    }

    return FnReturnContext(frombuf.st_ctime > tobuf.st_ctime);
}

/*********************************************************************/

static FnCallResult FnCallFileStat(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    char *path = RlistScalarValue(finalargs);
    struct stat statbuf;

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
        return FnReturnF("%ju", (uintmax_t) statbuf.st_size);
    }

    ProgrammingError("Unexpected function name in FnCallFileStat: %s", fp->name);
}

/*********************************************************************/

static FnCallResult FnCallFileStatDetails(ARG_UNUSED EvalContext *ctx,
                                          ARG_UNUSED const Policy *policy,
                                          const FnCall *fp,
                                          const Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *path = RlistScalarValue(finalargs);
    char *detail = RlistScalarValue(finalargs->next);
    struct stat statbuf;

    buffer[0] = '\0';

    if (lstat(path, &statbuf) == -1)
    {
        return FnFailure();
    }
    else if (!strcmp(detail, "xattr"))
    {
#if defined(WITH_XATTR)
        // Extended attributes include both POSIX ACLs and SELinux contexts.
        char attr_raw_names[CF_BUFSIZE];
        ssize_t attr_raw_names_size = llistxattr(path, attr_raw_names, sizeof(attr_raw_names));

        if (attr_raw_names_size < 0)
        {
            if (errno != ENOTSUP && errno != ENODATA)
            {
                Log(LOG_LEVEL_ERR, "Can't read extended attributes of '%s'. (llistxattr: %s)",
                    path, GetErrorStr());
            }
        }
        else
        {
            Buffer *printattr = BufferNew();
            for (int pos = 0; pos < attr_raw_names_size;)
            {
                const char *current = attr_raw_names + pos;
                pos += strlen(current) + 1;

                if (!StringIsPrintable(current))
                {
                    Log(LOG_LEVEL_INFO, "Skipping extended attribute of '%s', it has a non-printable name: '%s'",
                        path, current);
                    continue;
                }

                char data[CF_BUFSIZE];
                int datasize = lgetxattr(path, current, data, sizeof(data));
                if (datasize < 0)
                {
                    if (errno == ENOTSUP)
                    {
                        continue;
                    }
                    else
                    {
                        Log(LOG_LEVEL_ERR, "Can't read extended attribute '%s' of '%s'. (lgetxattr: %s)",
                            path, current, GetErrorStr());
                    }
                }
                else
                {
                  if (!StringIsPrintable(data))
                  {
                      Log(LOG_LEVEL_INFO, "Skipping extended attribute of '%s', it has non-printable data: '%s=%s'",
                          path, current, data);
                      continue;
                  }

                  BufferPrintf(printattr, "%s=%s", current, data);

                  // Append a newline for multiple attributes.
                  if (attr_raw_names_size > 0)
                  {
                      BufferAppendChar(printattr, '\n');
                  }
                }
            }

            snprintf(buffer, CF_MAXVARSIZE, "%s", BufferData(printattr));
            BufferDestroy(printattr);
        }
#else // !WITH_XATTR
    // do nothing, leave the buffer empty
#endif
    }
    else if (!strcmp(detail, "size"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) statbuf.st_size);
    }
    else if (!strcmp(detail, "gid"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) statbuf.st_gid);
    }
    else if (!strcmp(detail, "uid"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) statbuf.st_uid);
    }
    else if (!strcmp(detail, "ino"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) statbuf.st_ino);
    }
    else if (!strcmp(detail, "nlink"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) statbuf.st_nlink);
    }
    else if (!strcmp(detail, "ctime"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%jd", (intmax_t) statbuf.st_ctime);
    }
    else if (!strcmp(detail, "mtime"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%jd", (intmax_t) statbuf.st_mtime);
    }
    else if (!strcmp(detail, "atime"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%jd", (intmax_t) statbuf.st_atime);
    }
    else if (!strcmp(detail, "permstr"))
    {
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
    }
    else if (!strcmp(detail, "permoct"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%jo", (uintmax_t) (statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)));
    }
    else if (!strcmp(detail, "modeoct"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%jo", (uintmax_t) statbuf.st_mode);
    }
    else if (!strcmp(detail, "mode"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) statbuf.st_mode);
    }
    else if (!strcmp(detail, "type"))
    {
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
    }
    else if (!strcmp(detail, "dev_minor"))
    {
#if !defined(__MINGW32__)
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) minor(statbuf.st_dev) );
#else
        snprintf(buffer, CF_MAXVARSIZE, "Not available on Windows");
#endif
    }
    else if (!strcmp(detail, "dev_major"))
    {
#if !defined(__MINGW32__)
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) major(statbuf.st_dev) );
#else
        snprintf(buffer, CF_MAXVARSIZE, "Not available on Windows");
#endif
    }
    else if (!strcmp(detail, "devno"))
    {
#if !defined(__MINGW32__)
        snprintf(buffer, CF_MAXVARSIZE, "%ju", (uintmax_t) statbuf.st_dev );
#else
        snprintf(buffer, CF_MAXVARSIZE, "%c:", statbuf.st_dev + 'A');
#endif
    }
    else if (!strcmp(detail, "dirname"))
    {
        snprintf(buffer, CF_MAXVARSIZE, "%s", path);
        ChopLastNode(buffer);
        MapName(buffer);
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

        strlcpy(path_buffer, path, CF_MAXVARSIZE);

        // Iterate while we're looking at a link.
        while (S_ISLNK(statbuf.st_mode))
        {
            if (cycles > max_cycles)
            {
                Log(LOG_LEVEL_INFO,
                    "%s bailing on link '%s' (original '%s') because %d cycles were chased",
                    fp->name, path_buffer, path, cycles + 1);
                break;
            }

            Log(LOG_LEVEL_VERBOSE, "%s resolving link '%s', cycle %d", fp->name, path_buffer, cycles+1);

            /* Note we subtract 1 since we may need an extra char for '\0'. */
            ssize_t got = readlink(path_buffer, buffer, CF_BUFSIZE - 1);
            if (got < 0)
            {
                // An error happened.  Empty the buffer (don't keep the last target).
                Log(LOG_LEVEL_ERR, "%s could not readlink '%s'", fp->name, path_buffer);
                path_buffer[0] = '\0';
                break;
            }
            buffer[got] = '\0'; /* readlink() doesn't terminate */

            Log(LOG_LEVEL_VERBOSE, "%s resolved link '%s' to %s", fp->name, path_buffer, buffer);
            // We got a good link target into buffer.  Copy it to path_buffer.
            strlcpy(path_buffer, buffer, CF_MAXVARSIZE);

            if (!recurse)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "%s bailing on link '%s' (original '%s') because linktarget_shallow was requested",
                    fp->name, path_buffer, path);
                break;
            }
            else if (lstat(path_buffer, &statbuf) == -1)
            {
                Log(LOG_LEVEL_INFO,
                    "%s bailing on link '%s' (original '%s') because it could not be read",
                    fp->name, path_buffer, path);
                break;
            }

            // At this point we haven't bailed, path_buffer has the link target
            cycles++;
        }

        // Get the path_buffer back into buffer.
        strlcpy(buffer, path_buffer, CF_MAXVARSIZE);
#else
        // Always return the original path on W32.
        strlcpy(buffer, path, CF_MAXVARSIZE);
#endif
    }

    return FnReturn(buffer);
}

/*********************************************************************/

static FnCallResult FnCallFindfiles(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    Rlist *returnlist = NULL;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall %s-arg", fp->name);

    /* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, CF_DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (const Rlist *arg = finalargs;  /* Start with arg set to finalargs. */
         arg;              /* We must have arg to proceed. */
         arg = arg->next)  /* arg steps forward every time. */
    {
        const char *pattern = RlistScalarValue(arg);
        glob_t globbuf;
        int globflags = 0; // TODO: maybe add GLOB_BRACE later

        const char* r_candidates[] = { "*", "*/*", "*/*/*", "*/*/*/*", "*/*/*/*/*", "*/*/*/*/*/*" };
        bool starstar = strstr(pattern, "**");
        const char** candidates = starstar ? r_candidates : NULL;
        const int candidate_count = strstr(pattern, "**") ? 6 : 1;

        for (int pi = 0; pi < candidate_count; pi++)
        {
            char* expanded = starstar ? SearchAndReplace(pattern, "**", candidates[pi]) : (char*) pattern;

#ifdef _WIN32
            if (strchr(expanded, '\\'))
            {
                Log(LOG_LEVEL_VERBOSE, "Found backslash escape character in glob pattern '%s'. "
                    "Was forward slash intended?", expanded);
            }
#endif

            if (0 == glob(expanded, globflags, NULL, &globbuf))
            {
                for (int i = 0; i < globbuf.gl_pathc; i++)
                {
                    char* found = globbuf.gl_pathv[i];
                    char fname[CF_BUFSIZE];
                    // TODO: this truncates the filename and may be removed
                    // if Rlist and the core are OK with that possibility
                    strlcpy(fname, found, CF_BUFSIZE);
                    Log(LOG_LEVEL_VERBOSE, "%s pattern '%s' found match '%s'", fp->name, pattern, fname);
                    RlistAppendScalarIdemp(&returnlist, fname);
                }

                globfree(&globbuf);
            }

            if (starstar)
            {
                free(expanded);
            }
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFilter(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
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

static const Rlist *GetListReferenceArgument(const EvalContext *ctx, const const FnCall *fp, const char *lval_str, DataType *datatype_out)
{
    VarRef *ref = VarRefParse(lval_str);

    DataType value_type = CF_DATA_TYPE_NONE;
    const Rlist *value = EvalContextVariableGet(ctx, ref, &value_type);
    if (!value)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Could not resolve expected list variable '%s' in function '%s'",
            lval_str,
            fp->name);
        VarRefDestroy(ref);
        if (datatype_out)
        {
            *datatype_out = CF_DATA_TYPE_NONE;
        }
        return NULL;
    }

    VarRefDestroy(ref);

    if (DataTypeToRvalType(value_type) != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' expected a list variable reference, got variable of type '%s'",
            fp->name, DataTypeToString(value_type));
        if (datatype_out)
        {
            *datatype_out = CF_DATA_TYPE_NONE;
        }
        return NULL;
    }

    if (datatype_out)
    {
        *datatype_out = value_type;
    }
    return value;
}

/*********************************************************************/

static FnCallResult FilterInternal(EvalContext *ctx,
                                   const FnCall *fp,
                                   const char *regex,
                                   const char *name,
                                   bool do_regex,
                                   bool invert,
                                   long max)
{
    pcre *rx = NULL;
    if (do_regex)
    {
        rx = CompileRegex(regex);
        if (!rx)
        {
            return FnFailure();
        }
    }

    DataType type = CF_DATA_TYPE_NONE;
    VarRef *ref = VarRefParse(name);
    const void *value = EvalContextVariableGet(ctx, ref, &type);
    VarRefDestroy(ref);
    if (!value)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' did not resolve to a variable",
            fp->name, name);
        pcre_free(rx);
        return FnFailure();
    }

    Rlist *returnlist = NULL;
    Rlist *input_list = NULL;
    JsonElement *json = NULL;

    switch (DataTypeToRvalType(type))
    {
    case RVAL_TYPE_LIST:
        input_list = (Rlist *)value;
        if (NULL != input_list && NULL == input_list->next
            && input_list->val.type == RVAL_TYPE_SCALAR
            && strcmp(RlistScalarValue(input_list), CF_NULL_VALUE) == 0) // TODO: This... bullshit
        {
            input_list = NULL;
        }
        break;
    case RVAL_TYPE_CONTAINER:
        json = (JsonElement *)value;
        break;
    default:
        Log(LOG_LEVEL_ERR, "Function '%s', argument '%s' resolved to unsupported datatype '%s'",
            fp->name, name, DataTypeToString(type));
        pcre_free(rx);
        return FnFailure();
    }

    long match_count = 0;
    long total = 0;

    if (NULL != input_list)
    {
        for (const Rlist *rp = input_list; rp != NULL && match_count < max; rp = rp->next)
        {
            bool found;

            if (do_regex)
            {
                found = StringMatchFullWithPrecompiledRegex(rx, RlistScalarValue(rp));
            }
            else
            {
                found = (0 == strcmp(regex, RlistScalarValue(rp)));
            }

            if (invert ? !found : found)
            {
                RlistAppendScalar(&returnlist, RlistScalarValue(rp));
                match_count++;

                if (0 == strcmp(fp->name, "some"))
                {
                    break;
                }
            }
            else if (0 == strcmp(fp->name, "every"))
            {
                total++;
                break;
            }

            total++;
        }
    }
    else if (NULL != json)
    {
        if (JsonGetElementType(json) == JSON_ELEMENT_TYPE_CONTAINER)
        {
            JsonIterator iter = JsonIteratorInit(json);
            const JsonElement *el = NULL;
            while ((el = JsonIteratorNextValue(&iter)) && match_count < max)
            {
                char *val = JsonPrimitiveToString(el);
                if (NULL != val)
                {
                    bool found;
                    if (do_regex)
                    {
                        found = StringMatchFullWithPrecompiledRegex(rx, val);
                    }
                    else
                    {
                        found = (0==strcmp(regex, val));
                    }

                    if (invert ? !found : found)
                    {
                        RlistAppendScalar(&returnlist, val);
                        match_count++;

                        if (0 == strcmp(fp->name, "some"))
                        {
                            free(val);
                            break;
                        }
                    }
                    else if (0 == strcmp(fp->name, "every"))
                    {
                        total++;
                        free(val);
                        break;
                    }

                    total++;
                    free(val);
                }
            }
        }
    }

    if (rx)
    {
        pcre_free(rx);
    }

    bool contextmode = 0;
    bool ret;
    if (0 == strcmp(fp->name, "every"))
    {
        contextmode = 1;
        ret = (match_count == total && total > 0);
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
        RlistDestroy(returnlist);
        return FnReturnContext(ret);
    }

    // else, return the list itself
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallSublist(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs); // list identifier
    bool head = 0 == strcmp(RlistScalarValue(finalargs->next), "head"); // heads or tails
    long max = IntFromString(RlistScalarValue(finalargs->next->next)); // max results

    Rlist *returnlist = NULL;

    const Rlist *input_list = GetListReferenceArgument(ctx, fp, name, NULL);

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

static FnCallResult FnCallSetop(EvalContext *ctx,
                                ARG_UNUSED const Policy *policy,
                                const FnCall *fp, const Rlist *finalargs)
{
    bool difference = (0 == strcmp(fp->name, "difference"));

    const char *name_a = RlistScalarValue(finalargs);
    const char *name_b = RlistScalarValue(finalargs->next);

    const Rlist *input_list_a = GetListReferenceArgument(ctx, fp, name_a, NULL);
    const Rlist *input_list_b = GetListReferenceArgument(ctx, fp, name_b, NULL);

    Rlist *returnlist = NULL;

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

static FnCallResult FnCallLength(EvalContext *ctx,
                                 ARG_UNUSED const Policy *policy,
                                 const FnCall *fp, const Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs);

    DataType type = CF_DATA_TYPE_NONE;
    VarRef *ref = VarRefParse(name);
    const void *value = EvalContextVariableGet(ctx, ref, &type);
    VarRefDestroy(ref);
    if (!value)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function '%s', argument '%s' did not resolve to a variable",
            fp->name, name);
        return FnFailure();
    }

    switch (DataTypeToRvalType(type))
    {
    case RVAL_TYPE_LIST:
        {
            int len = RlistLen(value);
            if (len == 1
                && ((Rlist *)value)->val.type == RVAL_TYPE_SCALAR
                && strcmp(RlistScalarValue(value), CF_NULL_VALUE) == 0) // TODO: This... bullshit
            {
                return FnReturn("0");
            }
            else
            {
                return FnReturnF("%d", len);
            }
        }
    case RVAL_TYPE_CONTAINER:
        return FnReturnF("%zd", JsonLength(value));
    default:
        Log(LOG_LEVEL_ERR,
            "Function '%s', argument '%s' resolved to unsupported datatype '%s'",
            fp->name, name, DataTypeToString(type));
        return FnFailure();
    }
}

static FnCallResult FnCallFold(EvalContext *ctx,
                               ARG_UNUSED const Policy *policy,
                               const FnCall *fp, const Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs);
    const char *sort_type = finalargs->next ? RlistScalarValue(finalargs->next) : NULL;

    size_t count = 0;
    double mean = 0;
    double M2 = 0;
    char* min = NULL;
    char* max = NULL;
    bool variance_mode = strcmp(fp->name, "variance") == 0;
    bool mean_mode = strcmp(fp->name, "mean") == 0;
    bool max_mode = strcmp(fp->name, "max") == 0;
    bool min_mode = strcmp(fp->name, "min") == 0;

    VarRef *ref = ResolveAndQualifyVarName(fp, name);
    if (!ref)
    {
        return FnFailure();
    }

    JsonElement *json = VarRefValueToJson(ctx, fp, ref, NULL, 0);
    VarRefDestroy(ref);

    if (!json)
    {
        return FnFailure();
    }

    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *el;
    while ((el = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        char *value = JsonPrimitiveToString(el);

        if (NULL != value)
        {
            if (sort_type)
            {
                if (min_mode && (NULL == min || !GenericStringItemLess(sort_type, min, value)))
                {
                    free(min);
                    min = xstrdup(value);
                }

                if (max_mode && (NULL == max || GenericStringItemLess(sort_type, max, value)))
                {
                    free(max);
                    max = xstrdup(value);
                }
            }

            count++;

            if (mean_mode || variance_mode)
            {
                double x;
                if (1 != sscanf(value, "%lf", &x))
                {
                    x = 0; /* treat non-numeric entries as zero */
                }

                // Welford's algorithm
                double delta = x - mean;
                mean += delta/count;
                M2 += delta * (x - mean);
            }

            free(value);
        }
    }

    JsonDestroy(json);

    if (mean_mode)
    {
        return count == 0 ? FnFailure() : FnReturnF("%lf", mean);
    }
    else if (variance_mode)
    {
        double variance = 0;

        if (count == 0)
        {
            return FnFailure();
        }

        // if count is 1, variance is 0

        if (count > 1)
        {
            variance = M2/(count - 1);
        }

        return FnReturnF("%lf", variance);
    }
    else if (max_mode)
    {
        return NULL == max ? FnFailure() : FnReturnNoCopy(max);
    }
    else if (min_mode)
    {
        return NULL == min ? FnFailure() : FnReturnNoCopy(min);
    }

    // else, we don't know this fp->name
    ProgrammingError("Unknown function call %s to FnCallFold", fp->name);
    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallUnique(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char *name = RlistScalarValue(finalargs);

    Rlist *returnlist = NULL;
    const Rlist *input_list = GetListReferenceArgument(ctx, fp, name, NULL);

    for (const Rlist *rp = input_list; rp != NULL; rp = rp->next)
    {
        RlistAppendScalarIdemp(&returnlist, RlistScalarValue(rp));
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/
/* This function has been removed from the function list for now     */
/*********************************************************************/
#ifdef SUPPORT_FNCALL_DATATYPE
static FnCallResult FnCallDatatype(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char* const varname = RlistScalarValue(finalargs);

    VarRef* const ref = VarRefParse(varname);
    DataType type = CF_DATA_TYPE_NONE;
    const void *value = EvalContextVariableGet(ctx, ref, &type);
    VarRefDestroy(ref);

    Writer* const typestring = StringWriter();

    if (type == CF_DATA_TYPE_CONTAINER)
    {

        const JsonElement* const jelement = value;

        if (JsonGetElementType(jelement) == JSON_ELEMENT_TYPE_CONTAINER)
        {
            switch (JsonGetContainerType(jelement))
            {
            case JSON_CONTAINER_TYPE_OBJECT:
                WriterWrite(typestring, "json_object");
                break;
            case JSON_CONTAINER_TYPE_ARRAY:
                WriterWrite(typestring, "json_array");
                break;
            }
        }
        else if (JsonGetElementType(jelement) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            switch (JsonGetPrimitiveType(jelement))
            {
            case JSON_PRIMITIVE_TYPE_STRING:
                WriterWrite(typestring, "json_string");
                break;
            case JSON_PRIMITIVE_TYPE_INTEGER:
                WriterWrite(typestring, "json_integer");
                break;
            case JSON_PRIMITIVE_TYPE_REAL:
                WriterWrite(typestring, "json_real");
                break;
            case JSON_PRIMITIVE_TYPE_BOOL:
                WriterWrite(typestring, "json_bool");
                break;
            case JSON_PRIMITIVE_TYPE_NULL:
                WriterWrite(typestring, "json_null");
                break;
            }
        }

    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s: variable '%s' is not a data container", fp->name, varname);
        return FnFailure();
    }

    return FnReturnNoCopy(StringWriterClose(typestring));
}
#endif /* unused code */
/*********************************************************************/

static FnCallResult FnCallNth(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char* const varname = RlistScalarValue(finalargs);

    const char* const key = RlistScalarValue(finalargs->next);

    VarRef *ref = VarRefParse(varname);
    DataType type = CF_DATA_TYPE_NONE;
    const void *value = EvalContextVariableGet(ctx, ref, &type);
    VarRefDestroy(ref);

    if (type == CF_DATA_TYPE_CONTAINER)
    {
        Rlist *return_list = NULL;

        const char *jstring = NULL;
        if (JsonGetElementType(value) == JSON_ELEMENT_TYPE_CONTAINER)
        {
            JsonElement* jholder = (JsonElement*) value;
            JsonContainerType ct = JsonGetContainerType(value);
            JsonElement* jelement = NULL;

            if (JSON_CONTAINER_TYPE_OBJECT == ct)
            {
                jelement = JsonObjectGet(jholder, key);
            }
            else if (JSON_CONTAINER_TYPE_ARRAY == ct)
            {
                long index = IntFromString(key);
                if (index >= 0 && index < JsonLength(value))
                {
                    jelement = JsonAt(jholder, index);
                }
            }
            else
            {
                ProgrammingError("JSON Container is neither array nor object but type %d", (int) ct);
            }

            if (NULL != jelement && JsonGetElementType(jelement) == JSON_ELEMENT_TYPE_PRIMITIVE)
            {
                jstring = JsonPrimitiveGetAsString(jelement);
            }
        }

        if (NULL != jstring)
        {
            Log(LOG_LEVEL_DEBUG, "%s: from data container %s, got JSON data '%s'", fp->name, varname, jstring);
            RlistAppendScalar(&return_list, jstring);
        }

        if (!return_list)
        {
            return FnFailure();
        }

        FnCallResult result = FnReturn(RlistScalarValue(return_list));
        RlistDestroy(return_list);
        return result;
    }
    else
    {
        const Rlist *input_list = GetListReferenceArgument(ctx, fp, varname, NULL);

        const Rlist *return_list = NULL;
        long index = IntFromString(key);
        for (return_list = input_list; return_list && index--; return_list = return_list->next);

        if (!return_list)
        {
            return FnFailure();
        }

        return FnReturn(RlistScalarValue(return_list));
    }
}

/*********************************************************************/

static FnCallResult FnCallEverySomeNone(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex or string
                          RlistScalarValue(finalargs->next), // list identifier
                          1,
                          0,
                          LONG_MAX);
}

static FnCallResult FnCallSort(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const char *sort_type = RlistScalarValue(finalargs->next); // list identifier

    DataType list_var_dtype = CF_DATA_TYPE_NONE;
    const Rlist *input_list = GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), &list_var_dtype);

    if (list_var_dtype != CF_DATA_TYPE_STRING_LIST)
    {
        return FnFailure();
    }

    Rlist *sorted;

    if (strcmp(sort_type, "int") == 0)
    {
        sorted = IntSortRListNames(RlistCopy(input_list));
    }
    else if (strcmp(sort_type, "real") == 0)
    {
        sorted = RealSortRListNames(RlistCopy(input_list));
    }
    else if (strcmp(sort_type, "IP") == 0 || strcmp(sort_type, "ip") == 0)
    {
        sorted = IPSortRListNames(RlistCopy(input_list));
    }
    else if (strcmp(sort_type, "MAC") == 0 || strcmp(sort_type, "mac") == 0)
    {
        sorted = MACSortRListNames(RlistCopy(input_list));
    }
    else // "lex"
    {
        sorted = AlphaSortRListNames(RlistCopy(input_list));
    }

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { sorted, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFormat(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall %s-arg", fp->name);

    /* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, CF_DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

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
        BufferAppend(buf, format, check - format);
        Seq *s;

        while (check &&
               (s = StringMatchCaptures("^(%%|%[^diouxXeEfFgGaAcsCSpnm%]*?[diouxXeEfFgGaAcsCSpnm])([^%]*)(.*)$", check, false)))
        {
            {
                if (SeqLength(s) >= 2)
                {
                    const char *format_piece = BufferData(SeqAt(s, 1));
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
                        BufferDestroy(buf);
                        SeqDestroy(s);
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
                            BufferDestroy(buf);
                            SeqDestroy(s);
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
                    else if (strrchr(format_piece, 'S'))
                    {
                        char *found_format_spec = NULL;
                        char format_rewrite[CF_BUFSIZE];

                        strlcpy(format_rewrite, format_piece, CF_BUFSIZE);
                        found_format_spec = strrchr(format_rewrite, 'S');

                        if (found_format_spec)
                        {
                            *found_format_spec = 's';
                        }
                        else
                        {
                            ProgrammingError("Couldn't find the expected S format spec in %s", format_piece);
                        }

                        const char* const varname = data;
                        VarRef *ref = VarRefParse(varname);
                        DataType type = CF_DATA_TYPE_NONE;
                        const void *value = EvalContextVariableGet(ctx, ref, &type);
                        VarRefDestroy(ref);

                        if (type == CF_DATA_TYPE_CONTAINER)
                        {
                            Writer *w = StringWriter();
                            JsonWriteCompact(w, value);
                            snprintf(piece, CF_BUFSIZE, format_rewrite, StringWriterData(w));
                            WriterClose(w);
                            BufferAppend(buf, piece, strlen(piece));
                        }
                        else            // it might be a list reference
                        {
                            DataType data_type;
                            const Rlist *list = GetListReferenceArgument(ctx, fp, varname, &data_type);
                            if (data_type == CF_DATA_TYPE_STRING_LIST)
                            {
                                Writer *w = StringWriter();
                                WriterWrite(w, "{ ");
                                for (const Rlist *rp = list; rp; rp = rp->next)
                                {
                                    char *escaped = EscapeCharCopy(RlistScalarValue(rp), '"', '\\');
                                    if (0 == strcmp(escaped, CF_NULL_VALUE))
                                    {
                                        WriterWrite(w, "--empty-list--");
                                    }
                                    else
                                    {
                                        WriterWriteF(w, "\"%s\"", escaped);
                                    }
                                    free(escaped);

                                    if (NULL != rp && NULL != rp->next)
                                    {
                                        WriterWrite(w, ", ");
                                    }
                                }
                                WriterWrite(w, " }");

                                snprintf(piece, CF_BUFSIZE, format_rewrite, StringWriterData(w));
                                WriterClose(w);
                                BufferAppend(buf, piece, strlen(piece));
                            }
                            else        // whatever this is, it's not a list reference or a data container
                            {
                                Log(LOG_LEVEL_VERBOSE, "format() with %%S specifier needs a data container or a list instead of '%s'.",
                                    varname);
                                BufferDestroy(buf);
                                SeqDestroy(s);
                                return FnFailure();
                            }
                        }
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
                    BufferAppend(buf, BufferData(SeqAt(s, 2)), BufferSize(SeqAt(s, 2)));
                }
                else
                {
                    check = NULL;
                }
            }

            {
                if (SeqLength(s) >= 4)
                {
                    strlcpy(check_buffer, BufferData(SeqAt(s, 3)), CF_BUFSIZE);
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

    return FnReturnBuffer(buf);
}

/*********************************************************************/

static FnCallResult FnCallIPRange(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *range = RlistScalarValue(finalargs);

    if (!FuzzyMatchParse(range))
    {
        return FnFailure();
    }

    for (const Item *ip = EvalContextGetIpAddresses(ctx); ip != NULL; ip = ip->next)
    {
        if (FuzzySetMatch(range, VIPADDRESS) == 0 ||
            FuzzySetMatch(range, ip->name) == 0)
        {
            return FnReturnContext(true);
        }
    }

    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallHostRange(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *prefix = RlistScalarValue(finalargs);
    char *range = RlistScalarValue(finalargs->next);

    if (!FuzzyHostParse(range))
    {
        return FnFailure();
    }

    return FnReturnContext(FuzzyHostMatch(prefix, range, VUQNAME) == 0);
}

/*********************************************************************/

FnCallResult FnCallHostInNetgroup(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    setnetgrent(RlistScalarValue(finalargs));

    bool found = false;
    char *host, *user, *domain;
    while (getnetgrent(&host, &user, &domain))
    {
        if (host == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "Matched '%s' in netgroup '%s'",
                VFQNAME, RlistScalarValue(finalargs));
            found = true;
            break;
        }

        if (strcmp(host, VFQNAME) == 0 ||
            strcmp(host, VUQNAME) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Matched '%s' in netgroup '%s'",
                host, RlistScalarValue(finalargs));
            found = true;
            break;
        }
    }

    endnetgrent();

    return FnReturnContext(found);
}

/*********************************************************************/

static FnCallResult FnCallIsVariable(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const char *lval = RlistScalarValue(finalargs);
    bool found = false;

    if (lval)
    {
        VarRef *ref = VarRefParse(lval);
        found = EvalContextVariableGet(ctx, ref, NULL) != NULL;
        VarRefDestroy(ref);
    }

    return FnReturnContext(found);
}

/*********************************************************************/

static FnCallResult FnCallStrCmp(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    return FnReturnContext(strcmp(RlistScalarValue(finalargs), RlistScalarValue(finalargs->next)) == 0);
}

/*********************************************************************/

static FnCallResult FnCallTranslatePath(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char buffer[MAX_FILENAME];

    strlcpy(buffer, RlistScalarValue(finalargs), sizeof(buffer));
    MapName(buffer);

    return FnReturn(buffer);
}

/*********************************************************************/

#if defined(__MINGW32__)

static FnCallResult FnCallRegistryValue(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char buffer[CF_BUFSIZE] = "";

    if (GetRegistryValue(RlistScalarValue(finalargs),
                         RlistScalarValue(finalargs->next),
                         buffer, sizeof(buffer)))
    {
        return FnReturn(buffer);
    }

    return FnFailure();
}

#else /* !__MINGW32__ */

static FnCallResult FnCallRegistryValue(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    return FnFailure();
}

#endif /* !__MINGW32__ */

/*********************************************************************/

static FnCallResult FnCallRemoteScalar(EvalContext *ctx,
                                       ARG_UNUSED const Policy *policy,
                                       ARG_UNUSED const FnCall *fp,
                                       const Rlist *finalargs)
{
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
        char buffer[CF_BUFSIZE];
        buffer[0] = '\0';

        char *ret = GetRemoteScalar(ctx, "VAR", handle, server,
                                    encrypted, buffer);
        if (ret == NULL)
        {
            return FnFailure();
        }

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

static FnCallResult FnCallHubKnowledge(EvalContext *ctx,
                                       ARG_UNUSED const Policy *policy,
                                       ARG_UNUSED const FnCall *fp,
                                       const Rlist *finalargs)
{
    char *handle = RlistScalarValue(finalargs);

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        return FnReturn("<inaccessible remote scalar>");
    }
    else
    {
        char buffer[CF_BUFSIZE];
        buffer[0] = '\0';

        Log(LOG_LEVEL_VERBOSE, "Accessing hub knowledge base for '%s'", handle);

        char *ret = GetRemoteScalar(ctx, "VAR", handle, POLICY_SERVER,
                                    true, buffer);
        if (ret == NULL)
        {
            return FnFailure();
        }


        // This should always be successful - and this one doesn't cache

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            return FnReturn("0");
        }

        return FnReturn(buffer);
    }
}

/*********************************************************************/

static FnCallResult FnCallRemoteClassesMatching(EvalContext *ctx,
                                                ARG_UNUSED const Policy *policy,
                                                ARG_UNUSED const FnCall *fp,
                                                const Rlist *finalargs)
{
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
        char buffer[CF_BUFSIZE];
        buffer[0] = '\0';

        char *ret = GetRemoteScalar(ctx, "CONTEXT", regex, server,
                                    encrypted, buffer);
        if (ret == NULL)
        {
            return FnFailure();
        }

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            return FnFailure();
        }

        Rlist *classlist = RlistFromSplitString(buffer, ',');
        if (classlist)
        {
            for (const Rlist *rp = classlist; rp != NULL; rp = rp->next)
            {
                char class_name[CF_MAXVARSIZE];
                snprintf(class_name, sizeof(class_name), "%s_%s",
                         prefix, RlistScalarValue(rp));
                EvalContextClassPutSoft(ctx, class_name, CONTEXT_SCOPE_BUNDLE,
                                        "source=function,function=remoteclassesmatching");
            }
            RlistDestroy(classlist);
        }

        return FnReturnContext(true);
    }
}

/*********************************************************************/

static FnCallResult FnCallPeers(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    int maxent = 100000, maxsize = 100000;

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    int groupsize = IntFromString(RlistScalarValue(finalargs->next->next));

    if (2 > groupsize)
    {
        Log(LOG_LEVEL_WARNING, "Function %s: called with a nonsensical group size of %d, failing", fp->name, groupsize);
        return FnFailure();
    }

    char *file_buffer = CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return FnFailure();
    }

    file_buffer = StripPatterns(file_buffer, comment, filename);

    Rlist *const newlist =
        file_buffer ? RlistFromSplitRegex(file_buffer, "\n", maxent, true) : NULL;

    /* Slice up the list and discard everything except our slice */

    int i = 0;
    bool found = false;
    Rlist *pruned = NULL;

    for (const Rlist *rp = newlist; rp != NULL; rp = rp->next)
    {
        const char *s = RlistScalarValue(rp);
        if (EmptyString(s))
        {
            continue;
        }

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
    free(file_buffer); // OK if it's NULL

    if (pruned && found)
    {
        RlistReverse(&pruned);
    }
    else
    {
        RlistDestroy(pruned);
        pruned = NULL;
    }
    return (FnCallResult) { FNCALL_SUCCESS, { pruned, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallPeerLeader(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    int maxent = 100000, maxsize = 100000;

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    int groupsize = IntFromString(RlistScalarValue(finalargs->next->next));

    if (2 > groupsize)
    {
        Log(LOG_LEVEL_WARNING, "Function %s: called with a nonsensical group size of %d, failing", fp->name, groupsize);
        return FnFailure();
    }

    char *file_buffer = CfReadFile(filename, maxsize);
    if (file_buffer == NULL)
    {
        return FnFailure();
    }

    file_buffer = StripPatterns(file_buffer, comment, filename);

    Rlist *const newlist =
        file_buffer ? RlistFromSplitRegex(file_buffer, "\n", maxent, true) : NULL;

    /* Slice up the list and discard everything except our slice */

    int i = 0;
    bool found = false;
    char buffer[CF_MAXVARSIZE];
    buffer[0] = '\0';

    for (const Rlist *rp = newlist; !found && rp != NULL; rp = rp->next)
    {
        const char *s = RlistScalarValue(rp);
        if (EmptyString(s))
        {
            continue;
        }

        found = (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0);
        if (i % groupsize == 0)
        {
            strlcpy(buffer, found ? "localhost" : s, CF_MAXVARSIZE);
        }

        i++;
    }

    RlistDestroy(newlist);
    free(file_buffer);

    if (found)
    {
        return FnReturn(buffer);
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallPeerLeaders(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    int maxent = 100000, maxsize = 100000;

    char *filename = RlistScalarValue(finalargs);
    char *comment = RlistScalarValue(finalargs->next);
    int groupsize = IntFromString(RlistScalarValue(finalargs->next->next));

    if (2 > groupsize)
    {
        Log(LOG_LEVEL_WARNING, "Function %s: called with a nonsensical group size of %d, failing", fp->name, groupsize);
        return FnFailure();
    }

    char *file_buffer = CfReadFile(filename, maxsize);
    if (file_buffer == NULL)
    {
        return FnFailure();
    }

    file_buffer = StripPatterns(file_buffer, comment, filename);

    Rlist *const newlist =
        file_buffer ? RlistFromSplitRegex(file_buffer, "\n", maxent, true) : NULL;

    /* Slice up the list and discard everything except our slice */

    int i = 0;
    Rlist *pruned = NULL;

    for (const Rlist *rp = newlist; rp != NULL; rp = rp->next)
    {
        const char *s = RlistScalarValue(rp);
        if (EmptyString(s))
        {
            continue;
        }

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

    RlistReverse(&pruned);
    return (FnCallResult) { FNCALL_SUCCESS, { pruned, RVAL_TYPE_LIST } };

}

/*********************************************************************/

static FnCallResult FnCallRegCmp(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);

    return FnReturnContext(StringMatchFull(argv0, argv1));
}

/*********************************************************************/

static FnCallResult FnCallRegExtract(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const bool container_mode = strcmp(fp->name, "data_regextract") == 0;

    const char *regex = RlistScalarValue(finalargs);
    const char *data = RlistScalarValue(finalargs->next);
    char *arrayname = NULL;

    if (!container_mode)
    {
        arrayname = xstrdup(RlistScalarValue(finalargs->next->next));

        if (!IsQualifiedVariable(arrayname))
        {
            if (fp->caller)
            {
                VarRef *ref = VarRefParseFromBundle(arrayname, PromiseGetBundle(fp->caller));
                free(arrayname);
                arrayname = VarRefToString(ref, true);
                VarRefDestroy(ref);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Function '%s' called with an unqualifed array reference '%s', "
                    "and the reference could not be automatically qualified as the function was not called from a promise.",
                    fp->name, arrayname);
                free(arrayname);
                return FnFailure();
            }
        }
    }

    Seq *s = StringMatchCaptures(regex, data, true);

    if (!s || SeqLength(s) == 0)
    {
        SeqDestroy(s);
        free(arrayname);
        return container_mode ? FnFailure() : FnReturnContext(false);
    }

    JsonElement *json = NULL;

    if (container_mode)
    {
        json = JsonObjectCreate(SeqLength(s)/2);
    }

    for (int i = 0; i < SeqLength(s); i+=2)
    {
        Buffer *key = SeqAt(s, i);
        Buffer *value = SeqAt(s, i+1);


        if (container_mode)
        {
            JsonObjectAppendString(json, BufferData(key), BufferData(value));
        }
        else
        {
            char var[CF_MAXVARSIZE] = "";
            snprintf(var, CF_MAXVARSIZE - 1, "%s[%s]", arrayname, BufferData(key));
            VarRef *new_ref = VarRefParse(var);
            EvalContextVariablePut(ctx, new_ref, BufferData(value),
                                   CF_DATA_TYPE_STRING,
                                   "source=function,function=regextract");
            VarRefDestroy(new_ref);
        }
    }

    free(arrayname);
    SeqDestroy(s);

    if (container_mode)
    {
        return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
    }
    else
    {
        return FnReturnContext(true);
    }
}

/*********************************************************************/

static FnCallResult FnCallRegLine(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    pcre *rx = CompileRegex(RlistScalarValue(finalargs));
    if (!rx)
    {
        return FnFailure();
    }

    const char *arg_filename = RlistScalarValue(finalargs->next);

    FILE *fin = safe_fopen(arg_filename, "rt");
    if (!fin)
    {
        pcre_free(rx);
        return FnReturnContext(false);
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    while (CfReadLine(&line, &line_size, fin) != -1)
    {
        if (StringMatchFullWithPrecompiledRegex(rx, line))
        {
            free(line);
            fclose(fin);
            pcre_free(rx);
            return FnReturnContext(true);
        }
    }

    pcre_free(rx);
    free(line);

    if (!feof(fin))
    {
        Log(LOG_LEVEL_ERR, "In function '%s', error reading from file. (getline: %s)",
            fp->name, GetErrorStr());
    }

    fclose(fin);
    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallIsLessGreaterThan(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    char *argv0 = RlistScalarValue(finalargs);
    char *argv1 = RlistScalarValue(finalargs->next);
    bool rising = (0 == strcmp(fp->name, "isgreaterthan"));

    if (IsRealNumber(argv0) && IsRealNumber(argv1))
    {
        double a = 0, b = 0;
        if (!DoubleFromString(argv0, &a) ||
            !DoubleFromString(argv1, &b))
        {
            return FnFailure();
        }

        if (rising)
        {
            return FnReturnContext(a > b);
        }
        else
        {
            return FnReturnContext(a < b);
        }
    }

    if (rising)
    {
        return FnReturnContext(strcmp(argv0, argv1) > 0);
    }
    else
    {
        return FnReturnContext(strcmp(argv0, argv1) < 0);
    }
}

/*********************************************************************/

static FnCallResult FnCallIRange(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    long from = IntFromString(RlistScalarValue(finalargs));
    long to = IntFromString(RlistScalarValue(finalargs->next));

    if (from == CF_NOINT || to == CF_NOINT)
    {
        return FnFailure();
    }

    if (from > to)
    {
        long tmp = to;
        to = from;
        from = tmp;
    }

    return FnReturnF("%ld,%ld", from, to);
}

/*********************************************************************/

static FnCallResult FnCallRRange(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    double from = 0;
    if (!DoubleFromString(RlistScalarValue(finalargs), &from))
    {
        Log(LOG_LEVEL_ERR,
            "Function rrange, error reading assumed real value '%s'",
            RlistScalarValue(finalargs));
        return FnFailure();
    }

    double to = 0;
    if (!DoubleFromString(RlistScalarValue(finalargs), &to))
    {
        Log(LOG_LEVEL_ERR,
            "Function rrange, error reading assumed real value '%s'",
            RlistScalarValue(finalargs->next));
        return FnFailure();
    }

    if (from > to)
    {
        int tmp = to;
        to = from;
        from = tmp;
    }

    return FnReturnF("%lf,%lf", from, to);
}

static FnCallResult FnCallReverse(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    DataType list_dtype = CF_DATA_TYPE_NONE;
    const Rlist *input_list = GetListReferenceArgument(ctx, fp, RlistScalarValue(finalargs), &list_dtype);

    if (list_dtype != CF_DATA_TYPE_STRING_LIST)
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
    /* .tm_year stores year - 1900 */
    ret.tm_year = IntFromString(RlistScalarValue(rp)) - 1900;
    rp = rp->next;
    /* .tm_mon counts from Jan = 0 to Dec = 11 */
    ret.tm_mon = IntFromString(RlistScalarValue(rp));
    rp = rp->next;
    /* .tm_mday is day of month, 1 to 31, but we use 0 through 30 (for now) */
    ret.tm_mday = IntFromString(RlistScalarValue(rp)) + 1;
    rp = rp->next;
    ret.tm_hour = IntFromString(RlistScalarValue(rp));
    rp = rp->next;
    ret.tm_min = IntFromString(RlistScalarValue(rp));
    rp = rp->next;
    ret.tm_sec = IntFromString(RlistScalarValue(rp));
    return ret;
}

static FnCallResult FnCallOn(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    struct tm tmv = FnArgsToTm(finalargs);
    time_t cftime = mktime(&tmv);

    if (cftime == -1)
    {
        Log(LOG_LEVEL_INFO, "Illegal time value");
    }

    return FnReturnF("%jd", (intmax_t) cftime);
}

/*********************************************************************/

static FnCallResult FnCallOr(EvalContext *ctx,
                             ARG_UNUSED const Policy *policy,
                             ARG_UNUSED const FnCall *fp,
                             const Rlist *finalargs)
{
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall or-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, CF_DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        if (IsDefinedClass(ctx, RlistScalarValue(arg)))
        {
            return FnReturnContext(true);
        }
    }

    return FnReturnContext(false);
}

/*********************************************************************/

static FnCallResult FnCallLaterThan(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    time_t now = time(NULL);
    struct tm tmv = FnArgsToTm(finalargs);
    /* Adjust to 1-based counting (input) for month and day of month
     * (0-based in mktime): */
    tmv.tm_mon--;
    tmv.tm_mday--;
    time_t cftime = mktime(&tmv);

    if (cftime == -1)
    {
        Log(LOG_LEVEL_INFO, "Illegal time value");
    }

    return FnReturnContext(now > cftime);
}

static FnCallResult FnCallAgoDate(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
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
    if (cftime < 0)
    {
        return FnReturn("0");
    }

    return FnReturnF("%jd", (intmax_t) cftime);
}

/*********************************************************************/

static FnCallResult FnCallAccumulatedDate(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    struct tm tmv = FnArgsToTm(finalargs);

    time_t cftime = 0;
    cftime = 0;
    cftime += tmv.tm_sec;
    cftime += tmv.tm_min * 60;
    cftime += tmv.tm_hour * 3600;
    cftime += (tmv.tm_mday -1) * 24 * 3600;
    cftime += tmv.tm_mon * 30 * 24 * 3600;
    cftime += (tmv.tm_year + 1900) * 365 * 24 * 3600;

    return FnReturnF("%jd", (intmax_t) cftime);
}

/*********************************************************************/

static FnCallResult FnCallNot(EvalContext *ctx,
                              ARG_UNUSED const Policy *policy,
                              ARG_UNUSED const FnCall *fp,
                              const Rlist *finalargs)
{
    return FnReturnContext(!IsDefinedClass(ctx, RlistScalarValue(finalargs)));
}

/*********************************************************************/

static FnCallResult FnCallNow(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    return FnReturnF("%jd", (intmax_t)CFSTARTTIME);
}

/*********************************************************************/

#ifdef __sun /* Lacks %P and */
#define STRFTIME_F_HACK
#define STRFTIME_s_HACK
#define STRFTIME_R_HACK
#endif /* http://www.unix.com/man-page/opensolaris/3c/strftime/ */

#ifdef __hpux /* Unknown gaps, aside from: */
#define STRFTIME_F_HACK
#endif

#ifdef _WIN32 /* Has non-standard %z, lacks %[CDeGghklnPrRtTuV] and: */
#define STRFTIME_F_HACK
#define STRFTIME_R_HACK
#define STRFTIME_s_HACK
#endif /* http://msdn.microsoft.com/en-us/library/fe06s4ak.aspx */

bool PortablyFormatTime(char *buffer, size_t bufsiz,
                        const char *format,
#ifndef STRFTIME_s_HACK
                        ARG_UNUSED
#endif
                        time_t when,
                        const struct tm *tm)
{
    /* TODO: might be better done in a libcompat wrapper.
     *
     * The following GNU extensions may be worth adding at some point;
     * see individual platforms for lists of which they lack.
     *
     * %C (century)
     * %D => %m/%d/%y
     * %e: as %d but s/^0/ /
     * %G: like %Y but frobbed for ISO week numbers
     * %g: last two digits of %G
     * %h => %b
     * %k: as %H but s/^0/ /
     * %l: as %I but s/^0/ /
     * %n => \n
     * %P: as %p but lower-cased
     * %r => %I:%M:%S %p
     * %s: seconds since epoch
     * %t => \t
     * %T => %H:%M:%S
     * %u: dow, {1: Mon, ..., Sun: 7}
     * %V: ISO week number within year %G
     *
     * The "=>" ones can all be done by extending expansion[], below;
     * the rest would require actually implementing GNU strftime()
     * properly.
     */

#ifdef STRFTIME_s_HACK /* %s: seconds since epoch */
    char epoch[PRINTSIZE(when)];
    xsnprintf(epoch, sizeof(epoch), "%jd", (intmax_t) when);
#endif /* STRFTIME_s_HACK */

    typedef char * SearchReplacePair[2];
    SearchReplacePair expansion[] =
        {
            /* Each pair is { search, replace }. */
#ifdef STRFTIME_F_HACK
            { "%F", "%Y-%m-%d" },
#endif
#ifdef STRFTIME_R_HACK /* %R => %H:%M:%S */
            { "%R", "%H:%M:%S" },
#endif
#ifdef STRFTIME_s_HACK
            { "%s", epoch },
#endif

            /* Order as in GNU strftime's man page. */
            { NULL, NULL }
        };

    char *delenda = NULL; /* in need of destruction */
    /* No-op when no STRFTIME_*_HACK were defined. */
    for (size_t i = 0; expansion[i][0]; i++)
    {
        char *tmp = SearchAndReplace(format, expansion[i][0], expansion[i][1]);
        free(delenda);
        format = delenda = tmp;
    }

    size_t ans = strftime(buffer, bufsiz, format, tm);
    free(delenda);
    return ans > 0;
}
#undef STRFTIME_F_HACK
#undef STRFTIME_R_HACK
#undef STRFTIME_s_HACK

/*********************************************************************/

static FnCallResult FnCallStrftime(ARG_UNUSED EvalContext *ctx,
                                   ARG_UNUSED const Policy *policy,
                                   const FnCall *fp,
                                   const Rlist *finalargs)
{
    /* begin fn-specific content */

    char *mode = RlistScalarValue(finalargs);
    char *format_string = RlistScalarValue(finalargs->next);
    // this will be a problem on 32-bit systems...
    const time_t when = IntFromString(RlistScalarValue(finalargs->next->next));

    struct tm* tm;

    if (0 == strcmp("gmtime", mode))
    {
        tm = gmtime(&when);
    }
    else
    {
        tm = localtime(&when);
    }

    char buffer[CF_BUFSIZE];
    if (tm == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "Function %s, the given time stamp '%ld' was invalid. (strftime: %s)",
            fp->name, when, GetErrorStr());
    }
    else if (PortablyFormatTime(buffer, sizeof(buffer),
                                format_string, when, tm))
    {
        return FnReturn(buffer);
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallEval(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    char *input = RlistScalarValue(finalargs);
    char *type = RlistScalarValue(finalargs->next);

    /* Third argument can currently only be "infix". */
    /* char *options = RlistScalarValue(finalargs->next->next); */

    const bool context_mode = (strcmp(type, "class") == 0);

    char failure[CF_BUFSIZE];
    memset(failure, 0, sizeof(failure));

    double result = EvaluateMathInfix(ctx, input, failure);
    if (context_mode)
    {
        // see CLOSE_ENOUGH in math.peg
        return FnReturnContext(strlen(failure) == 0 &&
                               !(result < 0.00000000000000001 &&
                                 result > -0.00000000000000001));
    }

    if (strlen(failure) > 0)
    {
        Log(LOG_LEVEL_INFO, "%s error: %s (input '%s')", fp->name, failure, input);
        return FnReturn(""); /* TODO: why not FnFailure() ? */
    }

    return FnReturnF("%lf", result);
}

/*********************************************************************/
/* Read functions                                                    */
/*********************************************************************/

static FnCallResult FnCallReadFile(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *filename = RlistScalarValue(finalargs);
    char *requested_max = RlistScalarValue(finalargs->next);
    long maxsize = IntFromString(requested_max);

    if (maxsize == CF_INFINITY)                      /* "inf" in the policy */
    {
        maxsize = 0;
    }

    if (maxsize < 0)
    {
        Log(LOG_LEVEL_ERR, "%s: requested max size %s is less than 0", fp->name, requested_max);
        return FnFailure();
    }

    // Read once to validate structure of file in itemlist
    char *contents = CfReadFile(filename, maxsize);
    if (contents)
    {
        return FnReturnNoCopy(contents);
    }

    Log(LOG_LEVEL_ERR, "Function '%s' failed to read file: %s",
        fp->name, filename);
    return FnFailure();
}

/*********************************************************************/

static FnCallResult ReadList(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const FnCall *fp, const Rlist *finalargs, DataType type)
{
    const char *filename = RlistScalarValue(finalargs);
    const char *comment = RlistScalarValue(finalargs->next);
    const char *split = RlistScalarValue(finalargs->next->next);
    const int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next));
    const int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next));

    char *file_buffer = CfReadFile(filename, maxsize);
    if (!file_buffer)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' failed to read file: %s",
            fp->name, filename);
        return FnFailure();
    }

    bool blanks = false;
    Rlist *newlist = NULL;
    file_buffer = StripPatterns(file_buffer, comment, filename);
    if (!file_buffer)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { NULL, RVAL_TYPE_LIST } };
    }
    else
    {
        newlist = RlistFromSplitRegex(file_buffer, split, maxent, blanks);
    }

    bool noerrors = true;

    switch (type)
    {
    case CF_DATA_TYPE_STRING:
        break;

    case CF_DATA_TYPE_INT:
        for (Rlist *rp = newlist; rp != NULL; rp = rp->next)
        {
            if (IntFromString(RlistScalarValue(rp)) == CF_NOINT)
            {
                Log(LOG_LEVEL_ERR, "Presumed int value '%s' read from file '%s' has no recognizable value",
                      RlistScalarValue(rp), filename);
                noerrors = false;
            }
        }
        break;

    case CF_DATA_TYPE_REAL:
        for (Rlist *rp = newlist; rp != NULL; rp = rp->next)
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

    return FnFailure();
}

static FnCallResult FnCallReadStringList(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ReadList(ctx, fp, args, CF_DATA_TYPE_STRING);
}

static FnCallResult FnCallReadIntList(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ReadList(ctx, fp, args, CF_DATA_TYPE_INT);
}

static FnCallResult FnCallReadRealList(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ReadList(ctx, fp, args, CF_DATA_TYPE_REAL);
}

static FnCallResult FnCallReadData(ARG_UNUSED EvalContext *ctx,
                                   ARG_UNUSED const Policy *policy,
                                   const FnCall *fp,
                                   const Rlist *args)
{
    const char *input_path = RlistScalarValue(args);

    size_t size_max;
    const char *requested_mode = NULL;
    if (0 == strcmp(fp->name, "readdata"))
    {
        // readdata gets rid of the size
        size_max = 0;
        requested_mode = RlistScalarValue(args->next);
        if (0 == strcmp("auto", requested_mode))
        {
            if (StringEndsWithCase(input_path, ".csv", true))
            {
                requested_mode = "CSV";
            }
            else if (StringEndsWithCase(input_path, ".yaml", true))
            {
                size_max = IntFromString("inf");
                requested_mode = "YAML";
            }
            else // always default to JSON
            {
                size_max = IntFromString("inf");
                requested_mode = "JSON";
            }

            Log(LOG_LEVEL_VERBOSE, "%s: automatically selected data type %s from filename %s", fp->name, requested_mode, input_path);
        }
    }
    else
    {
        size_max = args->next ? IntFromString(RlistScalarValue(args->next)) : 0;
        if (0 == strcmp(fp->name, "readyaml"))
        {
            requested_mode = "YAML";
        }
        else if (0 == strcmp(fp->name, "readcsv"))
        {
            requested_mode = "CSV";
        }
        else
        {
            requested_mode = "JSON";
        }
    }

    if (0 == strcmp("CSV", requested_mode))
    {
        size_t size_max = 50 * (1024 * 1024);

        size_t byte_count = 0;

        FILE *fin = safe_fopen(input_path, "r");
        if (NULL == fin)
        {
            Log(LOG_LEVEL_VERBOSE, "%s cannot open the CSV file '%s' (fopen: %s)",
                fp->name, input_path, GetErrorStr());
            return FnFailure();
        }

        JsonElement *json = JsonArrayCreate(50);
        int linenumber = 0;
        char *line;
        while (NULL != (line = GetCsvLineNext(fin)))
        {
            ++linenumber;

            byte_count += strlen(line);
            if (byte_count > size_max)
            {
                Log(LOG_LEVEL_VERBOSE, "%s: line %d from CSV file '%s' exceeded byte limit %lu, done with file",
                    fp->name, linenumber, input_path, (long unsigned int)size_max);
                free(line);
                break;
            }

            Seq *list = SeqParseCsvString(line);
            free(line);

            if (NULL != list)
            {
                JsonElement *line_arr = JsonArrayCreate(SeqLength(list));

                for (size_t i = 0; i < SeqLength(list); i++)
                {
                    JsonArrayAppendString(line_arr, SeqAt(list, i));
                }

                SeqDestroy(list);
                JsonArrayAppendArray(json, line_arr);
            }
        }

        bool atend = feof(fin);
        fclose(fin);

        if (!atend)
        {
            Log(LOG_LEVEL_ERR,
                "%s: unable to read line from CSV file '%s'. (fread: %s)",
                fp->name, input_path, GetErrorStr());
            JsonDestroy(json);
            return FnFailure();
        }

        return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
    }

    bool yaml_mode = (0 == strcmp(requested_mode, "YAML"));
    const char* data_type = requested_mode;

    // TODO: eliminate truncation limits in json.c
    if (0 == size_max)
    {
        size_max = 50 * (1024 * 1024);
    }

    /* FIXME: fail if truncated? */
    JsonElement *json = NULL;
    JsonParseError res;
    if (yaml_mode)
    {
        res = JsonParseYamlFile(input_path, size_max, &json);
    }
    else
    {
        res = JsonParseFile(input_path, size_max, &json);
    }

    // the NO_DATA errors often happen when the file hasn't been created yet
    if (res == JSON_PARSE_ERROR_NO_DATA)
    {
        Log(LOG_LEVEL_ERR, "%s: data error parsing %s file '%s': %s",
            fp->name, data_type, input_path, JsonParseErrorToString(res));
    }
    else if (res != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_ERR, "%s: error parsing %s file '%s': %s",
            fp->name, data_type, input_path, JsonParseErrorToString(res));
    }
    else if (JsonGetElementType(json) == JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        Log(LOG_LEVEL_ERR, "%s: non-container from parsing %s file '%s'",fp->name, data_type, input_path);
        JsonDestroy(json);
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
    }

    return FnFailure();
}

static FnCallResult FnCallParseJson(ARG_UNUSED EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    ARG_UNUSED const FnCall *fp,
                                    const Rlist *args)
{
    const char *data = RlistScalarValue(args);
    JsonElement *json = NULL;
    bool yaml_mode = (0 == strcmp(fp->name, "parseyaml"));
    const char* data_type = yaml_mode ? "YAML" : "JSON";
    JsonParseError res;

    if (yaml_mode)
    {
        res = JsonParseYamlString(&data, &json);
    }
    else
    {
        res = JsonParse(&data, &json);
    }

    if (res != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_ERR, "Error parsing %s expression '%s': %s",
            data_type, data, JsonParseErrorToString(res));
    }
    else if (JsonGetElementType(json) == JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        Log(LOG_LEVEL_ERR, "Non-container from parsing %s expression '%s'", data_type, data);
        JsonDestroy(json);
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallStoreJson(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char *varname = RlistScalarValue(finalargs);

    VarRef *ref = VarRefParse(varname);
    DataType type = CF_DATA_TYPE_NONE;
    const JsonElement *value = EvalContextVariableGet(ctx, ref, &type);
    VarRefDestroy(ref);

    if (type == CF_DATA_TYPE_CONTAINER)
    {
        Writer *w = StringWriter();
        int length;

        JsonWrite(w, value, 0);
        Log(LOG_LEVEL_DEBUG, "%s: from data container %s, got JSON data '%s'", fp->name, varname, StringWriterData(w));

        length = strlen(StringWriterData(w));
        if (length >= CF_BUFSIZE)
        {
            Log(LOG_LEVEL_INFO, "%s: truncating data container %s JSON data from %d bytes to %d", fp->name, varname, length, CF_BUFSIZE);
        }

        char buf[CF_BUFSIZE];
        snprintf(buf, CF_BUFSIZE, "%s", StringWriterData(w));
        WriterClose(w);

        return FnReturn(buf);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s: data container %s could not be found or has an invalid type", fp->name, varname);
        return FnFailure();
    }
}


/*********************************************************************/

// this function is separate so other data container readers can use it
static FnCallResult DataRead(EvalContext *ctx, const FnCall *fp, const Rlist *finalargs)
{
    /* 5 args: filename,comment_regex,split_regex,max number of entries,maxfilesize  */

    const char *filename = RlistScalarValue(finalargs);
    const char *comment = RlistScalarValue(finalargs->next);
    const char *split = RlistScalarValue(finalargs->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next));

    bool make_array = 0 == strcmp(fp->name, "data_readstringarrayidx");
    JsonElement *json = NULL;

    // Read once to validate structure of file in itemlist
    char *file_buffer = CfReadFile(filename, maxsize);
    if (file_buffer)
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer != NULL)
        {
            json = BuildData(ctx, file_buffer, split, maxent, make_array);
        }
    }

    free(file_buffer);

    if (NULL == json)
    {
        Log(LOG_LEVEL_INFO, "%s: error reading from file '%s'", fp->name, filename);
        return FnFailure();
    }

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { json, RVAL_TYPE_CONTAINER } };
}

/*********************************************************************/

static FnCallResult FnCallDataExpand(EvalContext *ctx,
                                     ARG_UNUSED const Policy *policy,
                                     ARG_UNUSED const FnCall *fp,
                                     const Rlist *args)
{
    const char *varname = RlistScalarValue(args);
    VarRef *ref = ResolveAndQualifyVarName(fp, varname);
    if (!ref)
    {
        return FnFailure();
    }

    JsonElement *container = VarRefValueToJson(ctx, fp, ref, NULL, 0);
    VarRefDestroy(ref);

    if (NULL == container)
    {
        return FnFailure();
    }

    JsonElement *expanded = JsonExpandElement(ctx, container);
    JsonDestroy(container);

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { expanded, RVAL_TYPE_CONTAINER } };
}

/*********************************************************************/

static FnCallResult FnCallDataRead(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return DataRead(ctx, fp, args);
}

/*********************************************************************/

static FnCallResult ReadArray(EvalContext *ctx, const FnCall *fp, const Rlist *finalargs, DataType type, bool int_index)
/* lval,filename,separator,comment,Max number of bytes  */
{
    if (!fp->caller)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' can only be called from a promise", fp->name);
        return FnFailure();
    }

    /* 6 args: array_lval,filename,comment_regex,split_regex,max number of entries,maxfilesize  */

    const char *array_lval = RlistScalarValue(finalargs);
    const char *filename = RlistScalarValue(finalargs->next);
    const char *comment = RlistScalarValue(finalargs->next->next);
    const char *split = RlistScalarValue(finalargs->next->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next->next));
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next->next));

    // Read once to validate structure of file in itemlist
    char *file_buffer = CfReadFile(filename, maxsize);
    int entries = 0;
    if (file_buffer)
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer)
        {
            entries = BuildLineArray(ctx, PromiseGetBundle(fp->caller), array_lval, file_buffer, split, maxent, type, int_index);
        }
    }

    switch (type)
    {
    case CF_DATA_TYPE_STRING:
    case CF_DATA_TYPE_INT:
    case CF_DATA_TYPE_REAL:
        break;

    default:
        ProgrammingError("Unhandled type in switch: %d", type);
    }

    free(file_buffer);

    return FnReturnF("%d", entries);
}

/*********************************************************************/

static FnCallResult FnCallReadStringArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ReadArray(ctx, fp, args, CF_DATA_TYPE_STRING, false);
}

/*********************************************************************/

static FnCallResult FnCallReadStringArrayIndex(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ReadArray(ctx, fp, args, CF_DATA_TYPE_STRING, true);
}

/*********************************************************************/

static FnCallResult FnCallReadIntArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ReadArray(ctx, fp, args, CF_DATA_TYPE_INT, false);
}

/*********************************************************************/

static FnCallResult FnCallReadRealArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ReadArray(ctx, fp, args, CF_DATA_TYPE_REAL, false);
}

/*********************************************************************/

static FnCallResult ParseArray(EvalContext *ctx, const FnCall *fp, const Rlist *finalargs, DataType type, int intIndex)
/* lval,filename,separator,comment,Max number of bytes  */
{
    if (!fp->caller)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' can only be called from a promise", fp->name);
        return FnFailure();
    }

    /* 6 args: array_lval,instring,comment_regex,split_regex,max number of entries,maxtextsize  */

    const char *array_lval = RlistScalarValue(finalargs);
    int maxsize = IntFromString(RlistScalarValue(finalargs->next->next->next->next->next));
    char *instring = xstrndup(RlistScalarValue(finalargs->next), maxsize);
    const char *comment = RlistScalarValue(finalargs->next->next);
    const char *split = RlistScalarValue(finalargs->next->next->next);
    int maxent = IntFromString(RlistScalarValue(finalargs->next->next->next->next));

// Read once to validate structure of file in itemlist

    Log(LOG_LEVEL_DEBUG, "Parse string data from string '%s' - , maxent %d, maxsize %d", instring, maxent, maxsize);

    int entries = 0;
    if (instring)
    {
        instring = StripPatterns(instring, comment, "string argument 2");

        if (instring)
        {
            entries = BuildLineArray(ctx, PromiseGetBundle(fp->caller), array_lval, instring, split, maxent, type, intIndex);
        }
    }

    switch (type)
    {
    case CF_DATA_TYPE_STRING:
    case CF_DATA_TYPE_INT:
    case CF_DATA_TYPE_REAL:
        break;

    default:
        ProgrammingError("Unhandled type in switch: %d", type);
    }

    free(instring);

    return FnReturnF("%d", entries);
}

/*********************************************************************/

static FnCallResult FnCallParseStringArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ParseArray(ctx, fp, args, CF_DATA_TYPE_STRING, false);
}

/*********************************************************************/

static FnCallResult FnCallParseStringArrayIndex(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ParseArray(ctx, fp, args, CF_DATA_TYPE_STRING, true);
}

/*********************************************************************/

static FnCallResult FnCallParseIntArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ParseArray(ctx, fp, args, CF_DATA_TYPE_INT, false);
}

/*********************************************************************/

static FnCallResult FnCallParseRealArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *args)
{
    return ParseArray(ctx, fp, args, CF_DATA_TYPE_REAL, false);
}

/*********************************************************************/

static FnCallResult FnCallStringMustache(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (!finalargs)
    {
        return FnFailure();
    }

    const char* const mustache_template = RlistScalarValue(finalargs);
    JsonElement *json = NULL;
    bool destroy_json = false;

    if (finalargs->next) // we have a variable name...
    {
      VarRef *ref = VarRefParse(RlistScalarValue(finalargs->next));
      DataType type = CF_DATA_TYPE_NONE;
      const void *value = EvalContextVariableGet(ctx, ref, &type);
      VarRefDestroy(ref);

      if (type != CF_DATA_TYPE_CONTAINER)
      {
        Log(LOG_LEVEL_VERBOSE, "Function '%s' was called with an invalid data container name: '%s'.", fp->name, RlistScalarValue(finalargs->next));
        return FnFailure();
      }

      json = (JsonElement *)value;
    }
    else
    {
      json = DefaultTemplateData(ctx, NULL);
      destroy_json = true;
    }

    Buffer *result = BufferNew();
    bool success = MustacheRender(result, mustache_template, json);

    // if we allocated a new container, destroy it now
    if (destroy_json) JsonDestroy(json);

    if (success)
    {
        return FnReturnBuffer(result);
    }
    else
    {
        BufferDestroy(result);
        return FnFailure();
    }
}

/*********************************************************************/

static FnCallResult FnCallSplitString(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    /* 2args: string,split_regex,max  */

    char *string = RlistScalarValue(finalargs);
    char *split = RlistScalarValue(finalargs->next);
    int max = IntFromString(RlistScalarValue(finalargs->next->next));

    // Read once to validate structure of file in itemlist
    Rlist *newlist = RlistFromSplitRegex(string, split, max, true);

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallStringSplit(ARG_UNUSED EvalContext *ctx,
                                      ARG_UNUSED const Policy *policy,
                                      ARG_UNUSED const FnCall *fp,
                                      const Rlist *finalargs)
{
    /* 3 args: string, split_regex, max  */
    char *string = RlistScalarValue(finalargs);
    char *split = RlistScalarValue(finalargs->next);
    int max = IntFromString(RlistScalarValue(finalargs->next->next));

    if (max < 1)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s' called with invalid maxent argument: '%d' (should be > 0).", fp->name, max);
        return FnFailure();
    }

    Rlist *newlist = RlistFromRegexSplitNoOverflow(string, split, max);

    if (newlist == NULL)
    {
        /* We are logging error in RlistFromRegexSplitNoOverflow() so no need to do it here as well. */
        return FnFailure();
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFileSexist(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char naked[CF_MAXVARSIZE];
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
    DataType input_list_type = CF_DATA_TYPE_NONE;
    const Rlist *input_list = EvalContextVariableGet(ctx, ref, &input_list_type);
    if (!input_list)
    {
        Log(LOG_LEVEL_VERBOSE, "Function filesexist was promised a list called '%s' but this was not found", listvar);
        VarRefDestroy(ref);
        return FnFailure();
    }

    VarRefDestroy(ref);

    if (DataTypeToRvalType(input_list_type) != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_VERBOSE, "Function filesexist was promised a list called '%s' but this variable is not a list", listvar);
        return FnFailure();
    }

    for (const Rlist *rp = input_list; rp != NULL; rp = rp->next)
    {
        struct stat sb;
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

static FnCallResult FnCallLDAPValue(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], handle[CF_BUFSIZE];

    char *uri = RlistScalarValue(finalargs);
    char *dn = RlistScalarValue(finalargs->next);
    char *filter = RlistScalarValue(finalargs->next->next);
    char *name = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next);

    snprintf(handle, CF_BUFSIZE, "%s_%s_%s_%s", dn, filter, name, scope);

    void *newval = CfLDAPValue(uri, dn, filter, name, scope, sec);
    if (newval)
    {
        CacheUnreliableValue("ldapvalue", handle, newval);
    }
    else if (RetrieveUnreliableValue("ldapvalue", handle, buffer))
    {
        newval = xstrdup(buffer);
    }

    if (newval)
    {
        return FnReturnNoCopy(newval);
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallLDAPArray(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (!fp->caller)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' can only be called from a promise", fp->name);
        return FnFailure();
    }

    char *array = RlistScalarValue(finalargs);
    char *uri = RlistScalarValue(finalargs->next);
    char *dn = RlistScalarValue(finalargs->next->next);
    char *filter = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next);

    void *newval = CfLDAPArray(ctx, PromiseGetBundle(fp->caller), array, uri, dn, filter, scope, sec);
    if (newval)
    {
        return FnReturnNoCopy(newval);
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallLDAPList(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *uri = RlistScalarValue(finalargs);
    char *dn = RlistScalarValue(finalargs->next);
    char *filter = RlistScalarValue(finalargs->next->next);
    char *name = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next);

    void *newval = CfLDAPList(uri, dn, filter, name, scope, sec);
    if (newval)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, RVAL_TYPE_LIST } };
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallRegLDAP(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *uri = RlistScalarValue(finalargs);
    char *dn = RlistScalarValue(finalargs->next);
    char *filter = RlistScalarValue(finalargs->next->next);
    char *name = RlistScalarValue(finalargs->next->next->next);
    char *scope = RlistScalarValue(finalargs->next->next->next->next);
    char *regex = RlistScalarValue(finalargs->next->next->next->next->next);
    char *sec = RlistScalarValue(finalargs->next->next->next->next->next->next);

    void *newval = CfRegLDAP(ctx, uri, dn, filter, name, scope, regex, sec);
    if (newval)
    {
        return FnReturnNoCopy(newval);
    }

    return FnFailure();
}

/*********************************************************************/

#define KILOBYTE 1024

static FnCallResult FnCallDiskFree(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    off_t df = GetDiskUsage(RlistScalarValue(finalargs), CF_SIZE_ABS);

    if (df == CF_INFINITY)
    {
        df = 0;
    }

    return FnReturnF("%jd", (intmax_t) (df / KILOBYTE));
}


static FnCallResult FnCallMakerule(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const char *target = RlistScalarValue(finalargs);
    const char *listvar = RlistScalarValue(finalargs->next);
    Rlist *list = NULL;

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

        DataType input_list_type = CF_DATA_TYPE_NONE;
        const Rlist *input_list = EvalContextVariableGet(ctx, ref, &input_list_type);
        VarRefDestroy(ref);

        if (!input_list)
        {
            Log(LOG_LEVEL_VERBOSE, "Function 'makerule' was promised a list called '%s' but this was not found", listvar);
            return FnFailure();
        }

       if (DataTypeToRvalType(input_list_type) != RVAL_TYPE_LIST)
       {
           Log(LOG_LEVEL_WARNING, "Function 'makerule' was promised a list called '%s' but this variable is not a list", listvar);
           return FnFailure();
       }

       list = RlistCopy(input_list);
    }

    time_t target_time = 0;
    bool stale = false;
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
            Log(LOG_LEVEL_VERBOSE, "Function MAKERULE, source dependency %s was not (yet) readable",  RvalScalarValue(rp->val));
            RlistDestroy(list);
            return FnReturnContext(false);
        }
        else
        {
            if (statbuf.st_mtime > target_time)
            {
                stale = true;
            }
        }
    }

    RlistDestroy(list);

    return stale ? FnReturnContext(true) : FnReturnContext(false);
}


#if !defined(__MINGW32__)

FnCallResult FnCallUserExists(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *arg = RlistScalarValue(finalargs);

    if (StringIsNumeric(arg))
    {
        uid_t uid = Str2Uid(arg, NULL, NULL);
        if (uid == CF_SAME_OWNER || uid == CF_UNKNOWN_OWNER)
        {
            return FnFailure();
        }

        if (getpwuid(uid) == NULL)
        {
            return FnReturnContext(false);
        }
    }
    else if (getpwnam(arg) == NULL)
    {
        return FnReturnContext(false);
    }

    return FnReturnContext(true);
}

/*********************************************************************/

FnCallResult FnCallGroupExists(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *arg = RlistScalarValue(finalargs);

    if (StringIsNumeric(arg))
    {
        gid_t gid = Str2Gid(arg, NULL, NULL);
        if (gid == CF_SAME_GROUP || gid == CF_UNKNOWN_GROUP)
        {
            return FnFailure();
        }

        if (getgrgid(gid) == NULL)
        {
            return FnReturnContext(false);
        }
    }
    else if (getgrnam(arg) == NULL)
    {
        return FnReturnContext(false);
    }

    return FnReturnContext(true);
}

#endif /* !defined(__MINGW32__) */

static bool SingleLine(const char *s)
{
    size_t length = strcspn(s, "\n\r");
#ifdef __MINGW32__ /* Treat a CRLF as a single line-ending: */
    if (s[length] == '\r' && s[length + 1] == '\n')
    {
        length++;
    }
#endif
    /* [\n\r] followed by EOF */
    return s[length] && !s[length+1];
}

/* TODO change maxsize to size_t. */
static char *CfReadFile(const char *filename, int maxsize)
{
    /* TODO remove this stat() call, it's a remnant from old code
       that examined sb.st_size. */
    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
        {
            Log(LOG_LEVEL_INFO, "CfReadFile: Could not examine file '%s'", filename);
        }
        else
        {
            if (IsCf3VarString(filename))
            {
                Log(LOG_LEVEL_VERBOSE, "CfReadFile: Cannot converge/reduce variable '%s' yet .. assuming it will resolve later",
                      filename);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "CfReadFile: Could not examine file '%s' (stat: %s)",
                      filename, GetErrorStr());
            }
        }
        return NULL;
    }

    /* 0 means 'read until the end of file' */
    size_t limit = maxsize > 0 ? maxsize : SIZE_MAX;
    bool truncated = false;
    Writer *w = NULL;
    int fd = safe_open(filename, O_RDONLY | O_TEXT);
    if (fd >= 0)
    {
        w = FileReadFromFd(fd, limit, &truncated);
        close(fd);
    }

    if (!w)
    {
        Log(LOG_LEVEL_INFO, "CfReadFile: Error while reading file '%s' (%s)",
            filename, GetErrorStr());
        return NULL;
    }

    if (truncated)
    {
        Log(LOG_LEVEL_VERBOSE, "CfReadFile: Truncating file '%s' to %d bytes as "
            "requested by maxsize parameter", filename, maxsize);
    }

    size_t size = StringWriterLength(w);
    char *result = StringWriterClose(w);

    /* FIXME: Is it necessary here? Move to caller(s) */
    if (SingleLine(result))
    {
        StripTrailingNewline(result, size);
    }
    return result;
}

/*********************************************************************/

static char *StripPatterns(char *file_buffer, const char *pattern, const char *filename)
{
    if (NULL_OR_EMPTY(pattern))
    {
        return file_buffer;
    }

    pcre *rx = CompileRegex(pattern);
    if (!rx)
    {
        return file_buffer;
    }

    int start, end, count = 0;
    while (StringMatchWithPrecompiledRegex(rx, file_buffer, &start, &end))
    {
        CloseStringHole(file_buffer, start, end);

        if (count++ > strlen(file_buffer))
        {
            Log(LOG_LEVEL_ERR,
                "Comment regex '%s' was irreconcilable reading input '%s' probably because it legally matches nothing",
                pattern, filename);
            pcre_free(rx);
            return file_buffer;
        }
    }

    pcre_free(rx);
    return file_buffer;
}

/*********************************************************************/

static void CloseStringHole(char *s, int start, int end)
{
    if (end > start)
    {
        memmove(s + start, s + end,
                /* The 1+ ensures we copy the final '\0' */
                1 + strlen(s + end));
    }
}

static JsonElement* BuildData(ARG_UNUSED EvalContext *ctx, const char *file_buffer,  const char *split, int maxent, bool make_array)
{
    JsonElement *ret = make_array ? JsonArrayCreate(10) : JsonObjectCreate(10);
    Seq *lines = SeqStringFromString(file_buffer, '\n');

    char *line;
    int hcount = 0;

    for (size_t i = 0; i < SeqLength(lines) && hcount < maxent; i++)
    {
        line = (char*) SeqAt(lines, i);
        size_t line_len = strlen(line);

        if (line_len == 0 || (line_len == 1 && line[0] == '\r'))
        {
            continue;
        }

        if (line[line_len - 1] ==  '\r')
        {
            line[line_len - 1] = '\0';
        }

        Rlist *tokens = RlistFromSplitRegex(line, split, 99999, true);
        JsonElement *linearray = JsonArrayCreate(10);

        for (const Rlist *rp = tokens; rp; rp = rp->next)
        {
            const char *token = RlistScalarValue(rp);
            JsonArrayAppendString(linearray, token);
        }

        RlistDestroy(tokens);

        if (JsonLength(linearray) > 0)
        {
            if (make_array)
            {
                JsonArrayAppendArray(ret, linearray);
            }
            else
            {
                char *key = xstrdup(JsonArrayGetAsString(linearray, 0));
                JsonArrayRemoveRange(linearray, 0, 0);
                JsonObjectAppendArray(ret, key, linearray);
                free(key);
            }

            // only increase hcount if we actually got something
            hcount++;
        }
    }

    SeqDestroy(lines);

    return ret;
}

/*********************************************************************/

static int BuildLineArray(EvalContext *ctx, const Bundle *bundle,
                          const char *array_lval, const char *file_buffer,
                          const char *split, int maxent, DataType type,
                          bool int_index)
{
    Rlist *lines = RlistFromSplitString(file_buffer, '\n');
    int hcount = 0;

    for (Rlist *it = lines; it && hcount < maxent; it = it->next)
    {
        char *line = RlistScalarValue(it);
        size_t line_len = strlen(line);

        if (line_len == 0 || (line_len == 1 && line[0] == '\r'))
        {
            continue;
        }

        if (line[line_len - 1] == '\r')
        {
            line[line_len - 1] = '\0';
        }

        char* first_index = NULL;
        int vcount = 0;

        Rlist *tokens = RlistFromSplitRegex(line, split, 99999, true);

        for (const Rlist *rp = tokens; rp; rp = rp->next)
        {
            const char *token = RlistScalarValue(rp);
            char *converted = NULL;

            switch (type)
            {
            case CF_DATA_TYPE_STRING:
                converted = xstrdup(token);
                break;

            case CF_DATA_TYPE_INT:
                {
                    long value = IntFromString(token);
                    if (value == CF_NOINT)
                    {
                        FatalError(ctx, "Could not convert token to int");
                    }
                    converted = StringFormat("%ld", value);
                }
                break;

            case CF_DATA_TYPE_REAL:
                {
                    double real_value = 0;
                    if (!DoubleFromString(token, &real_value))
                    {
                        FatalError(ctx, "Could not convert token to double");
                    }
                    converted = xstrdup(token);
                }
                break;

            default:
                ProgrammingError("Unhandled type in switch: %d", type);
            }

            if (NULL == first_index)
            {
                first_index = xstrdup(converted);
            }

            char *name;
            if (int_index)
            {
                xasprintf(&name, "%s[%d][%d]", array_lval, hcount, vcount);
            }
            else
            {
                xasprintf(&name, "%s[%s][%d]", array_lval, first_index, vcount);
            }

            VarRef *ref = VarRefParseFromBundle(name, bundle);
            EvalContextVariablePut(ctx, ref, converted, type, "source=function,function=buildlinearray");
            VarRefDestroy(ref);

            free(name);
            free(converted);

            vcount++;
        }

        free(first_index);
        RlistDestroy(tokens);

        hcount++;
        line++;
    }

    RlistDestroy(lines);

    return hcount;
}

/*********************************************************************/

static int ExecModule(EvalContext *ctx, char *command)
{
    FILE *pp = cf_popen(command, "rt", true);
    if (!pp)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe from '%s'. (cf_popen: %s)", command, GetErrorStr());
        return false;
    }

    char context[CF_BUFSIZE] = "";
    StringSet *tags = StringSetNew();

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    while (CfReadLine(&line, &line_size, pp) != -1)
    {
        bool print = false;
        for (const char *sp = line; *sp != '\0'; sp++)
        {
            if (!isspace((unsigned char) *sp))
            {
                print = true;
                break;
            }
        }

        ModuleProtocol(ctx, command, line, print, context, tags);
    }
    bool atend = feof(pp);
    cf_pclose(pp);
    free(line);
    StringSetDestroy(tags);

    if (!atend)
    {
        Log(LOG_LEVEL_ERR, "Unable to read output from '%s'. (fread: %s)", command, GetErrorStr());
        return false;
    }

    return true;
}


void ModuleProtocol(EvalContext *ctx, char *command, const char *line, int print, char* context, StringSet *tags)
{
    assert(tags);

    if (*context == '\0')
    {
        /* Infer namespace from script name */
        char arg0[CF_BUFSIZE];
        strlcpy(arg0, CommandArg0(command), CF_BUFSIZE);
        char *filename = basename(arg0);

        /* Canonicalize filename into acceptable namespace name */
        CanonifyNameInPlace(filename);
        strcpy(context, filename);
        Log(LOG_LEVEL_VERBOSE, "Module context '%s'", context);
    }

    char name[CF_BUFSIZE], content[CF_BUFSIZE];
    name[0] = content[0] = '\0';

    size_t length = strlen(line);
    switch (*line)
    {
    case '^':
        // Allow modules to set their variable context (up to 50 characters)
        if (1 == sscanf(line + 1, "context=%50[^\n]", content) && content[0] != '\0')
        {
            /* Symbol ID without \200 to \377: */
            pcre *context_name_rx = CompileRegex("[a-zA-Z0-9_]+");
            if (!context_name_rx)
            {
                Log(LOG_LEVEL_ERR,
                    "Internal error compiling module protocol context regex, aborting!!!");
            }
            else if (StringMatchFullWithPrecompiledRegex(context_name_rx, content))
            {
                Log(LOG_LEVEL_VERBOSE, "Module changed variable context from '%s' to '%s'", context, content);
                strcpy(context, content);
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "Module protocol was given an unacceptable ^context directive '%s', skipping", content);
            }

            if (context_name_rx)
            {
                pcre_free(context_name_rx);
            }
        }
        else if (1 == sscanf(line + 1, "meta=%1024[^\n]", content) && content[0] != '\0')
        {
            Log(LOG_LEVEL_VERBOSE, "Module set meta tags to '%s'", content);
            StringSetClear(tags);

            StringSetAddSplit(tags, content, ',');
            StringSetAdd(tags, xstrdup("source=module"));
        }
        else
        {
            Log(LOG_LEVEL_INFO, "Unknown extended module command '%s'", line);
        }
        break;

    case '+':
        if (length > CF_MAXVARSIZE)
        {
            Log(LOG_LEVEL_ERR,
                "Module protocol was given an overlong +class line (%zu bytes), skipping",
                length);
            break;
        }

        // the class name will fit safely inside CF_MAXVARSIZE - 1 bytes
        sscanf(line + 1, "%1023[^\n]", content);
        Log(LOG_LEVEL_VERBOSE, "Activating classes from module protocol: '%s'", content);
        if (CheckID(content))
        {
            Buffer *tagbuf = StringSetToBuffer(tags, ',');
            EvalContextClassPutSoft(ctx, content, CONTEXT_SCOPE_NAMESPACE, BufferData(tagbuf));
            BufferDestroy(tagbuf);
        }
        break;
    case '-':
        if (length > CF_MAXVARSIZE)
        {
            Log(LOG_LEVEL_ERR,
                "Module protocol was given an overlong -class line (%zu bytes), skipping",
                length);
            break;
        }

        // the class name(s) will fit safely inside CF_MAXVARSIZE - 1 bytes
        sscanf(line + 1, "%1023[^\n]", content);
        Log(LOG_LEVEL_VERBOSE, "Deactivating classes from module protocol: '%s'", content);
        if (CheckID(content))
        {
            if (content[0] != '\0')
            {
                StringSet *negated = StringSetFromString(content, ',');
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
        if (length > CF_BUFSIZE + 256)
        {
            Log(LOG_LEVEL_ERR,
                "Module protocol was given an overlong variable =line (%zu bytes), skipping",
                length);
            break;
        }

        // TODO: the variable name is limited to 256 to accommodate the
        // context name once it's in the vartable.  Maybe this can be relaxed.
        sscanf(line + 1, "%256[^=]=%4095[^\n]", name, content);

        if (CheckID(name))
        {
            Log(LOG_LEVEL_VERBOSE, "Defined variable '%s' in context '%s' with value '%s'", name, context, content);
            VarRef *ref = VarRefParseFromScope(name, context);

            Buffer *tagbuf = StringSetToBuffer(tags, ',');
            EvalContextVariablePut(ctx, ref, content, CF_DATA_TYPE_STRING, BufferData(tagbuf));
            BufferDestroy(tagbuf);

            VarRefDestroy(ref);
        }
        break;

    case '%':
        // TODO: the variable name is limited to 256 to accommodate the
        // context name once it's in the vartable.  Maybe this can be relaxed.
        sscanf(line + 1, "%256[^=]=", name);

        if (CheckID(name))
        {
            JsonElement *json = NULL;
            Buffer *holder = BufferNewFrom(line+strlen(name)+1+1,
                                           length - strlen(name) - 1 - 1);
            const char *hold = BufferData(holder);
            Log(LOG_LEVEL_DEBUG, "Module protocol parsing JSON %s", content);

            JsonParseError res = JsonParse(&hold, &json);
            if (res != JSON_PARSE_OK)
            {
                Log(LOG_LEVEL_INFO,
                    "Failed to parse JSON '%s' for module protocol: %s",
                    content, JsonParseErrorToString(res));
            }
            else
            {
                if (JsonGetElementType(json) == JSON_ELEMENT_TYPE_PRIMITIVE)
                {
                    Log(LOG_LEVEL_INFO,
                        "Module protocol JSON '%s' should be object or array; wasn't",
                        content);
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE,
                        "Defined data container variable '%s' in context '%s' with value '%s'",
                        name, context, BufferData(holder));

                    Buffer *tagbuf = StringSetToBuffer(tags, ',');
                    VarRef *ref = VarRefParseFromScope(name, context);

                    EvalContextVariablePut(ctx, ref, json, CF_DATA_TYPE_CONTAINER, BufferData(tagbuf));
                    VarRefDestroy(ref);
                    BufferDestroy(tagbuf);
                }

                JsonDestroy(json);
            }

            BufferDestroy(holder);
        }
        break;

    case '@':
        // TODO: should not need to exist. entry size matters, not line size. bufferize module protocol
        if (length > CF_BUFSIZE + 256 - 1)
        {
            Log(LOG_LEVEL_ERR,
                "Module protocol was given an overlong variable @line (%zu bytes), skipping",
                length);
            break;
        }

        sscanf(line + 1, "%256[^=]=%4095[^\n]", name, content);

        if (CheckID(name))
        {
            Rlist *list = RlistParseString(content);
            if (!list)
            {
                Log(LOG_LEVEL_ERR, "Module protocol could not parse variable %s's data content %s", name, content);
            }
            else
            {
                bool has_oversize_entry = false;
                for (const Rlist *rp = list; rp; rp = rp->next)
                {
                    size_t entry_size = strlen(RlistScalarValue(rp));
                    if (entry_size > CF_MAXVARSIZE)
                    {
                        has_oversize_entry = true;
                        break;
                    }
                }

                if (has_oversize_entry)
                {
                    Log(LOG_LEVEL_ERR, "Module protocol was given a variable @ line with an oversize entry, skipping");
                    RlistDestroy(list);
                    break;
                }

                Log(LOG_LEVEL_VERBOSE, "Defined variable '%s' in context '%s' with value '%s'", name, context, content);

                VarRef *ref = VarRefParseFromScope(name, context);

                Buffer *tagbuf = StringSetToBuffer(tags, ',');
                EvalContextVariablePut(ctx, ref, list, CF_DATA_TYPE_STRING_LIST, BufferData(tagbuf));
                BufferDestroy(tagbuf);

                VarRefDestroy(ref);
                RlistDestroy(list);
            }
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

static bool CheckID(const char *id)
{
    for (const char *sp = id; *sp != '\0'; sp++)
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
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Newer filename"},
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Older filename"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg ACCUM_ARGS[] =
{
    {"0,1000", CF_DATA_TYPE_INT, "Years"},
    {"0,1000", CF_DATA_TYPE_INT, "Months"},
    {"0,1000", CF_DATA_TYPE_INT, "Days"},
    {"0,1000", CF_DATA_TYPE_INT, "Hours"},
    {"0,1000", CF_DATA_TYPE_INT, "Minutes"},
    {"0,40000", CF_DATA_TYPE_INT, "Seconds"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg AND_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg AGO_ARGS[] =
{
    {"0,1000", CF_DATA_TYPE_INT, "Years"},
    {"0,1000", CF_DATA_TYPE_INT, "Months"},
    {"0,1000", CF_DATA_TYPE_INT, "Days"},
    {"0,1000", CF_DATA_TYPE_INT, "Hours"},
    {"0,1000", CF_DATA_TYPE_INT, "Minutes"},
    {"0,40000", CF_DATA_TYPE_INT, "Seconds"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg LATERTHAN_ARGS[] =
{
    {"0,10000", CF_DATA_TYPE_INT, "Years"},
    {"0,1000", CF_DATA_TYPE_INT, "Months"},
    {"0,1000", CF_DATA_TYPE_INT, "Days"},
    {"0,1000", CF_DATA_TYPE_INT, "Hours"},
    {"0,1000", CF_DATA_TYPE_INT, "Minutes"},
    {"0,40000", CF_DATA_TYPE_INT, "Seconds"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATE_ARGS[] = /* for on() */
{
    {"1970,3000", CF_DATA_TYPE_INT, "Year"},
    {"0,1000", CF_DATA_TYPE_INT, "Month (January = 0)"},
    {"0,1000", CF_DATA_TYPE_INT, "Day (First day of month = 0)"},
    {"0,1000", CF_DATA_TYPE_INT, "Hour"},
    {"0,1000", CF_DATA_TYPE_INT, "Minute"},
    {"0,1000", CF_DATA_TYPE_INT, "Second"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CANONIFY_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "String containing non-identifier characters"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CHANGEDBEFORE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Newer filename"},
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Older filename"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CLASSIFY_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CLASSMATCH_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CONCAT_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg COUNTLINESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Filename"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DIRNAME_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "File path"},
    {NULL, CF_DATA_TYPE_NONE, NULL},
};

static const FnCallArg DISKFREE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File system directory"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg ESCAPE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "IP address or string to escape"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg EXECRESULT_ARGS[] =
{
    {CF_PATHRANGE, CF_DATA_TYPE_STRING, "Fully qualified command path"},
    {"noshell,useshell,powershell", CF_DATA_TYPE_OPTION, "Shell encapsulation option"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

// fileexists, isdir,isplain,islink

static const FnCallArg FILESTAT_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File object name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILESTAT_DETAIL_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File object name"},
    {"size,gid,uid,ino,nlink,ctime,atime,mtime,xattr,mode,modeoct,permstr,permoct,type,devno,dev_minor,dev_major,basename,dirname,linktarget,linktarget_shallow", CF_DATA_TYPE_OPTION, "stat() field to get"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILESEXIST_ARGS[] =
{
    {CF_NAKEDLRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FINDFILES_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILTER_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression or string"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Match as regular expression if true, as exact string otherwise"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Invert matches"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of matches to return"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETFIELDS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression to match line"},
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Filename to read"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression to split fields"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Return array name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETINDICES_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine array or data container identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETUSERS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Comma separated list of User names"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Comma separated list of UserID numbers"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETENV_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Name of environment variable"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of characters to read "},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETGID_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Group name in text"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETUID_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "User name in text"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GREP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GROUPEXISTS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Group name or identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HASH_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input text"},
    {"md5,sha1,sha256,sha384,sha512", CF_DATA_TYPE_OPTION, "Hash or digest algorithm"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILE_HASH_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File object name"},
    {"md5,sha1,sha256,sha384,sha512", CF_DATA_TYPE_OPTION, "Hash or digest algorithm"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HASHMATCH_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Filename to hash"},
    {"md5,sha1,sha256,sha384,sha512", CF_DATA_TYPE_OPTION, "Hash or digest algorithm"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "ASCII representation of hash for comparison"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOST2IP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Host name in ascii"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg IP2HOST_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "IP address (IPv4 or IPv6)"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTINNETGROUP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Netgroup name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTRANGE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Hostname prefix"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Enumerated range"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTSSEEN_ARGS[] =
{
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Horizon since last seen in hours"},
    {"lastseen,notseen", CF_DATA_TYPE_OPTION, "Complements for selection policy"},
    {"name,address", CF_DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTSWITHCLASS_ARGS[] =
{
    {"[a-zA-Z0-9_]+", CF_DATA_TYPE_STRING, "Class name to look for"},
    {"name,address", CF_DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg IFELSE_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg IPRANGE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "IP address range syntax"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg IRANGE_ARGS[] =
{
    {CF_INTRANGE, CF_DATA_TYPE_INT, "Integer start of range"},
    {CF_INTRANGE, CF_DATA_TYPE_INT, "Integer end of range"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISGREATERTHAN_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Larger string or value"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Smaller string or value"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISLESSTHAN_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Smaller string or value"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Larger string or value"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISNEWERTHAN_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Newer file name"},
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Older file name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISVARIABLE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Variable identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg JOIN_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Join glue-string"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg LASTNODE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Link separator, e.g. /,:"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg LDAPARRAY_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Array name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Filter"},
    {"subtree,onelevel,base", CF_DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", CF_DATA_TYPE_OPTION, "Security level"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg LDAPLIST_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", CF_DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", CF_DATA_TYPE_OPTION, "Security level"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg LDAPVALUE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", CF_DATA_TYPE_OPTION, "Search scope policy"},
    {"none,ssl,sasl", CF_DATA_TYPE_OPTION, "Security level"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg LSDIRLIST_ARGS[] =
{
    {CF_PATHRANGE, CF_DATA_TYPE_STRING, "Path to base directory"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression to match files or blank"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Include the base path in the list"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAPLIST_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Pattern based on $(this) as original text"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier, the list variable to map"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg EXPANDRANGE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "String containing numerical range e.g. string[13-47]"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Step size of numerical increments"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAPARRAY_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Pattern based on $(this.k) and $(this.v) as original text"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine array or data container identifier, the array variable to map"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAPDATA_ARGS[] =
{
    {"none,json", CF_DATA_TYPE_OPTION, "Conversion to apply to the mapped string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Pattern based on $(this.k) and $(this.v) as original text"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine array or data container identifier, the array variable to map"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg MERGEDATA_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg NOT_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Class value"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg NOW_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg OR_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SUM_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "A list of arbitrary real values"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PRODUCT_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "A list of arbitrary real values"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PACKAGESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression (unanchored) to match package name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression (unanchored) to match package version"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression (unanchored) to match package architecture"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression (unanchored) to match package method"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PEERS_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Comment regex pattern"},
    {"2,64", CF_DATA_TYPE_INT, "Peer group size"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PEERLEADER_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Comment regex pattern"},
    {"2,64", CF_DATA_TYPE_INT, "Peer group size"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PEERLEADERS_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name of host list"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Comment regex pattern"},
    {"2,64", CF_DATA_TYPE_INT, "Peer group size"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg RANDOMINT_ARGS[] =
{
    {CF_INTRANGE, CF_DATA_TYPE_INT, "Lower inclusive bound"},
    {CF_INTRANGE, CF_DATA_TYPE_INT, "Upper exclusive bound"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READFILE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READCSV_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READSTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name to read"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PARSESTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Array identifier to populate"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "A string to parse for input data"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READSTRINGLIST_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name to read"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READDATA_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name to read"},
    {"CSV,YAML,JSON,auto", CF_DATA_TYPE_OPTION, "Type of data to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READJSON_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name to read"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PARSEJSON_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "JSON string to parse"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STOREJSON_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine data container identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READTCP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Host name or IP address of server socket"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Port number or service name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Protocol query string"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGARRAY_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine array identifier"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGCMP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Match string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGEXTRACT_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Match string"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Identifier for back-references"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATA_REGEXTRACT_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Match string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGISTRYVALUE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Windows registry key"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Windows registry value-id"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGLINE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Filename to search"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGLIST_ARGS[] =
{
    {CF_NAKEDLRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAKERULE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Target filename"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Source filename or CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REGLDAP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "URI"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Distinguished name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Filter"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Record name"},
    {"subtree,onelevel,base", CF_DATA_TYPE_OPTION, "Search scope policy"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex to match results"},
    {"none,ssl,sasl", CF_DATA_TYPE_OPTION, "Security level"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REMOTESCALAR_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Variable identifier"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Hostname or IP address of server"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Use enryption"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HUB_KNOWLEDGE_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Variable identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REMOTECLASSESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Server name or address"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Use encryption"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Return class prefix"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg RETURNSZERO_ARGS[] =
{
    {CF_PATHRANGE, CF_DATA_TYPE_STRING, "Command path"},
    {"noshell,useshell,powershell", CF_DATA_TYPE_OPTION, "Shell encapsulation option"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg RRANGE_ARGS[] =
{
    {CF_REALRANGE, CF_DATA_TYPE_REAL, "Real number, start of range"},
    {CF_REALRANGE, CF_DATA_TYPE_REAL, "Real number, end of range"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SELECTSERVERS_ARGS[] =
{
    {CF_NAKEDLRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier, the list of hosts or addresses to contact"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Port number or service name."},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "A query string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "A regular expression to match success"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of bytes to read from server"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Name for array of results"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SPLAYCLASS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string for classification"},
    {"daily,hourly", CF_DATA_TYPE_OPTION, "Splay time policy"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SPLITSTRING_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "A data string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex to split on"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of pieces"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRCMP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "String"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "String"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRFTIME_ARGS[] =
{
    {"gmtime,localtime", CF_DATA_TYPE_OPTION, "Use GMT or local time"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "A format string"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "The time as a Unix epoch offset"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SUBLIST_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {"head,tail", CF_DATA_TYPE_OPTION, "Whether to return elements from the head or from the tail of the list"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of elements to return"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg TRANSLATEPATH_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Unix style path"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg USEMODULE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Name of module command"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Argument string for the module"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg UNIQUE_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg NTH_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list or data container identifier"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Offset or key of element to return"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg EVERY_SOME_NONE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression or string"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg USEREXISTS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "User name or identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SORT_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {"lex,int,real,IP,ip,MAC,mac", CF_DATA_TYPE_OPTION, "Sorting method: lex or int or real (floating point) or IPv4/IPv6 or MAC address"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REVERSE_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SHUFFLE_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Any seed string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STAT_FOLD_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SETOP_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine base list identifier"},
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine filter list identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FORMAT_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine format string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg EVAL_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string"},
    {"math,class", CF_DATA_TYPE_OPTION, "Evaluation type"},
    {"infix", CF_DATA_TYPE_OPTION, "Evaluation options"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg BUNDLESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg XFORM_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg XFORM_SUBSTR_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of characters to return"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATASTATE_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg BUNDLESTATE_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Bundle name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETCLASSMETATAGS_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Class identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETVARIABLEMETATAGS_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "Variable identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATA_READSTRINGARRAY_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name to read"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex matching comments"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regex to split data"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of entries to read"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATA_EXPAND_ARGS[] =
{
    {CF_IDRANGE, CF_DATA_TYPE_STRING, "CFEngine array or data container identifier, which will be expanded"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRING_MUSTACHE_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

const FnCallType CF_FNCALL_TYPES[] =
{
    FnCallTypeNew("accessedbefore", CF_DATA_TYPE_CONTEXT, ACCESSEDBEFORE_ARGS, &FnCallIsAccessedBefore, "True if arg1 was accessed before arg2 (atime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("accumulated", CF_DATA_TYPE_INT, ACCUM_ARGS, &FnCallAccumulatedDate, "Convert an accumulated amount of time into a system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ago", CF_DATA_TYPE_INT, AGO_ARGS, &FnCallAgoDate, "Convert a time relative to now to an integer system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("and", CF_DATA_TYPE_STRING, AND_ARGS, &FnCallAnd, "Calculate whether all arguments evaluate to true",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("bundlesmatching", CF_DATA_TYPE_STRING_LIST, BUNDLESMATCHING_ARGS, &FnCallBundlesMatching, "Find all the bundles that match a regular expression and tags.",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("bundlestate", CF_DATA_TYPE_CONTAINER, BUNDLESTATE_ARGS, &FnCallBundlestate, "Construct a container of the variables in a bundle and the global class state",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("canonify", CF_DATA_TYPE_STRING, CANONIFY_ARGS, &FnCallCanonify, "Convert an abitrary string into a legal class name",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("canonifyuniquely", CF_DATA_TYPE_STRING, CANONIFY_ARGS, &FnCallCanonify, "Convert an abitrary string into a unique legal class name",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("concat", CF_DATA_TYPE_STRING, CONCAT_ARGS, &FnCallConcat, "Concatenate all arguments into string",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("changedbefore", CF_DATA_TYPE_CONTEXT, CHANGEDBEFORE_ARGS, &FnCallIsChangedBefore, "True if arg1 was changed before arg2 (ctime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classify", CF_DATA_TYPE_CONTEXT, CLASSIFY_ARGS, &FnCallClassify, "True if the canonicalization of the argument is a currently defined class",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classmatch", CF_DATA_TYPE_CONTEXT, CLASSMATCH_ARGS, &FnCallClassesMatching, "True if the regular expression matches any currently defined class",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classesmatching", CF_DATA_TYPE_STRING_LIST, CLASSMATCH_ARGS, &FnCallClassesMatching, "List the defined classes matching regex arg1 and tag regexes arg2,arg3,...",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countclassesmatching", CF_DATA_TYPE_INT, CLASSMATCH_ARGS, &FnCallClassesMatching, "Count the number of defined classes matching regex arg1",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countlinesmatching", CF_DATA_TYPE_INT, COUNTLINESMATCHING_ARGS, &FnCallCountLinesMatching, "Count the number of lines matching regex arg1 in file arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("datastate", CF_DATA_TYPE_CONTAINER, DATASTATE_ARGS, &FnCallDatastate, "Construct a container of the variable and class state",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("difference", CF_DATA_TYPE_STRING_LIST, SETOP_ARGS, &FnCallSetop, "Returns all the unique elements of list arg1 that are not in list arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("dirname", CF_DATA_TYPE_STRING, DIRNAME_ARGS, &FnCallDirname, "Return the parent directory name for given path",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("diskfree", CF_DATA_TYPE_INT, DISKFREE_ARGS, &FnCallDiskFree, "Return the free space (in KB) available on the directory's current partition (0 if not found)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("escape", CF_DATA_TYPE_STRING, ESCAPE_ARGS, &FnCallEscape, "Escape regular expression characters in a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("eval", CF_DATA_TYPE_STRING, EVAL_ARGS, &FnCallEval, "Evaluate a mathematical expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("every", CF_DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if every element in the named list matches the given regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("execresult", CF_DATA_TYPE_STRING, EXECRESULT_ARGS, &FnCallExecResult, "Execute named command and assign output to variable",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("file_hash", CF_DATA_TYPE_STRING, FILE_HASH_ARGS, &FnCallHandlerHash, "Return the hash of file arg1, type arg2 and assign to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("expandrange", CF_DATA_TYPE_STRING_LIST, MAPLIST_ARGS, &FnCallExpandRange, "Expand a name as a list of names numered according to a range",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("fileexists", CF_DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named file can be accessed",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesexist", CF_DATA_TYPE_CONTEXT, FILESEXIST_ARGS, &FnCallFileSexist, "True if the named list of files can ALL be accessed",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesize", CF_DATA_TYPE_INT, FILESTAT_ARGS, &FnCallFileStat, "Returns the size in bytes of the file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filestat", CF_DATA_TYPE_STRING, FILESTAT_DETAIL_ARGS, &FnCallFileStatDetails, "Returns stat() details of the file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filter", CF_DATA_TYPE_STRING_LIST, FILTER_ARGS, &FnCallFilter, "Similarly to grep(), filter the list arg2 for matches to arg2.  The matching can be as a regular expression or exactly depending on arg3.  The matching can be inverted with arg4.  A maximum on the number of matches returned can be set with arg5.",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("findfiles", CF_DATA_TYPE_STRING_LIST, FINDFILES_ARGS, &FnCallFindfiles, "Find files matching a shell glob pattern",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("format", CF_DATA_TYPE_STRING, FORMAT_ARGS, &FnCallFormat, "Applies a list of string values in arg2,arg3... to a string format in arg1 with sprintf() rules",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getclassmetatags", CF_DATA_TYPE_STRING_LIST, GETCLASSMETATAGS_ARGS, &FnCallGetMetaTags, "Collect a class's meta tags into an slist",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getenv", CF_DATA_TYPE_STRING, GETENV_ARGS, &FnCallGetEnv, "Return the environment variable named arg1, truncated at arg2 characters",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getfields", CF_DATA_TYPE_INT, GETFIELDS_ARGS, &FnCallGetFields, "Get an array of fields in the lines matching regex arg1 in file arg2, split on regex arg3 as array name arg4",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getgid", CF_DATA_TYPE_INT, GETGID_ARGS, &FnCallGetGid, "Return the integer group id of the named group on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getindices", CF_DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetIndices, "Get a list of keys to the array whose id is the argument and assign to variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getuid", CF_DATA_TYPE_INT, GETUID_ARGS, &FnCallGetUid, "Return the integer user id of the named user on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getusers", CF_DATA_TYPE_STRING_LIST, GETUSERS_ARGS, &FnCallGetUsers, "Get a list of all system users defined, minus those names defined in arg1 and uids in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getvalues", CF_DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetValues, "Get a list of values corresponding to the right hand sides in an array whose id is the argument and assign to variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getvariablemetatags", CF_DATA_TYPE_STRING_LIST, GETVARIABLEMETATAGS_ARGS, &FnCallGetMetaTags, "Collect a variable's meta tags into an slist",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("grep", CF_DATA_TYPE_STRING_LIST, GREP_ARGS, &FnCallGrep, "Extract the sub-list if items matching the regular expression in arg1 of the list named in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("groupexists", CF_DATA_TYPE_CONTEXT, GROUPEXISTS_ARGS, &FnCallGroupExists, "True if group or numerical id exists on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hash", CF_DATA_TYPE_STRING, HASH_ARGS, &FnCallHandlerHash, "Return the hash of arg1, type arg2 and assign to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hashmatch", CF_DATA_TYPE_CONTEXT, HASHMATCH_ARGS, &FnCallHashMatch, "Compute the hash of arg1, of type arg2 and test if it matches the value in arg3",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("host2ip", CF_DATA_TYPE_STRING, HOST2IP_ARGS, &FnCallHost2IP, "Returns the primary name-service IP address for the named host",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ip2host", CF_DATA_TYPE_STRING, IP2HOST_ARGS, &FnCallIP2Host, "Returns the primary name-service host name for the IP address",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostinnetgroup", CF_DATA_TYPE_CONTEXT, HOSTINNETGROUP_ARGS, &FnCallHostInNetgroup, "True if the current host is in the named netgroup",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostrange", CF_DATA_TYPE_CONTEXT, HOSTRANGE_ARGS, &FnCallHostRange, "True if the current host lies in the range of enumerated hostnames specified",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostsseen", CF_DATA_TYPE_STRING_LIST, HOSTSSEEN_ARGS, &FnCallHostsSeen, "Extract the list of hosts last seen/not seen within the last arg1 hours",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostswithclass", CF_DATA_TYPE_STRING_LIST, HOSTSWITHCLASS_ARGS, &FnCallHostsWithClass, "Extract the list of hosts with the given class set from the hub database (enterprise extension)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hubknowledge", CF_DATA_TYPE_STRING, HUB_KNOWLEDGE_ARGS, &FnCallHubKnowledge, "Read global knowledge from the hub host by id (enterprise extension)",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ifelse", CF_DATA_TYPE_STRING, IFELSE_ARGS, &FnCallIfElse, "Do If-ElseIf-ElseIf-...-Else evaluation of arguments",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("intersection", CF_DATA_TYPE_STRING_LIST, SETOP_ARGS, &FnCallSetop, "Returns all the unique elements of list arg1 that are also in list arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("iprange", CF_DATA_TYPE_CONTEXT, IPRANGE_ARGS, &FnCallIPRange, "True if the current host lies in the range of IP addresses specified",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("irange", CF_DATA_TYPE_INT_RANGE, IRANGE_ARGS, &FnCallIRange, "Define a range of integer values for cfengine internal use",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isdir", CF_DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a directory",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isexecutable", CF_DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object has execution rights for the current user",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isgreaterthan", CF_DATA_TYPE_CONTEXT, ISGREATERTHAN_ARGS, &FnCallIsLessGreaterThan, "True if arg1 is numerically greater than arg2, else compare strings like strcmp",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("islessthan", CF_DATA_TYPE_CONTEXT, ISLESSTHAN_ARGS, &FnCallIsLessGreaterThan, "True if arg1 is numerically less than arg2, else compare strings like NOT strcmp",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("islink", CF_DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a symbolic link",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isnewerthan", CF_DATA_TYPE_CONTEXT, ISNEWERTHAN_ARGS, &FnCallIsNewerThan, "True if arg1 is newer (modified later) than arg2 (mtime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isplain", CF_DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a plain/regular file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isvariable", CF_DATA_TYPE_CONTEXT, ISVARIABLE_ARGS, &FnCallIsVariable, "True if the named variable is defined",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("join", CF_DATA_TYPE_STRING, JOIN_ARGS, &FnCallJoin, "Join the items of arg2 into a string, using the conjunction in arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("lastnode", CF_DATA_TYPE_STRING, LASTNODE_ARGS, &FnCallLastNode, "Extract the last of a separated string, e.g. filename from a path",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("laterthan", CF_DATA_TYPE_CONTEXT, LATERTHAN_ARGS, &FnCallLaterThan, "True if the current time is later than the given date",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldaparray", CF_DATA_TYPE_CONTEXT, LDAPARRAY_ARGS, &FnCallLDAPArray, "Extract all values from an ldap record",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldaplist", CF_DATA_TYPE_STRING_LIST, LDAPLIST_ARGS, &FnCallLDAPList, "Extract all named values from multiple ldap records",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ldapvalue", CF_DATA_TYPE_STRING, LDAPVALUE_ARGS, &FnCallLDAPValue, "Extract the first matching named value from ldap",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("lsdir", CF_DATA_TYPE_STRING_LIST, LSDIRLIST_ARGS, &FnCallLsDir, "Return a list of files in a directory matching a regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("makerule", CF_DATA_TYPE_STRING, MAKERULE_ARGS, &FnCallMakerule, "True if the target file arg1 does not exist or a source file in arg2 is newer",
                      FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maparray", CF_DATA_TYPE_STRING_LIST, MAPARRAY_ARGS, &FnCallMapData, "Return a list with each element mapped from a CFEngine array or data container by a pattern based on $(this.k) and $(this.v)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mapdata", CF_DATA_TYPE_CONTAINER, MAPDATA_ARGS, &FnCallMapData, "Return a data container with each element parsed from a JSON string applied to every key-value pair of the given CFEngine array or data container, given as $(this.k) and $(this.v)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maplist", CF_DATA_TYPE_STRING_LIST, MAPLIST_ARGS, &FnCallMapList, "Return a list with each element modified by a pattern based $(this)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mergedata", CF_DATA_TYPE_CONTAINER, MERGEDATA_ARGS, &FnCallMergeData, "Merge two or more data containers or lists",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("none", CF_DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if no element in the named list matches the given regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("not", CF_DATA_TYPE_STRING, NOT_ARGS, &FnCallNot, "Calculate whether argument is false",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("now", CF_DATA_TYPE_INT, NOW_ARGS, &FnCallNow, "Convert the current time into system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("nth", CF_DATA_TYPE_STRING, NTH_ARGS, &FnCallNth, "Get the element at arg2 in list or data container arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("on", CF_DATA_TYPE_INT, DATE_ARGS, &FnCallOn, "Convert an exact date/time to an integer system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("or", CF_DATA_TYPE_STRING, OR_ARGS, &FnCallOr, "Calculate whether any argument evaluates to true",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("packagesmatching", CF_DATA_TYPE_CONTAINER, PACKAGESMATCHING_ARGS, &FnCallPackagesMatching, "List the installed packages (\"name,version,arch,manager\") matching regex arg1=name,arg2=version,arg3=arch,arg4=method",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("packageupdatesmatching", CF_DATA_TYPE_CONTAINER, PACKAGESMATCHING_ARGS, &FnCallPackagesMatching, "List the available patches (\"name,version,arch,manager\") matching regex arg1=name,arg2=version,arg3=arch,arg4=method.  Enterprise only.",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parseintarray", CF_DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseIntArray, "Read an array of integers from a string, indexing by first entry on line and sequentially within each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsejson", CF_DATA_TYPE_CONTAINER, PARSEJSON_ARGS, &FnCallParseJson, "Parse a JSON data container from a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parserealarray", CF_DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseRealArray, "Read an array of real numbers from a string, indexing by first entry on line and sequentially within each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsestringarray", CF_DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseStringArray, "Read an array of strings from a string, indexing by first word on line and sequentially within each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parsestringarrayidx", CF_DATA_TYPE_INT, PARSESTRINGARRAY_ARGS, &FnCallParseStringArrayIndex, "Read an array of strings from a string, indexing by line number and sequentially within each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("parseyaml", CF_DATA_TYPE_CONTAINER, PARSEJSON_ARGS, &FnCallParseJson, "Parse a data container from a YAML string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peers", CF_DATA_TYPE_STRING_LIST, PEERS_ARGS, &FnCallPeers, "Get a list of peers (not including ourself) from the partition to which we belong",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peerleader", CF_DATA_TYPE_STRING, PEERLEADER_ARGS, &FnCallPeerLeader, "Get the assigned peer-leader of the partition to which we belong",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("peerleaders", CF_DATA_TYPE_STRING_LIST, PEERLEADERS_ARGS, &FnCallPeerLeaders, "Get a list of peer leaders from the named partitioning",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("product", CF_DATA_TYPE_REAL, PRODUCT_ARGS, &FnCallProduct, "Return the product of a list of reals",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("randomint", CF_DATA_TYPE_INT, RANDOMINT_ARGS, &FnCallRandomInt, "Generate a random integer between the given limits, excluding the upper",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readcsv", CF_DATA_TYPE_CONTAINER, READCSV_ARGS, &FnCallReadData, "Parse a CSV file and return a JSON data container with the contents",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readdata", CF_DATA_TYPE_CONTAINER, READDATA_ARGS, &FnCallReadData, "Parse a YAML, JSON, CSV, etc. file and return a JSON data container with the contents",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readfile", CF_DATA_TYPE_STRING, READFILE_ARGS, &FnCallReadFile, "Read max number of bytes from named file and assign to variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintarray", CF_DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadIntArray, "Read an array of integers from a file, indexed by first entry on line and sequentially on each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintlist", CF_DATA_TYPE_INT_LIST, READSTRINGLIST_ARGS, &FnCallReadIntList, "Read and assign a list variable from a file of separated ints",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readjson", CF_DATA_TYPE_CONTAINER, READJSON_ARGS, &FnCallReadData, "Read a JSON data container from a file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readrealarray", CF_DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadRealArray, "Read an array of real numbers from a file, indexed by first entry on line and sequentially on each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readreallist", CF_DATA_TYPE_REAL_LIST, READSTRINGLIST_ARGS, &FnCallReadRealList, "Read and assign a list variable from a file of separated real numbers",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringarray", CF_DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadStringArray, "Read an array of strings from a file, indexed by first entry on line and sequentially on each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringarrayidx", CF_DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadStringArrayIndex, "Read an array of strings from a file, indexed by line number and sequentially on each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readstringlist", CF_DATA_TYPE_STRING_LIST, READSTRINGLIST_ARGS, &FnCallReadStringList, "Read and assign a list variable from a file of separated strings",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readyaml", CF_DATA_TYPE_CONTAINER, READJSON_ARGS, &FnCallReadData, "Read a data container from a YAML file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readtcp", CF_DATA_TYPE_STRING, READTCP_ARGS, &FnCallReadTcp, "Connect to tcp port, send string and assign result to variable",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regarray", CF_DATA_TYPE_CONTEXT, REGARRAY_ARGS, &FnCallRegArray, "True if arg1 matches any item in the associative array with id=arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regcmp", CF_DATA_TYPE_CONTEXT, REGCMP_ARGS, &FnCallRegCmp, "True if arg1 is a regular expression matching that matches string arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regextract", CF_DATA_TYPE_CONTEXT, REGEXTRACT_ARGS, &FnCallRegExtract, "True if the regular expression in arg 1 matches the string in arg2 and sets a non-empty array of backreferences named arg3",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("registryvalue", CF_DATA_TYPE_STRING, REGISTRYVALUE_ARGS, &FnCallRegistryValue, "Returns a value for an MS-Win registry key,value pair",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regline", CF_DATA_TYPE_CONTEXT, REGLINE_ARGS, &FnCallRegLine, "True if the regular expression in arg1 matches a line in file arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reglist", CF_DATA_TYPE_CONTEXT, REGLIST_ARGS, &FnCallRegList, "True if the regular expression in arg2 matches any item in the list whose id is arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regldap", CF_DATA_TYPE_CONTEXT, REGLDAP_ARGS, &FnCallRegLDAP, "True if the regular expression in arg6 matches a value item in an ldap search",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("remotescalar", CF_DATA_TYPE_STRING, REMOTESCALAR_ARGS, &FnCallRemoteScalar, "Read a scalar value from a remote cfengine server",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("remoteclassesmatching", CF_DATA_TYPE_CONTEXT, REMOTECLASSESMATCHING_ARGS, &FnCallRemoteClassesMatching, "Read persistent classes matching a regular expression from a remote cfengine server and add them into local context with prefix",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("returnszero", CF_DATA_TYPE_CONTEXT, RETURNSZERO_ARGS, &FnCallReturnsZero, "True if named shell command has exit status zero",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("rrange", CF_DATA_TYPE_REAL_RANGE, RRANGE_ARGS, &FnCallRRange, "Define a range of real numbers for cfengine internal use",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reverse", CF_DATA_TYPE_STRING_LIST, REVERSE_ARGS, &FnCallReverse, "Reverse a string list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("selectservers", CF_DATA_TYPE_INT, SELECTSERVERS_ARGS, &FnCallSelectServers, "Select tcp servers which respond correctly to a query and return their number, set array of names",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("shuffle", CF_DATA_TYPE_STRING_LIST, SHUFFLE_ARGS, &FnCallShuffle, "Shuffle a string list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("some", CF_DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if an element in the named list matches the given regular expression",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sort", CF_DATA_TYPE_STRING_LIST, SORT_ARGS, &FnCallSort, "Sort a string list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splayclass", CF_DATA_TYPE_CONTEXT, SPLAYCLASS_ARGS, &FnCallSplayClass, "True if the first argument's time-slot has arrived, according to a policy in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splitstring", CF_DATA_TYPE_STRING_LIST, SPLITSTRING_ARGS, &FnCallSplitString, "Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_DEPRECATED),
    FnCallTypeNew("storejson", CF_DATA_TYPE_STRING, STOREJSON_ARGS, &FnCallStoreJson, "Convert a data container to a JSON string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strcmp", CF_DATA_TYPE_CONTEXT, STRCMP_ARGS, &FnCallStrCmp, "True if the two strings match exactly",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strftime", CF_DATA_TYPE_STRING, STRFTIME_ARGS, &FnCallStrftime, "Format a date and time string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sublist", CF_DATA_TYPE_STRING_LIST, SUBLIST_ARGS, &FnCallSublist, "Returns arg3 element from either the head or the tail (according to arg2) of list arg1.",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sum", CF_DATA_TYPE_REAL, SUM_ARGS, &FnCallSum, "Return the sum of a list of reals",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("translatepath", CF_DATA_TYPE_STRING, TRANSLATEPATH_ARGS, &FnCallTranslatePath, "Translate path separators from Unix style to the host's native",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("unique", CF_DATA_TYPE_STRING_LIST, UNIQUE_ARGS, &FnCallUnique, "Returns all the unique elements of list arg1",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("usemodule", CF_DATA_TYPE_CONTEXT, USEMODULE_ARGS, &FnCallUseModule, "Execute cfengine module script and set class if successful",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("userexists", CF_DATA_TYPE_CONTEXT, USEREXISTS_ARGS, &FnCallUserExists, "True if user name or numerical id exists on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("variablesmatching", CF_DATA_TYPE_STRING_LIST, CLASSMATCH_ARGS, &FnCallVariablesMatching, "List the variables matching regex arg1 and tag regexes arg2,arg3,...",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),

    // Functions section following new naming convention
    FnCallTypeNew("string_mustache", CF_DATA_TYPE_STRING, STRING_MUSTACHE_ARGS, &FnCallStringMustache, "Expand a Mustache template from arg1 into a string using the optional data container in arg2 or datastate()",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_split", CF_DATA_TYPE_STRING_LIST, SPLITSTRING_ARGS, &FnCallStringSplit, "Convert a string in arg1 into a list of at most arg3 strings by splitting on a regular expression in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // Text xform functions
    FnCallTypeNew("string_downcase", CF_DATA_TYPE_STRING, XFORM_ARGS, &FnCallTextXform, "Convert a string to lowercase",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_head", CF_DATA_TYPE_STRING, XFORM_SUBSTR_ARGS, &FnCallTextXform, "Extract characters from the head of the string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_reverse", CF_DATA_TYPE_STRING, XFORM_ARGS, &FnCallTextXform, "Reverse a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_length", CF_DATA_TYPE_INT, XFORM_ARGS, &FnCallTextXform, "Return the length of a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_tail", CF_DATA_TYPE_STRING, XFORM_SUBSTR_ARGS, &FnCallTextXform, "Extract characters from the tail of the string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_upcase", CF_DATA_TYPE_STRING, XFORM_ARGS, &FnCallTextXform, "Convert a string to UPPERCASE",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // List folding functions
    FnCallTypeNew("length", CF_DATA_TYPE_INT, STAT_FOLD_ARGS, &FnCallLength, "Return the length of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("max", CF_DATA_TYPE_STRING, SORT_ARGS, &FnCallFold, "Return the maximum of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mean", CF_DATA_TYPE_REAL, STAT_FOLD_ARGS, &FnCallFold, "Return the mean (average) of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("min", CF_DATA_TYPE_STRING, SORT_ARGS, &FnCallFold, "Return the minimum of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("variance", CF_DATA_TYPE_REAL, STAT_FOLD_ARGS, &FnCallFold, "Return the variance of a list",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // Data container functions
    FnCallTypeNew("data_regextract", CF_DATA_TYPE_CONTAINER, DATA_REGEXTRACT_ARGS, &FnCallRegExtract, "Matches the regular expression in arg 1 against the string in arg2 and returns a data container holding the backreferences by name",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("data_expand", CF_DATA_TYPE_CONTAINER, DATA_EXPAND_ARGS, &FnCallDataExpand, "Expands any CFEngine variables in a data container, keys or values",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // File parsing functions that output a data container
    FnCallTypeNew("data_readstringarray", CF_DATA_TYPE_CONTAINER, DATA_READSTRINGARRAY_ARGS, &FnCallDataRead, "Read an array of strings from a file into a data container map, using the first element as a key",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("data_readstringarrayidx", CF_DATA_TYPE_CONTAINER, DATA_READSTRINGARRAY_ARGS, &FnCallDataRead, "Read an array of strings from a file into a data container array",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNewNull()
};
