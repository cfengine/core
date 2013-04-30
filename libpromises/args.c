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

#include "args.h"

#include "promises.h"
#include "syntax.h"
#include "expand.h"
#include "vars.h"
#include "logging_old.h"
#include "fncall.h"
#include "evalfunction.h"
#include "misc_lib.h"
#include "scope.h"
#include "audit.h"

/******************************************************************/
/* Argument propagation                                           */
/******************************************************************/

/*

When formal parameters are passed, they should be literal strings, i.e.
values (check for this). But when the values are received the
receiving body should state only variable names without literal quotes.
That way we can feed in the received parameter name directly in as an lvalue

e.g.
       access => myaccess("$(person)"),

       body files myaccess(user)

leads to Hash Association (lval,rval) => (user,"$(person)")

*/

/******************************************************************/

Rlist *NewExpArgs(EvalContext *ctx, const FnCall *fp, const Promise *pp)
{
    int len;
    Rval rval;
    Rlist *newargs = NULL;
    FnCall *subfp;
    const FnCallType *fn = FnCallTypeGet(fp->name);

    len = RlistLen(fp->args);

    if (!fn->varargs)
    {
        if (len != FnNumArgs(fn))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Arguments to function %s(.) do not tally. Expect %d not %d",
                  fp->name, FnNumArgs(fn), len);
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            exit(1);
        }
    }

    for (const Rlist *rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case RVAL_TYPE_FNCALL:
            subfp = (FnCall *) rp->item;
            rval = FnCallEvaluate(ctx, subfp, pp).rval;
            break;
        default:
            rval = ExpandPrivateRval(ctx, ScopeGetCurrent()->scope, (Rval) {rp->item, rp->type});
            break;
        }

        CfDebug("EXPARG: %s.%s\n", ScopeGetCurrent()->scope, (char *) rval.item);
        RlistAppend(&newargs, rval.item, rval.type);
        RvalDestroy(rval);
    }

    return newargs;
}

/******************************************************************/

void DeleteExpArgs(Rlist *args)
{

    RlistDestroy(args);

}

/******************************************************************/

void ArgTemplate(EvalContext *ctx, FnCall *fp, const FnCallArg *argtemplate, Rlist *realargs)
{
    int argnum, i;
    Rlist *rp = fp->args;
    char id[CF_BUFSIZE], output[CF_BUFSIZE];
    const FnCallType *fn = FnCallTypeGet(fp->name);

    snprintf(id, CF_MAXVARSIZE, "built-in FnCall %s-arg", fp->name);

    for (argnum = 0; rp != NULL && argtemplate[argnum].pattern != NULL; argnum++)
    {
        if (rp->type != RVAL_TYPE_FNCALL)
        {
            /* Nested functions will not match to lval so don't bother checking */
            SyntaxTypeMatch err = CheckConstraintTypeMatch(id, (Rval) {rp->item, rp->type}, argtemplate[argnum].dtype, argtemplate[argnum].pattern, 1);
            if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
            {
                FatalError(ctx, "in %s: %s", id, SyntaxTypeMatchToString(err));
            }
        }

        rp = rp->next;
    }

    if (argnum != RlistLen(realargs) && !fn->varargs)
    {
        snprintf(output, CF_BUFSIZE, "Argument template mismatch handling function %s(", fp->name);
        RlistShow(stderr, realargs);
        fprintf(stderr, ")\n");

        for (i = 0, rp = realargs; i < argnum; i++)
        {
            printf("  arg[%d] range %s\t", i, argtemplate[i].pattern);
            if (rp != NULL)
            {
                RvalShow(stdout, (Rval) {rp->item, rp->type});
                rp = rp->next;
            }
            else
            {
                printf(" ? ");
            }
            printf("\n");
        }

        FatalError(ctx, "Bad arguments");
    }

    for (rp = realargs; rp != NULL; rp = rp->next)
    {
        CfDebug("finalarg: %s\n", (char *) rp->item);
    }

    CfDebug("End ArgTemplate\n");
}
