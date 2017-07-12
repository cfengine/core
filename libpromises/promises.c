/*
   Copyright 2017 Northern.tech AS

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

#include <promises.h>

#include <policy.h>
#include <syntax.h>
#include <expand.h>
#include <files_names.h>
#include <scope.h>
#include <vars.h>
#include <locks.h>
#include <misc_lib.h>
#include <fncall.h>
#include <eval_context.h>
#include <string_lib.h>
#include <audit.h>

static void AddDefaultBodiesToPromise(EvalContext *ctx, Promise *promise, const PromiseTypeSyntax *syntax);

void CopyBodyConstraintsToPromise(EvalContext *ctx, Promise *pp,
                                  const Body *bp)
{
    for (size_t k = 0; k < SeqLength(bp->conlist); k++)
    {
        Constraint *scp = SeqAt(bp->conlist, k);

        if (IsDefinedClass(ctx, scp->classes))
        {
            Rval returnval = ExpandPrivateRval(ctx, NULL, "body",
                                               scp->rval.item, scp->rval.type);
            PromiseAppendConstraint(pp, scp->lval, returnval, false);
        }
    }
}

/**
 * Get a map that rewrites body according to parameters.
 *
 * @NOTE make sure you free the returned map with JsonDestroy().
 */
static JsonElement *GetBodyRewriter(const EvalContext *ctx,
                                    const Body *current_body,
                                    const Rval *called_rval,
                                    bool in_inheritance_chain)
{
    size_t given_args = 0;
    JsonElement *arg_rewriter = JsonObjectCreate(2);

    if (called_rval == NULL)
    {
        // nothing needed, this is not an inherit_from rval
    }
    else if (called_rval->type == RVAL_TYPE_SCALAR)
    {
        // We leave the parameters as they were.

        // Unless the current body matches the
        // parameters of the inherited body, there
        // will be unexpanded variables. But the
        // alternative is to match up body and fncall
        // arguments, which is not trivial.
    }
    else if (called_rval->type == RVAL_TYPE_FNCALL)
    {
        const Rlist *call_args = RvalFnCallValue(*called_rval)->args;
        const Rlist *body_args = current_body->args;

        given_args = RlistLen(call_args);

        while (call_args != NULL &&
               body_args != NULL)
        {
            JsonObjectAppendString(arg_rewriter,
                                   RlistScalarValue(body_args),
                                   RlistScalarValue(call_args));
            call_args = call_args->next;
            body_args = body_args->next;
        }
    }

    size_t required_args = RlistLen(current_body->args);
    // only check arguments for inherited bodies
    if (in_inheritance_chain && required_args != given_args)
    {
        FatalError(ctx,
                   "Argument count mismatch for body "
                   "(gave %zu arguments) vs. inherited body '%s:%s' "
                   "(requires %zu arguments)",
                   given_args,
                   current_body->ns, current_body->name, required_args);
    }

    return arg_rewriter;
}

/**
 * Appends expanded bodies to the promise #pcopy. It expands the bodies based
 * on arguments, inheritance, and it can optionally flatten the '@' slists and
 * expand the variables in the body according to the EvalContext.
 */
static void AppendExpandedBodies(EvalContext *ctx, Promise *pcopy,
                                 const Seq *bodies_and_args,
                                 bool flatten_slists, bool expand_body_vars)
{
    size_t ba_len = SeqLength(bodies_and_args);

    /* Iterate over all parent bodies, and finally over the body of the
     * promise itself, expanding arguments.  We have already reversed the Seq
     * so we start with the most distant parent in the inheritance tree. */
    for (size_t i = 0; i < ba_len; i += 2)
    {
        const Rval *called_rval  = SeqAt(bodies_and_args, i);
        const Body *current_body = SeqAt(bodies_and_args, i + 1);
        bool in_inheritance_chain= (ba_len - i > 2);

        JsonElement *arg_rewriter =
            GetBodyRewriter(ctx, current_body, called_rval,
                            in_inheritance_chain);

        size_t constraints_num = SeqLength(current_body->conlist);
        for (size_t k = 0; k < constraints_num; k++)
        {
            const Constraint *scp = SeqAt(current_body->conlist, k);

            // we don't copy the inherit_from attribute or associated call
            if (strcmp("inherit_from", scp->lval) == 0)
            {
                continue;
            }

            if (IsDefinedClass(ctx, scp->classes))
            {
                /* We copy the Rval expanding all, including inherited,
                 * body arguments. */
                Rval newrv = RvalCopyRewriter(scp->rval, arg_rewriter);

                /* Expand '@' slists. */
                if (flatten_slists && newrv.type == RVAL_TYPE_LIST)
                {
                    RlistFlatten(ctx, (Rlist **) &newrv.item);
                }

                /* Expand body vars; note it has to happen ONLY ONCE. */
                if (expand_body_vars)
                {
                    Rval newrv2 = ExpandPrivateRval(ctx, NULL, "body",
                                                    newrv.item, newrv.type);
                    RvalDestroy(newrv);
                    newrv = newrv2;
                }

                /* PromiseAppendConstraint() overwrites existing constraints,
                   thus inheritance just works, as it correctly overwrites
                   parents' constraints. */
                Constraint *scp_copy =
                    PromiseAppendConstraint(pcopy, scp->lval,
                                            newrv, false);
                scp_copy->offset = scp->offset;

                char *rval_s     = RvalToString(scp->rval);
                char *rval_exp_s = RvalToString(scp_copy->rval);
                Log(LOG_LEVEL_DEBUG, "DeRefCopyPromise():         "
                    "expanding constraint '%s': '%s' -> '%s'",
                    scp->lval, rval_s, rval_exp_s);
                free(rval_exp_s);
                free(rval_s);
            }
        } /* for all body constraints */

        JsonDestroy(arg_rewriter);
    }
}

/**
 * Copies the promise, expanding the constraints.
 *
 * 1. copy the promise itself
 * 2. copy constraints: copy the bodies expanding arguments passed
 *    (arg_rewrite), copy the bundles, copy the rest of the constraints
 * 3. flatten '@' slists everywhere
 * 4. handle body inheritance
 */
Promise *DeRefCopyPromise(EvalContext *ctx, const Promise *pp)
{
    Log(LOG_LEVEL_DEBUG, "DeRefCopyPromise(): "
        "promiser:'%s'",
        SAFENULL(pp->promiser));

    Promise *pcopy = xcalloc(1, sizeof(Promise));

    if (pp->promiser)
    {
        pcopy->promiser = xstrdup(pp->promiser);
    }

    /* Copy promisee (if not NULL) while expanding '@' slists. */
    pcopy->promisee = RvalCopy(pp->promisee);
    if (pcopy->promisee.type == RVAL_TYPE_LIST)
    {
        RlistFlatten(ctx, (Rlist **) &pcopy->promisee.item);
    }

    if (pp->promisee.item != NULL)
    {
        char *promisee_string = RvalToString(pp->promisee);

        CF_ASSERT(pcopy->promisee.item != NULL,
                  "DeRefCopyPromise: Failed to copy promisee: %s",
                  promisee_string);
        Log(LOG_LEVEL_DEBUG, "DeRefCopyPromise():     "
            "expanded promisee: '%s'",
            promisee_string);
        free(promisee_string);
    }

    assert(pp->classes);
    pcopy->classes             = xstrdup(pp->classes);
    pcopy->parent_promise_type = pp->parent_promise_type;
    pcopy->offset.line         = pp->offset.line;
    pcopy->comment             = pp->comment ? xstrdup(pp->comment) : NULL;
    pcopy->conlist             = SeqNew(10, ConstraintDestroy);
    pcopy->org_pp              = pp->org_pp;
    pcopy->offset              = pp->offset;

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);
        const Policy *policy = PolicyFromPromise(pp);

        /* bodies_and_args: Do we have body to expand, possibly with arguments?
         * At position 0 we'll have the body, then its rval, then the same for
         * each of its inherit_from parents. */
        Seq *bodies_and_args       = NULL;
        const Rlist *args          = NULL;
        const char *body_reference = NULL;

        /* A body template reference could look like a scalar or fn to the parser w/w () */
        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:
            if (cp->references_body)
            {
                body_reference = RvalScalarValue(cp->rval);
                bodies_and_args = EvalContextResolveBodyExpression(ctx, policy, body_reference, cp->lval);
            }
            args = NULL;
            break;
        case RVAL_TYPE_FNCALL:
            body_reference = RvalFnCallValue(cp->rval)->name;
            bodies_and_args = EvalContextResolveBodyExpression(ctx, policy, body_reference, cp->lval);
            args = RvalFnCallValue(cp->rval)->args;
            break;
        default:
            break;
        }

        /* First case is: we have a body to expand lval = body(args). */

        if (bodies_and_args != NULL &&
            SeqLength(bodies_and_args) > 0)
        {
            const Body *bp = SeqAt(bodies_and_args, 0);
            assert(bp != NULL);

            SeqReverse(bodies_and_args); // when we iterate, start with the furthest parent

            EvalContextStackPushBodyFrame(ctx, pcopy, bp, args);

            if (strcmp(bp->type, cp->lval) != 0)
            {
                Log(LOG_LEVEL_ERR,
                    "Body type mismatch for body reference '%s' in promise "
                    "at line %zu of file '%s', '%s' does not equal '%s'",
                    body_reference, pp->offset.line,
                    PromiseGetBundle(pp)->source_path, bp->type, cp->lval);
            }

            Log(LOG_LEVEL_DEBUG, "DeRefCopyPromise():     "
                "copying body %s: '%s'",
                cp->lval, body_reference);

            if (IsDefinedClass(ctx, cp->classes))
            {
                /* For new package promises we need to have name of the
                 * package_manager body. */
                char body_name[strlen(cp->lval) + 6];
                xsnprintf(body_name, sizeof(body_name), "%s_name", cp->lval);
                PromiseAppendConstraint(pcopy, body_name,
                       (Rval) {xstrdup(bp->name), RVAL_TYPE_SCALAR }, false);

                /* Keep the referent body type as a boolean for convenience
                 * when checking later. */
                PromiseAppendConstraint(pcopy, cp->lval,
                       (Rval) {xstrdup("true"), RVAL_TYPE_SCALAR }, false);
            }

            if (bp->args)                  /* There are arguments to insert */
            {
                if (!args)
                {
                    Log(LOG_LEVEL_ERR,
                        "Argument mismatch for body reference '%s' in promise "
                        "at line %zu of file '%s'",
                        body_reference, pp->offset.line,
                        PromiseGetBundle(pp)->source_path);
                }

                AppendExpandedBodies(ctx, pcopy, bodies_and_args,
                                     false, true);
            }
            else                    /* No body arguments or body undeclared */
            {
                if (args)                                /* body undeclared */
                {
                    Log(LOG_LEVEL_ERR,
                        "Apparent body '%s' was undeclared or could "
                        "have incorrect args, but used in a promise near "
                        "line %zu of %s (possible unquoted literal value)",
                        RvalScalarValue(cp->rval), pp->offset.line,
                        PromiseGetBundle(pp)->source_path);
                }
                else /* no body arguments, but maybe the inherited bodies have */
                {
                    AppendExpandedBodies(ctx, pcopy, bodies_and_args,
                                         true, false);
                }
            }

            EvalContextStackPopFrame(ctx);
            SeqDestroy(bodies_and_args);
        }
        else                                    /* constraint is not a body */
        {
            if (cp->references_body)
            {
                // assume this is a typed bundle (e.g. edit_line)
                const Bundle *callee =
                    EvalContextResolveBundleExpression(ctx, policy,
                                                       body_reference,
                                                       cp->lval);
                if (!callee)
                {
                    // otherwise, assume this is a method-type call
                    callee = EvalContextResolveBundleExpression(ctx, policy,
                                                                body_reference,
                                                                "agent");
                    if (!callee)
                    {
                        callee = EvalContextResolveBundleExpression(ctx, policy,
                                                                    body_reference,
                                                                    "common");
                    }
                }

                if (callee == NULL &&
                    strcmp("ifvarclass", cp->lval) != 0 &&
                    strcmp("if",         cp->lval) != 0)
                {
                    Log(LOG_LEVEL_ERR,
                        "Apparent bundle '%s' was undeclared, but "
                        "used in a promise near line %zu of %s "
                        "(possible unquoted literal value)",
                        RvalScalarValue(cp->rval), pp->offset.line,
                        PromiseGetBundle(pp)->source_path);
                }

                Log(LOG_LEVEL_DEBUG,
                    "DeRefCopyPromise():     copying bundle: '%s'",
                    body_reference);
            }
            else
            {
                Log(LOG_LEVEL_DEBUG,
                    "DeRefCopyPromise():     copying constraint: '%s'",
                    cp->lval);
            }

            /* For all non-body constraints: copy the Rval expanding the
             * '@' list variables. */

            if (IsDefinedClass(ctx, cp->classes))
            {
                Rval newrv = RvalCopy(cp->rval);
                if (newrv.type == RVAL_TYPE_LIST)
                {
                    RlistFlatten(ctx, (Rlist **) &newrv.item);
                }

                PromiseAppendConstraint(pcopy, cp->lval, newrv, false);
            }
        }

    } /* for all constraints */

    // Add default body for promise body types that are not present
    char *bundle_type = pcopy->parent_promise_type->parent_bundle->type;
    char *promise_type = pcopy->parent_promise_type->name;
    const PromiseTypeSyntax *syntax = PromiseTypeSyntaxGet(bundle_type, promise_type);
    AddDefaultBodiesToPromise(ctx, pcopy, syntax);

    // Add default body for global body types that are not present
    const PromiseTypeSyntax *global_syntax = PromiseTypeSyntaxGet("*", "*");
    AddDefaultBodiesToPromise(ctx, pcopy, global_syntax);

    return pcopy;
}

// Try to add default bodies to promise for every body type found in syntax
static void AddDefaultBodiesToPromise(EvalContext *ctx, Promise *promise, const PromiseTypeSyntax *syntax)
{
    // do nothing if syntax is not defined
    if (syntax == NULL) {
        return;
    }

    // iterate over possible constraints
    for (int i = 0; syntax->constraints[i].lval; i++)
    {
        // of type body
        if(syntax->constraints[i].dtype == CF_DATA_TYPE_BODY) {
            const char *constraint_type = syntax->constraints[i].lval;
            // if there is no matching body in this promise
            if(!PromiseBundleOrBodyConstraintExists(ctx, constraint_type, promise)) {
                const Policy *policy = PolicyFromPromise(promise);
                // default format is <promise_type>_<body_type>
                char* default_body_name = StringConcatenate(3, promise->parent_promise_type->name, "_", constraint_type);
                const Body *bp = EvalContextFindFirstMatchingBody(policy, constraint_type, "bodydefault", default_body_name);
                if(bp) {
                    Log(LOG_LEVEL_VERBOSE, "Using the default body: %60s", default_body_name);
                    CopyBodyConstraintsToPromise(ctx, promise, bp);
                }
                free(default_body_name);
            }
        }
    }
}

/*****************************************************************************/

static bool EvaluateConstraintIteration(EvalContext *ctx, const Constraint *cp, Rval *rval_out)
{
    assert(cp->type == POLICY_ELEMENT_TYPE_PROMISE);
    const Promise *pp = cp->parent.promise;

    if (!IsDefinedClass(ctx, cp->classes))
    {
        return false;
    }

    if (ExpectedDataType(cp->lval) == CF_DATA_TYPE_BUNDLE)
    {
        *rval_out = ExpandBundleReference(ctx, NULL, "this", cp->rval);
    }
    else
    {
        *rval_out = EvaluateFinalRval(ctx, PromiseGetPolicy(pp), NULL,
                                      "this", cp->rval, false, pp);
    }

    return true;
}

/**
  @brief Helper function to determine whether the Rval of ifvarclass/if/unless is defined.
  If the Rval is a function, call that function.
*/
static bool IsVarClassDefined(const EvalContext *ctx, const Constraint *cp, Promise *pcopy)
{
    assert(ctx);
    assert(cp);
    assert(pcopy);

    /*
      This might fail to expand if there are unexpanded variables in function arguments
      (in which case the function won't be called at all), but the function still returns true.

      If expansion fails for other reasons, assume that we don't know this class.
    */
    Rval final;
    if (!EvaluateConstraintIteration((EvalContext*)ctx, cp, &final))
    {
        return false;
    }

    char *classes = NULL;
    PromiseAppendConstraint(pcopy, cp->lval, final, false);
    switch (final.type)
    {
    case RVAL_TYPE_SCALAR:
        classes = RvalScalarValue(final);
        break;

    case RVAL_TYPE_FNCALL:
        Log(LOG_LEVEL_DEBUG, "Function call in class expression did not succeed");
        break;

    default:
        break;
    }

    if (!classes)
    {
        return false;
    }
    // sanity check for unexpanded variables
    if (strchr(classes, '$') || strchr(classes, '@'))
    {
        Log(LOG_LEVEL_DEBUG, "Class expression did not evaluate");
        return false;
    }

    return IsDefinedClass(ctx, classes);
}

/* Expands "$(this.promiser)" comment if present. Writes the result to pp. */
static void DereferenceAndPutComment(Promise* pp, const char *comment)
{
    free(pp->comment);

    char *sp;
    if ((sp = strstr(comment, "$(this.promiser)")) != NULL ||
        (sp = strstr(comment, "${this.promiser}")) != NULL)
    {
        char *s;
        int this_len    = strlen("$(this.promiser)");
        int this_offset = sp - comment;
        xasprintf(&s, "%.*s%s%s",
                  this_offset, comment, pp->promiser,
                  &comment[this_offset + this_len]);

        pp->comment = s;
    }
    else
    {
        pp->comment = xstrdup(comment);
    }
}

Promise *ExpandDeRefPromise(EvalContext *ctx, const Promise *pp, bool *excluded)
{
    assert(pp->promiser);
    assert(pp->classes);
    assert(excluded);

    *excluded = false;

    Rval returnval = ExpandPrivateRval(ctx, NULL, "this", pp->promiser, RVAL_TYPE_SCALAR);
    if (returnval.item == NULL)
    {
        assert(returnval.type == RVAL_TYPE_LIST ||
               returnval.type == RVAL_TYPE_NOPROMISEE);
        /* TODO Log() empty slist, promise skipped? */
        *excluded = true;
        return NULL;
    }
    Promise *pcopy = xcalloc(1, sizeof(Promise));
    pcopy->promiser = RvalScalarValue(returnval);

    /* TODO remove the conditions here for fixing redmine#7880. */
    if ((strcmp("files", pp->parent_promise_type->name) != 0) &&
        (strcmp("storage", pp->parent_promise_type->name) != 0))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser", pcopy->promiser,
                                      CF_DATA_TYPE_STRING, "source=promise");
    }

    if (pp->promisee.item)
    {
        pcopy->promisee = EvaluateFinalRval(ctx, PromiseGetPolicy(pp), NULL, "this", pp->promisee, true, pp);
    }
    else
    {
        pcopy->promisee = (Rval) {NULL, RVAL_TYPE_NOPROMISEE };
    }

    pcopy->classes = xstrdup(pp->classes);
    pcopy->parent_promise_type = pp->parent_promise_type;
    pcopy->offset.line = pp->offset.line;
    pcopy->comment = pp->comment ? xstrdup(pp->comment) : NULL;
    pcopy->conlist = SeqNew(10, ConstraintDestroy);
    pcopy->org_pp = pp->org_pp;

    // if this is a class promise, check if it is already set, if so, skip
    if (strcmp("classes", pp->parent_promise_type->name) == 0)
    {
        if (IsDefinedClass(ctx, CanonifyName(pcopy->promiser)))
        {
            Log(LOG_LEVEL_DEBUG,
                "Skipping evaluation of classes promise as class '%s' is already set",
                CanonifyName(pcopy->promiser));

            *excluded = true;
            return pcopy;
        }
    }

    /* Look for 'if'/'ifvarclass' exclusion. */
    {
        const Constraint *ifvarclass = PromiseGetConstraint(pp, "ifvarclass");
        if (!ifvarclass)
        {
            ifvarclass = PromiseGetConstraint(pp, "if");
        }

        if (ifvarclass && !IsVarClassDefined(ctx, ifvarclass, pcopy))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping promise '%s'"
                " because 'if'/'ifvarclass' is not defined", pp->promiser);

            *excluded = true;
            return pcopy;
        }
    }

    /* Look for 'unless' exclusion. */
    {
        const Constraint *unless = PromiseGetConstraint(pp, "unless");

        if (unless && IsVarClassDefined(ctx, unless, pcopy))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping promise '%s',"
                " because 'unless' is defined", pp->promiser);

            *excluded = true;
            return pcopy;
        }
    }

    /* Look for depends_on exclusion. */
    {
        const Constraint *depends_on = PromiseGetConstraint(pp, "depends_on");
        if (depends_on)
        {
            Rval final;
            if (EvaluateConstraintIteration(ctx, depends_on, &final))
            {
                PromiseAppendConstraint(pcopy, depends_on->lval, final, false);

                if (MissingDependencies(ctx, pcopy))
                {
                    *excluded = true;
                    return pcopy;
                }
            }
        }
    }

    /* Evaluate all constraints. */
    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        // special constraints ifvarclass and depends_on are evaluated before the rest of the constraints
        if (strcmp(cp->lval, "ifvarclass") == 0 ||
            strcmp(cp->lval, "if")         == 0 ||
            strcmp(cp->lval, "unless")     == 0 ||
            strcmp(cp->lval, "depends_on") == 0)
        {
            continue;
        }

        Rval final;
        if (!EvaluateConstraintIteration(ctx, cp, &final))
        {
            continue;
        }

        PromiseAppendConstraint(pcopy, cp->lval, final, false);

        if (strcmp(cp->lval, "comment") == 0)
        {
            if (final.type != RVAL_TYPE_SCALAR)
            {
                Log(LOG_LEVEL_ERR, "Comments can only be scalar objects, not '%s' in '%s'",
                    RvalTypeToString(final.type), pp->promiser);
            }
            else
            {
                assert(final.item != NULL);             /* it's SCALAR type */
                DereferenceAndPutComment(pcopy, final.item);
            }
        }
    }

    return pcopy;
}

void PromiseRef(LogLevel level, const Promise *pp)
{
    if (pp == NULL)
    {
        return;
    }

    if (PromiseGetBundle(pp)->source_path)
    {
        Log(level, "Promise belongs to bundle '%s' in file '%s' near line %zu", PromiseGetBundle(pp)->name,
            PromiseGetBundle(pp)->source_path, pp->offset.line);
    }
    else
    {
        Log(level, "Promise belongs to bundle '%s' near line %zu", PromiseGetBundle(pp)->name,
            pp->offset.line);
    }

    if (pp->comment)
    {
        Log(level, "Comment is '%s'", pp->comment);
    }

    switch (pp->promisee.type)
    {
    case RVAL_TYPE_SCALAR:
        Log(level, "This was a promise to '%s'", (char *)(pp->promisee.item));
        break;
    case RVAL_TYPE_LIST:
    {
        Writer *w = StringWriter();
        RlistWrite(w, pp->promisee.item);
        char *p = StringWriterClose(w);
        Log(level, "This was a promise to '%s'", p);
        free(p);
        break;
    }
    default:
        break;
    }
}

/*******************************************************************/

/* Old legacy function from Enterprise, TODO remove static string. */
const char *PromiseID(const Promise *pp)
{
    static char id[CF_MAXVARSIZE];
    char vbuff[CF_MAXVARSIZE];
    const char *handle = PromiseGetHandle(pp);

    if (handle)
    {
        snprintf(id, CF_MAXVARSIZE, "%s", CanonifyName(handle));
    }
    else if (pp && PromiseGetBundle(pp)->source_path)
    {
        snprintf(vbuff, CF_MAXVARSIZE, "%s", ReadLastNode(PromiseGetBundle(pp)->source_path));
        snprintf(id, CF_MAXVARSIZE, "promise_%s_%zu", CanonifyName(vbuff), pp->offset.line);
    }
    else
    {
        snprintf(id, CF_MAXVARSIZE, "unlabelled_promise");
    }

    return id;
}
