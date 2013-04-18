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

#include "fncall.h"

#include "env_context.h"
#include "files_names.h"
#include "expand.h"
#include "vars.h"
#include "logging.h"
#include "args.h"
#include "evalfunction.h"
#include "policy.h"

#include <assert.h>

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
        CfDebug("%s is a builtin function\n", fp->name);
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
    FnCall *fp;

    CfDebug("Installing Function Call %s\n", name);

    fp = xmalloc(sizeof(FnCall));

    fp->name = xstrdup(name);
    fp->args = args;

    CfDebug("Installed ");
    if (DEBUG)
    {
        FnCallShow(stdout, fp);
    }
    CfDebug("\n\n");
    return fp;
}

/*******************************************************************/

FnCall *FnCallCopy(const FnCall *f)
{
    CfDebug("CopyFnCall()\n");
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

/*********************************************************************/

FnCall *ExpandFnCall(EvalContext *ctx, const char *contextid, FnCall *f)
{
    CfDebug("ExpandFnCall()\n");
    return FnCallNew(f->name, ExpandList(ctx, contextid, f->args, false));
}


/*******************************************************************/

void FnCallShow(FILE *fout, const FnCall *fp)
{
    fprintf(fout, "%s(", fp->name);

    for (const Rlist *rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case RVAL_TYPE_SCALAR:
            fprintf(fout, "%s,", (char *) rp->item);
            break;

        case RVAL_TYPE_FNCALL:
            FnCallShow(fout, (FnCall *) rp->item);
            break;

        default:
            fprintf(fout, "(** Unknown argument **)\n");
            break;
        }
    }

    fprintf(fout, ")");
}

/*******************************************************************/

FnCallResult FnCallEvaluate(EvalContext *ctx, FnCall *fp, const Promise *caller)
{
    Rlist *expargs;
    const FnCallType *fp_type = FnCallTypeGet(fp->name);

    if (fp_type)
    {
        if (DEBUG)
        {
            printf("EVALUATE FN CALL %s\n", fp->name);
            FnCallShow(stdout, fp);
            printf("\n");
        }
    }
    else
    {
        if (caller)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "No such FnCall \"%s()\" in promise @ %s near line %zd\n",
                  fp->name, PromiseGetBundle(caller)->source_path, caller->offset.line);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "No such FnCall \"%s()\" - context info unavailable\n", fp->name);
        }

        return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
    }

/* If the container classes seem not to be defined at this stage, then don't try to expand the function */

    if ((caller != NULL) && !IsDefinedClass(ctx, caller->classes, PromiseGetNamespace(caller)))
    {
        return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
    }

    expargs = NewExpArgs(ctx, fp, caller);

    if (UnresolvedArgs(expargs))
    {
        DeleteExpArgs(expargs);
        return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
    }

    fp->caller = caller;

    FnCallResult result = CallFunction(ctx, fp_type, fp, expargs);

    if (result.status == FNCALL_FAILURE)
    {
        /* We do not assign variables to failed function calls */
        DeleteExpArgs(expargs);
        return (FnCallResult) { FNCALL_FAILURE, { FnCallCopy(fp), RVAL_TYPE_FNCALL } };
    }

    DeleteExpArgs(expargs);
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
