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

#include <fncall.h>

#include <eval_context.h>
#include <files_names.h>
#include <expand.h>
#include <vars.h>
#include <evalfunction.h>
#include <policy.h>
#include <string_lib.h>
#include <promises.h>
#include <syntax.h>
#include <audit.h>

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

static Rlist *NewExpArgs(EvalContext *ctx, const Policy *policy, const FnCall *fp)
{
    {
        const FnCallType *fn = FnCallTypeGet(fp->name);
        int len = RlistLen(fp->args);

        if (!(fn->options & FNCALL_OPTION_VARARG))
        {
            if (len != FnNumArgs(fn))
            {
                Log(LOG_LEVEL_ERR, "Arguments to function '%s' do not tally. Expected %d not %d",
                      fp->name, FnNumArgs(fn), len);
                PromiseRef(LOG_LEVEL_ERR, fp->caller);
                exit(EXIT_FAILURE);
            }
        }
    }

    Rlist *expanded_args = NULL;
    for (const Rlist *rp = fp->args; rp != NULL; rp = rp->next)
    {
        Rval rval;

        switch (rp->val.type)
        {
        case RVAL_TYPE_FNCALL:
            {
                FnCall *subfp = RlistFnCallValue(rp);
                rval = FnCallEvaluate(ctx, policy, subfp, fp->caller).rval;
                assert(rval.item);
            }
            break;
        default:
            rval = ExpandPrivateRval(ctx, NULL, NULL, rp->val.item, rp->val.type);
            assert(rval.item);
            break;
        }

        RlistAppend(&expanded_args, rval.item, rval.type);
        RvalDestroy(rval);
    }

    return expanded_args;
}

/*******************************************************************/

bool FnCallIsBuiltIn(Rval rval)
{
    FnCall *fp;

    if (rval.type != RVAL_TYPE_FNCALL)
    {
        return false;
    }

    fp = (FnCall *) rval.item;

    if (FnCallTypeGet(fp->name))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*******************************************************************/

FnCall *FnCallNew(const char *name, Rlist *args)
{
    FnCall *fp = xmalloc(sizeof(FnCall));

    fp->name = xstrdup(name);
    fp->args = args;

    return fp;
}

/*******************************************************************/

FnCall *FnCallCopy(const FnCall *f)
{
    return FnCallNew(f->name, RlistCopy(f->args));
}

/*******************************************************************/

void FnCallDestroy(FnCall *fp)
{
    if (fp)
    {
        free(fp->name);
        RlistDestroy(fp->args);
    }
    free(fp);
}

unsigned FnCallHash(const FnCall *fp, unsigned seed, unsigned max)
{
    unsigned hash = StringHash(fp->name, seed, max);
    return RlistHash(fp->args, hash, max);
}


FnCall *ExpandFnCall(EvalContext *ctx, const char *ns, const char *scope, const FnCall *f)
{
    FnCall *result = NULL;
    if (IsCf3VarString(f->name))
    {
        // e.g. usebundle => $(m)(arg0, arg1);
        Buffer *buf = BufferNewWithCapacity(CF_MAXVARSIZE);
        ExpandScalar(ctx, ns, scope, f->name, buf);

        result = FnCallNew(BufferData(buf), ExpandList(ctx, ns, scope, f->args, false));
        BufferDestroy(buf);
    }
    else
    {
        result = FnCallNew(f->name, ExpandList(ctx, ns, scope, f->args, false));
    }

    return result;
}

void FnCallWrite(Writer *writer, const FnCall *call)
{
    WriterWrite(writer, call->name);
    WriterWriteChar(writer, '(');

    for (const Rlist *rp = call->args; rp != NULL; rp = rp->next)
    {
        switch (rp->val.type)
        {
        case RVAL_TYPE_SCALAR:
            WriterWrite(writer, RlistScalarValue(rp));
            break;

        case RVAL_TYPE_FNCALL:
            FnCallWrite(writer, RlistFnCallValue(rp));
            break;

        default:
            WriterWrite(writer, "(** Unknown argument **)\n");
            break;
        }

        if (rp->next != NULL)
        {
            WriterWriteChar(writer, ',');
        }
    }

    WriterWriteChar(writer, ')');
}

/*******************************************************************/

static FnCallResult CallFunction(EvalContext *ctx, const Policy *policy, const FnCall *fp, const Rlist *expargs)
{
    const Rlist *rp = fp->args;
    const FnCallType *fncall_type = FnCallTypeGet(fp->name);

    int argnum = 0;
    for (argnum = 0; rp != NULL && fncall_type->args[argnum].pattern != NULL; argnum++)
    {
        if (rp->val.type != RVAL_TYPE_FNCALL)
        {
            /* Nested functions will not match to lval so don't bother checking */
            SyntaxTypeMatch err = CheckConstraintTypeMatch(fp->name, rp->val,
                                                           fncall_type->args[argnum].dtype,
                                                           fncall_type->args[argnum].pattern, 1);
            if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
            {
                FatalError(ctx, "In function '%s', error in variable '%s', '%s'", fp->name, (const char *)rp->val.item, SyntaxTypeMatchToString(err));
            }
        }

        rp = rp->next;
    }

    char output[CF_BUFSIZE];
    if (argnum != RlistLen(expargs) && !(fncall_type->options & FNCALL_OPTION_VARARG))
    {
        snprintf(output, CF_BUFSIZE, "Argument template mismatch handling function %s(", fp->name);
        {
            Writer *w = FileWriter(stderr);
            RlistWrite(w, expargs);
            FileWriterDetach(w);
        }

        fprintf(stderr, ")\n");

        rp = expargs;
        for (int i = 0; i < argnum; i++)
        {
            printf("  arg[%d] range %s\t", i, fncall_type->args[i].pattern);
            if (rp != NULL)
            {
                Writer *w = FileWriter(stdout);
                RvalWrite(w, rp->val);
                FileWriterDetach(w);
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


    return (*fncall_type->impl) (ctx, policy, fp, expargs);
}

FnCallResult FnCallEvaluate(EvalContext *ctx, const Policy *policy, FnCall *fp, const Promise *caller)
{
    assert(ctx);
    assert(policy);
    assert(fp);
    fp->caller = caller;

    if (!EvalContextGetEvalOption(ctx, EVAL_OPTION_EVAL_FUNCTIONS))
    {
        Log(LOG_LEVEL_VERBOSE, "Skipping function '%s', because evaluation was turned off in the evaluator",
            fp->name);
        return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
    }

    const FnCallType *fp_type = FnCallTypeGet(fp->name);

    if (!fp_type)
    {
        if (caller)
        {
            Log(LOG_LEVEL_ERR, "No such FnCall '%s' in promise '%s' near line %zd",
                  fp->name, PromiseGetBundle(caller)->source_path, caller->offset.line);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "No such FnCall '%s', context info unavailable", fp->name);
        }

        return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
    }

    Rlist *expargs = NewExpArgs(ctx, policy, fp);

    Writer *fncall_writer;
    const char *fncall_string;
    if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
    {
        fncall_writer = StringWriter();
        FnCallWrite(fncall_writer, fp);
        fncall_string = StringWriterData(fncall_writer);
    }

    if (RlistIsUnresolved(expargs))
    {
        // Special case: ifelse(isvariable("x"), $(x), "default")
        // (the first argument will come down expanded as "!any")
        if (0 == strcmp(fp->name, "ifelse") &&
            RlistLen(expargs) == 3 &&
            expargs->val.type == RVAL_TYPE_SCALAR &&
            0 == strcmp("!any", RlistScalarValue(expargs)) &&
            !RlistIsUnresolved(expargs->next->next))
        {
                Log(LOG_LEVEL_DEBUG, "Allowing ifelse() function evaluation even"
                    " though its arguments contain unresolved variables: %s",
                    fncall_string);
        }
        else
        {
            if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
            {
                Log(LOG_LEVEL_DEBUG, "Skipping function evaluation for now,"
                    " arguments contain unresolved variables: %s",
                    fncall_string);
                WriterClose(fncall_writer);
            }
            RlistDestroy(expargs);
            return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
        }
    }

    Rval cached_rval;
    if ((fp_type->options & FNCALL_OPTION_CACHED) && EvalContextFunctionCacheGet(ctx, fp, expargs, &cached_rval))
    {
        if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
        {
            Log(LOG_LEVEL_DEBUG,
                "Using previously cached result for function: %s",
                fncall_string);
            WriterClose(fncall_writer);
        }
        Writer *w = StringWriter();
        FnCallWrite(w, fp);
        WriterClose(w);
        RlistDestroy(expargs);

        return (FnCallResult) { FNCALL_SUCCESS, RvalCopy(cached_rval) };
    }

    if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
    {
        Log(LOG_LEVEL_DEBUG, "Evaluating function: %s",
            fncall_string);
        WriterClose(fncall_writer);
    }

    FnCallResult result = CallFunction(ctx, policy, fp, expargs);

    if (result.status == FNCALL_FAILURE)
    {
        RlistDestroy(expargs);
        return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
    }
    else if (result.rval.type == RVAL_TYPE_LIST && !result.rval.item)
    {
        Rlist *seq = NULL;
        // don't pass NULL items to evaluator
        RlistPrepend(&seq, CF_NULL_VALUE, RVAL_TYPE_SCALAR);
        result.rval.item = seq;
    }

    if (fp_type->options & FNCALL_OPTION_CACHED)
    {
        Writer *w = StringWriter();
        FnCallWrite(w, fp);
        Log(LOG_LEVEL_VERBOSE, "Caching result for function '%s'", StringWriterData(w));
        WriterClose(w);

        EvalContextFunctionCachePut(ctx, fp, expargs, &result.rval);
    }

    RlistDestroy(expargs);

    return result;
}

/*******************************************************************/

const FnCallType *FnCallTypeGet(const char *name)
{
    int i;

    for (i = 0; CF_FNCALL_TYPES[i].name != NULL; i++)
    {
        if (strcmp(CF_FNCALL_TYPES[i].name, name) == 0)
        {
            return CF_FNCALL_TYPES + i;
        }
    }

    return NULL;
}
