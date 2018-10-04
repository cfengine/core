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


#include <iteration.h>

#include <scope.h>
#include <vars.h>
#include <fncall.h>
#include <eval_context.h>
#include <misc_lib.h>
#include <string_lib.h>
#include <assoc.h>
#include <expand.h>                                   /* ExpandScalar */
#include <conversion.h>                               /* DataTypeIsIterable */



/**
 * WHEELS
 *
 * The iteration engine for CFEngine is set up with a number of "wheels" that
 * roll in all combinations in order to iterate over everything - like the
 * combination lock found on suitcases. One wheel is added for each iterable
 * variable in the promiser-promisee-constraints strings. Iterable variables
 * are slists and containers. But wheels are created in other cases as well,
 * like variables that don't resolve yet but might change later on and
 * possibly become iterables.
 *
 * The wheels are in struct PromiseIterator_ and are added right after
 * initialisation of it, using PromiseIteratorPrepare() that calls ProcessVar().
 *
 * Wheels are added in the Seq in an order that matters: variables that depend
 * on others to expand are *on the right* of their dependencies. That means
 * that *independent variables are on the left*.
 *
 * EXAMPLE reports promise:
 *              "Value of A is $(A[$(i)][$(j)]) for indexes $(i) and $(j)"
 *
 * One appropriate wheels Seq for that would be:    i    j    A[$(i)][$(j)]
 *
 * So for that promise 3 wheels get generated, and always the dependent
 * variables are on the right of their dependencies. The wheels sequence would
 * be exactly the same if the reports promise was simply "$(A[$(i)][$(j)])",
 * because there are again the same 3 variables.
 */

/**
 * ITERATING
 *
 * We push a new iteration context for each iteration, and VariablePut() into
 * THIS context all selected single values of the iterable variables (slists
 * or containers) represented by the wheels.
 *
 * *Thus EvalContext "THIS" never contains iterables (lists or containers).*
 *
 * This presents a problem for absolute references like $(abs.var), since
 * these cannot be mapped into "this" without some magic (see MANGLING).
 *
 * The iteration context is popped and re-pushed for each iteration, until no
 * further combinations of the wheel variables are left to be selected.
 */

/**
 * SCOPE/NAMESPACE MANGLING
 *
 * One important thing to notice is that the variables that are
 * namespaced/scope need to be *mangled* in order to be added as wheels. This
 * means that the scope separator '.' and namespace separator ':' are replaced
 * with '#' and '*' respectively. This happens in ProcessVar(), see comments
 * for reasoning and further info.
 */



typedef struct {

    /* The unexpanded variable name, dependent on inner expansions. This
     * field never changes after Wheel initialisation. */
    char *varname_unexp;

    /* Number of dependencies of varname_unexp */
    //    const size_t deps;

    /* On each iteration of the wheels, the unexpanded string is
     * re-expanded, so the following is refilled, again and again. */
    char *varname_exp;

    /*
     * Values of varname_exp, to iterate on. WE DO NOT OWN THE RVALS, they
     * belong to EvalContext, so don't free(). Only if vartype is CONTAINER do
     * we own the strings and we must free() them.
     *
     * After the iteration engine has started (via PromiseIteratorNext())
     * "values" can be NULL when a variable does not resolve, or when it's
     * not an iterable but it's already there in EvalContext, so no need to
     * Put() separately; this means that it has exactly one value.
     *
     * When the variable resolves to an empty iterable (like empty slist or
     * container) then it's not NULL, but SeqLength(values)==0.
     *
     * TODO values==NULL should only be unresolved variable -
     *      non-iterable variable should be SeqLength()==1.
     */
    Seq *values;

    /* This is the list-type of the iterable variable, and this sets the type
     * of the elements stored in Seq values. Only possibilities are INTLIST,
     * REALLIST, SLIST, CONTAINER, NONE (if the variable did not resolve). */
    DataType vartype;

    size_t iter_index;                           /* current iteration index */

} Wheel;


struct PromiseIterator_ {
    Seq *wheels;
    const Promise *pp;                                   /* not owned by us */
    size_t count;                                 /* total iterations count */
};


/**
 * @NOTE #varname doesn't need to be '\0'-terminated, since the length is
 *                provided.
 */
static Wheel *WheelNew(const char *varname, size_t varname_len)
{
    Wheel new_wheel = {
        .varname_unexp = xstrndup(varname, varname_len),
        .varname_exp   = NULL,
        .values        = NULL,
        .vartype       = -1,
        .iter_index    = 0
    };

    return xmemdup(&new_wheel, sizeof(new_wheel));
}

static void WheelValuesSeqDestroy(Wheel *w)
{
    if (w->values != NULL)
    {
        /* Only if the variable resolved to type CONTAINER do we need to free
         * the values, since we trasformed it to a Seq of strings. */
        if (w->vartype == CF_DATA_TYPE_CONTAINER)
        {
            size_t values_len = SeqLength(w->values);
            for (size_t i = 0; i < values_len; i++)
            {
                char *value = SeqAt(w->values, i);
                free(value);
            }
        }
        SeqDestroy(w->values);
        w->values = NULL;
    }
    w->vartype = -1;
}

static void WheelDestroy(void *wheel)
{
    Wheel *w = wheel;
    free(w->varname_unexp);
    free(w->varname_exp);
    WheelValuesSeqDestroy(w);
    free(w);
}

/* Type of this function is SeqItemComparator for use in SeqLookup(). */
static int WheelCompareUnexpanded(const void *wheel1, const void *wheel2,
                                  void *user_data ARG_UNUSED)
{
    const Wheel *w1 = wheel1;
    const Wheel *w2 = wheel2;
    return strcmp(w1->varname_unexp, w2->varname_unexp);
}

PromiseIterator *PromiseIteratorNew(const Promise *pp)
{
    PromiseIterator iterctx = {
        .wheels = SeqNew(4, WheelDestroy),
        .pp     = pp,
        .count  = 0
    };
    return xmemdup(&iterctx, sizeof(iterctx));
}

void PromiseIteratorDestroy(PromiseIterator *iterctx)
{
    SeqDestroy(iterctx->wheels);
    free(iterctx);
}

size_t PromiseIteratorIndex(const PromiseIterator *iter_ctx)
{
    return iter_ctx->count;
}


/**
 * Returns offset to "$(" or "${" in the string.
 * Reads bytes up to s[max-1], s[max] is NOT read.
 * If a '\0' is encountered before the pattern, return offset to `\0` byte
 * If no '\0' byte or pattern is found within max bytes, max is returned
 */
static size_t FindDollarParen(const char *s, size_t max)
{
    size_t i = 0;

    while (i < max && s[i] != '\0')
    {
        if (i+1 < max && (s[i] == '$' && (s[i+1] == '(' || s[i+1] == '{')))
        {
            return i;
        }
        i++;
    }
    assert(i == max || s[i] == '\0');
    return i;
}

static char opposite(char c)
{
    switch (c)
    {
    case '(':  return ')';
    case '{':  return '}';
    default :  ProgrammingError("Was expecting '(' or '{' but got: '%c'", c);
    }
    return 0;
}

/**
 * Check if variable reference is mangled, while avoiding going into the inner
 * variables that are being expanded, or into array indexes.
 *
 * @NOTE variable name is naked, i.e. shouldn't start with dollar-paren.
 */
static bool IsMangled(const char *s)
{
    assert(s != NULL);
    size_t s_length = strlen(s);
    size_t dollar_paren = FindDollarParen(s, s_length);
    size_t bracket      = strchrnul(s, '[') - s;
    size_t upto = MIN(dollar_paren, bracket);
    size_t mangled_ns     = strchrnul(s, CF_MANGLED_NS)    - s;
    size_t mangled_scope  = strchrnul(s, CF_MANGLED_SCOPE) - s;

    if (mangled_ns    < upto ||
        mangled_scope < upto)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Mangle namespace and scope separators, up to '$(', '${', '[', '\0',
 * whichever comes first.
 *
 * "this" scope is never mangled, no need to VariablePut() a mangled reference
 * in THIS scope, since the non-manled one already exists.
 */
static void MangleVarRefString(char *ref_str, size_t len)
{
    //    printf("MangleVarRefString: %.*s\n", (int) len, ref_str);

    size_t dollar_paren = FindDollarParen(ref_str, len);
    size_t upto         = MIN(len, dollar_paren);
    char *bracket       = memchr(ref_str, '[', upto);
    if (bracket != NULL)
    {
        upto = bracket - ref_str;
    }

    char *ns = memchr(ref_str, ':', upto);
    char *ref_str2 = ref_str;
    if (ns != NULL)
    {
        *ns      = CF_MANGLED_NS;
        ref_str2 =  ns + 1;
        upto    -= (ns + 1 - ref_str);
        assert(upto >= 0);
    }

    bool mangled_scope = false;
    char *scope = memchr(ref_str2, '.', upto);
    if (scope != NULL    &&
        strncmp(ref_str2, "this", 4) != 0)
    {
        *scope = CF_MANGLED_SCOPE;
        mangled_scope = true;
    }

    if (mangled_scope || ns != NULL)
    {
        LogDebug(LOG_MOD_ITERATIONS,
                 "Mangled namespaced/scoped variable for iterating over it: %.*s",
                 (int) len, ref_str);
    }
}

/**
 * Lookup a variable within iteration context. Since the scoped or namespaced
 * variable names may be mangled, we have to look them up using special
 * separators CF_MANGLED_NS and CF_MANGLED_SCOPE.
 */
static const void *IterVariableGet(const PromiseIterator *iterctx,
                                   const EvalContext *evalctx,
                                   const char *varname, DataType *type)
{
    const void *value;
    const Bundle *bundle = PromiseGetBundle(iterctx->pp);

    /* Equivalent to:
           VarRefParseFromBundle(varname, PromiseGetBundle(iterctx->pp))

       but with custom namespace,scope separators. Even !IsMangled(varname) it
       should be resolved properly since the secondary separators shouldn't
       alter the result for an unqualified varname. */
    VarRef *ref =
        VarRefParseFromNamespaceAndScope(varname, bundle->ns, bundle->name,
                                         CF_MANGLED_NS, CF_MANGLED_SCOPE);
    value = EvalContextVariableGet(evalctx, ref, type);
    VarRefDestroy(ref);

    if (*type == CF_DATA_TYPE_NONE)                             /* did not resolve */
    {
        assert(value == NULL);

        if (!IsMangled(varname))
        {
            /* Lookup with no mangling, it might be a scoped/namespaced
             * variable that is not an iterable, so it was not mangled in
             * ProcessVar(). */
            VarRef *ref2 = VarRefParse(varname);
            value = EvalContextVariableGet(evalctx, ref2, type);
            VarRefDestroy(ref2);
        }
    }

    return value;
}

/* TODO this is ugly!!! mapdata() needs to be refactored to put a whole slist
        as "this.k". But how? It is executed *after* PromiseIteratorNext()! */
static bool VarIsSpecial(const char *s)
{
    if (strcmp(s, "this") == 0       ||
        strcmp(s, "this.k") == 0     ||
        strcmp(s, "this.v") == 0     ||
        strcmp(s, "this.k[1]") == 0  ||
        strcmp(s, "this.this") == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Decide whether to mangle varname and add wheel to the iteration engine.
 *
 * If variable contains inner expansions -> mangle and add wheel
 *        (because you don't know if it will be an iterable or not - you will
 *        know after inner variable is iterated and the variable is looked up)
 *
 * else if it resolves to iterable       -> mangle and add wheel
 *
 * else if it resolves to empty iterable -> mangle and add wheel
 *                                          (see comments in code)
 *
 * else if the variable name is special for some functions (this.k etc)
 *                                       -> mangle and add wheel
 *
 * else if it resolves to non-iterable   -> no mangle, no wheel
 *
 * else if it doesn't resolve            -> no mangle, no wheel
 *
 * @NOTE Important special scopes (e.g. "connection.ip" for cf-serverd) must
 *       not be mangled to work correctly. This is auto-OK because such
 *       variables do not resolve usually.
 */
static bool ShouldAddVariableAsIterationWheel(
    const PromiseIterator *iterctx,
    const EvalContext *evalctx,
    char *varname, size_t varname_len)
{
    bool result;
    /* Shorten string temporarily to the appropriate length. */
    char tmp_c = varname[varname_len];
    varname[varname_len] = '\0';

    VarRef *ref = VarRefParseFromBundle(varname,
                                        PromiseGetBundle(iterctx->pp));
    DataType t;
    ARG_UNUSED const void *value = EvalContextVariableGet(evalctx, ref, &t);
    VarRefDestroy(ref);

    size_t dollar_paren = FindDollarParen(varname, varname_len);
    if (dollar_paren < varname_len)
    {
        /* Varname contains inner expansions, so maybe the variable will
         * resolve to an iterable during the iteration - must add wheel. */
        result = true;
    }
    else if (DataTypeIsIterable(t))
    {
        result = true;

        /* NOTE: If it is an EMPTY ITERABLE i.e. value==NULL, we are still
         * adding an iteration wheel, but with "wheel->values" set to an empty
         * Seq. The reason is that the iteration engine will completely *skip*
         * all promise evaluations when one of the wheels is empty.
         *
         * Otherwise, if we didn't add the empty wheel, even if the promise
         * contained no other wheels, the promise would get evaluated exactly
         * once with "$(varname)" literally in there. */
    }
    else if (VarIsSpecial(varname))
    {
        result = true;
    }
    else
    {
        /*
         * Either varname resolves to a non-iterable, e.g. string.
         * Or it does not resolve.
         *
         * Since this variable does not contain inner expansions, this can't
         * change during iteration of other variables. So don't add wheel -
         * i.e. don't iterate over this variable's values, because we know
         * there will always be only one value.
         */
        result = false;
    }

    varname[varname_len] = tmp_c;                /* Restore original string */
    return result;
}

/**
 * Recursive function that adds wheels to the iteration engine, according to
 * the variable (and possibly its inner variables) in #s.
 *
 * Another important thing it does, is *modify* the string #s, mangling all
 * scoped or namespaced variable names. Mangling is done in order to iterate
 * over foreign variables, without modifying the foreign value. For example if
 * "test.var" is an slist, then we mangle it as "test#var" and on each
 * iteration we just VariablePut(test#var) in the local scope.
 * Mangling is skipped for variables that do not resolve, since they are not
 * to be iterated over.
 *
 * @param s is the start of a variable name, right after "$(" or "${".
 * @param c is the character after '$', i.e. must be either '(' or '{'.
 * @return pointer to the closing parenthesis or brace of the variable, or
 *         if not found, returns a pointer to terminating '\0' of #s.
 */
static char *ProcessVar(PromiseIterator *iterctx, const EvalContext *evalctx,
                        char *s, char c)
{
    assert(s != NULL);
    assert(c == '(' || c == '{');

    char closing_paren   = opposite(c);
    char *s_end    = strchrnul(s, closing_paren);
    const size_t s_max = strlen(s);
    char *next_var = s + FindDollarParen(s, s_max);
    size_t deps    = 0;

    while (next_var < s_end)              /* does it have nested variables? */
    {
        /* It's a dependent variable, the wheels of the dependencies must be
         * added first. Example: "$(blah_$(dependency))" */

        assert(next_var[0] != '\0');
        assert(next_var[1] != '\0');

        char *subvar_end = ProcessVar(iterctx, evalctx,
                                      &next_var[2], next_var[1]);

        /* Was there unbalanced paren for the inner expansion? */
        if (*subvar_end == '\0')
        {
            /* Despite unclosed parenthesis for the inner expansion,
             * the outer variable might close with a brace, or not. */
            const size_t s_end_len = strlen(s_end);
            next_var = s_end + FindDollarParen(s_end, s_end_len);
            /* s_end is already correct */
        }
        else                          /* inner variable processed correctly */
        {
            /* This variable depends on inner expansions. */
            deps++;
            /* We are sure (subvar_end+1) is not out of bounds. */
            char *s_next = subvar_end + 1;
            const size_t s_next_len = strlen(s_next);
            s_end    = strchrnul(s_next, closing_paren);
            next_var = s_next + FindDollarParen(s_next, s_next_len);
        }
    }

    if (*s_end == '\0')
    {
        Log(LOG_LEVEL_ERR, "No closing '%c' found for variable: %s",
            opposite(c), s);
        return s_end;
    }

    const size_t s_len = s_end - s;

    if (ShouldAddVariableAsIterationWheel(iterctx, evalctx, s, s_len))
    {
        /* Change the variable name in order to mangle namespaces and scopes. */
        MangleVarRefString(s, s_len);

        Wheel *new_wheel = WheelNew(s, s_len);

        /* If identical variable is already inserted, it means that it has
         * been seen before and has been inserted together with all
         * dependencies; skip. */
        /* It can happen if variables exist twice in a string, for example:
           "$(i) blah $(A[$(i)])" has i variable twice. */

        bool same_var_found = (SeqLookup(iterctx->wheels, new_wheel,
                                         WheelCompareUnexpanded)  !=  NULL);
        if (same_var_found)
        {
            LogDebug(LOG_MOD_ITERATIONS,
                "Skipped adding iteration wheel for already existing variable: %s",
                new_wheel->varname_unexp);
            WheelDestroy(new_wheel);
        }
        else
        {
            /* If this variable is dependent on other variables, we've already
             * appended the wheels of the dependencies during the recursive
             * calls. Or it happens and this is an independent variable. So
             * now APPEND the wheel for this variable. */
            SeqAppend(iterctx->wheels, new_wheel);

            LogDebug(LOG_MOD_ITERATIONS,
                "Added iteration wheel %zu for variable: %s",
                SeqLength(iterctx->wheels) - 1,
                new_wheel->varname_unexp);
        }
    }

    assert(*s_end == closing_paren);
    return s_end;
}

/**
 * @brief Fills up the wheels of the iterator according to the variables
 *         found in #s. Also mangles all namespaced/scoped variables in #s.
 *
 * @EXAMPLE Have a look in iteration_test.c:test_PromiseIteratorPrepare()
 *
 * @NOTE the wheel numbers can't change once iteration started, so make sure
 *       you call WheelIteratorPrepare() in advance, as many times it's
 *       needed.
 */
void PromiseIteratorPrepare(PromiseIterator *iterctx,
                            const EvalContext *evalctx,
                            char *s)
{
    assert(s != NULL);
    LogDebug(LOG_MOD_ITERATIONS, "PromiseIteratorPrepare(\"%s\")", s);
    const size_t s_len = strlen(s);
    char *var_start = s + FindDollarParen(s, s_len);

    while (*var_start != '\0')
    {
        char paren_or_brace = var_start[1];
        var_start += 2;                                /* skip dollar-paren */

        assert(paren_or_brace == '(' || paren_or_brace == '{');

        char *var_end = ProcessVar(iterctx, evalctx, var_start, paren_or_brace);
        char *var_next = var_end + 1;
        const size_t var_next_len = s_len - (var_next - s);

        var_start = var_next + FindDollarParen(var_next, var_next_len);
    }
}

static void IterListElementVariablePut(EvalContext *evalctx,
                                       const char *varname,
                                       DataType listtype, void *value)
{
    DataType t;

    switch (listtype)
    {
    case CF_DATA_TYPE_CONTAINER:   t = CF_DATA_TYPE_STRING; break;
    case CF_DATA_TYPE_STRING_LIST: t = CF_DATA_TYPE_STRING; break;
    case CF_DATA_TYPE_INT_LIST:    t = CF_DATA_TYPE_INT;    break;
    case CF_DATA_TYPE_REAL_LIST:   t = CF_DATA_TYPE_REAL;   break;
    default:
        t = CF_DATA_TYPE_NONE;                           /* silence warning */
        ProgrammingError("IterVariablePut() invalid type: %d",
                         listtype);
    }

    EvalContextVariablePutSpecial(evalctx, SPECIAL_SCOPE_THIS,
                                  varname, value,
                                  t, "source=promise_iteration");
}

static void SeqAppendContainerPrimitive(Seq *seq, const JsonElement *primitive)
{
    assert(JsonGetElementType(primitive) == JSON_ELEMENT_TYPE_PRIMITIVE);

    switch (JsonGetPrimitiveType(primitive))
    {
    case JSON_PRIMITIVE_TYPE_BOOL:
        SeqAppend(seq, (JsonPrimitiveGetAsBool(primitive) ?
                        xstrdup("true") : xstrdup("false")));
        break;
    case JSON_PRIMITIVE_TYPE_INTEGER:
    {
        char *str = StringFromLong(JsonPrimitiveGetAsInteger(primitive));
        SeqAppend(seq, str);
        break;
    }
    case JSON_PRIMITIVE_TYPE_REAL:
    {
        char *str = StringFromDouble(JsonPrimitiveGetAsReal(primitive));
        SeqAppend(seq, str);
        break;
    }
    case JSON_PRIMITIVE_TYPE_STRING:
        SeqAppend(seq, xstrdup(JsonPrimitiveGetAsString(primitive)));
        break;

    case JSON_PRIMITIVE_TYPE_NULL:
        break;
    }
}

static Seq *ContainerToSeq(const JsonElement *container)
{
    Seq *seq = SeqNew(5, NULL);

    switch (JsonGetElementType(container))
    {
    case JSON_ELEMENT_TYPE_PRIMITIVE:
        SeqAppendContainerPrimitive(seq, container);
        break;

    case JSON_ELEMENT_TYPE_CONTAINER:
    {
        JsonIterator iter = JsonIteratorInit(container);
        const JsonElement *child;

        while ((child = JsonIteratorNextValue(&iter)) != NULL)
        {
            if (JsonGetElementType(child) == JSON_ELEMENT_TYPE_PRIMITIVE)
            {
                SeqAppendContainerPrimitive(seq, child);
            }
        }
        break;
    }
    }

    /* TODO SeqFinalise() to save space? */
    return seq;
}

static Seq *RlistToSeq(const Rlist *p)
{
    Seq *seq = SeqNew(5, NULL);

    const Rlist *rlist = p;
    while(rlist != NULL)
    {
        Rval val = rlist->val;
        SeqAppend(seq, val.item);
        rlist = rlist->next;
    }

    /* TODO SeqFinalise() to save space? */
    return seq;
}

static Seq *IterableToSeq(const void *v, DataType t)
{
    switch (t)
    {
    case CF_DATA_TYPE_CONTAINER:
        return ContainerToSeq(v);
        break;
    case CF_DATA_TYPE_STRING_LIST:
    case CF_DATA_TYPE_INT_LIST:
    case CF_DATA_TYPE_REAL_LIST:
        /* All lists are stored as Rlist internally. */
        assert(DataTypeToRvalType(t) == RVAL_TYPE_LIST);
        return RlistToSeq(v);

    default:
        ProgrammingError("IterableToSeq() got non-iterable type: %d", t);
    }
}

/**
 * For each of the wheels to the right of wheel_idx (including this one)
 *
 * 1. varname_exp = expand the variable name
 *    - if it's same with previous varname_exp, skip steps 2-4
 * 2. values = VariableGet(varname_exp);
 * 3. if the value is an iterable (slist/container), set the wheel size.
 * 4. reset the wheel in order to re-iterate over all combinations.
 * 5. Put(varname_exp:first_value) in the EvalContext
 */
static void ExpandAndPutWheelVariablesAfter(
    const PromiseIterator *iterctx,
    EvalContext *evalctx,
    size_t wheel_idx)
{
    /* Buffer to store the expanded wheel variable name, for each wheel. */
    Buffer *tmpbuf = BufferNew();

    size_t wheels_num = SeqLength(iterctx->wheels);
    for (size_t i = wheel_idx; i < wheels_num; i++)
    {
        Wheel *wheel = SeqAt(iterctx->wheels, i);
        BufferClear(tmpbuf);

        /* Reset wheel in order to re-iterate over all combinations. */
        wheel->iter_index = 0;

        /* The wheel variable may depend on previous wheels, for example
         * "B_$(k)_$(v)" is dependent on variables "k" and "v", which are
         * wheels already set (to the left, or at lower i index). */
        const char *varname = ExpandScalar(evalctx,
                                           PromiseGetNamespace(iterctx->pp),
        /* Use NULL as scope so that we try both "this" and "bundle" scopes. */
                                           NULL,
                                           wheel->varname_unexp, tmpbuf);

        /* If it expanded to something different than before. */
        if (wheel->varname_exp == NULL
            || strcmp(varname, wheel->varname_exp) != 0)
        {
            free(wheel->varname_exp);                      /* could be NULL */
            wheel->varname_exp = xstrdup(varname);

            WheelValuesSeqDestroy(wheel);           /* free previous values */

            /* After expanding the variable name, we have to lookup its value,
               and set the size of the wheel if it's an slist or container. */
            DataType value_type;
            const void *value = IterVariableGet(iterctx, evalctx,
                                                varname, &value_type);
            wheel->vartype = value_type;

            /* Set wheel values and size according to variable type. */
            if (DataTypeIsIterable(value_type))
            {
                wheel->values = IterableToSeq(value, value_type);

                if (SeqLength(wheel->values) == 0)
                {
                    /*
                     * If this variable now expands to a 0-length list, then
                     * we should skip this iteration, no matter the
                     * other variables: "zero times whatever" multiplication
                     * always equals zero.
                     */
                    Log(LOG_LEVEL_VERBOSE,
                        "Skipping iteration since variable '%s'"
                        " resolves to an empty list", varname);
                }
                else
                {
                    assert(          wheel->values     != NULL);
                    assert(SeqLength(wheel->values)     > 0);
                    assert(    SeqAt(wheel->values, 0) != NULL);

                    /* Put the first value of the iterable. */
                    IterListElementVariablePut(evalctx, varname, value_type,
                                               SeqAt(wheel->values, 0));
                }
            }
            /* It it's NOT AN ITERABLE BUT IT RESOLVED AND IT IS MANGLED: this
             * is possibly a variable that was unresolvable during the
             * Prepare() stage, but now resolves to a string etc. We still
             * need to Put() it despite not being an iterable, since the
             * mangled version is not in the EvalContext.
             * The "values" Seq is left as NULL. */
            else if (value_type != CF_DATA_TYPE_NONE && IsMangled(varname))
            {
                EvalContextVariablePutSpecial(evalctx, SPECIAL_SCOPE_THIS,
                                              varname, value, value_type,
                                              "source=promise_iteration");
            }
            /* It's NOT AN ITERABLE AND IT'S NOT MANGLED, which means that
             * the variable with the correct value (the only value) is already
             * in the EvalContext, no need to Put() it again. */
            /* OR it doesn't resolve at all! */
            else
            {
                /* DO NOTHING, everything is already set. */

                assert(!DataTypeIsIterable(value_type));
                assert(value_type == CF_DATA_TYPE_NONE ||  /* var does not resolve */
                       !IsMangled(varname));               /* or is not mangled */
                /* We don't allocate Seq for non-iterables. */
                assert(wheel->values == NULL);
            }
        }
        else                 /* The variable name expanded to the same name */
        {
            /* speedup: the variable name expanded to the same name, so the
             * value is the same and wheel->values is already correct. So if
             * it's an iterable, we VariablePut() the first element. */
            if (wheel->values != NULL && SeqLength(wheel->values) > 0)
            {
                /* Put the first value of the iterable. */
                IterListElementVariablePut(evalctx,
                                           wheel->varname_exp, wheel->vartype,
                                           SeqAt(wheel->values, 0));
            }
        }
    }

    BufferDestroy(tmpbuf);
}

static bool IteratorHasEmptyWheel(const PromiseIterator *iterctx)
{
    size_t wheels_num = SeqLength(iterctx->wheels);
    for (size_t i = 0; i < wheels_num; i++)
    {
        Wheel *wheel  = SeqAt(iterctx->wheels, i);
        assert(wheel != NULL);

        if (VarIsSpecial(wheel->varname_unexp))       /* TODO this is ugly! */
        {
            return false;
        }

        /* If variable resolves to an empty iterable or it doesn't resolve. */
        if ((wheel->values != NULL &&
             SeqLength(wheel->values) == 0)
            ||
            wheel->vartype == CF_DATA_TYPE_NONE)
        {
            return true;
        }
    }

    return false;
}

/* Try incrementing the rightmost wheel first that has values left to iterate on.
   (rightmost  i.e. the most dependent variable). */
static size_t WheelRightmostIncrement(PromiseIterator *iterctx)
{
    size_t wheels_num = SeqLength(iterctx->wheels);
    size_t i          = wheels_num;
    Wheel *wheel;

    assert(wheels_num > 0);

    do
    {
        if (i == 0)
        {
            return (size_t) -1;       /* all wheels have been iterated over */
        }

        i--;                                  /* move one wheel to the left */
        wheel = SeqAt(iterctx->wheels, i);
        wheel->iter_index++;

    /* Stop when we have found a wheel with value available at iter_index. */
    } while (wheel->values == NULL ||
             wheel->vartype == CF_DATA_TYPE_NONE ||
             SeqLength(wheel->values) == 0 ||
             wheel->iter_index >= SeqLength(wheel->values));

    return i;                         /* return which wheel was incremented */
}

/* Nothing to iterate on, so get out after running the promise once.
 * Because all promises, even if there are zero variables to be
 * expanded in them, must be evaluated. */
static bool RunOnlyOnce(PromiseIterator *iterctx)
{
    assert(SeqLength(iterctx->wheels) == 0);

    if (iterctx->count == 0)
    {
        iterctx->count++;
        return true;
    }
    else
    {
        return false;
    }
}

bool PromiseIteratorNext(PromiseIterator *iterctx, EvalContext *evalctx)
{
    size_t wheels_num = SeqLength(iterctx->wheels);

    if (wheels_num == 0)
    {
        return RunOnlyOnce(iterctx);
    }

    bool done = false;

    /* First iteration: we initialise all wheels. */
    if (iterctx->count == 0)
    {
        Log(LOG_LEVEL_DEBUG, "Starting iteration engine with %zu wheels"
            "   ---   ENTERING WARP SPEED",
            wheels_num);

        ExpandAndPutWheelVariablesAfter(iterctx, evalctx, 0);

        done = ! IteratorHasEmptyWheel(iterctx);
    }

    while (!done)
    {
        size_t i = WheelRightmostIncrement(iterctx);
        if (i == (size_t) -1)       /* all combinations have been tried */
        {
            Log(LOG_LEVEL_DEBUG, "Iteration engine finished"
                "   ---   WARPING OUT");
            return false;
        }

        /*
         * Alright, incrementing the wheel at index "i" was successful. Now
         * Put() the new value of the variable in the EvalContext. This is the
         * *basic iteration step*, just going to the next value of the
         * iterable.
         */
        Wheel *wheel    = SeqAt(iterctx->wheels, i);
        void *new_value = SeqAt(wheel->values, wheel->iter_index);

        IterListElementVariablePut(
            evalctx, wheel->varname_exp, wheel->vartype, new_value);

        /* All the wheels to the right of the one we changed have to be reset
         * and recomputed, in order to do all possible combinations. */
        ExpandAndPutWheelVariablesAfter(iterctx, evalctx, i + 1);

        /* If any of the wheels has no values to offer, then this iteration
         * should be skipped completely; so the function doesn't yield any
         * result yet, it just loops over until it finds a meaningful one. */
        done = ! IteratorHasEmptyWheel(iterctx);

        LogDebug(LOG_MOD_ITERATIONS, "PromiseIteratorNext():"
                 " count=%zu wheels_num=%zu current_wheel=%zd",
                 iterctx->count, wheels_num, (ssize_t) i);

    /* TODO if not done, then we are re-Put()ing variables in the EvalContect,
     *      hopefully overwriting the previous values, but possibly not! */
    }

    // Recompute `with`
    for (size_t i = 0; i < SeqLength(iterctx->pp->conlist); i++)
    {
        Constraint *cp = SeqAt(iterctx->pp->conlist, i);
        if (StringSafeEqual(cp->lval, "with"))
        {
            Rval final = EvaluateFinalRval(evalctx, PromiseGetPolicy(iterctx->pp), NULL,
                                           "this", cp->rval, false, iterctx->pp);
            if (final.type == RVAL_TYPE_SCALAR && !IsCf3VarString(RvalScalarValue(final)))
            {
                EvalContextVariablePutSpecial(evalctx, SPECIAL_SCOPE_THIS,
                                              "with", RvalScalarValue(final),
                                              CF_DATA_TYPE_STRING,
                                              "source=promise_iteration/with");
            }
            else
            {
                RvalDestroy(final);
            }
        }
    }
    iterctx->count++;
    return true;
}
