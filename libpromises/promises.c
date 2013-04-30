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

#include "promises.h"

#include "policy.h"
#include "syntax.h"
#include "expand.h"
#include "files_names.h"
#include "scope.h"
#include "vars.h"
#include "logging_old.h"
#include "args.h"
#include "locks.h"
#include "misc_lib.h"
#include "fncall.h"
#include "env_context.h"

static void DereferenceComment(Promise *pp);

/*****************************************************************************/

static Body *IsBody(Seq *bodies, const char *ns, const char *key)
{
    char fqname[CF_BUFSIZE];

    for (size_t i = 0; i < SeqLength(bodies); i++)
    {
        Body *bp = SeqAt(bodies, i);

        // bp->namespace is where the body belongs, namespace is where we are now
        if (strchr(key, CF_NS) || strcmp(ns,"default") == 0)
        {
            if (strncmp(key,"default:",strlen("default:")) == 0) // CF_NS == ':'
            {
                strcpy(fqname,strchr(key,CF_NS)+1);
            }
            else
            {
                strcpy(fqname,key);
            }
        }
        else
        {
            snprintf(fqname,CF_BUFSIZE-1, "%s%c%s", ns, CF_NS, key);
        }

        if (strcmp(bp->name, fqname) == 0)
        {
            return bp;
        }
    }

    return NULL;
}

static Bundle *IsBundle(Seq *bundles, const char *key)
{
    char fqname[CF_BUFSIZE];

    for (size_t i = 0; i < SeqLength(bundles); i++)
    {
        Bundle *bp = SeqAt(bundles, i);

        if (strcmp(bp->ns, "default") == 0)
        {
            if (strncmp(key,"default:",strlen("default:")) == 0)  // CF_NS == ':'
            {
                strcpy(fqname,strchr(key, CF_NS)+1);
            }
            else
            {
                strcpy(fqname,key);
            }
        }
        else if (strncmp(bp->ns, key, strlen(bp->ns)) == 0)
        {
            strcpy(fqname,key);
        }
        else
        {
            snprintf(fqname, CF_BUFSIZE-1, "%s%c%s", bp->ns, CF_NS, key);
        }

        if (strcmp(bp->name, fqname) == 0)
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

    if (pp->promisee.item)
    {
        CfDebug("CopyPromise(%s->", pp->promiser);
        if (DEBUG)
        {
            RvalShow(stdout, pp->promisee);
        }
        CfDebug("\n");
    }
    else
    {
        CfDebug("CopyPromise(%s->)\n", pp->promiser);
    }

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

    CfDebug("Copying promise constraints\n\n");

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        Body *bp = NULL;
        FnCall *fp = NULL;
        char *bodyname = NULL;

        /* A body template reference could look like a scalar or fn to the parser w/w () */
        Policy *policy = PolicyFromPromise(pp);
        Seq *bodies = policy ? policy->bodies : NULL;

        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:
            bodyname = (char *) cp->rval.item;
            if (cp->references_body)
            {
                bp = IsBody(bodies, PromiseGetNamespace(pp), bodyname);
            }
            fp = NULL;
            break;
        case RVAL_TYPE_FNCALL:
            fp = (FnCall *) cp->rval.item;
            bodyname = fp->name;
            bp = IsBody(bodies, PromiseGetNamespace(pp), bodyname);
            break;
        default:
            bp = NULL;
            fp = NULL;
            bodyname = NULL;
            break;
        }

        /* First case is: we have a body template to expand lval = body(args), .. */

        if (bp)
        {
            EvalContextStackPushBodyFrame(ctx, bp);

            if (strcmp(bp->type, cp->lval) != 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Body type mismatch for body reference \"%s\" in promise at line %zu of %s (%s != %s)\n",
                      bodyname, pp->offset.line, PromiseGetBundle(pp)->source_path, bp->type, cp->lval);
            }

            /* Keep the referent body type as a boolean for convenience when checking later */

            PromiseAppendConstraint(pcopy, cp->lval, (Rval) {xstrdup("true"), RVAL_TYPE_SCALAR }, cp->classes, false);

            CfDebug("Handling body-lval \"%s\"\n", cp->lval);

            if (bp->args != NULL)
            {
                /* There are arguments to insert */

                if (fp == NULL || fp->args == NULL)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Argument mismatch for body reference \"%s\" in promise at line %zu of %s\n",
                          bodyname, pp->offset.line, PromiseGetBundle(pp)->source_path);
                }

                if (fp && bp && fp->args && bp->args && !ScopeMapBodyArgs(ctx, "body", fp->args, bp->args))
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "",
                          "Number of arguments does not match for body reference \"%s\" in promise at line %zu of %s\n",
                          bodyname, pp->offset.line, PromiseGetBundle(pp)->source_path);
                }

                for (size_t k = 0; k < SeqLength(bp->conlist); k++)
                {
                    Constraint *scp = SeqAt(bp->conlist, k);

                    CfDebug("Doing arg-mapped sublval = %s (promises.c)\n", scp->lval);
                    returnval = ExpandPrivateRval(ctx, "body", scp->rval);
                    PromiseAppendConstraint(pcopy, scp->lval, returnval, scp->classes, false);
                }

                ScopeClear("body");
            }
            else
            {
                /* No arguments to deal with or body undeclared */

                if (fp != NULL)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "",
                          "An apparent body \"%s()\" was undeclared or could have incorrect args, but used in a promise near line %zu of %s (possible unquoted literal value)",
                          bodyname, pp->offset.line, PromiseGetBundle(pp)->source_path);
                }
                else
                {
                    for (size_t k = 0; k < SeqLength(bp->conlist); k++)
                    {
                        Constraint *scp = SeqAt(bp->conlist, k);

                        CfDebug("Doing sublval = %s (promises.c)\n", scp->lval);

                        Rval newrv = RvalCopy(scp->rval);
                        if (newrv.type == RVAL_TYPE_LIST)
                        {
                            Rlist *new_list = RvalRlistValue(newrv);
                            RlistFlatten(ctx, &new_list);
                            newrv.item = new_list;
                        }

                        PromiseAppendConstraint(pcopy, scp->lval, newrv, scp->classes, false);
                    }
                }
            }

            EvalContextStackPopFrame(ctx);
        }
        else
        {
            Policy *policy = PolicyFromPromise(pp);

            if (cp->references_body && !IsBundle(policy->bundles, bodyname))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "",
                      "Apparent body \"%s()\" was undeclared, but used in a promise near line %zu of %s (possible unquoted literal value)",
                      bodyname, pp->offset.line, PromiseGetBundle(pp)->source_path);
            }

            Rval newrv = RvalCopy(cp->rval);
            if (newrv.type == RVAL_TYPE_LIST)
            {
                Rlist *new_list = RvalRlistValue(newrv);
                RlistFlatten(ctx, &new_list);
                newrv.item = new_list;
            }

            PromiseAppendConstraint(pcopy, cp->lval, newrv, cp->classes, false);
        }
    }

    return pcopy;
}

/*****************************************************************************/

Promise *ExpandDeRefPromise(EvalContext *ctx, const char *scopeid, const Promise *pp)
{
    Promise *pcopy;
    Rval returnval, final;

    CfDebug("ExpandDerefPromise()\n");

    pcopy = xcalloc(1, sizeof(Promise));

    returnval = ExpandPrivateRval(ctx, "this", (Rval) {pp->promiser, RVAL_TYPE_SCALAR });
    pcopy->promiser = (char *) returnval.item;

    if (pp->promisee.item)
    {
        pcopy->promisee = EvaluateFinalRval(ctx, scopeid, pp->promisee, true, pp);
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
            final = ExpandBundleReference(ctx, scopeid, cp->rval);
        }
        else
        {
            returnval = EvaluateFinalRval(ctx, scopeid, cp->rval, false, pp);
            final = ExpandDanglers(ctx, scopeid, returnval, pp);
            RvalDestroy(returnval);
        }

        PromiseAppendConstraint(pcopy, cp->lval, final, cp->classes, false);

        if (strcmp(cp->lval, "comment") == 0)
        {
            if (final.type != RVAL_TYPE_SCALAR)
            {
                char err[CF_BUFSIZE];

                snprintf(err, CF_BUFSIZE, "Comments can only be scalar objects (not %c in \"%s\")", final.type,
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

void PromiseRef(OutputLevel level, const Promise *pp)
{
    if (pp == NULL)
    {
        return;
    }

    if (PromiseGetBundle(pp)->source_path)
    {
        CfOut(level, "", "Promise belongs to bundle \'%s\' in file \'%s\' near line %zu\n", PromiseGetBundle(pp)->name,
             PromiseGetBundle(pp)->source_path, pp->offset.line);
    }
    else
    {
        CfOut(level, "", "Promise belongs to bundle \'%s\' near line %zu\n", PromiseGetBundle(pp)->name,
              pp->offset.line);
    }

    if (pp->comment)
    {
        CfOut(level, "", "Comment: %s\n", pp->comment);
    }

    switch (pp->promisee.type)
    {
       case RVAL_TYPE_SCALAR:
           CfOut(level, "", "This was a promise to: %s\n", (char *)(pp->promisee.item));
           break;
       case RVAL_TYPE_LIST:
       {
           Writer *w = StringWriter();
           RlistWrite(w, pp->promisee.item);
           char *p = StringWriterClose(w);
           CfOut(level, "", "This was a promise to: %s", p);
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
