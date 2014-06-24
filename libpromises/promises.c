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

static void DereferenceComment(Promise *pp);


Promise *DeRefCopyPromise(EvalContext *ctx, const Promise *pp)
{
    Promise *pcopy;

    pcopy = xcalloc(1, sizeof(Promise));

    if (pp->promiser)
    {
        pcopy->promiser = xstrdup(pp->promiser);
    }

    if (pp->promisee.item)
    {
        pcopy->promisee = RvalCopy(pp->promisee);
        if (pcopy->promisee.type == RVAL_TYPE_LIST)
        {
            Rlist *rval_list = RvalRlistValue(pcopy->promisee);
            RlistFlatten(ctx, &rval_list);
            pcopy->promisee.item = rval_list;
        }
    }

    assert(pp->classes);
    pcopy->classes = xstrdup(pp->classes);


/* FIXME: may it happen? */
    if ((pp->promisee.item != NULL && pcopy->promisee.item == NULL))
    {
        ProgrammingError("Unable to copy promise");
    }

    pcopy->parent_promise_type = pp->parent_promise_type;
    pcopy->offset.line = pp->offset.line;
    pcopy->comment = pp->comment ? xstrdup(pp->comment) : NULL;
    pcopy->has_subbundles = pp->has_subbundles;
    pcopy->conlist = SeqNew(10, ConstraintDestroy);
    pcopy->org_pp = pp->org_pp;
    pcopy->offset = pp->offset;

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        const Policy *policy = PolicyFromPromise(pp);
        const Body *bp = NULL;
        const Rlist *args = NULL;
        const char *body_reference = NULL;

        /* A body template reference could look like a scalar or fn to the parser w/w () */
        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:
            if (cp->references_body)
            {
                body_reference = RvalScalarValue(cp->rval);
                bp = EvalContextResolveBodyExpression(ctx, policy, body_reference, cp->lval);
            }
            args = NULL;
            break;
        case RVAL_TYPE_FNCALL:
            body_reference = RvalFnCallValue(cp->rval)->name;
            bp = EvalContextResolveBodyExpression(ctx, policy, body_reference, cp->lval);
            args = RvalFnCallValue(cp->rval)->args;
            break;
        default:
            bp = NULL;
            args = NULL;
            break;
        }

        /* First case is: we have a body template to expand lval = body(args), .. */

        if (bp)
        {
            EvalContextStackPushBodyFrame(ctx, pcopy, bp, args);

            if (strcmp(bp->type, cp->lval) != 0)
            {
                Log(LOG_LEVEL_ERR,
                    "Body type mismatch for body reference '%s' in promise "
                    "at line %zu of file '%s', '%s' does not equal '%s'",
                    body_reference, pp->offset.line,
                    PromiseGetBundle(pp)->source_path, bp->type, cp->lval);
            }

            /* Keep the referent body type as a boolean for convenience when checking later */

            if (IsDefinedClass(ctx, cp->classes))
            {
                Constraint *cp_copy = PromiseAppendConstraint(pcopy, cp->lval, (Rval) {xstrdup("true"), RVAL_TYPE_SCALAR }, false);
                cp_copy->offset = cp->offset;
            }

            if (bp->args)
            {
                /* There are arguments to insert */

                if (!args)
                {
                    Log(LOG_LEVEL_ERR,
                        "Argument mismatch for body reference '%s' in promise "
                        "at line %zu of file '%s'",
                        body_reference, pp->offset.line,
                        PromiseGetBundle(pp)->source_path);
                }

                for (size_t k = 0; k < SeqLength(bp->conlist); k++)
                {
                    Constraint *scp = SeqAt(bp->conlist, k);

                    if (IsDefinedClass(ctx, scp->classes))
                    {
                        Rval returnval = ExpandPrivateRval(ctx, NULL, "body", scp->rval.item, scp->rval.type);
                        Constraint *scp_copy = PromiseAppendConstraint(pcopy, scp->lval, returnval, false);
                        scp_copy->offset = scp->offset;
                    }
                }
            }
            else
            {
                /* No arguments to deal with or body undeclared */

                if (args)
                {
                    Log(LOG_LEVEL_ERR,
                        "Apparent body '%s' was undeclared or could "
                        "have incorrect args, but used in a promise near "
                        "line %zu of %s (possible unquoted literal value)",
                        RvalScalarValue(cp->rval), pp->offset.line,
                        PromiseGetBundle(pp)->source_path);
                }
                else
                {
                    for (size_t k = 0; k < SeqLength(bp->conlist); k++)
                    {
                        Constraint *scp = SeqAt(bp->conlist, k);

                        if (IsDefinedClass(ctx, scp->classes))
                        {
                            Rval newrv = RvalCopy(scp->rval);
                            if (newrv.type == RVAL_TYPE_LIST)
                            {
                                Rlist *new_list = RvalRlistValue(newrv);
                                RlistFlatten(ctx, &new_list);
                                newrv.item = new_list;
                            }

                            Constraint *scp_copy = PromiseAppendConstraint(pcopy, scp->lval, newrv, false);
                            scp_copy->offset = scp->offset;
                        }
                    }
                }
            }

            EvalContextStackPopFrame(ctx);
        }
        else
        {
            const Policy *policy = PolicyFromPromise(pp);

            if (cp->references_body)
            {
                // assume this is a typed bundle (e.g. edit_line)
                const Bundle *callee = EvalContextResolveBundleExpression(ctx, policy, RvalScalarValue(cp->rval), cp->lval);
                if (!callee)
                {
                    // otherwise, assume this is a method-type call
                    callee = EvalContextResolveBundleExpression(ctx, policy, RvalScalarValue(cp->rval), "agent");
                    if (!callee)
                    {
                        callee = EvalContextResolveBundleExpression(ctx, policy, RvalScalarValue(cp->rval), "common");
                    }
                }

                if (!callee)
                {
                    Log(LOG_LEVEL_ERR,
                        "Apparent bundle '%s' was undeclared, but "
                        "used in a promise near line %zu of %s "
                        "(possible unquoted literal value)",
                        RvalScalarValue(cp->rval), pp->offset.line,
                        PromiseGetBundle(pp)->source_path);
                }
            }

            if (IsDefinedClass(ctx, cp->classes))
            {
                Rval newrv = RvalCopy(cp->rval);
                if (newrv.type == RVAL_TYPE_LIST)
                {
                    Rlist *new_list = RvalRlistValue(newrv);
                    RlistFlatten(ctx, &new_list);
                    newrv.item = new_list;
                }

                Constraint *cp_copy = PromiseAppendConstraint(pcopy, cp->lval, newrv, false);
                cp_copy->offset = cp->offset;
            }
        }
    }

    return pcopy;
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
        *rval_out = EvaluateFinalRval(ctx, PromiseGetPolicy(pp), NULL, "this", cp->rval, false, pp);
    }

    return true;
}

Promise *ExpandDeRefPromise(EvalContext *ctx, const Promise *pp, bool *excluded)
{
    assert(pp->promiser);
    assert(pp->classes);

    if (excluded)
    {
        *excluded = false;
    }

    Promise *pcopy = xcalloc(1, sizeof(Promise));
    Rval returnval = ExpandPrivateRval(ctx, NULL, "this", pp->promiser, RVAL_TYPE_SCALAR);
    pcopy->promiser = RvalScalarValue(returnval);

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

    {
        // if this is a class promise, check if it is already set, if so, skip
        if (strcmp("classes", pp->parent_promise_type->name) == 0)
        {
            if (IsDefinedClass(ctx, pcopy->promiser))
            {
                Log(LOG_LEVEL_VERBOSE, "Skipping evaluation of classes promise as class '%s' is already set",
                    pcopy->promiser);

                if (excluded)
                {
                    *excluded = true;
                }

                return pcopy;
            }
        }

        // look for ifvarclass exclusion, to short-circuit evaluation of other constraints
        const Constraint *ifvarclass = PromiseGetConstraint(pp, "ifvarclass");
        if (ifvarclass)
        {
            Rval final;
            if (EvaluateConstraintIteration(ctx, ifvarclass, &final))
            {
                Constraint *cp_copy = PromiseAppendConstraint(pcopy, ifvarclass->lval, final, false);
                cp_copy->offset = ifvarclass->offset;

                char *excluding_class_expr = NULL;
                if (VarClassExcluded(ctx, pcopy, &excluding_class_expr))
                {
                    if (LEGACY_OUTPUT)
                    {
                        Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
                        Log(LOG_LEVEL_VERBOSE, "Skipping whole next promise (%s), as ifvarclass %s is not relevant", pp->promiser, excluding_class_expr ? excluding_class_expr : pp->classes);
                        Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
                    }
                    else
                    {
                        Log(LOG_LEVEL_VERBOSE, "Skipping next promise '%s', as ifvarclass '%s' is not relevant", pp->promiser, excluding_class_expr ? excluding_class_expr : pp->classes);
                    }

                    if (excluded)
                    {
                        *excluded = true;
                    }

                    return pcopy;
                }
            }
        }
    }

    {
        // look for depends_on exclusion, to short-circuit evaluation of other constraints
        const Constraint *depends_on = PromiseGetConstraint(pp, "depends_on");
        if (depends_on)
        {
            Rval final;
            if (EvaluateConstraintIteration(ctx, depends_on, &final))
            {
                Constraint *cp_copy = PromiseAppendConstraint(pcopy, depends_on->lval, final, false);
                cp_copy->offset = depends_on->offset;

                if (MissingDependencies(ctx, pcopy))
                {
                    if (excluded)
                    {
                        *excluded = true;
                    }

                    return pcopy;
                }
            }
        }
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        // special constraints ifvarclass and depends_on are evaluated before the rest of the constraints
        if (strcmp(cp->lval, "ifvarclass") == 0 || strcmp(cp->lval, "depends_on") == 0)
        {
            continue;
        }

        Rval final;
        if (!EvaluateConstraintIteration(ctx, cp, &final))
        {
            continue;
        }

        Constraint *cp_copy = PromiseAppendConstraint(pcopy, cp->lval, final, false);
        cp_copy->offset = cp->offset;

        if (strcmp(cp->lval, "comment") == 0)
        {
            if (final.type != RVAL_TYPE_SCALAR)
            {
                Log(LOG_LEVEL_ERR, "Comments can only be scalar objects, not '%s' in '%s'",
                    RvalTypeToString(final.type), pp->promiser);
            }
            else
            {
                free(pcopy->comment);
                pcopy->comment = final.item ? xstrdup(final.item) : NULL;

                if (pcopy->comment &&
                    (strstr(pcopy->comment, "$(this.promiser)") ||
                     strstr(pcopy->comment, "${this.promiser}")))
                {
                    DereferenceComment(pcopy);
                }
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

static void DereferenceComment(Promise *pp)
{
    char pre_buffer[CF_BUFSIZE], post_buffer[CF_BUFSIZE], buffer[CF_BUFSIZE], *sp;
    int offset = 0;

    strlcpy(pre_buffer, pp->comment, CF_BUFSIZE);

    if ((sp = strstr(pre_buffer, "$(this.promiser)")) || (sp = strstr(pre_buffer, "${this.promiser}")))
    {
        *sp = '\0';
        offset = sp - pre_buffer + strlen("$(this.promiser)");
        strlcpy(post_buffer, pp->comment + offset, CF_BUFSIZE);
        snprintf(buffer, CF_BUFSIZE, "%s%s%s", pre_buffer, pp->promiser, post_buffer);

        free(pp->comment);
        pp->comment = xstrdup(buffer);
    }
}
