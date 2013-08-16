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
  versions of CFEngine, the applicable Commerical Open Source License
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
#include <args.h>
#include <locks.h>
#include <misc_lib.h>
#include <fncall.h>
#include <env_context.h>
#include <string_lib.h>

static void DereferenceComment(Promise *pp);

/*****************************************************************************/

static Body *IsBody(Seq *bodies, const char *ns, const char *name)
{
    if (!ns)
    {
        ns = "default";
    }

    for (size_t i = 0; i < SeqLength(bodies); i++)
    {
        Body *bp = SeqAt(bodies, i);

        if (strcmp(bp->ns, ns) == 0 && strcmp(bp->name, name) == 0)
        {
            return bp;
        }
    }

    return NULL;
}

static Bundle *IsBundle(Seq *bundles, const char *ns, const char *name)
{
    if (!ns)
    {
        ns = "default";
    }

    for (size_t i = 0; i < SeqLength(bundles); i++)
    {
        Bundle *bp = SeqAt(bundles, i);

        if (strcmp(bp->ns, ns) == 0 && strcmp(bp->name, name) == 0)
        {
            return bp;
        }
    }

    return NULL;
}

Promise *DeRefCopyPromise(EvalContext *ctx, const Promise *pp)
{
    Promise *pcopy;
    Rval returnval;

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

    if (pp->classes)
    {
        pcopy->classes = xstrdup(pp->classes);
    }

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

        Body *bp = NULL;
        FnCall *fp = NULL;

        /* A body template reference could look like a scalar or fn to the parser w/w () */
        Policy *policy = PolicyFromPromise(pp);
        Seq *bodies = policy ? policy->bodies : NULL;

        char body_ns[CF_MAXVARSIZE] = "";
        char body_name[CF_MAXVARSIZE] = "";

        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:
            if (cp->references_body)
            {
                SplitScopeName(RvalScalarValue(cp->rval), body_ns, body_name);
                if (EmptyString(body_ns))
                {
                    strncpy(body_ns, PromiseGetNamespace(pp), CF_MAXVARSIZE);
                }
                bp = IsBody(bodies, body_ns, body_name);
            }
            fp = NULL;
            break;
        case RVAL_TYPE_FNCALL:
            fp = RvalFnCallValue(cp->rval);
            SplitScopeName(fp->name, body_ns, body_name);
            if (EmptyString(body_ns))
            {
                strncpy(body_ns, PromiseGetNamespace(pp), CF_MAXVARSIZE);
            }
            bp = IsBody(bodies, body_ns, body_name);
            break;
        default:
            bp = NULL;
            fp = NULL;
            break;
        }

        /* First case is: we have a body template to expand lval = body(args), .. */

        if (bp)
        {
            EvalContextStackPushBodyFrame(ctx, bp, fp ? fp->args : NULL);

            if (strcmp(bp->type, cp->lval) != 0)
            {
                Log(LOG_LEVEL_ERR,
                    "Body type mismatch for body reference '%s' in promise at line %zu of file '%s', '%s' does not equal '%s'",
                      body_name, pp->offset.line, PromiseGetBundle(pp)->source_path, bp->type, cp->lval);
            }

            /* Keep the referent body type as a boolean for convenience when checking later */

            {
                Constraint *cp_copy = PromiseAppendConstraint(pcopy, cp->lval, (Rval) {xstrdup("true"), RVAL_TYPE_SCALAR }, cp->classes, false);
                cp_copy->offset = cp->offset;
            }

            if (bp->args != NULL)
            {
                /* There are arguments to insert */

                if (fp == NULL || fp->args == NULL)
                {
                    Log(LOG_LEVEL_ERR, "Argument mismatch for body reference '%s' in promise at line %zu of file '%s'",
                          body_name, pp->offset.line, PromiseGetBundle(pp)->source_path);
                }

                for (size_t k = 0; k < SeqLength(bp->conlist); k++)
                {
                    Constraint *scp = SeqAt(bp->conlist, k);

                    returnval = ExpandPrivateRval(ctx, NULL, "body", scp->rval);
                    {
                        Constraint *scp_copy = PromiseAppendConstraint(pcopy, scp->lval, returnval, scp->classes, false);
                        scp_copy->offset = scp->offset;
                    }
                }
            }
            else
            {
                /* No arguments to deal with or body undeclared */

                if (fp != NULL)
                {
                    Log(LOG_LEVEL_ERR,
                          "An apparent body \"%s()\" was undeclared or could have incorrect args, but used in a promise near line %zu of %s (possible unquoted literal value)",
                          body_name, pp->offset.line, PromiseGetBundle(pp)->source_path);
                }
                else
                {
                    for (size_t k = 0; k < SeqLength(bp->conlist); k++)
                    {
                        Constraint *scp = SeqAt(bp->conlist, k);

                        Rval newrv = RvalCopy(scp->rval);
                        if (newrv.type == RVAL_TYPE_LIST)
                        {
                            Rlist *new_list = RvalRlistValue(newrv);
                            RlistFlatten(ctx, &new_list);
                            newrv.item = new_list;
                        }

                        {
                            Constraint *scp_copy = PromiseAppendConstraint(pcopy, scp->lval, newrv, scp->classes, false);
                            scp_copy->offset = scp->offset;
                        }
                    }
                }
            }

            EvalContextStackPopFrame(ctx);
        }
        else
        {
            Policy *policy = PolicyFromPromise(pp);

            if (cp->references_body && !IsBundle(policy->bundles, EmptyString(body_ns) ? NULL : body_ns, body_name))
            {
                Log(LOG_LEVEL_ERR,
                      "Apparent body \"%s()\" was undeclared, but used in a promise near line %zu of %s (possible unquoted literal value)",
                      body_name, pp->offset.line, PromiseGetBundle(pp)->source_path);
            }

            Rval newrv = RvalCopy(cp->rval);
            if (newrv.type == RVAL_TYPE_LIST)
            {
                Rlist *new_list = RvalRlistValue(newrv);
                RlistFlatten(ctx, &new_list);
                newrv.item = new_list;
            }

            {
                Constraint *cp_copy = PromiseAppendConstraint(pcopy, cp->lval, newrv, cp->classes, false);
                cp_copy->offset = cp->offset;
            }
        }
    }

    return pcopy;
}

/*****************************************************************************/

Promise *ExpandDeRefPromise(EvalContext *ctx, const Promise *pp)
{
    Promise *pcopy;
    Rval returnval, final;

    pcopy = xcalloc(1, sizeof(Promise));

    returnval = ExpandPrivateRval(ctx, NULL, "this", (Rval) {pp->promiser, RVAL_TYPE_SCALAR });
    pcopy->promiser = (char *) returnval.item;

    if (pp->promisee.item)
    {
        pcopy->promisee = EvaluateFinalRval(ctx, NULL, "this", pp->promisee, true, pp);
    }
    else
    {
        pcopy->promisee = (Rval) {NULL, RVAL_TYPE_NOPROMISEE };
    }

    if (pp->classes)
    {
        pcopy->classes = xstrdup(pp->classes);
    }
    else
    {
        pcopy->classes = xstrdup("any");
    }

    if (pcopy->promiser == NULL)
    {
        ProgrammingError("ExpandPromise returned NULL");
    }

    pcopy->parent_promise_type = pp->parent_promise_type;
    pcopy->offset.line = pp->offset.line;
    pcopy->comment = pp->comment ? xstrdup(pp->comment) : NULL;
    pcopy->conlist = SeqNew(10, ConstraintDestroy);
    pcopy->org_pp = pp->org_pp;

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        Rval returnval;

        if (ExpectedDataType(cp->lval) == DATA_TYPE_BUNDLE)
        {
            final = ExpandBundleReference(ctx, NULL, "this", cp->rval);
        }
        else
        {
            returnval = EvaluateFinalRval(ctx, NULL, "this", cp->rval, false, pp);
            final = ExpandDanglers(ctx, NULL, "this", returnval, pp);
            RvalDestroy(returnval);
        }

        {
            Constraint *cp_copy = PromiseAppendConstraint(pcopy, cp->lval, final, cp->classes, false);
            cp_copy->offset = cp->offset;
        }

        if (strcmp(cp->lval, "comment") == 0)
        {
            if (final.type != RVAL_TYPE_SCALAR)
            {
                char err[CF_BUFSIZE];

                snprintf(err, CF_BUFSIZE, "Comments can only be scalar objects, not %c in '%s'", final.type,
                         pp->promiser);
                yyerror(err);
            }
            else
            {
                pcopy->comment = final.item ? xstrdup(final.item) : NULL;

                if (pcopy->comment && (strstr(pcopy->comment, "$(this.promiser)") || strstr(pcopy->comment, "${this.promiser}")))
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
        strncpy(post_buffer, pp->comment + offset, CF_BUFSIZE);
        snprintf(buffer, CF_BUFSIZE, "%s%s%s", pre_buffer, pp->promiser, post_buffer);

        free(pp->comment);
        pp->comment = xstrdup(buffer);
    }
}
