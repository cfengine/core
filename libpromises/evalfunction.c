/*
  Copyright 2024 Northern.tech AS

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

#ifdef __sun
#define _POSIX_PTHREAD_SEMANTICS /* Required on Solaris 11 (see ENT-13146) */
#endif /* __sun */

#include <limits.h>
#include <platform.h>
#include <evalfunction.h>

#include <policy_server.h>
#include <promises.h>
#include <dir.h>
#include <file_lib.h>
#include <dbm_api.h>
#include <lastseen.h>
#include <files_copy.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <hash.h>
#include <stddef.h>
#include <stdint.h>
#include <vars.h>
#include <addr_lib.h>
#include <syntax.h>
#include <item_lib.h>
#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <keyring.h>
#include <matching.h>
#include <unix.h>           /* GetUserName(), GetGroupName() */
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
#include <json.h>
#include <json-yaml.h>
#include <json-utils.h>
#include <known_dirs.h>
#include <mustache.h>
#include <processes_select.h>
#include <sysinfo.h>
#include <string_sequence.h>
#include <string_lib.h>
#include <version_comparison.h>
#include <mutex.h>          /* ThreadWait */
#include <glob_lib.h>

#include <math_eval.h>

#include <libgen.h>

#include <ctype.h>
#include <cf3.defs.h>
#include <compiler.h>
#include <rlist.h>
#include <acl_tools.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#ifdef HAVE_LIBCURL
static bool CURL_INITIALIZED = false; /* GLOBAL */
static JsonElement *CURL_CACHE = NULL;
#endif

#define SPLAY_PSEUDO_RANDOM_CONSTANT 8192

static FnCallResult FilterInternal(EvalContext *ctx, const FnCall *fp, const char *regex, const Rlist* rp, bool do_regex, bool invert, long max);

static char *StripPatterns(char *file_buffer, const char *pattern, const char *filename);
static int BuildLineArray(EvalContext *ctx, const Bundle *bundle, const char *array_lval, const char *file_buffer,
                          const char *split, int maxent, DataType type, bool int_index);
static JsonElement* BuildData(EvalContext *ctx, const char *file_buffer,  const char *split, int maxent, bool make_array);
static bool ExecModule(EvalContext *ctx, char *command);

static bool CheckIDChar(const char ch);
static bool CheckID(const char *id);
static const Rlist *GetListReferenceArgument(const EvalContext *ctx, const FnCall *fp, const char *lval_str, DataType *datatype_out);
static char *CfReadFile(const char *filename, size_t maxsize);

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

static FnCallResult FnReturnContainerNoCopy(JsonElement *container)
{
    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { container, RVAL_TYPE_CONTAINER }};
}

// Currently only used for LIBCURL function, macro can be removed later
#ifdef HAVE_LIBCURL
static FnCallResult FnReturnContainer(JsonElement *container)
{
    return FnReturnContainerNoCopy(JsonCopy(container));
}
#endif // HAVE_LIBCURL

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
    return (FnCallResult) { FNCALL_FAILURE, { 0 } };
}

static VarRef* ResolveAndQualifyVarName(const FnCall *fp, const char *varname)
{
    VarRef *ref = NULL;
    if (varname != NULL &&
        IsVarList(varname) &&
        strlen(varname) < CF_MAXVARSIZE)
    {
        char naked[CF_MAXVARSIZE] = "";
        GetNaked(naked, varname);
        ref = VarRefParse(naked);
    }
    else
    {
        ref = VarRefParse(varname);
    }

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

static JsonElement* VarRefValueToJson(const EvalContext *ctx, const FnCall *fp, const VarRef *ref,
                                      const DataType disallowed_datatypes[], size_t disallowed_count,
                                      bool allow_scalars, bool *allocated)
{
    assert(ref);

    DataType value_type = CF_DATA_TYPE_NONE;
    const void *value = EvalContextVariableGet(ctx, ref, &value_type);
    bool want_type = true;

    // Convenience storage for the name of the function, since fp can be NULL
    const char* fp_name = (fp ? fp->name : "VarRefValueToJson");

    for (size_t di = 0; di < disallowed_count; di++)
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
                if (rp->val.type == RVAL_TYPE_SCALAR) /* TODO what if it's an ilist */
                {
                    JsonArrayAppendString(convert, RlistScalarValue(rp));
                }
                else
                {
                    ProgrammingError("Ignored Rval of list type: %s",
                        RvalTypeToString(rp->val.type));
                }
            }

            *allocated = true;
            break;

        case RVAL_TYPE_CONTAINER:
            // TODO: look into optimizing this if necessary
            convert = JsonCopy(value);
            *allocated = true;
            break;

        case RVAL_TYPE_SCALAR:
        {
            const char* data = value;
            if (allow_scalars)
            {
                convert = JsonStringCreate(value);
                *allocated = true;
                break;
            }
            else
            {
                /* regarray,mergedata,maparray,mapdata only care for arrays
                 * and ignore strings, so they go through this path. */
                Log(LOG_LEVEL_DEBUG,
                    "Skipping scalar '%s' because 'allow_scalars' is false",
                    data);
            }
        }
        // fallthrough
        default:
            *allocated = true;

            {
                VariableTableIterator *iter = EvalContextVariableTableFromRefIteratorNew(ctx, ref);
                convert = JsonObjectCreate(10);
                const size_t ref_num_indices = ref->num_indices;
                char *last_key = NULL;
                Variable *var;

                while ((var = VariableTableIteratorNext(iter)) != NULL)
                {
                    JsonElement *holder = convert;
                    JsonElement *holder_parent = NULL;
                    const VarRef *var_ref = VariableGetRef(var);
                    if (var_ref->num_indices - ref_num_indices == 1)
                    {
                        last_key = var_ref->indices[ref_num_indices];
                    }
                    else if (var_ref->num_indices - ref_num_indices > 1)
                    {
                        Log(LOG_LEVEL_DEBUG, "%s: got ref with starting depth %zu and index count %zu",
                            fp_name, ref_num_indices, var_ref->num_indices);
                        for (size_t index = ref_num_indices; index < var_ref->num_indices-1; index++)
                        {
                            JsonElement *local = JsonObjectGet(holder, var_ref->indices[index]);
                            if (local == NULL)
                            {
                                local = JsonObjectCreate(1);
                                JsonObjectAppendObject(holder, var_ref->indices[index], local);
                            }

                            last_key = var_ref->indices[index+1];
                            holder_parent = holder;
                            holder = local;
                        }
                    }

                    if (last_key != NULL && holder != NULL)
                    {
                        Rval var_rval = VariableGetRval(var, true);
                        switch (var_rval.type)
                        {
                        case RVAL_TYPE_SCALAR:
                            if (JsonGetElementType(holder) != JSON_ELEMENT_TYPE_CONTAINER)
                            {
                                Log(LOG_LEVEL_WARNING,
                                    "Replacing a non-container JSON element '%s' with a new empty container"
                                    " (for the '%s' subkey)",
                                    JsonGetPropertyAsString(holder), last_key);

                                assert(holder_parent != NULL);

                                JsonElement *empty_container = JsonObjectCreate(10);

                                /* we have to duplicate 'holder->propertyName'
                                 * instead of just using a pointer to it here
                                 * because 'holder' is destroyed as part of the
                                 * JsonObjectAppendElement() call below */
                                char *element_name = xstrdup(JsonGetPropertyAsString(holder));
                                JsonObjectAppendElement(holder_parent,
                                                        element_name,
                                                        empty_container);
                                free (element_name);
                                holder = empty_container;
                                JsonObjectAppendString(holder, last_key, var_rval.item);
                            }
                            else
                            {
                                JsonElement *child = JsonObjectGet(holder, last_key);
                                if (child != NULL && JsonGetElementType(child) == JSON_ELEMENT_TYPE_CONTAINER)
                                {
                                    Rval var_rval_secret = VariableGetRval(var, false);
                                    Log(LOG_LEVEL_WARNING,
                                        "Not replacing the container '%s' with a non-container value '%s'",
                                        JsonGetPropertyAsString(child), (char*) var_rval_secret.item);
                                }
                                else
                                {
                                    /* everything ok, just append the string */
                                    JsonObjectAppendString(holder, last_key, var_rval.item);
                                }
                            }
                            break;

                        case RVAL_TYPE_LIST:
                        {
                            JsonElement *array = JsonArrayCreate(10);
                            for (const Rlist *rp = RvalRlistValue(var_rval); rp != NULL; rp = rp->next)
                            {
                                if (rp->val.type == RVAL_TYPE_SCALAR)
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
                    Log(LOG_LEVEL_VERBOSE, "%s: argument '%s' does not resolve to a container or a list or a CFEngine array",
                        fp_name, varname);
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
        Log(LOG_LEVEL_DEBUG, "%s: argument '%s' resolved to an undesired data type",
            fp_name, varname);
        free(varname);
    }

    return convert;
}

static JsonElement *LookupVarRefToJson(void *ctx, const char **data)
{
    Buffer* varname = NULL;
    Seq *s = StringMatchCaptures("^(([a-zA-Z0-9_]+\\.)?[a-zA-Z0-9._]+)(\\[[^\\[\\]]+\\])?", *data, false);

    if (s && SeqLength(s) > 0) // got a variable name
    {
        varname = BufferCopy((const Buffer*) SeqAt(s, 0));
    }

    if (s)
    {
        SeqDestroy(s);
    }

    VarRef *ref = NULL;
    if (varname)
    {
        ref = VarRefParse(BufferData(varname));
        // advance to the last character of the matched variable name
        *data += strlen(BufferData(varname))-1;
        BufferDestroy(varname);
    }

    if (!ref)
    {
        return NULL;
    }

    bool allocated = false;
    JsonElement *vardata = VarRefValueToJson(ctx, NULL, ref, NULL, 0, true, &allocated);
    VarRefDestroy(ref);

    // This should always return a copy
    if (!allocated)
    {
        vardata = JsonCopy(vardata);
    }

    return vardata;
}

static JsonElement* VarNameOrInlineToJson(EvalContext *ctx, const FnCall *fp, const Rlist* rp, bool allow_scalars, bool *allocated)
{
    JsonElement *inline_data = NULL;

    assert(rp);

    if (rp->val.type == RVAL_TYPE_CONTAINER)
    {
        return (JsonElement*) rp->val.item;
    }

    const char* data = RlistScalarValue(rp);

    JsonParseError res = JsonParseWithLookup(ctx, &LookupVarRefToJson, &data, &inline_data);

    if (res == JSON_PARSE_OK)
    {
        if (JsonGetElementType(inline_data) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            JsonDestroy(inline_data);
            inline_data = NULL;
        }
        else
        {
            *allocated = true;
            return inline_data;
        }
    }

    VarRef *ref = ResolveAndQualifyVarName(fp, data);
    if (!ref)
    {
        return NULL;
    }

    JsonElement *vardata = VarRefValueToJson(ctx, fp, ref, NULL, 0, allow_scalars, allocated);
    VarRefDestroy(ref);

    return vardata;
}

typedef struct {
    char *address;
    char *hostkey;
    time_t lastseen;
} HostData;

static HostData *HostDataNew(const char *address, const char *hostkey, time_t lastseen)
{
    HostData *data = xmalloc(sizeof(HostData));
    data->address = SafeStringDuplicate(address);
    data->hostkey = SafeStringDuplicate(hostkey);
    data->lastseen = lastseen;
    return data;
}

static void HostDataFree(HostData *hd)
{
    if (hd != NULL)
    {
        free(hd->address);
        free(hd->hostkey);
        free(hd);
    }
}

typedef enum {
    NAME,
    ADDRESS,
    HOSTKEY,
    NONE
} HostsSeenFieldOption;

static HostsSeenFieldOption ParseHostsSeenFieldOption(const char *field)
{
    if (StringEqual(field, "name"))
    {
        return NAME;
    }
    else if (StringEqual(field, "address"))
    {
        return ADDRESS;
    }
    else if (StringEqual(field, "hostkey"))
    {
        return HOSTKEY;
    }
    else
    {
        return NONE;
    }
}

static Rlist *GetHostsFromLastseenDB(Seq *host_data, time_t horizon, HostsSeenFieldOption return_what, bool return_recent)
{
    Rlist *recent = NULL, *aged = NULL;
    time_t now = time(NULL);
    time_t entrytime;
    char ret_host_data[CF_MAXVARSIZE]; // TODO: Could this be 1025 / NI_MAXHOST ?

    const size_t length = SeqLength(host_data);
    for (size_t i = 0; i < length; ++i)
    {
        HostData *hd = SeqAt(host_data, i);
        entrytime = hd->lastseen;

        if ((return_what == NAME || return_what == ADDRESS)
             && HostKeyAddressUnknown(hd->hostkey))
        {
            continue;
        }

        switch (return_what)
        {
            case NAME:
            {
                char hostname[NI_MAXHOST];
                if (IPString2Hostname(hostname, hd->address, sizeof(hostname)) != -1)
                {
                    StringCopy(hostname, ret_host_data, sizeof(ret_host_data));
                }
                else
                {
                    /* Not numeric address was requested, but IP was unresolvable. */
                    StringCopy(hd->address, ret_host_data, sizeof(ret_host_data));
                }
                break;
            }
            case ADDRESS:
                StringCopy(hd->address, ret_host_data, sizeof(ret_host_data));
                break;
            case HOSTKEY:
                StringCopy(hd->hostkey, ret_host_data, sizeof(ret_host_data));
                break;
            default:
                ProgrammingError("Parser allowed invalid hostsseen() field argument");
        }

        if (entrytime < now - horizon)
        {
            Log(LOG_LEVEL_DEBUG, "Old entry");

            if (RlistKeyIn(recent, ret_host_data))
            {
                Log(LOG_LEVEL_DEBUG, "There is recent entry for this ret_host_data. Do nothing.");
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Adding to list of aged hosts.");
                RlistPrependScalarIdemp(&aged, ret_host_data);
            }
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Recent entry");

            Rlist *r = RlistKeyIn(aged, ret_host_data);
            if (r)
            {
                Log(LOG_LEVEL_DEBUG, "Purging from list of aged hosts.");
                RlistDestroyEntry(&aged, r);
            }

            Log(LOG_LEVEL_DEBUG, "Adding to list of recent hosts.");
            RlistPrependScalarIdemp(&recent, ret_host_data);
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

static FnCallResult FnCallGetACLs(
    ARG_UNUSED EvalContext *ctx,
    ARG_UNUSED const Policy *policy,
    const FnCall *fp,
    const Rlist *final_args)
{
    assert(fp != NULL);
    assert(final_args != NULL);
    assert(final_args->next != NULL);

    const char *path = RlistScalarValue(final_args);
    const char *type = RlistScalarValue(final_args->next);
    assert(StringEqual(type, "default") || StringEqual(type, "access"));

#ifdef _WIN32
    /* TODO: Policy function to read Windows ACLs (ENT-13019) */
    Rlist *acls = NULL;
    errno = ENOTSUP;
#else
    Rlist *acls = GetACLs(path, StringEqual(type, "access"));
#endif /* _WIN32 */
    if (acls == NULL)
    {
        Log((errno != ENOTSUP) ? LOG_LEVEL_ERR : LOG_LEVEL_VERBOSE,
            "Function %s failed to get ACLs for '%s': %s",
            fp->name, path, GetErrorStr());

        if (errno != ENOTSUP)
        {
            return FnFailure();
        } /* else we'll just return an empty list instead */
    }

    return (FnCallResult) { FNCALL_SUCCESS, { acls, RVAL_TYPE_LIST } };
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
    SeqAppend(ctx, HostDataNew(address, hostkey, quality->lastseen));
    return true;
}

/*******************************************************************/

static FnCallResult FnCallHostsSeen(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    Seq *host_data = SeqNew(1, HostDataFree);

    int horizon = IntFromString(RlistScalarValue(finalargs)) * 3600;
    char *hostseen_policy = RlistScalarValue(finalargs->next);
    char *field_str = RlistScalarValue(finalargs->next->next);
    HostsSeenFieldOption field = ParseHostsSeenFieldOption(field_str);

    Log(LOG_LEVEL_DEBUG, "Calling hostsseen(%d,%s,%s)",
        horizon, hostseen_policy, field_str);

    if (!ScanLastSeenQuality(&CallHostsSeenCallback, host_data))
    {
        SeqDestroy(host_data);
        return FnFailure();
    }

    Rlist *returnlist = GetHostsFromLastseenDB(host_data, horizon,
                                               field,
                                               StringEqual(hostseen_policy, "lastseen"));

    SeqDestroy(host_data);

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

static FnCallResult FnCallHostsWithField(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);
    Rlist *returnlist = NULL;

    char *field_value = RlistScalarValue(finalargs);
    char *return_format = RlistScalarValue(finalargs->next);
    if (StringEqual(fp->name, "hostswithclass"))
    {
        if (!ListHostsWithField(ctx, &returnlist, field_value, return_format, HOSTS_WITH_CLASS))
        {
            return FnFailure();
        }
    }
    else if (StringEqual(fp->name, "hostswithgroup"))
    {
        if (!ListHostsWithField(ctx, &returnlist, field_value, return_format, HOSTS_WITH_GROUP))
        {
            return FnFailure();
        }
    }
    else
    {
        ProgrammingError("HostsWithField with unknown call function %s, aborting", fp->name);
        return FnFailure();
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

/** @brief Convert function call from/to variables to range
 *
 *  Swap the two integers in place if the first is bigger
 *  Check for CF_NOINT, indicating invalid arguments
 *
 *  @return Absolute (positive) difference, -1 for error (0 for equal)
*/
static int int_range_convert(int *from, int *to)
{
    int old_from = *from;
    int old_to = *to;
    if (old_from == CF_NOINT || old_to == CF_NOINT)
    {
        return -1;
    }
    if (old_from == old_to)
    {
        return 0;
    }
    if (old_from > old_to)
    {
        *from = old_to;
        *to = old_from;
    }
    assert(*to > *from);
    return (*to) - (*from);
}

static FnCallResult FnCallRandomInt(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    if (finalargs->next == NULL)
    {
        return FnFailure();
    }

    int from = IntFromString(RlistScalarValue(finalargs));
    int to = IntFromString(RlistScalarValue(finalargs->next));

    int range = int_range_convert(&from, &to);
    if (range == -1)
    {
        return FnFailure();
    }
    if (range == 0)
    {
        return FnReturnF("%d", from);
    }

    assert(range > 0);

    int result = from + (int) (drand48() * (double) range);

    return FnReturnF("%d", result);
}

// Read an array of bytes as unsigned integers
// Convert to 64 bit unsigned integer
// Cross platform/arch, bytes[0] is always LSB of result
static uint64_t BytesToUInt64(uint8_t *bytes)
{
    uint64_t result = 0;
    size_t n = 8;
    for (size_t i = 0; i<n; ++i)
    {
        result += ((uint64_t)(bytes[i])) << (8 * i);
    }
    return result;
}

static FnCallResult FnCallHashToInt(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    if (finalargs->next == NULL || finalargs->next->next == NULL)
    {
        return FnFailure();
    }
    signed int from = IntFromString(RlistScalarValue(finalargs));
    signed int to = IntFromString(RlistScalarValue(finalargs->next));

    signed int range = int_range_convert(&from, &to);
    if (range == -1)
    {
        return FnFailure();
    }
    if (range == 0)
    {
        return FnReturnF("%d", from);
    }
    assert(range > 0);

    const unsigned char * const inp = RlistScalarValue(finalargs->next->next);

    // Use beginning of SHA checksum as basis:
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    memset(digest, 0, sizeof(digest));
    HashString(inp, strlen(inp), digest, HASH_METHOD_SHA256);
    uint64_t converted_sha = BytesToUInt64((uint8_t*)digest);

    // Limit using modulo:
    signed int result = from + (converted_sha % range);
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

#if defined(HAVE_GETPWENT) && !defined(__ANDROID__)

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


#if defined(HAVE_GETPWENT) && !defined(__ANDROID__)

static FnCallResult FnCallFindLocalUsers(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' couldn't parse argument '%s'",
            fp->name, RlistScalarValueSafe(finalargs));
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_ERR, "Bad argument '%s' in function '%s': Expected data container or slist",
            RlistScalarValueSafe(finalargs), fp->name);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    JsonElement *parent = JsonObjectCreate(10);
    setpwent();
    struct passwd *pw;
    while ((pw = getpwent()) != NULL)
    {
        JsonIterator iter = JsonIteratorInit(json);
        bool can_add_to_json = true;
        JsonElement *element = JsonIteratorNextValue(&iter);
        while (element != NULL)
        {
            if (JsonGetElementType(element) != JSON_ELEMENT_TYPE_PRIMITIVE)
            {
                Log(LOG_LEVEL_ERR, "Bad argument '%s' in function '%s': Filter cannot include nested data",
                    RlistScalarValueSafe(finalargs), fp->name);
                JsonDestroyMaybe(json, allocated);
                JsonDestroy(parent);
                return FnFailure();
            }
            const char *field = JsonPrimitiveGetAsString(element);
            const Rlist *tuple = RlistFromSplitString(field, '=');
            assert(tuple != NULL);
            const char *attribute = TrimWhitespace(RlistScalarValue(tuple));

            if (tuple->next == NULL)
            {
                Log(LOG_LEVEL_ERR, "Invalid filter field '%s' in function '%s': Expected attributes and values to be separated with '='",
                    fp->name, field);
                JsonDestroyMaybe(json, allocated);
                JsonDestroy(parent);
                return FnFailure();
            }
            const char *value = TrimWhitespace(RlistScalarValue(tuple->next));

            if (StringEqual(attribute, "name"))
            {
                if (!StringMatchFull(value, pw->pw_name))
                {
                    can_add_to_json = false;
                }
            }
            else if (StringEqual(attribute, "uid"))
            {
                char uid_string[PRINTSIZE(pw->pw_uid)];
                int ret = snprintf(uid_string, sizeof(uid_string), "%u", pw->pw_uid);

                if (ret < 0)
                {
                    Log(LOG_LEVEL_ERR, "Couldn't convert the uid of '%s' to string in function '%s'",
                        pw->pw_name, fp->name);
                    JsonDestroyMaybe(json, allocated);
                    JsonDestroy(parent);
                    return FnFailure();
                }
                assert((size_t) ret < sizeof(uid_string));

                if (!StringMatchFull(value, uid_string))
                {
                    can_add_to_json = false;
                }
            }
            else if (StringEqual(attribute, "gid"))
            {
                char gid_string[PRINTSIZE(pw->pw_uid)];
                int ret = snprintf(gid_string, sizeof(gid_string), "%u", pw->pw_uid);

                if (ret < 0)
                {
                    Log(LOG_LEVEL_ERR, "Couldn't convert the gid of '%s' to string in function '%s'",
                        pw->pw_name, fp->name);
                    JsonDestroyMaybe(json, allocated);
                    JsonDestroy(parent);
                    return FnFailure();
                }
                assert((size_t) ret < sizeof(gid_string));

                if (!StringMatchFull(value, gid_string))
                {
                    can_add_to_json = false;
                }
            }
            else if (StringEqual(attribute, "gecos"))
            {
                if (!StringMatchFull(value, pw->pw_gecos))
                {
                    can_add_to_json = false;
                }
            }
            else if (StringEqual(attribute, "dir"))
            {
                if ((!StringMatchFull(value, pw->pw_dir)))
                {
                    can_add_to_json = false;
                }
            }
            else if (StringEqual(attribute, "shell"))
            {
                if (!StringMatchFull(value, pw->pw_shell))
                {
                    can_add_to_json = false;
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Invalid attribute '%s' in function '%s': not supported",
                    attribute, fp->name);
                JsonDestroyMaybe(json, allocated);
                JsonDestroy(parent);
                return FnFailure();
            }
            element = JsonIteratorNextValue(&iter);
        }
        if (can_add_to_json)
        {
            JsonElement *child = JsonObjectCreate(6);
            JsonObjectAppendInteger(child, "uid", pw->pw_uid);
            JsonObjectAppendInteger(child, "gid", pw->pw_gid);
            JsonObjectAppendString(child, "gecos", pw->pw_gecos);
            JsonObjectAppendString(child, "dir", pw->pw_dir);
            JsonObjectAppendString(child, "shell", pw->pw_shell);
            JsonObjectAppendObject(parent, pw->pw_name, child);
        }
    }
    endpwent();
    JsonDestroyMaybe(json, allocated);

    return FnReturnContainerNoCopy(parent);
}

#else

static FnCallResult FnCallFindLocalUsers(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    Log(LOG_LEVEL_ERR, "findlocalusers is not implemented");
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
    char hostname[NI_MAXHOST];
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

static FnCallResult FnCallSysctlValue(ARG_UNUSED EvalContext *ctx,
                                      ARG_UNUSED ARG_UNUSED const Policy *policy,
                                      ARG_LINUX_ONLY const FnCall *fp,
                                      ARG_LINUX_ONLY const Rlist *finalargs)
{
#ifdef __linux__
    const bool sysctlvalue_mode = (strcmp(fp->name, "sysctlvalue") == 0);

    size_t max_sysctl_data = 16 * 1024;
    Buffer *procrootbuf = BufferNew();
    // Assumes that FILE_SEPARATOR is /
    BufferAppendString(procrootbuf, GetRelocatedProcdirRoot());
    BufferAppendString(procrootbuf, "/proc/sys");

    if (sysctlvalue_mode)
    {
        Buffer *key = BufferNewFrom(RlistScalarValue(finalargs),
                                    strlen(RlistScalarValue(finalargs)));

        // Note that in the single-key mode, we just reuse procrootbuf.
        Buffer *filenamebuf = procrootbuf;
        // Assumes that FILE_SEPARATOR is /
        BufferAppendChar(filenamebuf, '/');
        BufferSearchAndReplace(key, "\\.", "/", "gT");
        BufferAppendString(filenamebuf, BufferData(key));
        BufferDestroy(key);

        if (IsDir(BufferData(filenamebuf)))
        {
            Log(LOG_LEVEL_INFO, "Error while reading file '%s' because it's a directory (%s)",
                BufferData(filenamebuf), GetErrorStr());
            BufferDestroy(filenamebuf);
            return FnFailure();
        }

        Writer *w = NULL;
        bool truncated = false;
        int fd = safe_open(BufferData(filenamebuf), O_RDONLY | O_TEXT);
        if (fd >= 0)
        {
            w = FileReadFromFd(fd, max_sysctl_data, &truncated);
            close(fd);
        }

        if (w == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "Error while reading file '%s' (%s)",
                BufferData(filenamebuf), GetErrorStr());
            BufferDestroy(filenamebuf);
            return FnFailure();
        }

        BufferDestroy(filenamebuf);

        char *result = StringWriterClose(w);
        StripTrailingNewline(result, max_sysctl_data);
        return FnReturnNoCopy(result);
    }

    JsonElement *sysctl_data = JsonObjectCreate(10);

    // For the remaining operations, we want the trailing slash on this.
    BufferAppendChar(procrootbuf, '/');

    Buffer *filematchbuf = BufferCopy(procrootbuf);
    BufferAppendString(filematchbuf, "**/*");

    StringSet *sysctls = GlobFileList(BufferData(filematchbuf));
    BufferDestroy(filematchbuf);

    StringSetIterator it = StringSetIteratorInit(sysctls);
    const char *filename = NULL;
    while ((filename = StringSetIteratorNext(&it)))
    {
        Writer *w = NULL;
        bool truncated = false;

        if (IsDir(filename))
        {
            // No warning: this is normal as we match wildcards.
            continue;
        }

        int fd = safe_open(filename, O_RDONLY | O_TEXT);
        if (fd >= 0)
        {
            w = FileReadFromFd(fd, max_sysctl_data, &truncated);
            close(fd);
        }

        if (!w)
        {
            Log(LOG_LEVEL_INFO, "Error while reading file '%s' (%s)",
                filename, GetErrorStr());
            continue;
        }

        char *result = StringWriterClose(w);
        StripTrailingNewline(result, max_sysctl_data);

        Buffer *var = BufferNewFrom(filename, strlen(filename));
        BufferSearchAndReplace(var, BufferData(procrootbuf), "", "T");
        BufferSearchAndReplace(var, "/", ".", "gT");
        JsonObjectAppendString(sysctl_data, BufferData(var), result);
        free(result);
        BufferDestroy(var);
    }

    StringSetDestroy(sysctls);
    BufferDestroy(procrootbuf);
    return FnReturnContainerNoCopy(sysctl_data);
#else
    return FnFailure();
#endif
}

/*********************************************************************/

/* TODO move platform-specific code to libenv. */

static FnCallResult FnCallGetUserInfo(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
#ifdef __MINGW32__
    // TODO NetUserGetInfo(NULL, username, 1, &buf), see:
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa370654(v=vs.85).aspx
    return FnFailure();

#else /* !__MINGW32__ */

    /* TODO: implement and use new GetUserInfo(uid_or_username) */
    struct passwd *pw = NULL;

    if (finalargs == NULL)
    {
        pw = getpwuid(getuid());
    }
    else
    {
        char *arg = RlistScalarValue(finalargs);
        if (StringIsNumeric(arg))
        {
            uid_t uid = Str2Uid(arg, NULL, NULL);
            if (uid == CF_SAME_OWNER) // user "*"
            {
                uid = getuid();
            }
            else if (uid == CF_UNKNOWN_OWNER)
            {
                return FnFailure();
            }

            pw = getpwuid(uid);
        }
        else
        {
            pw = getpwnam(arg);
        }
    }

    JsonElement *result = GetUserInfo(pw);

    if (result == NULL)
    {
        return FnFailure();
    }

    return FnReturnContainerNoCopy(result);
#endif
}

/*********************************************************************/

static FnCallResult FnCallGetUid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
#ifdef __MINGW32__
    return FnFailure();                                         /* TODO */

#else /* !__MINGW32__ */

    uid_t uid;
    if (!GetUserID(RlistScalarValue(finalargs), &uid, LOG_LEVEL_ERR))
    {
        return FnFailure();
    }

    return FnReturnF("%ju", (uintmax_t)uid);
#endif
}

/*********************************************************************/

static FnCallResult FnCallGetGid(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
#ifdef __MINGW32__
    return FnFailure();                                         /* TODO */

#else /* !__MINGW32__ */

    gid_t gid;
    if (!GetGroupID(RlistScalarValue(finalargs), &gid, LOG_LEVEL_ERR))
    {
        return FnFailure();
    }

    return FnReturnF("%ju", (uintmax_t)gid);
#endif
}

/*********************************************************************/

static FnCallResult no_entry(int ret, const FnCall *fp, const char *group_name, bool is_user_db)
{
    assert(fp != NULL);

    const char *entry_type = is_user_db ? "user" : "group";
    const char *db_type = is_user_db ? "passwd" : "group";
    if (ret == 0 || ret == ENOENT || ret == ESRCH || ret == EBADF || ret == EPERM)
    {
        Log(LOG_LEVEL_DEBUG, "Couldn't find %s '%s' in %s database", entry_type, group_name, db_type);
        return FnReturnContext(false);
    }
    const char *const error_msg = GetErrorStrFromCode(ret);
    Log(LOG_LEVEL_ERR, "Couldn't open %s database in function '%s': %s", db_type, fp->name, error_msg);
    return FnFailure();
}

static FnCallResult FnCallUserInGroup(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);
    assert(finalargs != NULL);
#ifdef _WIN32
    Log(LOG_LEVEL_ERR, "Function '%s' is POSIX specific", fp->name);
    return FnFailure();
#else

    const char *user_name = RlistScalarValue(finalargs);
    const char *group_name = RlistScalarValue(finalargs->next);
    assert(group_name != NULL); // Guaranteed by parser

    int ret;

    // Check secondary group. This is what we expect the user to check for, thus it comes first.
    struct group grp;
    struct group *grent;
    char gr_buf[GETGR_R_SIZE_MAX] = {0};
    ret = getgrnam_r(group_name, &grp, gr_buf, GETGR_R_SIZE_MAX, &grent);

    if (grent == NULL)
    {
        // Group does not exist at all, so cannot be
        // primary or secondary group of user
        return no_entry(ret, fp, group_name, false);
    }
    while (grent->gr_mem[0] != NULL)
    {
        if (StringEqual(grent->gr_mem[0], user_name))
        {
            return FnReturnContext(true);
        }
        grent->gr_mem++;
    }

    // Check primary group
    struct passwd pwd;
    struct passwd *pwent;
    char pw_buf[GETPW_R_SIZE_MAX] = {0};
    ret = getpwnam_r(user_name, &pwd, pw_buf, GETPW_R_SIZE_MAX, &pwent);

    if (pwent == NULL)
    {
        return no_entry(ret, fp, user_name, true);
    }
    return FnReturnContext(grent->gr_gid == pwent->pw_gid);

#endif
}

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
        HashFile(string_or_filename, digest, type, false);
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
    HashFile(string, digest, type, false);

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
    unsigned int argcount = 0;
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
    if ((argcount % 2) == 0)
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
    assert(finalargs != NULL);
    bool count_only = false;
    bool check_only = false;
    unsigned count = 0;

    if (StringEqual(fp->name, "classesmatching"))
    {
        // Expected / default case
    }
    else if (StringEqual(fp->name, "classmatch"))
    {
        check_only = true;
    }
    else if (StringEqual(fp->name, "countclassesmatching"))
    {
        count_only = true;
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
        StringSet *global_matches = ClassesMatchingGlobal(ctx, RlistScalarValue(finalargs), finalargs->next, check_only);

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
    }

    if (check_only && count >= 1)
    {
        return FnReturnContext(true);
    }

    {
        StringSet *local_matches = ClassesMatchingLocal(ctx, RlistScalarValue(finalargs), finalargs->next, check_only);

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


static JsonElement *VariablesMatching(const EvalContext *ctx, const FnCall *fp, VariableTableIterator *iter, const Rlist *args, bool collect_full_data)
{
    JsonElement *matching = JsonObjectCreate(10);

    const char *regex = RlistScalarValue(args);
    Regex *rx = CompileRegex(regex);

    Variable *v = NULL;
    while ((v = VariableTableIteratorNext(iter)))
    {
        const VarRef *var_ref = VariableGetRef(v);
        char *expr = VarRefToString(var_ref, true);

        if (rx != NULL && StringMatchFullWithPrecompiledRegex(rx, expr))
        {
            StringSet *tagset = EvalContextVariableTags(ctx, var_ref);
            bool pass = false;

            if ((tagset != NULL) && (args->next != NULL))
            {
                for (const Rlist *arg = args->next; arg; arg = arg->next)
                {
                    const char* tag_regex = RlistScalarValue(arg);
                    const char *element = NULL;
                    StringSetIterator it = StringSetIteratorInit(tagset);
                    while ((element = SetIteratorNext(&it)))
                    {
                        if (StringMatchFull(tag_regex, element))
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
                JsonElement *data = NULL;
                bool allocated = false;
                if (collect_full_data)
                {
                    data = VarRefValueToJson(ctx, fp, var_ref, NULL, 0, true, &allocated);
                }

                /*
                 * When we don't collect the full variable data
                 * (collect_full_data is false), we still create a JsonObject
                 * with empty strings as the values. It will be destroyed soon
                 * afterwards, but the code is cleaner if we do it this way than
                 * if we make a JsonArray in one branch and a JsonObject in the
                 * other branch. The empty strings provide assurance that
                 * serializing this JsonObject (e.g. for logging) will not
                 * create problems. The extra memory usage from the empty
                 * strings is negligible.
                 */
                if (data == NULL)
                {
                    JsonObjectAppendString(matching, expr, "");
                }
                else
                {
                    if (!allocated)
                    {
                        data = JsonCopy(data);
                    }
                    JsonObjectAppendElement(matching, expr, data);
                }
            }
        }
        free(expr);
    }

    if (rx)
    {
        RegexDestroy(rx);
    }

    return matching;
}

static FnCallResult FnCallVariablesMatching(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    bool fulldata = (strcmp(fp->name, "variablesmatching_as_data") == 0);

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
        JsonElement *global_matches = VariablesMatching(ctx, fp, iter, finalargs, fulldata);
        VariableTableIteratorDestroy(iter);

        assert (JsonGetContainerType(global_matches) == JSON_CONTAINER_TYPE_OBJECT);

        if (fulldata)
        {
            return FnReturnContainerNoCopy(global_matches);

        }

        JsonIterator jiter = JsonIteratorInit(global_matches);
        const char *key;
        while ((key = JsonIteratorNextKey(&jiter)) != NULL)
        {
            assert (key != NULL);
            RlistPrepend(&matches, key, RVAL_TYPE_SCALAR);
        }

        JsonDestroy(global_matches);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}

/*********************************************************************/
static Bundle *GetBundleFromPolicy(const Policy *policy, const char *namespace, const char *bundlename)
{
    assert(policy != NULL);
    const size_t bundles_length = SeqLength(policy->bundles);

    for (size_t i = 0; i < bundles_length; i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);
        if (StringEqual(bp->name, bundlename) && StringEqual(bp->ns, namespace))
        {
            return bp;
        }
    }
    return NULL;
}

static Promise *GetPromiseFromBundle(const Bundle *bundle, const char *promise_type, const char *promiser)
{
    assert(bundle != NULL);
    const size_t sections_length = SeqLength(bundle->sections);
    for (size_t i = 0; i < sections_length; i++)
    {
        BundleSection *bsection = SeqAt(bundle->sections, i);
        if (StringEqual(bsection->promise_type, promise_type))
        {
            const size_t promises_length = SeqLength(bsection->promises);
            for (size_t i = 0; i < promises_length; i++)
            {
                Promise *promise = SeqAt(bsection->promises, i);
                if (StringEqual(promise->promiser, promiser))
                {
                    return promise;
                }
            }
        }
    }
    return NULL;
}

static FnCallResult FnCallGetMetaTags(EvalContext *ctx, const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);
    if (!finalargs)
    {
        FatalError(ctx, "Function '%s' requires at least one argument", fp->name);
    }

    Rlist *tags = NULL;
    StringSet *tagset = NULL;

    if (StringEqual(fp->name, "getvariablemetatags"))
    {
        VarRef *ref = VarRefParse(RlistScalarValue(finalargs));
        tagset = EvalContextVariableTags(ctx, ref);
        VarRefDestroy(ref);
    }
    else if (StringEqual(fp->name, "getclassmetatags"))
    {
        ClassRef ref = ClassRefParse(RlistScalarValue(finalargs));
        tagset = EvalContextClassTags(ctx, ref.ns, ref.name);
        ClassRefDestroy(ref);
    }
    else if (StringEqual(fp->name, "getbundlemetatags"))
    {
        const char *bundleref = RlistScalarValue(finalargs);
        assert(bundleref != NULL);
        const Rlist *args = RlistFromSplitString(bundleref, ':');
        const char *namespace = (args->next == NULL) ? "default" : RlistScalarValue(args);
        const char *name = RlistScalarValue((args->next == NULL) ? args : args->next);

        const Bundle *bundle = GetBundleFromPolicy(policy, namespace, name);
        if (bundle == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Function %s couldn't find bundle '%s' with namespace '%s'",
                fp->name,
                name,
                namespace);
            return FnFailure();
        }
        const Promise *promise = GetPromiseFromBundle(bundle, "meta", "tags");
        if (bundle == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Function %s couldn't find meta tags in '%s:%s'",
                fp->name,
                namespace,
                name);
            return FnFailure();
        }
        Rlist *start = PromiseGetConstraintAsList(ctx, "slist", promise);
        if (start == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Function %s couldn't find meta tags constraint string list",
                fp->name);
            return FnFailure();
        }

        tagset = StringSetNew();
        Rlist *temp = start;
        while (temp != NULL)
        {
            StringSetAdd(tagset, temp->val.item);
            temp = temp->next;
        }
    }
    else
    {
        FatalError(ctx, "FnCallGetMetaTags: got unknown function name '%s', aborting", fp->name);
    }

    if (tagset == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "%s found variable or class %s without a tagset", fp->name, RlistScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE, { 0 } };
    }

    char *key = NULL;
    if (finalargs->next != NULL)
    {
        Buffer *keybuf = BufferNew();
        BufferPrintf(keybuf, "%s=", RlistScalarValue(finalargs->next));
        key = BufferClose(keybuf);
    }

    char *element;
    StringSetIterator it = StringSetIteratorInit(tagset);
    while ((element = SetIteratorNext(&it)))
    {
        if (key != NULL)
        {
            if (StringStartsWith(element, key))
            {
                RlistAppendScalar(&tags, element+strlen(key));
            }
        }
        else
        {
            RlistAppendScalar(&tags, element);
        }
    }

    free(key);
    return (FnCallResult) { FNCALL_SUCCESS, { tags, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallBasename(ARG_UNUSED EvalContext *ctx,
                                   ARG_UNUSED const Policy *policy,
                                   const FnCall *fp,
                                   const Rlist *args)
{
    assert(fp != NULL);
    assert(fp->name != NULL);
    if (args == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires a filename as first arg!",
            fp->name);
        return FnFailure();
    }

    char dir[PATH_MAX];
    strlcpy(dir, RlistScalarValue(args), PATH_MAX);
    if (dir[0] == '\0')
    {
        return FnReturn(dir);
    }

    char *base = basename(dir);

    if (args->next != NULL)
    {
        char *suffix = RlistScalarValue(args->next);
        if (StringEndsWith(base, suffix))
        {
            size_t base_len = strlen(base);
            size_t suffix_len = strlen(suffix);

            // Remove only if actually a suffix, not the same string
            if (suffix_len < base_len)
            {
                // On Solaris, trying to edit the buffer returned by basename
                // causes segfault(!)
                base = xstrndup(base, base_len - suffix_len);
                return FnReturnNoCopy(base);
            }
        }
    }

    return FnReturn(base);
}

/*********************************************************************/

static FnCallResult FnCallBundlesMatching(EvalContext *ctx, const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (!finalargs)
    {
        return FnFailure();
    }

    const char *regex = RlistScalarValue(finalargs);
    Regex *rx = CompileRegex(regex);
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
            DataType type;
            const void *bundle_tags = EvalContextVariableGet(ctx, ref, &type);
            VarRefDestroy(ref);

            bool found = false; // case where tag_args are given and the bundle has no tags

            if (tag_args == NULL)
            {
                // we declare it found if no tags were requested
                found = true;
            }
            /* was the variable "tags" found? */
            else if (type != CF_DATA_TYPE_NONE)
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

    RegexDestroy(rx);

    return (FnCallResult) { FNCALL_SUCCESS, { matches, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static bool AddPackagesMatchingJsonLine(Regex *matcher, JsonElement *json, char *line)
{
    const size_t line_length = strlen(line);
    if (line_length > CF_BUFSIZE - 80)
    {
        Log(LOG_LEVEL_ERR,
            "Line from package inventory is too long (%zu) to be sensible",
            line_length);
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

static bool GetLegacyPackagesMatching(Regex *matcher, JsonElement *json, const bool installed_mode)
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
            "or it has not had time to have an effect yet or you are using "
            "new package promise and check for legacy promise is made."
            "A future call may still succeed. (fopen: %s)",
            installed_mode ? "installed" : "available",
            filename,
            GetErrorStr());

        return true;
    }

    char *line;
    while ((line = GetCsvLineNext(fin)) != NULL)
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

static bool GetPackagesMatching(Regex *matcher, JsonElement *json, const bool installed_mode, Rlist *default_inventory)
{
    dbid database = (installed_mode == true ? dbid_packages_installed : dbid_packages_updates);

    bool read_some_db = false;

    for (const Rlist *rp = default_inventory; rp != NULL; rp = rp->next)
    {
        const char *pm_name =  RlistScalarValue(rp);
        size_t pm_name_size = strlen(pm_name);

        if (StringContainsUnresolved(pm_name))
        {
            Log(LOG_LEVEL_DEBUG, "Package module '%s' contains unresolved variables", pm_name);
            continue;
        }

        Log(LOG_LEVEL_DEBUG, "Reading packages (%d) for package module [%s]",
                database, pm_name);

        CF_DB *db_cached;
        if (!OpenSubDB(&db_cached, database, pm_name))
        {
            Log(LOG_LEVEL_ERR, "Can not open database %d to get packages data.", database);
            return false;
        }
        else
        {
            read_some_db = true;
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
                for (size_t i = 0; i < SeqLength(packages_from_module); i++)
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
    return read_some_db;
}

static FnCallResult FnCallPackagesMatching(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const bool installed_mode = (strcmp(fp->name, "packagesmatching") == 0);
    Regex *matcher;
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

    bool inventory_allocated = false;
    if (default_inventory == NULL)
    {
        // Did not find default inventory from context, try looking for
        // existing LMDB databases in the state directory
        dbid database = (installed_mode ? dbid_packages_installed
                                        : dbid_packages_updates);
        Seq *const seq = SearchExistingSubDBNames(database);
        const size_t length = SeqLength(seq);
        for (size_t i = 0; i < length; i++)
        {
            const char *const db_name = SeqAt(seq, i);
            RlistAppendString(&default_inventory, db_name);
            inventory_allocated = true;
        }
        SeqDestroy(seq);
    }

    if (!default_inventory)
    {
        // Legacy package promise
        ret = GetLegacyPackagesMatching(matcher, json, installed_mode);
    }
    else
    {
        // We are using package modules.
        bool some_valid_inventory = false;
        for (const Rlist *rp = default_inventory; !some_valid_inventory && (rp != NULL); rp = rp->next)
        {
            const char *pm_name =  RlistScalarValue(rp);
            if (!StringContainsUnresolved(pm_name))
            {
                some_valid_inventory = true;
            }
        }

        if (some_valid_inventory)
        {
            ret = GetPackagesMatching(matcher, json, installed_mode, default_inventory);
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "No valid package module inventory found");
            RegexDestroy(matcher);
            JsonDestroy(json);
            if (inventory_allocated)
            {
                RlistDestroy(default_inventory);
            }
            return FnFailure();
        }
    }

    if (inventory_allocated)
    {
        RlistDestroy(default_inventory);
    }
    RegexDestroy(matcher);

    if (ret == false)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Unable to read package inventory.", fp->name);
        JsonDestroy(json);
        return FnFailure();
    }

    return FnReturnContainerNoCopy(json);
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
    const size_t len = strlen(string);
    /* In case of string_length(), buf needs enough space to hold a number. */
    const size_t bufsiz = MAX(len + 1, PRINTSIZE(len));
    char *buf = xcalloc(bufsiz, sizeof(char));
    memcpy(buf, string, len + 1);

    if (StringEqual(fp->name, "string_downcase"))
    {
        for (size_t pos = 0; pos < len; pos++)
        {
            buf[pos] = tolower(buf[pos]);
        }
    }
    else if (StringEqual(fp->name, "string_upcase"))
    {
        for (size_t pos = 0; pos < len; pos++)
        {
            buf[pos] = toupper(buf[pos]);
        }
    }
    else if (StringEqual(fp->name, "string_reverse"))
    {
        if (len > 1) {
            size_t c, i, j;
            for (i = 0, j = len - 1; i < j; i++, j--)
            {
                c = buf[i];
                buf[i] = buf[j];
                buf[j] = c;
            }
        }
    }
    else if (StringEqual(fp->name, "string_length"))
    {
        xsnprintf(buf, bufsiz, "%zu", len);
    }
    else if (StringEqual(fp->name, "string_head"))
    {
        long max = IntFromString(RlistScalarValue(finalargs->next));
        // A negative offset -N on string_head() means the user wants up to the Nth from the end
        if (max < 0)
        {
            max = len - labs(max);
        }

        // If the negative offset was too big, return an empty string
        if (max < 0)
        {
            max = 0;
        }

        if ((size_t) max < bufsiz)
        {
            buf[max] = '\0';
        }
    }
    else if (StringEqual(fp->name, "string_tail"))
    {
        const long max = IntFromString(RlistScalarValue(finalargs->next));
        // A negative offset -N on string_tail() means the user wants up to the Nth from the start

        if (max < 0)
        {
            size_t offset = MIN(labs(max), len);
            memcpy(buf, string + offset , len - offset + 1);
        }
        else if ((size_t) max < len)
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

static VersionComparison GenericVersionCheck(
        const FnCall *fp,
        const Rlist *args)
{
    assert(fp != NULL);
    assert(fp->name != NULL);
    if (args == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Policy fuction %s requires version to compare against",
            fp->name);
        return VERSION_ERROR;
    }

    const char *ver_string = RlistScalarValue(args);
    VersionComparison comparison = CompareVersion(Version(), ver_string);
    if (comparison == VERSION_ERROR)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Format of version comparison string '%s' is incorrect",
            fp->name, ver_string);
        return VERSION_ERROR;
    }

    return comparison;
}

static FnCallResult FnCallVersionCompare(
        ARG_UNUSED EvalContext *ctx,
        ARG_UNUSED const Policy *policy,
        const FnCall *fp,
        const Rlist *args)
{
    assert(fp != NULL);
    assert(fp->name != NULL);
    if (args == NULL || args->next == NULL || args->next->next == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Policy function %s requires 3 arguments:"
            " %s(version, operator, version)",
            fp->name,
            fp->name);
        return FnFailure();
    }
    const char *const version_a = RlistScalarValue(args);
    const char *const operator = RlistScalarValue(args->next);
    const char *const version_b = RlistScalarValue(args->next->next);

    const BooleanOrError result = CompareVersionExpression(version_a, operator, version_b);
    if (result == BOOLEAN_ERROR) {
        Log(LOG_LEVEL_ERR,
            "Cannot compare versions: %s(\"%s\", \"%s\", \"%s\")",
            fp->name,
            version_a,
            operator,
            version_b);
        return FnFailure();
    }
    return FnReturnContext(result == BOOLEAN_TRUE);
}

static FnCallResult FnCallVersionMinimum(
        ARG_UNUSED EvalContext *ctx,
        ARG_UNUSED const Policy *policy,
        const FnCall *fp,
        const Rlist *args)
{
    const VersionComparison comparison = GenericVersionCheck(fp, args);
    if (comparison == VERSION_ERROR)
    {
        return FnFailure();
    }

    return FnReturnContext(comparison == VERSION_GREATER ||
                           comparison == VERSION_EQUAL);
}

static FnCallResult FnCallVersionAfter(
        ARG_UNUSED EvalContext *ctx,
        ARG_UNUSED const Policy *policy,
        const FnCall *fp,
        const Rlist *args)
{
    const VersionComparison comparison = GenericVersionCheck(fp, args);
    if (comparison == VERSION_ERROR)
    {
        return FnFailure();
    }

    return FnReturnContext(comparison == VERSION_GREATER);
}

static FnCallResult FnCallVersionMaximum(
        ARG_UNUSED EvalContext *ctx,
        ARG_UNUSED const Policy *policy,
        const FnCall *fp,
        const Rlist *args)
{
    const VersionComparison comparison = GenericVersionCheck(fp, args);
    if (comparison == VERSION_ERROR)
    {
        return FnFailure();
    }

    return FnReturnContext(comparison == VERSION_SMALLER ||
                           comparison == VERSION_EQUAL);
}

static FnCallResult FnCallVersionBefore(
        ARG_UNUSED EvalContext *ctx,
        ARG_UNUSED const Policy *policy,
        const FnCall *fp,
        const Rlist *args)
{
    const VersionComparison comparison = GenericVersionCheck(fp, args);
    if (comparison == VERSION_ERROR)
    {
        return FnFailure();
    }

    return FnReturnContext(comparison == VERSION_SMALLER);
}

static FnCallResult FnCallVersionAt(
        ARG_UNUSED EvalContext *ctx,
        ARG_UNUSED const Policy *policy,
        const FnCall *fp,
        const Rlist *args)
{
    const VersionComparison comparison = GenericVersionCheck(fp, args);
    if (comparison == VERSION_ERROR)
    {
        return FnFailure();
    }

    return FnReturnContext(comparison == VERSION_EQUAL);
}

static FnCallResult FnCallVersionBetween(
        ARG_UNUSED EvalContext *ctx,
        ARG_UNUSED const Policy *policy,
        const FnCall *fp,
        const Rlist *args)
{
    assert(fp != NULL);
    assert(fp->name != NULL);
    if (args == NULL || args->next == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Policy fuction %s requires lower "
            "and upper versions to compare against",
            fp->name);
        return FnFailure();
    }

    const char *ver_string_lower = RlistScalarValue(args);
    const VersionComparison lower_comparison =
        CompareVersion(Version(), ver_string_lower);
    if (lower_comparison == VERSION_ERROR)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Format of lower version comparison string '%s' is incorrect",
            fp->name, ver_string_lower);
        return FnFailure();
    }

    const char *ver_string_upper = RlistScalarValue(args->next);
    const VersionComparison upper_comparison =
        CompareVersion(Version(), ver_string_upper);
    if (upper_comparison == VERSION_ERROR)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Format of upper version comparison string '%s' is incorrect",
            fp->name, ver_string_upper);
        return FnFailure();
    }

    return FnReturnContext((lower_comparison == VERSION_GREATER ||
                            lower_comparison == VERSION_EQUAL) &&
                           (upper_comparison == VERSION_SMALLER ||
                            upper_comparison == VERSION_EQUAL));
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
    assert(fp != NULL);

    const char *const function = fp->name;
    size_t args = RlistLen(finalargs);
    if (args == 0)
    {
        FatalError(ctx, "Missing argument to %s() - Must specify command", function);
    }
    else if (args == 1)
    {
        FatalError(ctx, "Missing argument to %s() - Must specify 'noshell', 'useshell', or 'powershell'", function);
    }
    else if (args > 3)
    {
        FatalError(ctx, "Too many arguments to %s() - Maximum 3 allowed", function);
    }
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

    const char *const command = RlistScalarValue(finalargs);
    if (IsAbsoluteFileName(command))
    {
        need_executable_check = true;
    }
    else if (shelltype == SHELL_TYPE_NONE)
    {
        Log(LOG_LEVEL_ERR, "%s '%s' does not have an absolute path", fp->name, command);
        return FnFailure();
    }

    if (need_executable_check && !IsExecutable(CommandArg0(command)))
    {
        Log(LOG_LEVEL_ERR, "%s '%s' is assumed to be executable but isn't", fp->name, command);
        return FnFailure();
    }

    size_t buffer_size = CF_EXPANDSIZE;
    char *buffer = xcalloc(1, buffer_size);

    OutputSelect output_select = OUTPUT_SELECT_BOTH;

    if (args >= 3)
    {
        const char *output = RlistScalarValue(finalargs->next->next);
        if (StringEqual(output, "stderr"))
        {
            output_select = OUTPUT_SELECT_STDERR;
        }
        else if (StringEqual(output, "stdout"))
        {
            output_select = OUTPUT_SELECT_STDOUT;
        }
        else
        {
            assert(StringEqual(output, "both"));
            assert(output_select == OUTPUT_SELECT_BOTH);
        }
    }

    int exit_code;

    if (GetExecOutput(command, &buffer, &buffer_size, shelltype, output_select, &exit_code))
    {
        Log(LOG_LEVEL_VERBOSE, "%s ran '%s' successfully", fp->name, command);
        if (StringEqual(function, "execresult"))
        {
            FnCallResult res = FnReturn(buffer);
            free(buffer);
            return res;
        }
        else
        {
            assert(StringEqual(function, "execresult_as_data"));
            JsonElement *result = JsonObjectCreate(2);
            JsonObjectAppendInteger(result, "exit_code", exit_code);
            JsonObjectAppendString(result, "output", buffer);
            free(buffer);
            return FnReturnContainerNoCopy(result);
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "%s could not run '%s' successfully", fp->name, command);
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
        int slot = StringHash(RlistScalarValue(finalargs), 0);
        slot &= (SPLAY_PSEUDO_RANDOM_CONSTANT - 1);
        slot = slot * 12 / SPLAY_PSEUDO_RANDOM_CONSTANT;
        snprintf(class_name, CF_MAXVARSIZE, "Min%02d_%02d", slot * 5, ((slot + 1) * 5) % 60);
    }
    else
    {
        /* 12*24 5-minute slots in day */
        int dayslot = StringHash(RlistScalarValue(finalargs), 0);
        dayslot &= (SPLAY_PSEUDO_RANDOM_CONSTANT - 1);
        dayslot = dayslot * 12 * 24 / SPLAY_PSEUDO_RANDOM_CONSTANT;
        int hour = dayslot / 12;
        int slot = dayslot % 12;

        snprintf(class_name, CF_MAXVARSIZE, "Min%02d_%02d.Hr%02d", slot * 5, ((slot + 1) * 5) % 60, hour);
    }

    Log(LOG_LEVEL_VERBOSE, "Computed context for '%s' splayclass: '%s'", RlistScalarValue(finalargs), class_name);
    return FnReturnContext(IsDefinedClass(ctx, class_name));
}

/*********************************************************************/

#ifdef HAVE_LIBCURL
struct _curl_userdata
{
    const FnCall *fp;
    const char *desc;
    size_t max_size;
    Buffer* content;
};

static size_t cfengine_curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct _curl_userdata *options = (struct _curl_userdata*) userdata;
    unsigned int old = BufferSize(options->content);
    size_t requested = size*nmemb;
    size_t granted = requested;

    if (old + requested > options->max_size)
    {
        granted = options->max_size - old;
        Log(LOG_LEVEL_VERBOSE,
            "%s: while receiving %s, current %u + requested %zu bytes would be over the maximum %zu; only accepting %zu bytes",
            options->fp->name, options->desc, old, requested, options->max_size, granted);
    }

    BufferAppend(options->content, ptr, granted);

    // `written` is actually (BufferSize(options->content) - old) but
    // libcurl doesn't like that
    size_t written = requested;

    // extra caution
    BufferTrimToMaxLength(options->content, options->max_size);
    return written;
}

static void CurlCleanup()
{
    if (CURL_CACHE == NULL)
    {
        JsonElement *temp = CURL_CACHE;
        CURL_CACHE = NULL;
        JsonDestroy(temp);
    }

    if (CURL_INITIALIZED)
    {
        curl_global_cleanup();
        CURL_INITIALIZED = false;
    }

}
#endif

static FnCallResult FnCallUrlGet(ARG_UNUSED EvalContext *ctx,
                                 ARG_UNUSED const Policy *policy,
                                 const FnCall *fp,
                                 const Rlist *finalargs)
{

#ifdef HAVE_LIBCURL

    char *url = RlistScalarValue(finalargs);
    bool allocated = false;
    JsonElement *options = VarNameOrInlineToJson(ctx, fp, finalargs->next, false, &allocated);

    if (options == NULL)
    {
        return FnFailure();
    }

    if (JsonGetElementType(options) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonGetContainerType(options) != JSON_CONTAINER_TYPE_OBJECT)
    {
        JsonDestroyMaybe(options, allocated);
        return FnFailure();
    }

    Writer *cache_w = StringWriter();
    WriterWriteF(cache_w, "url = %s; options = ", url);
    JsonWriteCompact(cache_w, options);

    if (CURL_CACHE == NULL)
    {
        CURL_CACHE = JsonObjectCreate(10);
        atexit(&CurlCleanup);
    }

    JsonElement *old_result = JsonObjectGetAsObject(CURL_CACHE, StringWriterData(cache_w));

    if (old_result != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "%s: found cached request for %s", fp->name, url);
        WriterClose(cache_w);
        JsonDestroyMaybe(options, allocated);
        return FnReturnContainer(old_result);
    }

    if (!CURL_INITIALIZED && curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
    {
        Log(LOG_LEVEL_ERR, "%s: libcurl initialization failed, sorry", fp->name);

        WriterClose(cache_w);
        JsonDestroyMaybe(options, allocated);
        return FnFailure();
    }

    CURL_INITIALIZED = true;

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        Log(LOG_LEVEL_ERR, "%s: libcurl easy_init failed, sorry", fp->name);

        WriterClose(cache_w);
        JsonDestroyMaybe(options, allocated);
        return FnFailure();
    }

    Buffer *content = BufferNew();
    Buffer *headers = BufferNew();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // do not use signals
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L); // set default timeout
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl,
                     CURLOPT_PROTOCOLS_STR,
                     // Allowed protocols
                     "file,ftp,ftps,http,https");

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cfengine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, cfengine_curl_write_callback);

    size_t max_content = 4096;
    size_t max_headers = 4096;
    JsonIterator iter = JsonIteratorInit(options);
    const JsonElement *e;
    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        const char *key = JsonIteratorCurrentKey(&iter);
        const char *value = JsonPrimitiveGetAsString(e);

        if (strcmp(key, "url.timeout") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: setting timeout to %ld seconds", fp->name, IntFromString(value));
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, IntFromString(value));
        }
        else if (strcmp(key, "url.verbose") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: setting verbosity to %ld", fp->name, IntFromString(value));
            curl_easy_setopt(curl, CURLOPT_VERBOSE, IntFromString(value));
        }
        else if (strcmp(key, "url.header") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: setting inline headers to %ld", fp->name, IntFromString(value));
            curl_easy_setopt(curl, CURLOPT_HEADER, IntFromString(value));
        }
        else if (strcmp(key, "url.referer") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: setting referer to %s", fp->name, value);
            curl_easy_setopt(curl, CURLOPT_REFERER, value);
        }
        else if (strcmp(key, "url.user-agent") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: setting user agent string to %s", fp->name, value);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, value);
        }
        else if (strcmp(key, "url.max_content") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: setting max contents to %ld", fp->name, IntFromString(value));
            max_content = IntFromString(value);
        }
        else if (strcmp(key, "url.max_headers") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "%s: setting max headers to %ld", fp->name, IntFromString(value));
            max_headers = IntFromString(value);
        }
        else
        {
            Log(LOG_LEVEL_INFO, "%s: unknown option %s", fp->name, key);
        }
    }

    struct _curl_userdata data = { fp, "content", max_content, content };
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    struct _curl_userdata header_data = { fp, "headers", max_headers, headers };
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);

    JsonElement *options_headers = JsonObjectGetAsArray(options, "url.headers");
    struct curl_slist *header_list = NULL;

    if (options_headers != NULL)
    {
        iter = JsonIteratorInit(options_headers);
        while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
        {
            header_list = curl_slist_append(header_list, JsonPrimitiveGetAsString(e));
        }
    }

    if (header_list != NULL)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    JsonElement *result = JsonObjectCreate(10);
    CURLcode res = curl_easy_perform(curl);
    if (header_list != NULL)
    {
        curl_slist_free_all(header_list);
        header_list = NULL;
    }

    long returncode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &returncode);
    JsonObjectAppendInteger(result, "returncode",  returncode);

    curl_easy_cleanup(curl);

    JsonObjectAppendInteger(result, "rc",  0+res);

    bool success = (CURLE_OK == res);
    JsonObjectAppendBool(result, "success", success);

    if (!success)
    {
        JsonObjectAppendString(result, "error_message", curl_easy_strerror(res));
    }



    BufferTrimToMaxLength(content, max_content);
    JsonObjectAppendString(result, "content",  BufferData(content));
    BufferDestroy(content);

    BufferTrimToMaxLength(headers, max_headers);
    JsonObjectAppendString(result, "headers",  BufferData(headers));
    BufferDestroy(headers);

    JsonObjectAppendObject(CURL_CACHE, StringWriterData(cache_w), JsonCopy(result));
    WriterClose(cache_w);

    JsonDestroyMaybe(options, allocated);
    return FnReturnContainerNoCopy(result);

#else

    UNUSED(finalargs);                 /* suppress unused parameter warning */
    Log(LOG_LEVEL_ERR,
        "%s: libcurl integration is not compiled into CFEngine, sorry", fp->name);
    return FnFailure();

#endif
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
        } while ((size_t) sent < length);
    }

    char recvbuf[CF_BUFSIZE];
    ssize_t n_read = recv(sd, recvbuf, maxbytes, 0);
    cf_closesocket(sd);

    if (n_read < 0)
    {
        Log(LOG_LEVEL_INFO, "readtcp: Error while receiving (%s)",
            GetErrorStr());
        return FnFailure();
    }

    assert((size_t) n_read < sizeof(recvbuf));
    recvbuf[n_read] = '\0';

    Log(LOG_LEVEL_VERBOSE,
        "readtcp: requested %zd maxbytes, got %zd bytes from %s",
        maxbytes, n_read, txtaddr);

    return FnReturn(recvbuf);
}

/*********************************************************************/

static FnCallResult FnCallIsConnectable(ARG_UNUSED EvalContext *ctx,
                                        ARG_UNUSED const Policy *policy,
                                        const FnCall *fp,
                                        const Rlist *finalargs)
{
    assert(fp != NULL);
    if (finalargs == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires a host name, or IP address as first argument",
            fp->name);
        return FnFailure();
    }
    char *hostnameip = RlistScalarValue(finalargs);
    if (finalargs->next == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires a port number as second argument",
            fp->name);
        return FnFailure();
    }
    char *port = RlistScalarValue(finalargs->next);
    long connect_timeout = (finalargs->next->next != NULL) ? IntFromString(RlistScalarValue(finalargs->next->next)) : CONNTIMEOUT;

    char txtaddr[CF_MAX_IP_LEN] = "";
    int sd = SocketConnect(hostnameip, port, connect_timeout, false, txtaddr, sizeof(txtaddr));
    if (sd > -1) {
        cf_closesocket(sd);
    }

    return FnReturnContext(sd > -1);
}

/*********************************************************************/

/**
 * Look for the indices of a variable in #finalargs if it is an array.
 *
 * @return *Always* return an slist of the indices; if the variable is not an
 *         array or does not resolve at all, return an empty slist.
 *
 * @NOTE
 * This is needed for literally one acceptance test:
 * 01_vars/02_functions/getindices_returns_expected_list_from_array.cf
 *
 * The case is that we have a[x] = "1" AND a[x][y] = "2" which is
 * ambiguous, but classic CFEngine arrays allow it. So we want the
 * classic getindices("a[x]") to return "y" in this case.
 */
static FnCallResult FnCallGetIndicesClassic(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    VarRef *ref = VarRefParse(RlistScalarValue(finalargs));
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
                "and it was not called from a promise. "
                "No way to automatically qualify the reference '%s'",
                fp->name, RlistScalarValue(finalargs));
            VarRefDestroy(ref);
            return FnFailure();
        }
    }

    Rlist *keys = NULL;

    VariableTableIterator *iter = EvalContextVariableTableFromRefIteratorNew(ctx, ref);
    const Variable *itervar;
    while ((itervar = VariableTableIteratorNext(iter)) != NULL)
    {
        const VarRef *itervar_ref = VariableGetRef(itervar);
        /*
        Log(LOG_LEVEL_DEBUG,
            "%s(%s): got itervar->ref->num_indices %zu while ref->num_indices is %zu",
            fp->name, RlistScalarValue(finalargs),
            itervar->ref->num_indices, ref->num_indices);
        */
        /* Does the variable we found have more indices than the one we
         * requested? For example, if we requested the variable "blah", it has
         * 0 indices, so a found variable blah[i] will be acceptable. */
        if (itervar_ref->num_indices > ref->num_indices)
        {
            RlistAppendScalarIdemp(&keys, itervar_ref->indices[ref->num_indices]);
        }
    }

    VariableTableIteratorDestroy(iter);
    VarRefDestroy(ref);

    return (FnCallResult) { FNCALL_SUCCESS, { keys, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGetIndices(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char *name_str = RlistScalarValueSafe(finalargs);
    bool allocated = false;
    JsonElement *json = NULL;

    // Protect against collected args (their rval type will be data
    // container). This is a special case to preserve legacy behavior
    // for array lookups that requires a scalar in finalargs.
    if (RlistValueIsType(finalargs, RVAL_TYPE_SCALAR))
    {
        VarRef *ref = ResolveAndQualifyVarName(fp, name_str);
        DataType type;
        EvalContextVariableGet(ctx, ref, &type);

        /* A variable holding a data container. */
        if (DataTypeToRvalType(type) == RVAL_TYPE_CONTAINER || DataTypeToRvalType(type) == RVAL_TYPE_LIST)
        {
            json = VarRefValueToJson(ctx, fp, ref, NULL, 0, true, &allocated);
        }
        /* Resolves to a different type or does not resolve at all. It's
         * normal not to resolve, for example "blah" will not resolve if the
         * variable table only contains "blah[1]"; we have to go through
         * FnCallGetIndicesClassic() to extract these indices. */
        else
        {
            JsonParseError res = JsonParseWithLookup(ctx, &LookupVarRefToJson, &name_str, &json);
            if (res == JSON_PARSE_OK)
            {
                if (JsonGetElementType(json) == JSON_ELEMENT_TYPE_PRIMITIVE)
                {
                    // VarNameOrInlineToJson() would now look up this primitive
                    // in the variable table, returning a JSON container for
                    // whatever type it is, but since we already know that it's
                    // not a native container type (thanks to the
                    // CF_DATA_TYPE_CONTAINER check above) we skip that, and
                    // stick to the legacy data types.
                    JsonDestroy(json);
                    VarRefDestroy(ref);
                    return FnCallGetIndicesClassic(ctx, policy, fp, finalargs);
                }
                else
                {
                    // Inline JSON of some sort.
                    allocated = true;
                }
            }
            else
            {
                /* Invalid inline JSON. */
                VarRefDestroy(ref);
                return FnCallGetIndicesClassic(ctx, policy, fp, finalargs);
            }
        }

        VarRefDestroy(ref);
    }
    else
    {
        json = VarNameOrInlineToJson(ctx, fp, finalargs, true, &allocated);
    }

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }

    Rlist *keys = NULL;

    if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        JsonDestroyMaybe(json, allocated);
        return (FnCallResult) { FNCALL_SUCCESS, { keys, RVAL_TYPE_LIST } };
    }

    if (JsonGetContainerType(json) == JSON_CONTAINER_TYPE_OBJECT)
    {
        JsonIterator iter = JsonIteratorInit(json);
        const char *key;
        while ((key = JsonIteratorNextKey(&iter)))
        {
            RlistAppendScalar(&keys, key);
        }
    }
    else
    {
        for (size_t i = 0; i < JsonLength(json); i++)
        {
            Rval key = (Rval) { StringFromLong(i), RVAL_TYPE_SCALAR };
            RlistAppendRval(&keys, key);
        }
    }

    JsonDestroyMaybe(json, allocated);
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
                if (value != NULL)
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
        if (value != NULL)
        {
            RlistAppendScalar(values, value);
            free(value);
        }
    }
}

/*********************************************************************/

static FnCallResult FnCallGetValues(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, true, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        /* CFE-2479: Inexistent variable, return an empty slist. */
        Log(LOG_LEVEL_DEBUG, "getvalues('%s'):"
            " unresolvable variable, returning an empty list",
            RlistScalarValueSafe(finalargs));
        return (FnCallResult) { FNCALL_SUCCESS, { NULL, RVAL_TYPE_LIST } };
    }

    Rlist *values = NULL;                      /* start with an empty Rlist */
    CollectContainerValues(ctx, &values, json);

    JsonDestroyMaybe(json, allocated);
    return (FnCallResult) { FNCALL_SUCCESS, { values, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGrep(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex
                          finalargs->next, // list identifier
                          1, // regex match = TRUE
                          0, // invert matches = FALSE
                          LONG_MAX); // max results = max int
}

/*********************************************************************/

static FnCallResult FnCallRegList(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs->next), // regex or string
                          finalargs, // list identifier
                          1,
                          0,
                          LONG_MAX);
}

/*********************************************************************/

static FnCallResult JoinContainer(const JsonElement *container, const char *delimiter)
{
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
    const char *name_str = RlistScalarValueSafe(finalargs->next);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs->next, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    FnCallResult result = JoinContainer(json, delimiter);
    JsonDestroyMaybe(json, allocated);
    return result;

}

/*********************************************************************/

static FnCallResult FnCallGetFields(EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    const FnCall *fp,
                                    const Rlist *finalargs)
{
    Regex *rx = CompileRegex(RlistScalarValue(finalargs));
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
        RegexDestroy(rx);
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
                        RegexDestroy(rx);
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

    RegexDestroy(rx);
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
    Regex *rx = CompileRegex(RlistScalarValue(finalargs));
    if (!rx)
    {
        return FnFailure();
    }

    char *filename = RlistScalarValue(finalargs->next);

    FILE *fin = safe_fopen(filename, "rt");
    if (!fin)
    {
        Log(LOG_LEVEL_ERR, "File '%s' could not be read in countlinesmatching(). (fopen: %s)", filename, GetErrorStr());
        RegexDestroy(rx);
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

    RegexDestroy(rx);

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

/*********************************************************************/

static JsonElement* ExecJSON_Pipe(const char *cmd, JsonElement *container)
{
    IOData io = cf_popen_full_duplex(cmd, false, false);

    if (io.write_fd == -1 || io.read_fd == -1)
    {
        Log(LOG_LEVEL_INFO, "An error occurred while communicating with '%s'", cmd);

        return NULL;
    }

    Log(LOG_LEVEL_DEBUG, "Opened fds %d and %d for command '%s'.",
        io.read_fd, io.write_fd, cmd);

    // write the container to a string
    Writer *w = StringWriter();
    JsonWrite(w, container, 0);
    char *container_str = StringWriterClose(w);

    ssize_t written = PipeWrite(&io, container_str);
    if (written < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to write to pipe (fd = %d): %s",
            io.write_fd, GetErrorStr());
        free(container_str);

        container_str = NULL;
    }
    else if ((size_t) written != strlen(container_str))
    {
        Log(LOG_LEVEL_VERBOSE, "Couldn't send whole container data to '%s'.", cmd);
        free(container_str);

        container_str = NULL;
    }

    Rlist *returnlist = NULL;
    if (container_str)
    {
        free(container_str);
        /* We can have some error message here. */
        returnlist = PipeReadData(&io, 5, 5);
    }

    /* If script returns non 0 status */
    int close = cf_pclose_full_duplex(&io);
    if (close != EXIT_SUCCESS)
    {
        Log(LOG_LEVEL_VERBOSE,
            "%s returned with non zero return code: %d",
            cmd, close);
    }

    // Exit if no data was obtained from the pipe
    if (returnlist == NULL)
    {
        return NULL;
    }

    JsonElement *returnjq = JsonArrayCreate(5);

    Buffer *buf = BufferNew();
    for (const Rlist *rp = returnlist; rp != NULL; rp = rp->next)
    {
        const char *data = RlistScalarValue(rp);

        if (BufferSize(buf) != 0)
        {
            // simulate the newline
            BufferAppendString(buf, "\n");
        }

        BufferAppendString(buf, data);
        const char *bufdata = BufferData(buf);
        JsonElement *parsed = NULL;
        if (JsonParse(&bufdata, &parsed) == JSON_PARSE_OK)
        {
            JsonArrayAppendElement(returnjq, parsed);
            BufferClear(buf);
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "'%s' generated invalid JSON '%s', appending next line", cmd, data);
        }
    }

    BufferDestroy(buf);

    RlistDestroy(returnlist);

    return returnjq;
}

/*********************************************************************/

static FnCallResult FnCallMapData(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    if (!fp->caller)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' must be called from a promise", fp->name);
        return FnFailure();
    }

    bool mapdatamode = (strcmp(fp->name, "mapdata") == 0);
    Rlist *returnlist = NULL;

    // This is a delayed evaluation function, so we have to resolve arguments ourselves
    // We resolve them once now, to get the second or third argument with the iteration data
    Rlist *expargs = NewExpArgs(ctx, policy, fp, NULL);

    Rlist *varpointer = NULL;
    const char* conversion = NULL;

    if (mapdatamode)
    {
        if (expargs == NULL || RlistIsUnresolved(expargs->next->next))
        {
            RlistDestroy(expargs);
            return FnFailure();
        }

        conversion = RlistScalarValue(expargs);
        varpointer = expargs->next->next;
    }
    else
    {
        if (expargs == NULL || RlistIsUnresolved(expargs->next))
        {
            RlistDestroy(expargs);
            return FnFailure();
        }

        conversion = "none";
        varpointer = expargs->next;
    }

    const char* varname = RlistScalarValueSafe(varpointer);

    bool jsonmode      = (strcmp(conversion, "json")      == 0);
    bool canonifymode  = (strcmp(conversion, "canonify")  == 0);
    bool json_pipemode = (strcmp(conversion, "json_pipe") == 0);

    bool allocated = false;
    JsonElement *container = VarNameOrInlineToJson(ctx, fp, varpointer, false, &allocated);

    if (container == NULL)
    {
        RlistDestroy(expargs);
        return FnFailure();
    }

    if (JsonGetElementType(container) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' got an unexpected non-container from argument '%s'", fp->name, varname);
        JsonDestroyMaybe(container, allocated);

        RlistDestroy(expargs);
        return FnFailure();
    }

    if (mapdatamode && json_pipemode)
    {
        JsonElement *returnjson_pipe = ExecJSON_Pipe(RlistScalarValue(expargs->next), container);

        RlistDestroy(expargs);

        if (returnjson_pipe == NULL)
        {
            Log(LOG_LEVEL_ERR, "Function %s failed to get output from 'json_pipe' execution", fp->name);
            return FnFailure();
        }

        JsonDestroyMaybe(container, allocated);

        return FnReturnContainerNoCopy(returnjson_pipe);
    }

    Buffer *expbuf = BufferNew();
    if (JsonGetContainerType(container) != JSON_CONTAINER_TYPE_OBJECT)
    {
        JsonElement *temp = JsonObjectCreate(0);
        JsonElement *temp2 = JsonMerge(temp, container);
        JsonDestroy(temp);
        JsonDestroyMaybe(container, allocated);

        container = temp2;
        allocated = true;
    }

    JsonIterator iter = JsonIteratorInit(container);
    const JsonElement *e;

    while ((e = JsonIteratorNextValue(&iter)) != NULL)
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

            // This is a delayed evaluation function, so we have to resolve arguments ourselves
            // We resolve them every time now, to get the arg_map argument
            Rlist *local_expargs = NewExpArgs(ctx, policy, fp, NULL);
            const char *arg_map = RlistScalarValueSafe(mapdatamode ? local_expargs->next : local_expargs);
            ExpandScalar(ctx, PromiseGetBundle(fp->caller)->ns, PromiseGetBundle(fp->caller)->name, arg_map, expbuf);
            RlistDestroy(local_expargs);

            if (strstr(BufferData(expbuf), "$(this.k)") || strstr(BufferData(expbuf), "${this.k}") ||
                strstr(BufferData(expbuf), "$(this.v)") || strstr(BufferData(expbuf), "${this.v}"))
            {
                RlistDestroy(returnlist);
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
                BufferDestroy(expbuf);
                JsonDestroyMaybe(container, allocated);
                RlistDestroy(expargs);
                return FnFailure();
            }

            if (canonifymode)
            {
                BufferCanonify(expbuf);
            }

            RlistAppendScalar(&returnlist, BufferData(expbuf));
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");

            break;

        case JSON_ELEMENT_TYPE_CONTAINER:
        {
            const JsonElement *e2;
            JsonIterator iter2 = JsonIteratorInit(e);
            while ((e2 = JsonIteratorNextValueByType(&iter2, JSON_ELEMENT_TYPE_PRIMITIVE, true)) != NULL)
            {
                char *key = (char*) JsonGetPropertyAsString(e2);
                bool havekey = (key != NULL);
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

                // This is a delayed evaluation function, so we have to resolve arguments ourselves
                // We resolve them every time now, to get the arg_map argument
                Rlist *local_expargs = NewExpArgs(ctx, policy, fp, NULL);
                const char *arg_map = RlistScalarValueSafe(mapdatamode ? local_expargs->next : local_expargs);
                ExpandScalar(ctx, PromiseGetBundle(fp->caller)->ns, PromiseGetBundle(fp->caller)->name, arg_map, expbuf);
                RlistDestroy(local_expargs);

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
                    JsonDestroyMaybe(container, allocated);
                    RlistDestroy(expargs);
                    return FnFailure();
                }

                if (canonifymode)
                {
                    BufferCanonify(expbuf);
                }

                RlistAppendScalarIdemp(&returnlist, BufferData(expbuf));
                if (havekey)
                {
                    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k[1]");
                }
                EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "v");
            }
        }
        break;

        default:
            break;
        }
        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "k");
    }

    BufferDestroy(expbuf);
    JsonDestroyMaybe(container, allocated);
    RlistDestroy(expargs);

    JsonElement *returnjson = NULL;

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
                if (JsonParseWithLookup(ctx, &LookupVarRefToJson, &data, &parsed) == JSON_PARSE_OK)
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
        return FnReturnContainerNoCopy(returnjson);
    }

    // this is maparray()
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallMapList(EvalContext *ctx,
                                  const Policy *policy,
                                  const FnCall *fp,
                                  ARG_UNUSED const Rlist *finalargs)
{
    // This is a delayed evaluation function, so we have to resolve arguments ourselves
    // We resolve them once now, to get the second argument
    Rlist *expargs = NewExpArgs(ctx, policy, fp, NULL);

    if (expargs == NULL || RlistIsUnresolved(expargs->next))
    {
        RlistDestroy(expargs);
        return FnFailure();
    }

    const char *name_str = RlistScalarValueSafe(expargs->next);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, expargs->next, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        RlistDestroy(expargs);
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        RlistDestroy(expargs);
        return FnFailure();
    }

    Rlist *newlist = NULL;
    Buffer *expbuf = BufferNew();
    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *e;
    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        const char* value = JsonPrimitiveGetAsString(e);

        BufferClear(expbuf);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "this", value, CF_DATA_TYPE_STRING, "source=function,function=maplist");

        // This is a delayed evaluation function, so we have to resolve arguments ourselves
        // We resolve them every time now, to get the first argument
        Rlist *local_expargs = NewExpArgs(ctx, policy, fp, NULL);
        const char *arg_map = RlistScalarValueSafe(local_expargs);
        ExpandScalar(ctx, NULL, "this", arg_map, expbuf);
        RlistDestroy(local_expargs);

        if (strstr(BufferData(expbuf), "$(this)") || strstr(BufferData(expbuf), "${this}"))
        {
            RlistDestroy(newlist);
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "this");
            BufferDestroy(expbuf);
            JsonDestroyMaybe(json, allocated);
            RlistDestroy(expargs);
            return FnFailure();
        }

        RlistAppendScalar(&newlist, BufferData(expbuf));
        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "this");
    }
    BufferDestroy(expbuf);
    JsonDestroyMaybe(json, allocated);
    RlistDestroy(expargs);

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

    if (step_size < 1 || abs(from-to) < step_size)
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
    free(work);

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
        if (args->val.type != RVAL_TYPE_SCALAR &&
            args->val.type != RVAL_TYPE_CONTAINER)
        {
            Log(LOG_LEVEL_ERR, "%s: argument is not a variable reference", fp->name);
            return FnFailure();
        }
    }

    Seq *containers = SeqNew(10, &JsonDestroy);

    for (const Rlist *arg = args; arg; arg = arg->next)
    {
        // try to load directly
        bool allocated = false;
        JsonElement *json = VarNameOrInlineToJson(ctx, fp, arg, false, &allocated);

        // we failed to produce a valid JsonElement, so give up
        if (json == NULL)
        {
            SeqDestroy(containers);

            return FnFailure();
        }

        // Fail on json primitives, only merge containers
        if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
        {
            if (allocated)
            {
                JsonDestroy(json);
            }
            char *const as_string = RvalToString(arg->val);
            Log(LOG_LEVEL_ERR, "%s is not mergeable as it it not a container", as_string);
            free(as_string);
            SeqDestroy(containers);
            return FnFailure();
        }

        // This can be optimized better
        if (allocated)
        {
            SeqAppend(containers, json);
        }
        else
        {
            SeqAppend(containers, JsonCopy(json));
        }

    } // end of args loop

    if (SeqLength(containers) == 1)
    {
        JsonElement *first = JsonCopy(SeqAt(containers, 0));
        SeqDestroy(containers);
        return FnReturnContainerNoCopy(first);
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
        return FnReturnContainerNoCopy(result);
    }

    assert(false);
}

JsonElement *DefaultTemplateData(const EvalContext *ctx, const char *wantbundle)
{
    JsonElement *hash = JsonObjectCreate(30);
    JsonElement *classes = NULL;
    JsonElement *bundles = NULL;

    bool want_all_bundles = (wantbundle == NULL);

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
            const VarRef *var_ref = VariableGetRef(var);
            // TODO: need to get a CallRef, this is bad
            char *scope_key = ClassRefToString(var_ref->ns, var_ref->scope);

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
            else if (StringEqual(scope_key, wantbundle))
            {
                scope_obj = hash;
            }

            free(scope_key);

            if (scope_obj != NULL)
            {
                char *lval_key = VarRefToString(var_ref, false);
                Rval var_rval = VariableGetRval(var, true);
                // don't collect mangled refs
                if (strchr(lval_key, CF_MANGLED_SCOPE) == NULL)
                {
                    JsonObjectAppendElement(scope_obj, lval_key, RvalToJson(var_rval));
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
    return FnReturnContainerNoCopy(state);
}

static FnCallResult FnCallBundlestate(EvalContext *ctx,
                                      ARG_UNUSED const Policy *policy,
                                      ARG_UNUSED const FnCall *fp,
                                      ARG_UNUSED const Rlist *args)
{
    JsonElement *state = DefaultTemplateData(ctx, RlistScalarValue(args));

    if (state == NULL ||
        JsonGetElementType(state) != JSON_ELEMENT_TYPE_CONTAINER ||
        JsonLength(state) < 1)
    {
        if (state != NULL)
        {
            JsonDestroy(state);
        }

        return FnFailure();
    }
    else
    {
        return FnReturnContainerNoCopy(state);
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
    if (value_type == CF_DATA_TYPE_NONE)
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
        BundleSection *sp = BundleAppendSection(bp, "select_server");

        BundleSectionAppendPromise(sp, "function", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, NULL, NULL);
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

                if (n_read >= 0)
                {
                    /* maxbytes was checked earlier, but just make sure... */
                    assert((size_t) n_read < sizeof(recvbuf));
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

    const char *name_str = RlistScalarValueSafe(finalargs);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    Seq *seq = SeqNew(100, NULL);
    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *e;
    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        SeqAppend(seq, (void*)JsonPrimitiveGetAsString(e));
    }

    SeqShuffle(seq, StringHash(seed_str, 0));

    Rlist *shuffled = NULL;
    for (size_t i = 0; i < SeqLength(seq); i++)
    {
        RlistPrepend(&shuffled, SeqAt(seq, i), RVAL_TYPE_SCALAR);
    }

    SeqDestroy(seq);
    JsonDestroyMaybe(json, allocated);
    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { shuffled, RVAL_TYPE_LIST } };
}

static FnCallResult FnCallInt(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    assert(finalargs != NULL);

    char *str = RlistScalarValueSafe(finalargs);

    double val;
    bool ok = DoubleFromString(str, &val);
    if (!ok)
    {
        // Log from DoubleFromString
        return FnFailure();
    }

    return FnReturnF("%jd", (intmax_t) val);  // Discard decimals
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

static FnCallResult FnCallIsNewerThanTime(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    assert(finalargs != NULL);

    const char *arg_file = RlistScalarValue(finalargs);
    // a comment in `FnCallStrftime`: "this will be a problem on 32-bit systems..."
    const time_t arg_mtime = IntFromString(RlistScalarValue(finalargs->next));

    struct stat file_buf;
    int exit_code = stat(arg_file, &file_buf);
    if (exit_code == -1)
    {
        return FnFailure();
    }

    time_t file_mtime = file_buf.st_mtime;

    bool result = file_mtime > arg_mtime;

    return FnReturnContext(result);
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
        if (StringEqual(fp->name, "filesize"))
        {
            return FnFailure();
        }
        return FnReturnContext(false);
    }

    if (!strcmp(fp->name, "isexecutable"))
    {
        if (S_ISLNK(statbuf.st_mode) && stat(path, &statbuf) == -1)
        {
            // stat on link target failed - probably broken link
            return FnReturnContext(false);
        }
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
    char buffer[CF_BUFSIZE];
    const char *path = RlistScalarValue(finalargs);
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

            Log(LOG_LEVEL_VERBOSE, "%s cycle %d, resolving link: %s", fp->name, cycles+1, path_buffer);

            /* Note we subtract 1 since we may need an extra char for '\0'. */
            ssize_t got = readlink(path_buffer, buffer, CF_BUFSIZE - 1);
            if (got < 0)
            {
                // An error happened.  Empty the buffer (don't keep the last target).
                Log(LOG_LEVEL_ERR, "%s could not readlink '%s'", fp->name, path_buffer);
                path_buffer[0] = '\0';
                break;
            }
            buffer[got] = '\0';             /* readlink() doesn't terminate */

            /* If it is a relative path, then in order to follow it further we
             * need to prepend the directory. */
            if (!IsAbsoluteFileName(buffer) &&
                strcmp(detail, "linktarget") == 0)
            {
                DeleteSlash(path_buffer);
                ChopLastNode(path_buffer);
                AddSlash(path_buffer);
                strlcat(path_buffer, buffer, sizeof(path_buffer));
                /* Use buffer again as a tmp buffer. */
                CompressPath(buffer, sizeof(buffer), path_buffer);
            }

            // We got a good link target into buffer.  Copy it to path_buffer.
            strlcpy(path_buffer, buffer, CF_MAXVARSIZE);

            Log(LOG_LEVEL_VERBOSE, "Link resolved to: %s", path_buffer);

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

        if (!IsAbsoluteFileName(pattern))
        {
            Log(LOG_LEVEL_WARNING,
                "Non-absolute path in findfiles(), skipping: %s",
                pattern);
            continue;
        }

        StringSet *found = GlobFileList(pattern);

        char fname[CF_BUFSIZE];

        StringSetIterator it = StringSetIteratorInit(found);
        const char *element = NULL;
        while ((element = StringSetIteratorNext(&it)))
        {
            // TODO: this truncates the filename and may be removed
            // if Rlist and the core are OK with that possibility
            strlcpy(fname, element, CF_BUFSIZE);
            Log(LOG_LEVEL_VERBOSE, "%s pattern '%s' found match '%s'", fp->name, pattern, fname);
            RlistAppendScalarIdemp(&returnlist, fname);
        }
        StringSetDestroy(found);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFilter(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex or string
                          finalargs->next, // list identifier
                          BooleanFromString(RlistScalarValue(finalargs->next->next)), // match as regex or exactly
                          BooleanFromString(RlistScalarValue(finalargs->next->next->next)), // invert matches
                          IntFromString(RlistScalarValue(finalargs->next->next->next->next))); // max results
}

/*********************************************************************/

static const Rlist *GetListReferenceArgument(const EvalContext *ctx, const FnCall *fp, const char *lval_str, DataType *datatype_out)
{
    VarRef *ref = VarRefParse(lval_str);
    DataType value_type;
    const Rlist *value = EvalContextVariableGet(ctx, ref, &value_type);
    VarRefDestroy(ref);

    /* Error 1: variable not found. */
    if (value_type == CF_DATA_TYPE_NONE)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Could not resolve expected list variable '%s' in function '%s'",
            lval_str, fp->name);
        assert(value == NULL);
    }
    /* Error 2: variable is not a list. */
    else if (DataTypeToRvalType(value_type) != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' expected a list variable,"
            " got variable of type '%s'",
            fp->name, DataTypeToString(value_type));

        value      = NULL;
        value_type = CF_DATA_TYPE_NONE;
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
                                   const Rlist* rp,
                                   bool do_regex,
                                   bool invert,
                                   long max)
{
    Regex *rx = NULL;
    if (do_regex)
    {
        rx = CompileRegex(regex);
        if (!rx)
        {
            return FnFailure();
        }
    }

    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, rp, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        RegexDestroy(rx);
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, RlistScalarValueSafe(rp));
        JsonDestroyMaybe(json, allocated);
        RegexDestroy(rx);
        return FnFailure();
    }

    Rlist *returnlist = NULL;

    long match_count = 0;
    long total = 0;

    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *el = NULL;
    while ((el = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)) &&
           match_count < max)
    {
        char *val = JsonPrimitiveToString(el);
        if (val != NULL)
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

                if (strcmp(fp->name, "some")     == 0 ||
                    strcmp(fp->name, "regarray") == 0)
                {
                    free(val);
                    break;
                }
            }
            else if (strcmp(fp->name, "every") == 0)
            {
                total++;
                free(val);
                break;
            }

            total++;
            free(val);
        }
    }

    JsonDestroyMaybe(json, allocated);

    if (rx)
    {
        RegexDestroy(rx);
    }

    bool contextmode = false;
    bool ret = false;
    if (strcmp(fp->name, "every") == 0)
    {
        contextmode = true;
        ret = (match_count == total && total > 0);
    }
    else if (strcmp(fp->name, "none") == 0)
    {
        contextmode = true;
        ret = (match_count == 0);
    }
    else if (strcmp(fp->name, "some")     == 0 ||
             strcmp(fp->name, "regarray") == 0 ||
             strcmp(fp->name, "reglist")  == 0)
    {
        contextmode = true;
        ret = (match_count > 0);
    }
    else if (strcmp(fp->name, "grep")   != 0 &&
             strcmp(fp->name, "filter") != 0)
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
    const char *name_str = RlistScalarValueSafe(finalargs);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    bool head = (strcmp(RlistScalarValue(finalargs->next), "head") == 0); // heads or tails
    long max = IntFromString(RlistScalarValue(finalargs->next->next)); // max results

    Rlist *input_list = NULL;
    Rlist *returnlist = NULL;

    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *e;
    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        RlistAppendScalar(&input_list, JsonPrimitiveGetAsString(e));
    }

    JsonDestroyMaybe(json, allocated);

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

    RlistDestroy(input_list);
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
}

/*********************************************************************/

// TODO: This monstrosity needs refactoring
static FnCallResult FnCallSetop(EvalContext *ctx,
                                ARG_UNUSED const Policy *policy,
                                const FnCall *fp, const Rlist *finalargs)
{
    bool difference_mode = (strcmp(fp->name, "difference") == 0);
    bool unique_mode = (strcmp(fp->name, "unique") == 0);

    const char *name_str = RlistScalarValueSafe(finalargs);
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    JsonElement *json_b = NULL;
    bool allocated_b = false;
    if (!unique_mode)
    {
        const char *name_str_b = RlistScalarValueSafe(finalargs->next);
        json_b = VarNameOrInlineToJson(ctx, fp, finalargs->next, false, &allocated_b);

        // we failed to produce a valid JsonElement, so give up
        if (json_b == NULL)
        {
            JsonDestroyMaybe(json, allocated);
            return FnFailure();
        }
        else if (JsonGetElementType(json_b) != JSON_ELEMENT_TYPE_CONTAINER)
        {
            Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
                fp->name, name_str_b);
            JsonDestroyMaybe(json, allocated);
            JsonDestroyMaybe(json_b, allocated_b);
            return FnFailure();
        }
    }

    StringSet *set_b = StringSetNew();
    if (!unique_mode)
    {
        JsonIterator iter = JsonIteratorInit(json_b);
        const JsonElement *e;
        while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
        {
            StringSetAdd(set_b, xstrdup(JsonPrimitiveGetAsString(e)));
        }
    }

    Rlist *returnlist = NULL;

    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *e;
    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        const char *value = JsonPrimitiveGetAsString(e);

        // Yes, this is an XOR.  But it's more legible this way.
        if (!unique_mode && difference_mode && StringSetContains(set_b, value))
        {
            continue;
        }

        if (!unique_mode && !difference_mode && !StringSetContains(set_b, value))
        {
            continue;
        }

        RlistAppendScalarIdemp(&returnlist, value);
    }

    JsonDestroyMaybe(json, allocated);
    if (json_b != NULL)
    {
        JsonDestroyMaybe(json_b, allocated_b);
    }


    StringSetDestroy(set_b);

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { returnlist, RVAL_TYPE_LIST } };
}

static FnCallResult FnCallLength(EvalContext *ctx,
                                 ARG_UNUSED const Policy *policy,
                                 const FnCall *fp, const Rlist *finalargs)
{
    const char *name_str = RlistScalarValueSafe(finalargs);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    size_t len = JsonLength(json);
    JsonDestroyMaybe(json, allocated);
    return FnReturnF("%zu", len);
}

static FnCallResult FnCallFold(EvalContext *ctx,
                               ARG_UNUSED const Policy *policy,
                               const FnCall *fp, const Rlist *finalargs)
{
    const char *sort_type = finalargs->next ? RlistScalarValue(finalargs->next) : NULL;

    size_t count = 0;
    double product = 1.0; // this could overflow
    double sum = 0; // this could overflow
    double mean = 0;
    double M2 = 0;
    char* min = NULL;
    char* max = NULL;
    bool variance_mode = strcmp(fp->name, "variance") == 0;
    bool mean_mode = strcmp(fp->name, "mean") == 0;
    bool max_mode = strcmp(fp->name, "max") == 0;
    bool min_mode = strcmp(fp->name, "min") == 0;
    bool sum_mode = strcmp(fp->name, "sum") == 0;
    bool product_mode = strcmp(fp->name, "product") == 0;

    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    if (!json)
    {
        return FnFailure();
    }

    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *el;
    while ((el = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        char *value = JsonPrimitiveToString(el);

        if (value != NULL)
        {
            if (sort_type)
            {
                if (min_mode && (min == NULL || !GenericStringItemLess(sort_type, min, value)))
                {
                    free(min);
                    min = xstrdup(value);
                }

                if (max_mode && (max == NULL || GenericStringItemLess(sort_type, max, value)))
                {
                    free(max);
                    max = xstrdup(value);
                }
            }

            count++;

            if (mean_mode || variance_mode || sum_mode || product_mode)
            {
                double x;
                if (sscanf(value, "%lf", &x) != 1)
                {
                    x = 0; /* treat non-numeric entries as zero */
                }

                // Welford's algorithm
                double delta = x - mean;
                mean += delta/count;
                M2 += delta * (x - mean);
                sum += x;
                product *= x;
            }

            free(value);
        }
    }

    JsonDestroyMaybe(json, allocated);

    if (mean_mode)
    {
        return count == 0 ? FnFailure() : FnReturnF("%lf", mean);
    }
    else if (sum_mode)
    {
        return FnReturnF("%lf", sum);
    }
    else if (product_mode)
    {
        return FnReturnF("%lf", product);
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
        return max == NULL ? FnFailure() : FnReturnNoCopy(max);
    }
    else if (min_mode)
    {
        return min == NULL ? FnFailure() : FnReturnNoCopy(min);
    }

    // else, we don't know this fp->name
    ProgrammingError("Unknown function call %s to FnCallFold", fp->name);
    return FnFailure();
}

/*********************************************************************/

static char *DataTypeStringFromVarName(EvalContext *ctx, const char *var_name, bool detail)
{
    assert(var_name != NULL);

    VarRef *const var_ref = VarRefParse(var_name);
    DataType type;
    const void *value = EvalContextVariableGet(ctx, var_ref, &type);
    VarRefDestroy(var_ref);

    const char *const type_str =
        (type == CF_DATA_TYPE_NONE) ? "none" : DataTypeToString(type);

    if (!detail)
    {
        return SafeStringDuplicate(type_str);
    }

    if (type == CF_DATA_TYPE_CONTAINER)
    {
        const char *subtype_str;
        const JsonElement *const element = value;

        switch (JsonGetType(element))
        {
        case JSON_CONTAINER_TYPE_OBJECT:
            subtype_str = "object";
            break;
        case JSON_CONTAINER_TYPE_ARRAY:
            subtype_str = "array";
            break;
        case JSON_PRIMITIVE_TYPE_STRING:
            subtype_str = "string";
            break;
        case JSON_PRIMITIVE_TYPE_INTEGER:
            subtype_str = "int";
            break;
        case JSON_PRIMITIVE_TYPE_REAL:
            subtype_str = "real";
            break;
        case JSON_PRIMITIVE_TYPE_BOOL:
            subtype_str = "boolean";
            break;
        case JSON_PRIMITIVE_TYPE_NULL:
            subtype_str = "null";
            break;
        default:
            return NULL;
        }

        return StringConcatenate(3, type_str, " ", subtype_str);
    }
    return StringConcatenate(2, "policy ", type_str);
}

static FnCallResult FnCallDatatype(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);
    assert(fp->name != NULL);

    if (finalargs == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Function %s requires variable identifier as first argument",
            fp->name);
        return FnFailure();
    }
    const char *const var_name = RlistScalarValue(finalargs);

    /* detail argument defaults to false */
    bool detail = false;
    if (finalargs->next != NULL)
    {
        detail = BooleanFromString(RlistScalarValue(finalargs->next));
    }
    char *const output_string = DataTypeStringFromVarName(ctx, var_name, detail);

    if (output_string == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s could not parse var type",
            fp->name);
        return FnFailure();
    }

    return FnReturnNoCopy(output_string);
}

/*********************************************************************/

static FnCallResult FnCallIsDatatype(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);
    assert(fp->name != NULL);

    // check args
    const Rlist *const var_arg = finalargs;
    if (var_arg == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires a variable as first argument",
            fp->name);
        return FnFailure();
    }

    assert(finalargs != NULL); // assumes finalargs is already checked by var_arg
    const Rlist *const type_arg = finalargs->next;
    if (type_arg == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires a type as second argument",
            fp->name);
        return FnFailure();
    }

    const char *const var_name = RlistScalarValue(var_arg);
    const char *const type_name = RlistScalarValue(type_arg);
    bool detail = StringContainsChar(type_name, ' ');

    char *const type_string = DataTypeStringFromVarName(ctx, var_name, detail);

    if (type_string == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s could not determine type of the variable '%s'",
            fp->name, var_name);
        return FnFailure();
    }
    const bool matching = StringEqual(type_name, type_string);
    free(type_string);

    return FnReturnContext(matching);
}

/*********************************************************************/

static FnCallResult FnCallNth(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char* const key = RlistScalarValue(finalargs->next);

    const char *name_str = RlistScalarValueSafe(finalargs);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    const char *jstring = NULL;
    FnCallResult result;
    if (JsonGetElementType(json) == JSON_ELEMENT_TYPE_CONTAINER)
    {
        JsonContainerType ct = JsonGetContainerType(json);
        JsonElement* jelement = NULL;

        if (JSON_CONTAINER_TYPE_OBJECT == ct)
        {
            jelement = JsonObjectGet(json, key);
        }
        else if (JSON_CONTAINER_TYPE_ARRAY == ct)
        {
            long index = IntFromString(key);
            if (index < 0)
            {
                index += JsonLength(json);
            }

            if (index >= 0 && index < (long) JsonLength(json))
            {
                jelement = JsonAt(json, index);
            }
        }
        else
        {
            ProgrammingError("JSON Container is neither array nor object but type %d", (int) ct);
        }

        if (jelement != NULL &&
            JsonGetElementType(jelement) == JSON_ELEMENT_TYPE_PRIMITIVE)
        {
            jstring = JsonPrimitiveGetAsString(jelement);
            if (jstring != NULL)
            {
                result = FnReturn(jstring);
            }
        }
    }

    JsonDestroyMaybe(json, allocated);

    if (jstring == NULL)
    {
        return FnFailure();
    }

    return result;
}

/*********************************************************************/

static FnCallResult FnCallEverySomeNone(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    return FilterInternal(ctx,
                          fp,
                          RlistScalarValue(finalargs), // regex or string
                          finalargs->next, // list identifier
                          1,
                          0,
                          LONG_MAX);
}

static FnCallResult FnCallSort(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (finalargs == NULL)
    {
        FatalError(ctx, "in built-in FnCall %s: missing first argument, a list name", fp->name);
    }

    const char *sort_type = NULL;

    if (finalargs->next)
    {
        sort_type = RlistScalarValue(finalargs->next); // sort mode
    }
    else
    {
        sort_type = "lex";
    }

    const char *name_str = RlistScalarValueSafe(finalargs);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    Rlist *sorted = NULL;
    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *e;
    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        RlistAppendScalar(&sorted, JsonPrimitiveGetAsString(e));
    }
    JsonDestroyMaybe(json, allocated);

    if (strcmp(sort_type, "int") == 0)
    {
        sorted = IntSortRListNames(sorted);
    }
    else if (strcmp(sort_type, "real") == 0)
    {
        sorted = RealSortRListNames(sorted);
    }
    else if (strcmp(sort_type, "IP") == 0 || strcmp(sort_type, "ip") == 0)
    {
        sorted = IPSortRListNames(sorted);
    }
    else if (strcmp(sort_type, "MAC") == 0 || strcmp(sort_type, "mac") == 0)
    {
        sorted = MACSortRListNames(sorted);
    }
    else // "lex"
    {
        sorted = AlphaSortRListNames(sorted);
    }

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { sorted, RVAL_TYPE_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFormat(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char *const id = "built-in FnCall format-arg";

    /* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (const Rlist *arg = finalargs; arg; arg = arg->next)
    {
        SyntaxTypeMatch err = CheckConstraintTypeMatch(id, arg->val, CF_DATA_TYPE_STRING, "", 1);
        if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
        {
            FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
        }
    }

    if (finalargs == NULL)
    {
        return FnFailure();
    }

    char *format = RlistScalarValue(finalargs);

    if (format == NULL)
    {
        return FnFailure();
    }

    const Rlist *rp = finalargs->next;

    char *check = strchr(format, '%');
    char check_buffer[CF_BUFSIZE];
    Buffer *buf = BufferNew();

    if (check != NULL)
    {
        BufferAppend(buf, format, check - format);
        Seq *s;

        while (check != NULL &&
               (s = StringMatchCaptures("^(%%|%[^diouxXeEfFgGaAcsCSpnm%]*?[diouxXeEfFgGaAcsCSpnm])([^%]*)(.*)$", check, false)))
        {
            {
                if (SeqLength(s) >= 2)
                {
                    const char *format_piece = BufferData(SeqAt(s, 1));
                    bool percent = StringEqualN(format_piece, "%%", 2);
                    char *data = NULL;

                    if (percent)
                    {
                        // "%%" in format string
                    }
                    else if (rp != NULL)
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

                    const char bad_modifiers[] = "hLqjzt";
                    const size_t length = strlen(bad_modifiers);
                    for (size_t b = 0; b < length; b++)
                    {
                        if (strchr(format_piece, bad_modifiers[b]) != NULL)
                        {
                            Log(LOG_LEVEL_ERR, "format() does not allow modifier character '%c' in format specifier '%s'.",
                                  bad_modifiers[b],
                                  format_piece);
                            BufferDestroy(buf);
                            SeqDestroy(s);
                            return FnFailure();
                        }
                    }

                    if (strrchr(format_piece, 'd') != NULL || strrchr(format_piece, 'o') != NULL || strrchr(format_piece, 'x') != NULL)
                    {
                        long x = 0;
                        sscanf(data, "%ld", &x);
                        snprintf(piece, CF_BUFSIZE, format_piece, x);
                        BufferAppend(buf, piece, strlen(piece));
                    }
                    else if (percent)
                    {
                        // "%%" -> "%"
                        BufferAppend(buf, "%", 1);
                    }
                    else if (strrchr(format_piece, 'f') != NULL)
                    {
                        double x = 0;
                        sscanf(data, "%lf", &x);
                        snprintf(piece, CF_BUFSIZE, format_piece, x);
                        BufferAppend(buf, piece, strlen(piece));
                    }
                    else if (strrchr(format_piece, 's') != NULL)
                    {
                        BufferAppendF(buf, format_piece, data);
                    }
                    else if (strrchr(format_piece, 'S') != NULL)
                    {
                        char *found_format_spec = NULL;
                        char format_rewrite[CF_BUFSIZE];

                        strlcpy(format_rewrite, format_piece, CF_BUFSIZE);
                        found_format_spec = strrchr(format_rewrite, 'S');

                        if (found_format_spec != NULL)
                        {
                            *found_format_spec = 's';
                        }
                        else
                        {
                            ProgrammingError("Couldn't find the expected S format spec in %s", format_piece);
                        }

                        const char* const varname = data;
                        VarRef *ref = VarRefParse(varname);
                        DataType type;
                        const void *value = EvalContextVariableGet(ctx, ref, &type);
                        VarRefDestroy(ref);

                        if (type == CF_DATA_TYPE_CONTAINER)
                        {
                            Writer *w = StringWriter();
                            JsonWriteCompact(w, value);
                            BufferAppendF(buf, format_rewrite, StringWriterData(w));
                            WriterClose(w);
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
                                    WriterWriteF(w, "\"%s\"", escaped);
                                    free(escaped);

                                    if (rp != NULL && rp->next != NULL)
                                    {
                                        WriterWrite(w, ", ");
                                    }
                                }
                                WriterWrite(w, " }");

                                BufferAppendF(buf, format_rewrite, StringWriterData(w));
                                WriterClose(w);
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

static FnCallResult FnCallIPRange(EvalContext *ctx, ARG_UNUSED const Policy *policy,
                                  const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);

    if (finalargs == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' requires at least one argument", fp->name);
        return FnFailure();
    }

    const char *range   = RlistScalarValue(finalargs);
    const Rlist *ifaces = finalargs->next;

    if (!FuzzyMatchParse(range))
    {
        Log(LOG_LEVEL_VERBOSE,
            "%s(%s): argument is not a valid address range",
            fp->name, range);
        return FnFailure();
    }

    for (const Item *ip = EvalContextGetIpAddresses(ctx);
         ip != NULL;
         ip = ip->next)
    {
        if (FuzzySetMatch(range, ip->name) == 0)
        {
            /*
             * MODE1: iprange(range)
             *        Match range on the address of any interface.
             */
            if (ifaces == NULL)
            {
                Log(LOG_LEVEL_DEBUG, "%s(%s): Match on IP '%s'",
                    fp->name, range, ip->name);
                return FnReturnContext(true);
            }
            /*
             * MODE2: iprange(range, args...)
             *        Match range only on the addresses of args interfaces.
             */
            else
            {
                for (const Rlist *i = ifaces; i != NULL; i = i->next)
                {
                    char *iface = xstrdup(RlistScalarValue(i));
                    CanonifyNameInPlace(iface);

                    const char *ip_iface = ip->classes;

                    if (ip_iface != NULL &&
                        strcmp(iface, ip_iface) == 0)
                    {
                        Log(LOG_LEVEL_DEBUG,
                            "%s(%s): Match on IP '%s' interface '%s'",
                            fp->name, range, ip->name, ip->classes);

                        free(iface);
                        return FnReturnContext(true);
                    }
                    free(iface);
                }
            }
        }
    }

    Log(LOG_LEVEL_DEBUG, "%s(%s): no match", fp->name, range);
    return FnReturnContext(false);
}

static FnCallResult FnCallIsIpInSubnet(ARG_UNUSED EvalContext *ctx,
                                       ARG_UNUSED const Policy *policy,
                                       const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);

    if (finalargs == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' requires at least one argument", fp->name);
        return FnFailure();
    }

    const char *range = RlistScalarValue(finalargs);
    const Rlist *ips  = finalargs->next;

    if (!FuzzyMatchParse(range))
    {
        Log(LOG_LEVEL_VERBOSE,
            "%s(%s): argument is not a valid address range",
            fp->name, range);
        return FnFailure();
    }

    for (const Rlist *ip = ips; ip != NULL; ip = ip->next)
    {
        const char *ip_s = RlistScalarValue(ip);

        if (FuzzySetMatch(range, ip_s) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "%s(%s): Match on IP '%s'",
                fp->name, range, ip_s);
            return FnReturnContext(true);
        }
    }

    Log(LOG_LEVEL_DEBUG, "%s(%s): no match", fp->name, range);
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
        DataType value_type;
        EvalContextVariableGet(ctx, ref, &value_type);
        if (value_type != CF_DATA_TYPE_NONE)
        {
            found = true;
        }
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

    const char *const key = RlistScalarValue(finalargs);
    const char *const value = RlistScalarValue(finalargs->next);
    if (GetRegistryValue(key, value, buffer, sizeof(buffer)))
    {
        return FnReturn(buffer);
    }

    Log(LOG_LEVEL_ERR, "Could not read existing registry data for key '%s' and value '%s'.",
        key, value);
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
            if (RetrieveUnreliableValue("remotescalar", handle, buffer) == 0)
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
        char *ret = GetRemoteScalar(ctx, "VAR", handle, PolicyServerGetIP(),
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

static FnCallResult FnCallRegReplace(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    const char *data = RlistScalarValue(finalargs);
    const char *regex = RlistScalarValue(finalargs->next);
    const char *replacement = RlistScalarValue(finalargs->next->next);
    const char *options = RlistScalarValue(finalargs->next->next->next);

    Buffer *rewrite = BufferNewFrom(data, strlen(data));
    const char* error = BufferSearchAndReplace(rewrite, regex, replacement, options);

    if (error)
    {
        BufferDestroy(rewrite);
        Log(LOG_LEVEL_ERR, "%s: couldn't use regex '%s', replacement '%s', and options '%s': error=%s",
            fp->name, regex, replacement, options, error);
        return FnFailure();
    }

    return FnReturnBuffer(rewrite);
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

    for (size_t i = 0; i < SeqLength(s); i+=2)
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
        return FnReturnContainerNoCopy(json);
    }
    else
    {
        return FnReturnContext(true);
    }
}

/*********************************************************************/

static FnCallResult FnCallRegLine(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    Regex *rx = CompileRegex(RlistScalarValue(finalargs));
    if (!rx)
    {
        return FnFailure();
    }

    const char *arg_filename = RlistScalarValue(finalargs->next);

    FILE *fin = safe_fopen(arg_filename, "rt");
    if (!fin)
    {
        RegexDestroy(rx);
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
            RegexDestroy(rx);
            return FnReturnContext(true);
        }
    }

    RegexDestroy(rx);
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
    bool rising = (strcmp(fp->name, "isgreaterthan") == 0);

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
    const char *name_str = RlistScalarValueSafe(finalargs);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    Rlist *returnlist = NULL;

    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *e;
    while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
    {
        RlistPrepend(&returnlist, JsonPrimitiveGetAsString(e), RVAL_TYPE_SCALAR);
    }
    JsonDestroyMaybe(json, allocated);

    return (FnCallResult) { FNCALL_SUCCESS, (Rval) { returnlist, RVAL_TYPE_LIST } };
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

    intmax_t cftime = 0;
    cftime = 0;
    cftime += tmv.tm_sec;
    cftime += (intmax_t) tmv.tm_min * 60;
    cftime += (intmax_t) tmv.tm_hour * 3600;
    cftime += (intmax_t) (tmv.tm_mday - 1) * 24 * 3600;
    cftime += (intmax_t) tmv.tm_mon * 30 * 24 * 3600;
    cftime += (intmax_t) (tmv.tm_year + 1900) * 365 * 24 * 3600;

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

    struct tm tm_value;
    struct tm *tm_pointer;

    if (strcmp("gmtime", mode) == 0)
    {
        tm_pointer = gmtime_r(&when, &tm_value);
    }
    else
    {
        tm_pointer = localtime_r(&when, &tm_value);
    }

    char buffer[CF_BUFSIZE];
    if (tm_pointer == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "Function %s, the given time stamp '%ld' was invalid. (strftime: %s)",
            fp->name, when, GetErrorStr());
    }
    else if (PortablyFormatTime(buffer, sizeof(buffer),
                                format_string, when, tm_pointer))
    {
        return FnReturn(buffer);
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallEval(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    if (finalargs == NULL)
    {
        FatalError(ctx, "in built-in FnCall %s: missing first argument, an evaluation input", fp->name);
    }

    const char *input =  RlistScalarValue(finalargs);

    const char *type = NULL;

    if (finalargs->next)
    {
        type = RlistScalarValue(finalargs->next);
    }
    else
    {
        type = "math";
    }

    /* Third argument can currently only be "infix". */
    // So we completely ignore finalargs->next->next

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
    if (finalargs == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function 'readfile' requires at least one argument");
        return FnFailure();
    }

    char *filename = RlistScalarValue(finalargs);
    const Rlist *next = finalargs->next; // max_size argument, default to inf:
    long maxsize = next ? IntFromString(RlistScalarValue(next)) : IntFromString("inf");

    if (maxsize == CF_INFINITY)                      /* "inf" in the policy */
    {
        maxsize = 0;
    }

    if (maxsize < 0)
    {
        Log(LOG_LEVEL_ERR, "%s: requested max size %li is less than 0", fp->name, maxsize);
        return FnFailure();
    }

    // Read once to validate structure of file in itemlist
    char *contents = CfReadFile(filename, maxsize);
    if (contents)
    {
        return FnReturnNoCopy(contents);
    }

    Log(LOG_LEVEL_VERBOSE, "Function '%s' failed to read file: %s",
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
        Log(LOG_LEVEL_VERBOSE, "Function '%s' failed to read file: %s",
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

    if (noerrors)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newlist, RVAL_TYPE_LIST } };
    }

    RlistDestroy(newlist);
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

static FnCallResult ReadDataGeneric(const char *const fname,
                                     const char *const input_path,
                                     const size_t size_max,
                                     const DataFileType requested_mode)
{
    assert(fname != NULL);
    assert(input_path != NULL);

    JsonElement *json = JsonReadDataFile(fname, input_path, requested_mode, size_max);
    if (json == NULL)
    {
        return FnFailure();
    }

    return FnReturnContainerNoCopy(json);
}

static FnCallResult FnCallReadData(ARG_UNUSED EvalContext *ctx,
                                   ARG_UNUSED const Policy *policy,
                                   const FnCall *fp,
                                   const Rlist *args)
{
    assert(fp != NULL);
    if (args == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' requires at least one argument", fp->name);
        return FnFailure();
    }

    const char *input_path = RlistScalarValue(args);
    const char *const mode_string = RlistScalarValue(args->next);
    DataFileType requested_mode = DATAFILETYPE_UNKNOWN;
    if (StringEqual("auto", mode_string))
    {
        requested_mode = GetDataFileTypeFromSuffix(input_path);
        Log(LOG_LEVEL_VERBOSE,
            "%s: automatically selected data type %s from filename %s",
            fp->name, DataFileTypeToString(requested_mode), input_path);
    }
    else
    {
        requested_mode = GetDataFileTypeFromString(mode_string);
    }

    return ReadDataGeneric(fp->name, input_path, CF_INFINITY, requested_mode);
}

static FnCallResult ReadGenericDataType(const FnCall *fp,
                                         const Rlist *args,
                                         const DataFileType requested_mode)
{
    assert(fp != NULL);
    if (args == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Function '%s' requires at least one argument",
            fp->name);
        return FnFailure();
    }

    const char *const input_path = RlistScalarValue(args);
    size_t size_max = args->next ?
            IntFromString(RlistScalarValue(args->next)) :
            CF_INFINITY;
    return ReadDataGeneric(fp->name, input_path, size_max, requested_mode);
}

static FnCallResult FnCallReadCsv(ARG_UNUSED EvalContext *ctx,
                                  ARG_UNUSED const Policy *policy,
                                  const FnCall *fp,
                                  const Rlist *args)
{
    return ReadGenericDataType(fp, args, DATAFILETYPE_CSV);
}

static FnCallResult FnCallReadEnvFile(ARG_UNUSED EvalContext *ctx,
                                      ARG_UNUSED const Policy *policy,
                                      const FnCall *fp,
                                      const Rlist *args)
{
    return ReadGenericDataType(fp, args, DATAFILETYPE_ENV);
}

static FnCallResult FnCallReadYaml(ARG_UNUSED EvalContext *ctx,
                                   ARG_UNUSED const Policy *policy,
                                   const FnCall *fp,
                                   const Rlist *args)
{
    return ReadGenericDataType(fp, args, DATAFILETYPE_YAML);
}

static FnCallResult FnCallReadJson(ARG_UNUSED EvalContext *ctx,
                                   ARG_UNUSED const Policy *policy,
                                   const FnCall *fp,
                                   const Rlist *args)
{
    return ReadGenericDataType(fp, args, DATAFILETYPE_JSON);
}

static FnCallResult ValidateDataGeneric(const char *const fname,
                                        const char *data,
                                        const DataFileType requested_mode,
                                        bool strict)
{
    assert(data != NULL);
    if (requested_mode != DATAFILETYPE_JSON)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Data type %s is not supported by this function",
            fname, DataFileTypeToString(requested_mode));
        return FnFailure();
    }

    JsonElement *json = NULL;
    JsonParseError err = JsonParseAll(&data, &json);
    if (err != JSON_PARSE_OK)
    {
        Log(LOG_LEVEL_VERBOSE, "%s: %s", fname, JsonParseErrorToString(err));
    }

    bool isvalid = json != NULL;
    if (strict)
    {
        isvalid = isvalid && JsonGetElementType(json) != JSON_ELEMENT_TYPE_PRIMITIVE;
    }

    FnCallResult ret = FnReturnContext(isvalid);
    JsonDestroy(json);
    return ret;
}

static FnCallResult FnCallValidData(ARG_UNUSED EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    const FnCall *fp,
                                    const Rlist *args)
{
    assert(fp != NULL);
    if (args == NULL || args->next == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' requires two arguments", fp->name);
        return FnFailure();
    }
    bool strict = false;
    if (args->next != NULL)
    {
        strict = BooleanFromString(RlistScalarValue(args->next));
    }

    const char *data = RlistScalarValue(args);
    const char *const mode_string = RlistScalarValue(args->next);
    DataFileType requested_mode = GetDataFileTypeFromString(mode_string);

    return ValidateDataGeneric(fp->name, data, requested_mode, strict);
}

static FnCallResult FnCallValidJson(ARG_UNUSED EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    const FnCall *fp,
                                    const Rlist *args)
{
    assert(fp != NULL);
    if (args == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' requires one argument", fp->name);
        return FnFailure();
    }
    bool strict = false;
    if (args->next != NULL)
    {
        strict = BooleanFromString(RlistScalarValue(args->next));
    }

    const char *data = RlistScalarValue(args);
    return ValidateDataGeneric(fp->name, data, DATAFILETYPE_JSON, strict);
}

static FnCallResult FnCallReadModuleProtocol(
    ARG_UNUSED EvalContext *ctx,
    ARG_UNUSED const Policy *policy,
    const FnCall *fp,
    const Rlist *args)
{
    assert(fp != NULL);

    if (args == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function '%s' requires at least one argument", fp->name);
        return FnFailure();
    }

    const char *input_path = RlistScalarValue(args);

    char module_context[CF_BUFSIZE] = {0};

    FILE *file = safe_fopen(input_path, "rt");
    if (file == NULL)
    {
        return FnReturnContext(false);
    }

    StringSet *module_tags = StringSetNew();
    long persistence = 0;

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    bool success = true;
    for (;;)
    {
        const ssize_t res = CfReadLine(&line, &line_size, file);
        if (res == -1)
        {
            if (!feof(file))
            {
                Log(LOG_LEVEL_ERR, "Unable to read from file '%s'. (fread: %s)", input_path, GetErrorStr());
                success = false;
            }
            break;
        }

        ModuleProtocol(ctx, input_path, line, false, module_context, sizeof(module_context), module_tags, &persistence);
    }

    StringSetDestroy(module_tags);
    free(line);
    fclose(file);

    return FnReturnContext(success);
}

static int JsonPrimitiveComparator(JsonElement const *left_obj,
                                   JsonElement const *right_obj,
                                   void *user_data)
{
    size_t const index = *(size_t *)user_data;

    char const *left = JsonPrimitiveGetAsString(JsonAt(left_obj, index));
    char const *right = JsonPrimitiveGetAsString(JsonAt(right_obj, index));
    return StringSafeCompare(left, right);
}

static bool ClassFilterDataArrayOfArrays(
    EvalContext *ctx,
    const char *fn_name,
    JsonElement *json_array,
    const char *class_expr_index,
    bool *remove)
{
    size_t index;
    assert(SIZE_MAX >= ULONG_MAX); /* make sure returned value can fit in size_t */
    if (StringToUlong(class_expr_index, &index) != 0) {
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Bad class expression index '%s': Failed to parse integer",
            fn_name, class_expr_index);
        return false;
    }

    size_t length = JsonLength(json_array);
    if (index >= length)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Bad class expression index '%s': Index out of bounds (%zu >= %zu)",
            fn_name, class_expr_index, index, length);
        return false;
    }

    JsonElement *json_child = JsonArrayGet(json_array, index);
    if (JsonGetType(json_child) != JSON_TYPE_STRING)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Bad class expression at index '%zu': Expected type string",
            fn_name, index);
        return false;
    }

    const char *class_expr = JsonPrimitiveGetAsString(json_child);
    assert(class_expr != NULL);

    *remove = !IsDefinedClass(ctx, class_expr);
    return true;
}

static bool ClassFilterDataArrayOfObjects(
    EvalContext *ctx,
    const char *fn_name,
    JsonElement *json_object,
    const char *class_expr_key,
    bool *remove)
{
    JsonElement *json_child = JsonObjectGet(json_object, class_expr_key);
    if (json_child == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Bad class expression key '%s': Key not found",
            fn_name, class_expr_key);
        return false;
    }

    if (JsonGetType(json_child) != JSON_TYPE_STRING)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Bad class expression at key '%s': Expected type string",
            fn_name, class_expr_key);
        return false;
    }

    const char *class_expr = JsonPrimitiveGetAsString(json_child);
    assert(class_expr != NULL);

    *remove = !IsDefinedClass(ctx, class_expr);
    return true;
}

static bool ClassFilterDataArray(
    EvalContext *ctx,
    const char *fn_name,
    const char *data_structure,
    const char *key_or_index,
    JsonElement *child,
    bool *remove)
{
    switch (JsonGetType(child))
    {
    case JSON_TYPE_ARRAY:
        if (StringEqual(data_structure, "auto") ||
            StringEqual(data_structure, "array_of_arrays"))
        {
            return ClassFilterDataArrayOfArrays(
                ctx, fn_name, child, key_or_index, remove);
        }
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Expected child element to be of container type array",
            fn_name);
        break;

    case JSON_TYPE_OBJECT:
        if (StringEqual(data_structure, "auto") ||
            StringEqual(data_structure, "array_of_objects"))
        {
            return ClassFilterDataArrayOfObjects(
                ctx, fn_name, child, key_or_index, remove);
        }
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Expected child element to be of container type object",
            fn_name);
        break;

    default:
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Expected child element to be of container type",
            fn_name);
        break;
    }

    return false;
}

static FnCallResult FnCallClassFilterData(
    EvalContext *ctx,
    ARG_UNUSED Policy const *policy,
    FnCall const *fp,
    Rlist const *args)
{
    assert(ctx != NULL);
    assert(fp != NULL);
    assert(args != NULL);
    assert(args->next != NULL);
    assert(args->next->next != NULL);

    bool allocated = false;
    JsonElement *parent = VarNameOrInlineToJson(ctx, fp, args, false, &allocated);
    if (parent == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Function %s(): Expected parent element to be of container type array",
            fp->name);
        return FnFailure();
    }

    /* Currently only parent type array is supported */
    if (JsonGetType(parent) != JSON_TYPE_ARRAY)
    {
        JsonDestroyMaybe(parent, allocated);
        return FnFailure();
    }

    if (!allocated)
    {
        parent = JsonCopy(parent);
        assert(parent != NULL);
    }

    const char *data_structure = RlistScalarValue(args->next);
    const char *key_or_index = RlistScalarValue(args->next->next);

    /* Iterate through array backwards so we can avoid having to compute index
     * offsets for each removed element */
    for (size_t index_plus_one = JsonLength(parent); index_plus_one > 0; index_plus_one--)
    {
        assert(index_plus_one > 0);
        size_t index = index_plus_one - 1;
        JsonElement *child = JsonArrayGet(parent, index);
        assert(child != NULL);

        bool remove;
        if (!ClassFilterDataArray(ctx, fp->name, data_structure, key_or_index, child, &remove))
        {
            /* Error is already logged */
            JsonDestroy(parent);
            return FnFailure();
        }

        if (remove)
        {
            JsonArrayRemoveRange(parent, index, index);
        }
    }

    return FnReturnContainerNoCopy(parent);
}

static FnCallResult FnCallClassFilterCsv(EvalContext *ctx,
                                         ARG_UNUSED Policy const *policy,
                                         FnCall const *fp,
                                         Rlist const *args)
{
    if (args == NULL || args->next == NULL || args->next->next == NULL)
    {
        FatalError(ctx, "Function %s requires at least 3 arguments",
                   fp->name);
    }

    char const *path = RlistScalarValue(args);
    bool const has_heading = BooleanFromString(RlistScalarValue(args->next));
    size_t const class_index = IntFromString(RlistScalarValue(args->next->next));
    Rlist const *sort_arg = args->next->next->next;

    FILE *csv_file = safe_fopen(path, "r");
    if (csv_file == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "%s: Failed to read file %s: %s",
            fp->name, path, GetErrorStrFromCode(errno));
        return FnFailure();
    }

    Seq *heading = NULL;
    JsonElement *json = JsonArrayCreate(50);
    char *line;
    size_t num_columns = 0;

    // Current line number, for debugging
    size_t line_number = 0;

    while ((line = GetCsvLineNext(csv_file)) != NULL)
    {
        ++line_number;
        if (line[0] == '#')
        {
            Log(LOG_LEVEL_DEBUG, "%s: Ignoring comment at line %zu",
                fp->name, line_number);
            free(line);
            continue;
        }

        Seq *list = SeqParseCsvString(line);
        free(line);
        if (list == NULL)
        {
            Log(LOG_LEVEL_WARNING,
                "%s: Failed to parse line %zu, line ignored.",
                fp->name, line_number);
            continue;
        }

        if (SeqLength(list) == 1 &&
            strlen(SeqAt(list, 0)) == 0)
        {
            Log(LOG_LEVEL_DEBUG,
                "%s: Found empty line at line %zu, line ignored",
                fp->name, line_number);
            SeqDestroy(list);
            continue;
        }

        if (num_columns == 0)
        {
            num_columns = SeqLength(list);
            assert(num_columns != 0);

            if (class_index >= num_columns)
            {
                Log(LOG_LEVEL_ERR,
                    "%s: Class expression index is out of bounds. "
                    "Row length %zu, index %zu",
                    fp->name, num_columns, class_index);
                SeqDestroy(list);
                JsonDestroy(json);
                return FnFailure();
            }
        }
        else if (num_columns != SeqLength(list))
        {
            Log(LOG_LEVEL_WARNING,
                "%s: Line %zu has incorrect amount of elements, "
                "%zu instead of %zu. Line ignored.",
                fp->name, line_number, SeqLength(list), num_columns);
            SeqDestroy(list);
            continue;
        }

        // First parsed line is set to be heading if has_heading is true
        if (has_heading && heading == NULL)
        {
            Log(LOG_LEVEL_DEBUG, "%s: Found header at line %zu",
                fp->name, line_number);
            heading = list;
            SeqRemove(heading, class_index);
        }
        else
        {
            if (!IsDefinedClass(ctx, SeqAt(list, class_index)))
            {
                SeqDestroy(list);
                continue;
            }

            SeqRemove(list, class_index);
            JsonElement *class_container = JsonObjectCreate(num_columns);

            size_t const num_fields = SeqLength(list);
            for (size_t i = 0; i < num_fields; i++)
            {
                if (has_heading)
                {
                    JsonObjectAppendString(class_container,
                                           SeqAt(heading, i),
                                           SeqAt(list, i));
                }
                else
                {
                    size_t const key_len = PRINTSIZE(size_t);
                    char key[key_len];
                    xsnprintf(key, key_len, "%zu", i);

                    JsonObjectAppendString(class_container,
                                           key,
                                           SeqAt(list, i));
                }
            }

            JsonArrayAppendObject(json, class_container);
            SeqDestroy(list);
        }
    }

    if (sort_arg != NULL)
    {
        size_t sort_index = IntFromString(RlistScalarValue(sort_arg));
        if (sort_index == class_index)
        {
            Log(LOG_LEVEL_WARNING,
                "%s: sorting column (%zu) is the same as class "
                "expression column (%zu). Not sorting data container.",
                fp->name, sort_index, class_index);
        }
        else if (sort_index >= num_columns)
        {
            Log(LOG_LEVEL_WARNING,
                "%s: sorting index %zu out of bounds. "
                "Not sorting data container.",
                fp->name, sort_index);
        }
        else
        {
            /* The sorting index needs to be decremented if it is higher than
             * the class expression index, since the class column is removed
             * in the data containers. */
            if (sort_index > class_index)
            {
                sort_index--;
            }

            JsonSort(json, JsonPrimitiveComparator, &sort_index);
        }
    }

    fclose(csv_file);
    if (heading != NULL)
    {
        SeqDestroy(heading);
    }

    return FnReturnContainerNoCopy(json);
}

static FnCallResult FnCallParseJson(ARG_UNUSED EvalContext *ctx,
                                    ARG_UNUSED const Policy *policy,
                                    ARG_UNUSED const FnCall *fp,
                                    const Rlist *args)
{
    const char *data = RlistScalarValue(args);
    JsonElement *json = NULL;
    bool yaml_mode = (strcmp(fp->name, "parseyaml") == 0);
    const char* data_type = yaml_mode ? "YAML" : "JSON";
    JsonParseError res;

    if (yaml_mode)
    {
        res = JsonParseYamlString(&data, &json);
    }
    else
    {
        res = JsonParseWithLookup(ctx, &LookupVarRefToJson, &data, &json);
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
        return FnReturnContainerNoCopy(json);
    }

    return FnFailure();
}

/*********************************************************************/

static FnCallResult FnCallStoreJson(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    const char *name_str = RlistScalarValueSafe(finalargs);

    // try to load directly
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, name_str);
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    Writer *w = StringWriter();

    JsonWrite(w, json, 0);
    JsonDestroyMaybe(json, allocated);
    Log(LOG_LEVEL_DEBUG, "%s: from data container %s, got JSON data '%s'", fp->name, name_str, StringWriterData(w));

    return FnReturnNoCopy(StringWriterClose(w));
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

    bool make_array = (strcmp(fp->name, "data_readstringarrayidx") == 0);
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

    if (json == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: error reading from file '%s'", fp->name, filename);
        return FnFailure();
    }

    return FnReturnContainerNoCopy(json);
}

/*********************************************************************/

static FnCallResult FnCallDataExpand(EvalContext *ctx,
                                     ARG_UNUSED const Policy *policy,
                                     ARG_UNUSED const FnCall *fp,
                                     const Rlist *args)
{
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, args, false, &allocated);

    if (json == NULL)
    {
        return FnFailure();
    }

    JsonElement *expanded = JsonExpandElement(ctx, json);
    JsonDestroyMaybe(json, allocated);

    return FnReturnContainerNoCopy(expanded);
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
    bool allocated = false;

    if (finalargs->next) // we have a variable name...
    {
        // try to load directly
        json = VarNameOrInlineToJson(ctx, fp, finalargs->next, false, &allocated);

        // we failed to produce a valid JsonElement, so give up
        if (json == NULL)
        {
            return FnFailure();
        }
    }
    else
    {
        allocated = true;
        json = DefaultTemplateData(ctx, NULL);
    }

    Buffer *result = BufferNew();
    bool success = MustacheRender(result, mustache_template, json);

    JsonDestroyMaybe(json, allocated);

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

static FnCallResult FnCallStringReplace(ARG_UNUSED EvalContext *ctx,
                                        ARG_UNUSED Policy const *policy,
                                        ARG_UNUSED FnCall const *fp,
                                        Rlist const *finalargs)
{
    if (finalargs->next == NULL || finalargs->next->next == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "Incorrect number of arguments for function '%s'",
            fp->name);
        return FnFailure();
    }

    char *string = RlistScalarValue(finalargs);
    char *match = RlistScalarValue(finalargs->next);
    char *substitute = RlistScalarValue(finalargs->next->next);

    char *ret = SearchAndReplace(string, match, substitute);

    if (ret == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "Failed to replace with function '%s', string: '%s', match: '%s', "
            "substitute: '%s'",
            fp->name, string, match, substitute);
        return FnFailure();
    }

    return FnReturnNoCopy(ret);
}

/*********************************************************************/

static FnCallResult FnCallStringTrim(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    char *string = RlistScalarValue(finalargs);

    return FnReturn(TrimWhitespace(string));
}

/*********************************************************************/

static FnCallResult FnCallFileSexist(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, const Rlist *finalargs)
{
    bool allocated = false;
    JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

    // we failed to produce a valid JsonElement, so give up
    if (json == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Cannot produce valid JSON from the argument '%s' of the function '%s'",
            fp->name, RlistScalarValueSafe(finalargs));
        return FnFailure();
    }
    else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
    {
        Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
            fp->name, RlistScalarValueSafe(finalargs));
        JsonDestroyMaybe(json, allocated);
        return FnFailure();
    }

    JsonIterator iter = JsonIteratorInit(json);
    const JsonElement *el = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true);

    /* no elements mean 'false' should be returned, otherwise let's see if the files exist */
    bool file_found = el != NULL;

    while (file_found && (el != NULL))
    {
        char *val = JsonPrimitiveToString(el);
        struct stat sb;
        if (stat(val, &sb) == -1)
        {
            file_found = false;
        }
        free(val);
        el = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true);
    }

    JsonDestroyMaybe(json, allocated);
    return FnReturnContext(file_found);
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
    else if (RetrieveUnreliableValue("ldapvalue", handle, buffer) > 0)
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

    const char *name_str = RlistScalarValueSafe(finalargs->next);

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
            Log(LOG_LEVEL_WARNING, "Function '%s' target-file '%s' exists and is not a plain file", fp->name, target);
            // Not a probe's responsibility to fix - but have this for debugging
        }

        target_time = statbuf.st_mtime;
    }

    // Check if the file name (which should be a scalar if given directly) is explicit
    if (RlistValueIsType(finalargs->next, RVAL_TYPE_SCALAR) &&
        lstat(name_str, &statbuf) != -1)
    {
        if (statbuf.st_mtime > target_time)
        {
            stale = true;
        }
    }
    else
    {
        // try to load directly from a container of collected values
        bool allocated = false;
        JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs->next, false, &allocated);

        // we failed to produce a valid JsonElement, so give up
        if (json == NULL)
        {
            return FnFailure();
        }
        else if (JsonGetElementType(json) != JSON_ELEMENT_TYPE_CONTAINER)
        {
            Log(LOG_LEVEL_VERBOSE, "Function '%s', argument '%s' was not a data container or list",
                fp->name, name_str);
            JsonDestroyMaybe(json, allocated);
            return FnFailure();
        }

        JsonIterator iter = JsonIteratorInit(json);
        const JsonElement *e;
        while ((e = JsonIteratorNextValueByType(&iter, JSON_ELEMENT_TYPE_PRIMITIVE, true)))
        {
            const char *value = JsonPrimitiveGetAsString(e);
            if (lstat(value, &statbuf) == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Function '%s', source dependency %s was not (yet) readable",  fp->name, value);
                JsonDestroyMaybe(json, allocated);
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

        JsonDestroyMaybe(json, allocated);
    }

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

        if (!GetUserName(uid, NULL, 0, LOG_LEVEL_VERBOSE))
        {
            return FnReturnContext(false);
        }
    }
    else if (!GetUserID(arg, NULL, LOG_LEVEL_VERBOSE))
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

        if (!GetGroupName(gid, NULL, 0, LOG_LEVEL_VERBOSE))
        {
            return FnReturnContext(false);
        }
    }
    else if (!GetGroupID(arg, NULL, LOG_LEVEL_VERBOSE))
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

static char *CfReadFile(const char *filename, size_t maxsize)
{
    /* TODO remove this stat() call, it's a remnant from old code
       that examined sb.st_size. */
    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
        {
            Log(LOG_LEVEL_ERR, "Could not examine file '%s'", filename);
        }
        else
        {
            if (IsCf3VarString(filename))
            {
                Log(LOG_LEVEL_VERBOSE, "Cannot converge/reduce variable '%s' yet .. assuming it will resolve later",
                      filename);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "CfReadFile: Could not examine file '%s' (stat: %s)",
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
        Log(LOG_LEVEL_ERR, "CfReadFile: Error while reading file '%s' (%s)",
            filename, GetErrorStr());
        return NULL;
    }

    if (truncated)
    {
        Log(LOG_LEVEL_VERBOSE, "CfReadFile: Truncating file '%s' to %zu bytes as "
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

    Regex *rx = CompileRegex(pattern);
    if (!rx)
    {
        return file_buffer;
    }

    size_t start, end, count = 0;
    const size_t original_length = strlen(file_buffer);
    while (StringMatchWithPrecompiledRegex(rx, file_buffer, &start, &end))
    {
        StringCloseHole(file_buffer, start, end);

        if (start == end)
        {
            Log(LOG_LEVEL_WARNING,
                "Comment regex '%s' matched empty string in '%s'",
                pattern,
                filename);
            break;
        }
        assert(start < end);
        if (count++ > original_length)
        {
            debug_abort_if_reached();
            Log(LOG_LEVEL_ERR,
                "Comment regex '%s' was irreconcilable reading input '%s' probably because it legally matches nothing",
                pattern, filename);
            break;
        }
    }

    RegexDestroy(rx);
    return file_buffer;
}

/*********************************************************************/

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

            if (first_index == NULL)
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

static FnCallResult FnCallProcessExists(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    char *regex = RlistScalarValue(finalargs);

    const bool is_context_processexists = strcmp(fp->name, "processexists") == 0;

    if (!LoadProcessTable())
    {
        Log(LOG_LEVEL_ERR, "%s: could not load the process table?!?!", fp->name);
        return FnFailure();
    }

    ProcessSelect ps = PROCESS_SELECT_INIT;
    ps.owner = NULL;
    ps.process_result = "";

    // ps is unused because attrselect = false below
    Item *matched = SelectProcesses(regex, &ps, false);
    ClearProcessTable();

    if (is_context_processexists)
    {
        const bool ret = (matched != NULL);
        DeleteItemList(matched);
        return FnReturnContext(ret);
    }

    JsonElement *json = JsonArrayCreate(50);
    // we're in process gathering mode
    for (Item *ip = matched; ip != NULL; ip = ip->next)
    {
        // we only have the ps line and PID

        // TODO: this properly, by including more properties of the
        // processes, when the underlying code stops using a plain
        // ItemList
        JsonElement *pobj = JsonObjectCreate(2);
        JsonObjectAppendString(pobj, "line", ip->name);
        JsonObjectAppendInteger(pobj, "pid", ip->counter);

        JsonArrayAppendObject(json, pobj);
    }
    DeleteItemList(matched);

    return FnReturnContainerNoCopy(json);
}

/*********************************************************************/

static FnCallResult FnCallNetworkConnections(EvalContext *ctx, ARG_UNUSED const Policy *policy, ARG_UNUSED const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    JsonElement *json = GetNetworkingConnections(ctx);

    if (json == NULL)
    {
        // nothing was collected, this is a failure
        return FnFailure();
    }

    return FnReturnContainerNoCopy(json);
}

/*********************************************************************/

struct IsReadableThreadData
{
    pthread_t thread;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    const char *path;
    FnCallResult result;
};

static void *IsReadableThreadRoutine(void *data)
{
    assert(data != NULL);

    struct IsReadableThreadData *const thread_data = data;

    // Give main thread time to call pthread_cond_timedwait(3)
    int ret = pthread_mutex_lock(&thread_data->mutex);
    if (ret != 0)
    {
        ProgrammingError("Failed to lock mutex: %s",
                         GetErrorStrFromCode(ret));
    }

    thread_data->result = FnReturnContext(false);

    // Allow main thread to require lock on pthread_cond_timedwait(3)
    ret = pthread_mutex_unlock(&thread_data->mutex);
    if (ret != 0)
    {
        ProgrammingError("Failed to unlock mutex: %s",
                         GetErrorStrFromCode(ret));
    }

    char buf[1];
    const int fd = open(thread_data->path, O_RDONLY);
    if (fd < 0) {
        Log(LOG_LEVEL_DEBUG, "Failed to open file '%s': %s",
            thread_data->path, GetErrorStr());
    }
    else if (read(fd, buf, sizeof(buf)) < 0)
    {
        Log(LOG_LEVEL_DEBUG, "Failed to read from file '%s': %s",
            thread_data->path, GetErrorStr());
        close(fd);
    }
    else
    {
        close(fd);
        thread_data->result = FnReturnContext(true);
    }

    ret = pthread_cond_signal(&(thread_data->cond));
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to signal waiting thread: %s",
            GetErrorStrFromCode(ret));
    }

    return NULL;
}

static FnCallResult FnCallIsReadable(ARG_UNUSED EvalContext *const ctx,
                                     ARG_UNUSED const Policy *const policy,
                                     const FnCall *const fp,
                                     const Rlist *finalargs)
{
    assert(fp != NULL);
    assert(fp->name != NULL);

    if (finalargs == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires path as first argument",
            fp->name);
        return FnFailure();
    }
    const char *const path = RlistScalarValue(finalargs);

    long timeout = (finalargs->next == NULL) ? 3L /* default timeout */
                 : IntFromString(RlistScalarValue(finalargs->next));
    assert(timeout >= 0);

    if (timeout == 0) // Try read in main thread, possibly blocking forever
    {
        Log(LOG_LEVEL_DEBUG,
            "Checking if file '%s' is readable in main thread, "
            "possibly blocking forever.", path);

        char buf[1];
        const int fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            Log(LOG_LEVEL_DEBUG, "Failed to open file '%s': %s", path,
                GetErrorStr());
            return FnReturnContext(false);
        }
        else if (read(fd, buf, sizeof(buf)) < 0)
        {
            Log(LOG_LEVEL_DEBUG, "Failed to read from file '%s': %s", path,
                GetErrorStr());
            close(fd);
            return FnReturnContext(false);
        }

        close(fd);
        return FnReturnContext(true);
    }

    // Else try read in separate thread, possibly blocking for N seconds
    Log(LOG_LEVEL_DEBUG,
        "Checking if file '%s' is readable in separate thread, "
        "possibly blocking for %ld seconds.", path, timeout);

    struct IsReadableThreadData thread_data = {0};
    thread_data.path = path;

    int ret = pthread_mutex_init(&thread_data.mutex, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to initialize mutex: %s",
            GetErrorStrFromCode(ret));
        return FnFailure();
    }

    ret = pthread_cond_init(&thread_data.cond, NULL);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to initialize condition: %s",
            GetErrorStrFromCode(ret));
        return FnFailure();
    }

    // Keep thread from doing its thing until we call
    // pthread_cond_timedwait(3)
    ret = pthread_mutex_lock(&thread_data.mutex);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to lock mutex: %s",
            GetErrorStrFromCode(ret));
        return FnFailure();
    }

    ret = pthread_create(&thread_data.thread, NULL, &IsReadableThreadRoutine,
                         &thread_data);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to create thread: %s",
            GetErrorStrFromCode(ret));
        return FnFailure();
    }

    FnCallResult result;
    // Wait on thread to finish or timeout
    ret = ThreadWait(&thread_data.cond, &thread_data.mutex, timeout);
    switch (ret)
    {
        case 0: // Thread finished in time
            result = thread_data.result;
            break;

        case ETIMEDOUT: // Thread timed out
            Log(LOG_LEVEL_DEBUG, "File '%s' is not readable: "
                "Read operation timed out, exceeded %ld seconds.", path,
                timeout);

            ret = pthread_cancel(thread_data.thread);
            if (ret != 0)
            {
                Log(LOG_LEVEL_ERR, "Failed to cancel thread");
                return FnFailure();
            }

            result = FnReturnContext(false);
            break;

        default:
            Log(LOG_LEVEL_ERR, "Failed to wait for condition: %s",
                GetErrorStrFromCode(ret));
            return FnFailure();

    }

    ret = pthread_mutex_unlock(&thread_data.mutex);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to unlock mutex");
        return FnFailure();
    }

    void *status;
    ret = pthread_join(thread_data.thread, &status);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to join thread");
        return FnFailure();
    }

    if (status == PTHREAD_CANCELED)
    {
        Log(LOG_LEVEL_DEBUG, "Thread was canceled");
    }

    return result;
}

/*********************************************************************/

static FnCallResult FnCallFindfilesUp(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, const Rlist *finalargs)
{
    assert(fp != NULL);
    assert(fp->name != NULL);

    const Rlist *const path_arg = finalargs;
    if (path_arg == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires path as first argument",
            fp->name);
        return FnFailure();
    }

    const Rlist *const glob_arg = finalargs->next;
    if (glob_arg == NULL)
    {
        Log(LOG_LEVEL_ERR, "Function %s requires glob as second argument",
            fp->name);
        return FnFailure();
    }

    const Rlist *const level_arg = finalargs->next->next;

    char path[PATH_MAX];
    size_t copied = strlcpy(path, RlistScalarValue(path_arg), sizeof(path));
    if (copied >= sizeof(path))
    {
        Log(LOG_LEVEL_ERR, "Function %s, path is too long (%zu > %zu)",
            fp->name, copied, sizeof(path));
        return FnFailure();
    }

    if (!IsAbsoluteFileName(path))
    {
        Log(LOG_LEVEL_ERR, "Function %s, not an absolute path '%s'",
            fp->name, path);
        return FnFailure();
    }

    if (!IsDir(path))
    {
        Log(LOG_LEVEL_ERR, "Function %s, path '%s' is not a directory",
            fp->name, path);
        return FnFailure();
    }

    MapName(path); // Makes sure we get host native path separators
    DeleteRedundantSlashes(path);

    size_t len = strlen(path);
    if (path[len - 1] == FILE_SEPARATOR)
    {
        path[len - 1] = '\0';
    }

    char glob[PATH_MAX];
    copied = strlcpy(glob, RlistScalarValue(glob_arg), sizeof(glob));
    if (copied >= sizeof(glob))
    {
        Log(LOG_LEVEL_ERR, "Function %s, glob is too long (%zu > %zu)",
            fp->name, copied, sizeof(glob));
        return FnFailure();
    }

    if (IsAbsoluteFileName(glob))
    {
        Log(LOG_LEVEL_ERR,
            "Function %s, glob '%s' cannot be an absolute path", fp->name,
            glob);
        return FnFailure();
    }

    DeleteRedundantSlashes(glob);

    /* level defaults to inf */
    long level = IntFromString("inf");
    if (level_arg != NULL)
    {
        level = IntFromString(RlistScalarValue(level_arg));
    }

    JsonElement *json = JsonArrayCreate(1);

    while (level-- >= 0)
    {
        char filepath[PATH_MAX];
        copied = strlcpy(filepath, path, sizeof(filepath));
        assert(copied < sizeof(path));
        if (JoinPaths(filepath, sizeof(filepath), glob) == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Function %s, failed to join path '%s' and glob '%s'",
                fp->name, path, glob);
        }

        StringSet *matches = GlobFileList(filepath);
        JsonElement *matches_json = StringSetToJson(matches);
        JsonArrayExtend(json, matches_json);
        StringSetDestroy(matches);

        char *sep = strrchr(path, FILE_SEPARATOR);
        if (sep == NULL)
        {
            /* don't search beyond root directory */
            break;
        }
        *sep = '\0';
    }

    return FnReturnContainerNoCopy(json);
}

/*********************************************************************/

static bool ExecModule(EvalContext *ctx, char *command)
{
    FILE *pp = cf_popen(command, "rt", true);
    if (!pp)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe from '%s'. (cf_popen: %s)", command, GetErrorStr());
        return false;
    }

    char context[CF_BUFSIZE] = "";
    StringSet *tags = StringSetNew();
    long persistence = 0;

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

        ModuleProtocol(ctx, command, line, print, context, sizeof(context), tags, &persistence);
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

void ModuleProtocol(EvalContext *ctx, const char *command, const char *line, int print, char* context, size_t context_size, StringSet *tags, long *persistence)
{
    assert(tags);

    StringSetAdd(tags, xstrdup("source=module"));
    StringSetAddF(tags, "derived_from=%s", command);

    // see the sscanf() limit below
    if(context_size < 51)
    {
        ProgrammingError("ModuleProtocol: context_size too small (%zu)",
                         context_size);
    }

    if (*context == '\0')
    {
        /* Infer namespace from script name */
        char arg0[CF_BUFSIZE];
        strlcpy(arg0, CommandArg0(command), CF_BUFSIZE);
        char *filename = basename(arg0);

        /* Canonicalize filename into acceptable namespace name */
        CanonifyNameInPlace(filename);
        strlcpy(context, filename, context_size);
        Log(LOG_LEVEL_VERBOSE, "Module context '%s'", context);
    }

    char name[CF_BUFSIZE], content[CF_BUFSIZE];
    name[0] = content[0] = '\0';

    size_t length = strlen(line);
    switch (*line)
    {
    case '^':
        // Allow modules to set their variable context (up to 50 characters)
        if (sscanf(line + 1, "context=%50[^\n]", content) == 1 &&
            content[0] != '\0')
        {
            /* Symbol ID without \200 to \377: */
            Regex *context_name_rx = CompileRegex("[a-zA-Z0-9_]+");
            if (!context_name_rx)
            {
                Log(LOG_LEVEL_ERR,
                    "Internal error compiling module protocol context regex, aborting!!!");
            }
            else if (StringMatchFullWithPrecompiledRegex(context_name_rx, content))
            {
                Log(LOG_LEVEL_VERBOSE, "Module changed variable context from '%s' to '%s'", context, content);
                strlcpy(context, content, context_size);
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "Module protocol was given an unacceptable ^context directive '%s', skipping", content);
            }

            if (context_name_rx)
            {
                RegexDestroy(context_name_rx);
            }
        }
        else if (sscanf(line + 1, "meta=%1024[^\n]", content) == 1 &&
                 content[0] != '\0')
        {
            Log(LOG_LEVEL_VERBOSE, "Module set meta tags to '%s'", content);
            StringSetClear(tags);

            StringSetAddSplit(tags, content, ',');
            StringSetAdd(tags, xstrdup("source=module"));
            StringSetAddF(tags, "derived_from=%s", command);
        }
        else if (sscanf(line + 1, "persistence=%ld", persistence) == 1)
        {
            Log(LOG_LEVEL_VERBOSE, "Module set persistence to %ld minutes", *persistence);
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
            if (*persistence > 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Module set persistent class '%s' for %ld minutes", content, *persistence);
                EvalContextHeapPersistentSave(ctx, content, *persistence, CONTEXT_STATE_POLICY_PRESERVE, BufferData(tagbuf));
            }

            BufferDestroy(tagbuf);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Automatically canonifying '%s'", content);
            CanonifyNameInPlace(content);
            Log(LOG_LEVEL_VERBOSE, "Automatically canonified to '%s'", content);
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

    case '&':
        // TODO: the variable name is limited to 256 to accommodate the
        // context name once it's in the vartable.  Maybe this can be relaxed.
        // &policy_varname=/path/to/file can be json/env/yaml/csv type, default: json
        sscanf(line + 1, "%256[^=]=%4095[^\n]", name, content);

        if (CheckID(name))
        {
            if (FileCanOpen(content, "r"))
            {
                const int size_max = IntFromString("inf");
                const DataFileType requested_mode = GetDataFileTypeFromSuffix(content);

                Log(LOG_LEVEL_DEBUG, "Module protocol parsing %s file '%s'",
                    DataFileTypeToString(requested_mode), content);

                JsonElement *json = JsonReadDataFile("module file protocol", content, requested_mode, size_max);
                if (json != NULL)
                {
                    Buffer *tagbuf = StringSetToBuffer(tags, ',');
                    VarRef *ref = VarRefParseFromScope(name, context);

                    EvalContextVariablePut(ctx, ref, json, CF_DATA_TYPE_CONTAINER, BufferData(tagbuf));
                    VarRefDestroy(ref);
                    BufferDestroy(tagbuf);
                    JsonDestroy(json);
                }
                else
                {
                    // reading / parsing failed, error message printed by JsonReadDataFile,
                    // variable will not be created
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR, "could not load module file '%s'", content);
            }
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

static FnCallResult FnCallCFEngineCallers(EvalContext *ctx, ARG_UNUSED const Policy *policy, const FnCall *fp, ARG_UNUSED const Rlist *finalargs)
{
    bool promisersmode = (strcmp(fp->name, "callstack_promisers") == 0);

    if (promisersmode)
    {
        Rlist *returnlist = EvalContextGetPromiseCallerMethods(ctx);
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, RVAL_TYPE_LIST } };
    }

    JsonElement *callers = EvalContextGetPromiseCallers(ctx);
    return FnReturnContainerNoCopy(callers);
}

static FnCallResult FnCallString(EvalContext *ctx,
                                 ARG_UNUSED const Policy *policy,
                                 const FnCall *fp,
                                 const Rlist *finalargs)
{
    assert(finalargs != NULL);

    char *ret = RvalToString(finalargs->val);
    if (StringStartsWith(ret, "@(") || StringStartsWith(ret, "@{"))
    {
        bool allocated = false;
        JsonElement *json = VarNameOrInlineToJson(ctx, fp, finalargs, false, &allocated);

        if (json != NULL)
        {
            Writer *w = StringWriter();
            JsonWriteCompact(w, json);
            ret = StringWriterClose(w);
            if (allocated)
            {
                JsonDestroy(json);
            }
        }
    }

    return FnReturnNoCopy(ret);
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static bool CheckIDChar(const char ch)
{
    return isalnum((int) ch) || (ch == '.') || (ch == '-') || (ch == '_') ||
                                (ch == '[') || (ch == ']') || (ch == '/') ||
                                (ch == '@');
}

static bool CheckID(const char *id)
{
    for (const char *sp = id; *sp != '\0'; sp++)
    {
        if (!CheckIDChar(*sp))
        {
            Log(LOG_LEVEL_VERBOSE,
                  "Module protocol contained an illegal character '%c' in class/variable identifier '%s'.", *sp,
                  id);
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

static const FnCallArg GET_ACLS_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Path to file or directory"},
    {"default,access", CF_DATA_TYPE_OPTION, "Whether to get default or access ACL"},
    {NULL, CF_DATA_TYPE_NONE, NULL},
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

static const FnCallArg BASENAME_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "File path"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Optional suffix"},
    {NULL, CF_DATA_TYPE_NONE, NULL},
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

static const FnCallArg CFVERSION_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine version number to compare against"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CFVERSIONBETWEEN_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Lower CFEngine version number to compare against"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Upper CFEngine version number to compare against"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg VERSION_COMPARE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "First version to compare"},
    {"=,==,!=,>,<,>=,<=", CF_DATA_TYPE_OPTION, "Operator to use in comparison"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Second version to compare"},
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
    {"both,stdout,stderr", CF_DATA_TYPE_OPTION, "Which output to return; stdout or stderr"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FINDFILES_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FILTER_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression or string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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

static const FnCallArg USERINGROUP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "User name"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Group name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GETUSERINFO_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "User name in text"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg GREP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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
    {"name,address,hostkey", CF_DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTSWITHCLASS_ARGS[] =
{
    {"[a-zA-Z0-9_]+", CF_DATA_TYPE_STRING, "Class name to look for"},
    {"name,address,hostkey", CF_DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg HOSTSWITHGROUP_ARGS[] =
{
    {"[a-zA-Z0-9_]+", CF_DATA_TYPE_STRING, "Group name to look for"},
    {"name,address,hostkey", CF_DATA_TYPE_OPTION, "Type of return value desired"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg IFELSE_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg INT_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Numeric string to convert to integer"},
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

static const FnCallArg ISCONNECTABLE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Host name, domain name or IP address"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Port number"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Connection timeout (in seconds)"},
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

static const FnCallArg FILE_TIME_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Time as a Unix epoch offset"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON, the array variable to map"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAPDATA_ARGS[] =
{
    {"none,canonify,json,json_pipe", CF_DATA_TYPE_OPTION, "Conversion to apply to the mapped string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Pattern based on $(this.k) and $(this.v) as original text"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PRODUCT_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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

static const FnCallArg HASH_TO_INT_ARGS[] =
{
    {CF_INTRANGE, CF_DATA_TYPE_INT, "Lower inclusive bound"},
    {CF_INTRANGE, CF_DATA_TYPE_INT, "Upper exclusive bound"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string to hash"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRING_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Convert argument to string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READFILE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Maximum number of bytes to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg VALIDJSON_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "String to validate as JSON"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Enable more strict validation, requiring the result to be a valid data container, matching the requirements of parsejson()."},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CLASSFILTERDATA_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {"array_of_arrays,array_of_objects,auto", CF_DATA_TYPE_OPTION, "Specify type of data structure"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Key or index of class expressions"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CLASSFILTERCSV_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "CSV file has heading"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Column index to filter by, "
                                    "contains classes"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Column index to sort by"},
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
    {"CSV,YAML,JSON,ENV,auto", CF_DATA_TYPE_OPTION, "Type of data to read"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg VALIDDATA_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "String to validate as JSON"},
    {"JSON", CF_DATA_TYPE_OPTION, "Type of data to validate"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Enable more strict validation, requiring the result to be a valid data container, matching the requirements of parsejson()."},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg READMODULE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "File name to read and parse from"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PARSEJSON_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "JSON string to parse"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STOREJSON_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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

static const FnCallArg REGEX_REPLACE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Source string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression pattern"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Replacement string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "sed/Perl-style options: gmsixUT"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg MAKERULE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Target filename"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Source filename or CFEngine variable identifier or inline JSON"},
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
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Use encryption"},
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

static const FnCallArg STRING_REPLACE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Source string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "String to replace"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Replacement string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRING_TRIM_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Input string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SUBLIST_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg NTH_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Offset or key of element to return"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg EVERY_SOME_NONE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression or string"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg USEREXISTS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "User name or identifier"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SORT_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {"lex,int,real,IP,ip,MAC,mac", CF_DATA_TYPE_OPTION, "Sorting method: lex or int or real (floating point) or IPv4/IPv6 or MAC address"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg REVERSE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SHUFFLE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Any seed string"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STAT_FOLD_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SETOP_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine base variable identifier or inline JSON"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine filter variable identifier or inline JSON"},
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
    {CF_INTRANGE, CF_DATA_TYPE_INT, "Maximum number of characters to return"},
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
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg STRING_MUSTACHE_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CURL_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "URL to retrieve"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "CFEngine variable identifier or inline JSON, can be blank"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg PROCESSEXISTS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Regular expression to match process name"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg NETWORK_CONNECTIONS_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CFENGINE_PROMISERS_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg CFENGINE_CALLERS_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg SYSCTLVALUE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "sysctl key"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATA_SYSCTLVALUES_ARGS[] =
{
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg FINDFILES_UP_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Path to search from"},
    {CF_PATHRANGE, CF_DATA_TYPE_STRING, "Glob pattern to match files"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Number of levels to search"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg ISREADABLE_ARGS[] =
{
    {CF_ABSPATHRANGE, CF_DATA_TYPE_STRING, "Path to file"},
    {CF_VALRANGE, CF_DATA_TYPE_INT, "Timeout interval"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

static const FnCallArg DATATYPE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Variable identifier"},
    {CF_BOOL, CF_DATA_TYPE_OPTION, "Enable detailed type decription"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};
static const FnCallArg IS_DATATYPE_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Variable identifier"},
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Type"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};
static const FnCallArg FIND_LOCAL_USERS_ARGS[] =
{
    {CF_ANYSTRING, CF_DATA_TYPE_STRING, "Filter list"},
    {NULL, CF_DATA_TYPE_NONE, NULL}
};

/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see fncall.h enum FnCallType */

/* In evalfunction_test.c we both include this file and link with libpromises.
 * This guard makes sure we don't get a duplicate definition of this symbol */
#ifndef CFENGINE_EVALFUNCTION_TEST_C
const FnCallType CF_FNCALL_TYPES[] =
{
    FnCallTypeNew("accessedbefore", CF_DATA_TYPE_CONTEXT, ACCESSEDBEFORE_ARGS, &FnCallIsAccessedBefore, "True if arg1 was accessed before arg2 (atime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("accumulated", CF_DATA_TYPE_INT, ACCUM_ARGS, &FnCallAccumulatedDate, "Convert an accumulated amount of time into a system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getacls", CF_DATA_TYPE_STRING_LIST, GET_ACLS_ARGS, &FnCallGetACLs, "Get ACLs of a given file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ago", CF_DATA_TYPE_INT, AGO_ARGS, &FnCallAgoDate, "Convert a time relative to now to an integer system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("and", CF_DATA_TYPE_CONTEXT, AND_ARGS, &FnCallAnd, "Calculate whether all arguments evaluate to true",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("basename", CF_DATA_TYPE_STRING, BASENAME_ARGS, &FnCallBasename, "Retrieves the basename of a filename.",
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
    FnCallTypeNew("cf_version_minimum", CF_DATA_TYPE_CONTEXT, CFVERSION_ARGS, &FnCallVersionMinimum, "True if local CFEngine version is newer than or equal to input",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("cf_version_after", CF_DATA_TYPE_CONTEXT, CFVERSION_ARGS, &FnCallVersionAfter, "True if local CFEngine version is newer than input",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("cf_version_maximum", CF_DATA_TYPE_CONTEXT, CFVERSION_ARGS, &FnCallVersionMaximum, "True if local CFEngine version is older than or equal to input",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("cf_version_before", CF_DATA_TYPE_CONTEXT, CFVERSION_ARGS, &FnCallVersionBefore, "True if local CFEngine version is older than input",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("cf_version_at", CF_DATA_TYPE_CONTEXT, CFVERSION_ARGS, &FnCallVersionAt, "True if local CFEngine version is the same as input",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("cf_version_between", CF_DATA_TYPE_CONTEXT, CFVERSIONBETWEEN_ARGS, &FnCallVersionBetween, "True if local CFEngine version is between two input versions (inclusive)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("changedbefore", CF_DATA_TYPE_CONTEXT, CHANGEDBEFORE_ARGS, &FnCallIsChangedBefore, "True if arg1 was changed before arg2 (ctime)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classify", CF_DATA_TYPE_CONTEXT, CLASSIFY_ARGS, &FnCallClassify, "True if the canonicalization of the argument is a currently defined class",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classmatch", CF_DATA_TYPE_CONTEXT, CLASSMATCH_ARGS, &FnCallClassesMatching, "True if the regular expression matches any currently defined class",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classesmatching", CF_DATA_TYPE_STRING_LIST, CLASSMATCH_ARGS, &FnCallClassesMatching, "List the defined classes matching regex arg1 and tag regexes arg2,arg3,...",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classfilterdata", CF_DATA_TYPE_CONTAINER, CLASSFILTERDATA_ARGS, &FnCallClassFilterData, "Filter data container by defined classes",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("classfiltercsv", CF_DATA_TYPE_CONTAINER, CLASSFILTERCSV_ARGS, &FnCallClassFilterCsv, "Parse a CSV file and create data container filtered by defined classes",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countclassesmatching", CF_DATA_TYPE_INT, CLASSMATCH_ARGS, &FnCallClassesMatching, "Count the number of defined classes matching regex arg1",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("countlinesmatching", CF_DATA_TYPE_INT, COUNTLINESMATCHING_ARGS, &FnCallCountLinesMatching, "Count the number of lines matching regex arg1 in file arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("url_get", CF_DATA_TYPE_CONTAINER, CURL_ARGS, &FnCallUrlGet, "Retrieve the contents at a URL with libcurl",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("datastate", CF_DATA_TYPE_CONTAINER, DATASTATE_ARGS, &FnCallDatastate, "Construct a container of the variable and class state",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("difference", CF_DATA_TYPE_STRING_LIST, SETOP_ARGS, &FnCallSetop, "Returns all the unique elements of list or array or data container arg1 that are not in list or array or data container arg2",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("dirname", CF_DATA_TYPE_STRING, DIRNAME_ARGS, &FnCallDirname, "Return the parent directory name for given path",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("diskfree", CF_DATA_TYPE_INT, DISKFREE_ARGS, &FnCallDiskFree, "Return the free space (in KB) available on the directory's current partition (0 if not found)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("escape", CF_DATA_TYPE_STRING, ESCAPE_ARGS, &FnCallEscape, "Escape regular expression characters in a string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("eval", CF_DATA_TYPE_STRING, EVAL_ARGS, &FnCallEval, "Evaluate a mathematical expression",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("every", CF_DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if every element in the list or array or data container matches the given regular expression",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("execresult", CF_DATA_TYPE_STRING, EXECRESULT_ARGS, &FnCallExecResult, "Execute named command and assign output to variable",
                  FNCALL_OPTION_CACHED | FNCALL_OPTION_VARARG | FNCALL_OPTION_UNSAFE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("execresult_as_data", CF_DATA_TYPE_CONTAINER, EXECRESULT_ARGS, &FnCallExecResult, "Execute command and return exit code and output in data container",
                  FNCALL_OPTION_CACHED | FNCALL_OPTION_VARARG | FNCALL_OPTION_UNSAFE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("file_hash", CF_DATA_TYPE_STRING, FILE_HASH_ARGS, &FnCallHandlerHash, "Return the hash of file arg1, type arg2 and assign to a variable",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("expandrange", CF_DATA_TYPE_STRING_LIST, EXPANDRANGE_ARGS, &FnCallExpandRange, "Expand a name as a list of names numered according to a range",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("fileexists", CF_DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named file can be accessed",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesexist", CF_DATA_TYPE_CONTEXT, FILESEXIST_ARGS, &FnCallFileSexist, "True if the named list of files can ALL be accessed",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filesize", CF_DATA_TYPE_INT, FILESTAT_ARGS, &FnCallFileStat, "Returns the size in bytes of the file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filestat", CF_DATA_TYPE_STRING, FILESTAT_DETAIL_ARGS, &FnCallFileStatDetails, "Returns stat() details of the file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("filter", CF_DATA_TYPE_STRING_LIST, FILTER_ARGS, &FnCallFilter, "Similarly to grep(), filter the list or array or data container arg2 for matches to arg2.  The matching can be as a regular expression or exactly depending on arg3.  The matching can be inverted with arg4.  A maximum on the number of matches returned can be set with arg5.",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("findfiles", CF_DATA_TYPE_STRING_LIST, FINDFILES_ARGS, &FnCallFindfiles, "Find files matching a shell glob pattern",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("findprocesses", CF_DATA_TYPE_CONTAINER, PROCESSEXISTS_ARGS, &FnCallProcessExists, "Returns data container of processes matching the regular expression",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("format", CF_DATA_TYPE_STRING, FORMAT_ARGS, &FnCallFormat, "Applies a list of string values in arg2,arg3... to a string format in arg1 with sprintf() rules",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getclassmetatags", CF_DATA_TYPE_STRING_LIST, GETCLASSMETATAGS_ARGS, &FnCallGetMetaTags, "Collect the class arg1's meta tags into an slist, optionally collecting only tag key arg2",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getenv", CF_DATA_TYPE_STRING, GETENV_ARGS, &FnCallGetEnv, "Return the environment variable named arg1, truncated at arg2 characters",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getfields", CF_DATA_TYPE_INT, GETFIELDS_ARGS, &FnCallGetFields, "Get an array of fields in the lines matching regex arg1 in file arg2, split on regex arg3 as array name arg4",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getgid", CF_DATA_TYPE_INT, GETGID_ARGS, &FnCallGetGid, "Return the integer group id of the named group on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getindices", CF_DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetIndices, "Get a list of keys to the list or array or data container whose id is the argument and assign to variable",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getuid", CF_DATA_TYPE_INT, GETUID_ARGS, &FnCallGetUid, "Return the integer user id of the named user on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getusers", CF_DATA_TYPE_STRING_LIST, GETUSERS_ARGS, &FnCallGetUsers, "Get a list of all system users defined, minus those names defined in arg1 and uids in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getuserinfo", CF_DATA_TYPE_CONTAINER, GETUSERINFO_ARGS, &FnCallGetUserInfo, "Get a data container describing user arg1, defaulting to current user",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getvalues", CF_DATA_TYPE_STRING_LIST, GETINDICES_ARGS, &FnCallGetValues, "Get a list of values in the list or array or data container arg1",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getvariablemetatags", CF_DATA_TYPE_STRING_LIST, GETVARIABLEMETATAGS_ARGS, &FnCallGetMetaTags, "Collect the variable arg1's meta tags into an slist, optionally collecting only tag key arg2",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("getbundlemetatags", CF_DATA_TYPE_STRING_LIST, GETVARIABLEMETATAGS_ARGS, &FnCallGetMetaTags, "Collect the bundle arg1's meta tags into an slist, optionally collecting only tag key arg2",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("grep", CF_DATA_TYPE_STRING_LIST, GREP_ARGS, &FnCallGrep, "Extract the sub-list if items matching the regular expression in arg1 of the list or array or data container arg2",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
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
    FnCallTypeNew("hostswithclass", CF_DATA_TYPE_STRING_LIST, HOSTSWITHCLASS_ARGS, &FnCallHostsWithField, "Extract the list of hosts with the given class set from the hub database (enterprise extension)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hostswithgroup", CF_DATA_TYPE_STRING_LIST, HOSTSWITHGROUP_ARGS, &FnCallHostsWithField, "Extract the list of hosts of a given group from the hub database (enterprise extension)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hubknowledge", CF_DATA_TYPE_STRING, HUB_KNOWLEDGE_ARGS, &FnCallHubKnowledge, "Read global knowledge from the hub host by id (enterprise extension)",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("ifelse", CF_DATA_TYPE_STRING, IFELSE_ARGS, &FnCallIfElse, "Do If-ElseIf-ElseIf-...-Else evaluation of arguments",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("int", CF_DATA_TYPE_INT, INT_ARGS, &FnCallInt, "Convert numeric string to int",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("intersection", CF_DATA_TYPE_STRING_LIST, SETOP_ARGS, &FnCallSetop, "Returns all the unique elements of list or array or data container arg1 that are also in list or array or data container arg2",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("iprange", CF_DATA_TYPE_CONTEXT, IPRANGE_ARGS, &FnCallIPRange, "True if the current host lies in the range of IP addresses specified in arg1 (can be narrowed to specific interfaces with arg2).",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isipinsubnet", CF_DATA_TYPE_CONTEXT, IPRANGE_ARGS, &FnCallIsIpInSubnet, "True if an IP address specified in arg2, arg3, ... lies in the range of IP addresses specified in arg1",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("irange", CF_DATA_TYPE_INT_RANGE, IRANGE_ARGS, &FnCallIRange, "Define a range of integer values for cfengine internal use",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isconnectable", CF_DATA_TYPE_CONTEXT, ISCONNECTABLE_ARGS, &FnCallIsConnectable, "Check if a port is connectable",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
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
    FnCallTypeNew("isnewerthantime", CF_DATA_TYPE_CONTEXT, FILE_TIME_ARGS, &FnCallIsNewerThanTime, "True if arg1 is newer (modified later) (has larger mtime) than time arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isplain", CF_DATA_TYPE_CONTEXT, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a plain/regular file",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isvariable", CF_DATA_TYPE_CONTEXT, ISVARIABLE_ARGS, &FnCallIsVariable, "True if the named variable is defined",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("join", CF_DATA_TYPE_STRING, JOIN_ARGS, &FnCallJoin, "Join the items of list or array or data container arg2 into a string, using the conjunction in arg1",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
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
    FnCallTypeNew("makerule", CF_DATA_TYPE_STRING, MAKERULE_ARGS, &FnCallMakerule, "True if the target file arg1 does not exist or a source file arg2 or the list or array or data container arg2 is newer",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maparray", CF_DATA_TYPE_STRING_LIST, MAPARRAY_ARGS, &FnCallMapData, "Return a list with each element mapped from a list or array or data container by a pattern based on $(this.k) and $(this.v)",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mapdata", CF_DATA_TYPE_CONTAINER, MAPDATA_ARGS, &FnCallMapData, "Return a data container with each element parsed from a JSON string applied to every key-value pair of the list or array or data container, given as $(this.k) and $(this.v)",
                  FNCALL_OPTION_COLLECTING|FNCALL_OPTION_DELAYED_EVALUATION, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("maplist", CF_DATA_TYPE_STRING_LIST, MAPLIST_ARGS, &FnCallMapList, "Return a mapping of the list or array or data container with each element modified by a pattern based $(this)",
                  FNCALL_OPTION_COLLECTING|FNCALL_OPTION_DELAYED_EVALUATION, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mergedata", CF_DATA_TYPE_CONTAINER, MERGEDATA_ARGS, &FnCallMergeData, "Merge two or more items, each a list or array or data container",
                  FNCALL_OPTION_COLLECTING|FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("none", CF_DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if no element in the list or array or data container matches the given regular expression",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("not", CF_DATA_TYPE_CONTEXT, NOT_ARGS, &FnCallNot, "Calculate whether argument is false",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("now", CF_DATA_TYPE_INT, NOW_ARGS, &FnCallNow, "Convert the current time into system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("nth", CF_DATA_TYPE_STRING, NTH_ARGS, &FnCallNth, "Get the element at arg2 in list or array or data container arg1",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("on", CF_DATA_TYPE_INT, DATE_ARGS, &FnCallOn, "Convert an exact date/time to an integer system representation",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("or", CF_DATA_TYPE_CONTEXT, OR_ARGS, &FnCallOr, "Calculate whether any argument evaluates to true",
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
    FnCallTypeNew("processexists", CF_DATA_TYPE_CONTEXT, PROCESSEXISTS_ARGS, &FnCallProcessExists, "True if the regular expression matches a process",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("randomint", CF_DATA_TYPE_INT, RANDOMINT_ARGS, &FnCallRandomInt, "Generate a random integer between the given limits, excluding the upper",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("hash_to_int", CF_DATA_TYPE_INT, HASH_TO_INT_ARGS, &FnCallHashToInt, "Generate an integer in given range based on string hash",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("useringroup", CF_DATA_TYPE_CONTEXT, USERINGROUP_ARGS, &FnCallUserInGroup, "Checks whether a user is in a group",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    FnCallTypeNew("string", CF_DATA_TYPE_STRING, STRING_ARGS, &FnCallString, "Convert argument to string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    // read functions for reading from file
    FnCallTypeNew("readdata", CF_DATA_TYPE_CONTAINER, READDATA_ARGS, &FnCallReadData, "Parse a YAML, JSON, CSV, etc. file and return a JSON data container with the contents",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readfile", CF_DATA_TYPE_STRING, READFILE_ARGS, &FnCallReadFile,       "Read max number of bytes from named file and assign to variable",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readcsv", CF_DATA_TYPE_CONTAINER, READFILE_ARGS, &FnCallReadCsv,     "Parse a CSV file and return a JSON data container with the contents",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readenvfile", CF_DATA_TYPE_CONTAINER, READFILE_ARGS, &FnCallReadEnvFile, "Parse a ENV-style file and return a JSON data container with the contents",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readjson", CF_DATA_TYPE_CONTAINER, READFILE_ARGS, &FnCallReadJson,    "Read a JSON data container from a file",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readyaml", CF_DATA_TYPE_CONTAINER, READFILE_ARGS, &FnCallReadYaml,    "Read a data container from a YAML file",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("read_module_protocol", CF_DATA_TYPE_CONTEXT, READMODULE_ARGS, &FnCallReadModuleProtocol, "Parse a file containing module protocol output (for cached modules)",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintarray", CF_DATA_TYPE_INT, READSTRINGARRAY_ARGS, &FnCallReadIntArray, "Read an array of integers from a file, indexed by first entry on line and sequentially on each line; return line count",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("readintlist", CF_DATA_TYPE_INT_LIST, READSTRINGLIST_ARGS, &FnCallReadIntList, "Read and assign a list variable from a file of separated ints",
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
    FnCallTypeNew("readtcp", CF_DATA_TYPE_STRING, READTCP_ARGS, &FnCallReadTcp, "Connect to tcp port, send string and assign result to variable",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),

    // reg functions for regex
    FnCallTypeNew("regarray", CF_DATA_TYPE_CONTEXT, REGARRAY_ARGS, &FnCallRegList, "True if the regular expression in arg1 matches any item in the list or array or data container arg2",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regcmp", CF_DATA_TYPE_CONTEXT, REGCMP_ARGS, &FnCallRegCmp, "True if arg1 is a regular expression matching that matches string arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regextract", CF_DATA_TYPE_CONTEXT, REGEXTRACT_ARGS, &FnCallRegExtract, "True if the regular expression in arg 1 matches the string in arg2 and sets a non-empty array of backreferences named arg3",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("registryvalue", CF_DATA_TYPE_STRING, REGISTRYVALUE_ARGS, &FnCallRegistryValue, "Returns a value for an MS-Win registry key,value pair",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regline", CF_DATA_TYPE_CONTEXT, REGLINE_ARGS, &FnCallRegLine, "True if the regular expression in arg1 matches a line in file arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reglist", CF_DATA_TYPE_CONTEXT, REGLIST_ARGS, &FnCallRegList, "True if the regular expression in arg2 matches any item in the list or array or data container whose id is arg1",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regldap", CF_DATA_TYPE_CONTEXT, REGLDAP_ARGS, &FnCallRegLDAP, "True if the regular expression in arg6 matches a value item in an ldap search",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("remotescalar", CF_DATA_TYPE_STRING, REMOTESCALAR_ARGS, &FnCallRemoteScalar, "Read a scalar value from a remote cfengine server",
                  FNCALL_OPTION_CACHED, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),

    FnCallTypeNew("remoteclassesmatching", CF_DATA_TYPE_CONTEXT, REMOTECLASSESMATCHING_ARGS, &FnCallRemoteClassesMatching, "Read persistent classes matching a regular expression from a remote cfengine server and add them into local context with prefix",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("returnszero", CF_DATA_TYPE_CONTEXT, RETURNSZERO_ARGS, &FnCallReturnsZero, "True if named shell command has exit status zero",
                  FNCALL_OPTION_CACHED | FNCALL_OPTION_UNSAFE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("rrange", CF_DATA_TYPE_REAL_RANGE, RRANGE_ARGS, &FnCallRRange, "Define a range of real numbers for cfengine internal use",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("reverse", CF_DATA_TYPE_STRING_LIST, REVERSE_ARGS, &FnCallReverse, "Reverse a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("selectservers", CF_DATA_TYPE_INT, SELECTSERVERS_ARGS, &FnCallSelectServers, "Select tcp servers which respond correctly to a query and return their number, set array of names",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("shuffle", CF_DATA_TYPE_STRING_LIST, SHUFFLE_ARGS, &FnCallShuffle, "Shuffle the items in a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("some", CF_DATA_TYPE_CONTEXT, EVERY_SOME_NONE_ARGS, &FnCallEverySomeNone, "True if an element in the list or array or data container matches the given regular expression",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sort", CF_DATA_TYPE_STRING_LIST, SORT_ARGS, &FnCallSort, "Sort a list or array or data container",
                  FNCALL_OPTION_COLLECTING | FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splayclass", CF_DATA_TYPE_CONTEXT, SPLAYCLASS_ARGS, &FnCallSplayClass, "True if the first argument's time-slot has arrived, according to a policy in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("splitstring", CF_DATA_TYPE_STRING_LIST, SPLITSTRING_ARGS, &FnCallSplitString, "Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_DEPRECATED),
    FnCallTypeNew("storejson", CF_DATA_TYPE_STRING, STOREJSON_ARGS, &FnCallStoreJson, "Convert a list or array or data container to a JSON string",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strcmp", CF_DATA_TYPE_CONTEXT, STRCMP_ARGS, &FnCallStrCmp, "True if the two strings match exactly",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("strftime", CF_DATA_TYPE_STRING, STRFTIME_ARGS, &FnCallStrftime, "Format a date and time string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sublist", CF_DATA_TYPE_STRING_LIST, SUBLIST_ARGS, &FnCallSublist, "Returns arg3 element from either the head or the tail (according to arg2) of list or array or data container arg1.",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sysctlvalue", CF_DATA_TYPE_STRING, SYSCTLVALUE_ARGS, &FnCallSysctlValue, "Returns a value for sysctl key arg1 pair",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("data_sysctlvalues", CF_DATA_TYPE_CONTAINER, DATA_SYSCTLVALUES_ARGS, &FnCallSysctlValue, "Returns a data container map of all the sysctl key,value pairs",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("translatepath", CF_DATA_TYPE_STRING, TRANSLATEPATH_ARGS, &FnCallTranslatePath, "Translate path separators from Unix style to the host's native",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("unique", CF_DATA_TYPE_STRING_LIST, UNIQUE_ARGS, &FnCallSetop, "Returns all the unique elements of list or array or data container arg1",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("usemodule", CF_DATA_TYPE_CONTEXT, USEMODULE_ARGS, &FnCallUseModule, "Execute cfengine module script and set class if successful",
                  FNCALL_OPTION_NONE | FNCALL_OPTION_UNSAFE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("userexists", CF_DATA_TYPE_CONTEXT, USEREXISTS_ARGS, &FnCallUserExists, "True if user name or numerical id exists on this host",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_SYSTEM, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("validdata", CF_DATA_TYPE_CONTEXT, VALIDDATA_ARGS, &FnCallValidData, "Check for errors in JSON or YAML data",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("validjson", CF_DATA_TYPE_CONTEXT, VALIDJSON_ARGS, &FnCallValidJson, "Check for errors in JSON data",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("variablesmatching", CF_DATA_TYPE_STRING_LIST, CLASSMATCH_ARGS, &FnCallVariablesMatching, "List the variables matching regex arg1 and tag regexes arg2,arg3,...",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("version_compare", CF_DATA_TYPE_CONTEXT, VERSION_COMPARE_ARGS, &FnCallVersionCompare, "Compare two version numbers with a specified operator",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_UTILS, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("findlocalusers", CF_DATA_TYPE_CONTAINER, FIND_LOCAL_USERS_ARGS, &FnCallFindLocalUsers, "Find matching local users",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // Functions section following new naming convention
    FnCallTypeNew("string_mustache", CF_DATA_TYPE_STRING, STRING_MUSTACHE_ARGS, &FnCallStringMustache, "Expand a Mustache template from arg1 into a string using the optional data container in arg2 or datastate()",
                  FNCALL_OPTION_COLLECTING|FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_split", CF_DATA_TYPE_STRING_LIST, SPLITSTRING_ARGS, &FnCallStringSplit, "Convert a string in arg1 into a list of at most arg3 strings by splitting on a regular expression in arg2",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_replace", CF_DATA_TYPE_STRING, STRING_REPLACE_ARGS, &FnCallStringReplace, "Search through arg1, replacing occurences of arg2 with arg3.",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("string_trim", CF_DATA_TYPE_STRING, STRING_TRIM_ARGS, &FnCallStringTrim, "Trim whitespace from beginning and end of string",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("regex_replace", CF_DATA_TYPE_STRING, REGEX_REPLACE_ARGS, &FnCallRegReplace, "Replace occurrences of arg1 in arg2 with arg3, allowing backreferences.  Perl-style options accepted in arg4.",
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
    FnCallTypeNew("length", CF_DATA_TYPE_INT, STAT_FOLD_ARGS, &FnCallLength, "Return the length of a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("max", CF_DATA_TYPE_STRING, SORT_ARGS, &FnCallFold, "Return the maximum value in a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("mean", CF_DATA_TYPE_REAL, STAT_FOLD_ARGS, &FnCallFold, "Return the mean (average) in a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("min", CF_DATA_TYPE_STRING, SORT_ARGS, &FnCallFold, "Return the minimum in a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("product", CF_DATA_TYPE_REAL, PRODUCT_ARGS, &FnCallFold, "Return the product of a list or array or data container of reals",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("sum", CF_DATA_TYPE_REAL, SUM_ARGS, &FnCallFold, "Return the sum of a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("variance", CF_DATA_TYPE_REAL, STAT_FOLD_ARGS, &FnCallFold, "Return the variance of a list or array or data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // CFEngine internal functions
    FnCallTypeNew("callstack_promisers", CF_DATA_TYPE_STRING_LIST, CFENGINE_PROMISERS_ARGS, &FnCallCFEngineCallers, "Get the list of promisers to the current promise execution path",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_INTERNAL, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("callstack_callers", CF_DATA_TYPE_CONTAINER, CFENGINE_CALLERS_ARGS, &FnCallCFEngineCallers, "Get the current promise execution stack in detail",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_INTERNAL, SYNTAX_STATUS_NORMAL),

                  // Data container functions
    FnCallTypeNew("data_regextract", CF_DATA_TYPE_CONTAINER, DATA_REGEXTRACT_ARGS, &FnCallRegExtract, "Matches the regular expression in arg 1 against the string in arg2 and returns a data container holding the backreferences by name",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("data_expand", CF_DATA_TYPE_CONTAINER, DATA_EXPAND_ARGS, &FnCallDataExpand, "Expands any CFEngine variables in a list or array or data container, converting to a data container",
                  FNCALL_OPTION_COLLECTING, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("variablesmatching_as_data", CF_DATA_TYPE_CONTAINER, CLASSMATCH_ARGS, &FnCallVariablesMatching, "Capture the variables matching regex arg1 and tag regexes arg2,arg3,... with their data",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    // File parsing functions that output a data container
    FnCallTypeNew("data_readstringarray", CF_DATA_TYPE_CONTAINER, DATA_READSTRINGARRAY_ARGS, &FnCallDataRead, "Read an array of strings from a file into a data container map, using the first element as a key",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("data_readstringarrayidx", CF_DATA_TYPE_CONTAINER, DATA_READSTRINGARRAY_ARGS, &FnCallDataRead, "Read an array of strings from a file into a data container array",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_IO, SYNTAX_STATUS_NORMAL),

    // Network probe functions
    FnCallTypeNew("network_connections", CF_DATA_TYPE_CONTAINER, NETWORK_CONNECTIONS_ARGS, &FnCallNetworkConnections, "Get the full list of TCP, TCP6, UDP, and UDP6 connections from /proc/net",
                  FNCALL_OPTION_NONE, FNCALL_CATEGORY_COMM, SYNTAX_STATUS_NORMAL),

    // Files functions
    FnCallTypeNew("findfiles_up", CF_DATA_TYPE_CONTAINER, FINDFILES_UP_ARGS, &FnCallFindfilesUp, "Find files matching a glob pattern by searching up the directory three from a given point in the tree structure",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("search_up", CF_DATA_TYPE_CONTAINER, FINDFILES_UP_ARGS, &FnCallFindfilesUp, "Hush... This is a super secret alias name for function 'findfiles_up'",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("isreadable", CF_DATA_TYPE_CONTEXT, ISREADABLE_ARGS, &FnCallIsReadable, "Check if file is readable. Timeout immediately or after optional timeout interval",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_FILES, SYNTAX_STATUS_NORMAL),

    // Datatype functions
    FnCallTypeNew("type", CF_DATA_TYPE_STRING, DATATYPE_ARGS, &FnCallDatatype, "Get type description as string",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),
    FnCallTypeNew("is_type", CF_DATA_TYPE_STRING, IS_DATATYPE_ARGS, &FnCallIsDatatype, "Compare type of variable with type",
                  FNCALL_OPTION_VARARG, FNCALL_CATEGORY_DATA, SYNTAX_STATUS_NORMAL),

    FnCallTypeNewNull()
};
#endif // CFENGINE_EVALFUNCTION_TEST_C
