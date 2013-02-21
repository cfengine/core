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
#include "reporting.h"
#include "expand.h"
#include "vars.h"
#include "cfstream.h"
#include "fncall.h"
#include "logging.h"
#include "evalfunction.h"
#include "misc_lib.h"

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

int MapBodyArgs(const char *scopeid, Rlist *give, const Rlist *take)
{
    Rlist *rpg = NULL;
    const Rlist *rpt = NULL;
    FnCall *fp;
    DataType dtg = DATA_TYPE_NONE, dtt = DATA_TYPE_NONE;
    char *lval;
    void *rval;
    int len1, len2;

    CfDebug("MapBodyArgs(begin)\n");

    len1 = RlistLen(give);
    len2 = RlistLen(take);

    if (len1 != len2)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Argument mismatch in body template give[+args] = %d, take[-args] = %d", len1, len2);
        return false;
    }

    for (rpg = give, rpt = take; rpg != NULL && rpt != NULL; rpg = rpg->next, rpt = rpt->next)
    {
        dtg = StringDataType(scopeid, (char *) rpg->item);
        dtt = StringDataType(scopeid, (char *) rpt->item);

        if (dtg != dtt)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Type mismatch between logical/formal parameters %s/%s\n", (char *) rpg->item,
                  (char *) rpt->item);
            CfOut(OUTPUT_LEVEL_ERROR, "", "%s is %s whereas %s is %s\n", (char *) rpg->item, CF_DATATYPES[dtg],
                  (char *) rpt->item, CF_DATATYPES[dtt]);
        }

        switch (rpg->type)
        {
        case RVAL_TYPE_SCALAR:
            lval = (char *) rpt->item;
            rval = rpg->item;
            CfDebug("MapBodyArgs(SCALAR,%s,%s)\n", lval, (char *) rval);
            AddVariableHash(scopeid, lval, (Rval) { rval, RVAL_TYPE_SCALAR }, dtg, NULL, 0);
            break;

        case RVAL_TYPE_LIST:
            lval = (char *) rpt->item;
            rval = rpg->item;
            AddVariableHash(scopeid, lval, (Rval) { rval, RVAL_TYPE_LIST }, dtg, NULL, 0);
            break;

        case RVAL_TYPE_FNCALL:
            fp = (FnCall *) rpg->item;
            dtg = DATA_TYPE_NONE;
            {
                const FnCallType *fncall_type = FnCallTypeGet(fp->name);
                if (fncall_type)
                {
                    dtg = fncall_type->dtype;
                }
            }

            FnCallResult res = FnCallEvaluate(fp, NULL);

            if (res.status == FNCALL_FAILURE && THIS_AGENT_TYPE != AGENT_TYPE_COMMON)
            {
                // Unresolved variables
                if (VERBOSE)
                {
                    printf
                        (" !! Embedded function argument does not resolve to a name - probably too many evaluation levels for ");
                    FnCallShow(stdout, fp);
                    printf(" (try simplifying)\n");
                }
            }
            else
            {
                FnCallDestroy(fp);

                rpg->item = res.rval.item;
                rpg->type = res.rval.type;

                lval = (char *) rpt->item;
                rval = rpg->item;

                AddVariableHash(scopeid, lval, (Rval) {rval, RVAL_TYPE_SCALAR }, dtg, NULL, 0);
            }

            break;

        default:
            /* Nothing else should happen */
            ProgrammingError("Software error: something not a scalar/function in argument literal");
        }
    }

    CfDebug("MapBodyArgs(end)\n");
    return true;
}

/******************************************************************/

Rlist *NewExpArgs(const FnCall *fp, const Promise *pp)
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
            rval = FnCallEvaluate(subfp, pp).rval;
            break;
        default:
            rval = ExpandPrivateRval(CONTEXTID, (Rval) {rp->item, rp->type});
            break;
        }

        CfDebug("EXPARG: %s.%s\n", CONTEXTID, (char *) rval.item);
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

void ArgTemplate(FnCall *fp, const FnCallArg *argtemplate, Rlist *realargs)
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
                FatalError("in %s: %s", id, SyntaxTypeMatchToString(err));
            }
        }

        rp = rp->next;
    }

    if (argnum != RlistLen(realargs) && !fn->varargs)
    {
        snprintf(output, CF_BUFSIZE, "Argument template mismatch handling function %s(", fp->name);
        ReportError(output);
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

        FatalError("Bad arguments");
    }

    for (rp = realargs; rp != NULL; rp = rp->next)
    {
        CfDebug("finalarg: %s\n", (char *) rp->item);
    }

    CfDebug("End ArgTemplate\n");
}
