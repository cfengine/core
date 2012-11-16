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
#include "unix.h"
#include "cfstream.h"
#include "args.h"

#include <assert.h>

/*******************************************************************/

int IsBuiltinFnCall(Rval rval)
{
    FnCall *fp;

    if (rval.rtype != CF_FNCALL)
    {
        return false;
    }

    fp = (FnCall *) rval.item;

    if (FindFunction(fp->name))
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

FnCall *NewFnCall(const char *name, Rlist *args)
{
    FnCall *fp;

    CfDebug("Installing Function Call %s\n", name);

    fp = xmalloc(sizeof(FnCall));

    fp->name = xstrdup(name);
    fp->args = args;

    CfDebug("Installed ");
    if (DEBUG)
    {
        ShowFnCall(stdout, fp);
    }
    CfDebug("\n\n");
    return fp;
}

/*******************************************************************/

FnCall *CopyFnCall(const FnCall *f)
{
    CfDebug("CopyFnCall()\n");
    return NewFnCall(f->name, CopyRlist(f->args));
}

/*******************************************************************/

void DeleteFnCall(FnCall *fp)
{
    if (fp->name)
    {
        free(fp->name);
    }

    if (fp->args)
    {
        DeleteRlist(fp->args);
    }

    free(fp);
}

/*********************************************************************/

FnCall *ExpandFnCall(const char *contextid, FnCall *f, int expandnaked)
{
    CfDebug("ExpandFnCall()\n");
//return NewFnCall(f->name,ExpandList(contextid,f->args,expandnaked));
    return NewFnCall(f->name, ExpandList(contextid, f->args, false));
}

/*******************************************************************/

int PrintFnCall(char *buffer, int bufsize, const FnCall *fp)
{
    Rlist *rp;
    char work[CF_MAXVARSIZE];

    snprintf(buffer, bufsize, "%s(", fp->name);

    for (rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            Join(buffer, (char *) rp->item, bufsize);
            break;

        case CF_FNCALL:
            PrintFnCall(work, CF_MAXVARSIZE, (FnCall *) rp->item);
            Join(buffer, work, bufsize);
            break;

        default:
            break;
        }

        if (rp->next != NULL)
        {
            strcat(buffer, ",");
        }
    }

    strcat(buffer, ")");

    return strlen(buffer);
}

/*******************************************************************/

void ShowFnCall(FILE *fout, const FnCall *fp)
{
    if (XML)
    {
        fprintf(fout, "%s(", fp->name);
    }
    else
    {
        fprintf(fout, "%s(", fp->name);
    }

    for (const Rlist *rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            fprintf(fout, "%s,", (char *) rp->item);
            break;

        case CF_FNCALL:
            ShowFnCall(fout, (FnCall *) rp->item);
            break;

        default:
            fprintf(fout, "(** Unknown argument **)\n");
            break;
        }
    }

    if (XML)
    {
        fprintf(fout, ")");
    }
    else
    {
        fprintf(fout, ")");
    }
}

/*******************************************************************/

enum cfdatatype FunctionReturnType(const char *name)
{
    const FnCallType *fn = FindFunction(name);

    return fn ? fn->dtype : cf_notype;
}

/*******************************************************************/

FnCallResult EvaluateFunctionCall(FnCall *fp, const Promise *pp)
{
    Rlist *expargs;
    const FnCallType *this = FindFunction(fp->name);

    if (this)
    {
        if (DEBUG)
        {
            printf("EVALUATE FN CALL %s\n", fp->name);
            ShowFnCall(stdout, fp);
            printf("\n");
        }
    }
    else
    {
        if (pp)
        {
            CfOut(cf_error, "", "No such FnCall \"%s()\" in promise @ %s near line %zd\n",
                  fp->name, pp->audit->filename, pp->offset.line);
        }
        else
        {
            CfOut(cf_error, "", "No such FnCall \"%s()\" - context info unavailable\n", fp->name);
        }

        return (FnCallResult) { FNCALL_FAILURE, { CopyFnCall(fp), CF_FNCALL } };
    }

/* If the container classes seem not to be defined at this stage, then don't try to expand the function */

    if ((pp != NULL) && !IsDefinedClass(pp->classes, pp->namespace))
    {
        return (FnCallResult) { FNCALL_FAILURE, { CopyFnCall(fp), CF_FNCALL } };
    }

    expargs = NewExpArgs(fp, pp);

    if (UnresolvedArgs(expargs))
    {
        DeleteExpArgs(expargs);
        return (FnCallResult) { FNCALL_FAILURE, { CopyFnCall(fp), CF_FNCALL } };
    }

    if (pp != NULL)
    {
        fp->namespace = pp->namespace;
    }
    else
    {
        fp->namespace = "default";
    }
    
    FnCallResult result = CallFunction(this, fp, expargs);

    if (result.status == FNCALL_FAILURE)
    {
        /* We do not assign variables to failed function calls */
        DeleteExpArgs(expargs);
        return (FnCallResult) { FNCALL_FAILURE, { CopyFnCall(fp), CF_FNCALL } };
    }

    DeleteExpArgs(expargs);
    return result;
}

/*******************************************************************/

const FnCallType *FindFunction(const char *name)
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

/*****************************************************************************/

void FnCallPrint(Writer *writer, const FnCall *call)
{
    for (const Rlist *rp = call->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            WriterWriteF(writer, "%s,", (const char *) rp->item);
            break;

        case CF_FNCALL:
            FnCallPrint(writer, (FnCall *) rp->item);
            break;

        default:
            WriterWrite(writer, "(** Unknown argument **)\n");
            break;
        }
    }
}

/*****************************************************************************/

JsonElement *FnCallToJson(const FnCall *fp)
{
    assert(fp);

    JsonElement *object = JsonObjectCreate(3);

    JsonObjectAppendString(object, "name", fp->name);
    JsonObjectAppendString(object, "type", "function-call");

    JsonElement *argsArray = JsonArrayCreate(5);

    for (Rlist *rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            JsonArrayAppendString(argsArray, (const char *) rp->item);
            break;

        case CF_FNCALL:
            JsonArrayAppendObject(argsArray, FnCallToJson((FnCall *) rp->item));
            break;

        default:
            assert(false && "Unknown argument type");
            break;
        }
    }
    JsonObjectAppendArray(object, "arguments", argsArray);

    return object;
}
