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

#include "reporting.h"

#include "env_context.h"
#include "mod_files.h"
#include "constraints.h"
#include "promises.h"
#include "files_names.h"
#include "item_lib.h"
#include "sort.h"
#include "writer.h"
#include "hashes.h"
#include "vars.h"
#include "cfstream.h"
#include "logging.h"
#include "string_lib.h"
#include "evalfunction.h"
#include "misc_lib.h"
#include "fncall.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "nova_reporting.h"
#endif

#include <assert.h>

static void ReportBannerText(Writer *writer, const char *s);
static void IndentText(Writer *writer, int i);
static void ShowBodyText(Writer *writer, const Body *body, int indent);
static void ShowPromiseInReportText(const ReportContext *context, const char *version, const Promise *pp, int indent);
static void ShowPromisesInReportText(const ReportContext *context, const Seq *bundles, const Seq *bodies);

/*******************************************************************/

ReportContext *ReportContextNew(void)
{
    ReportContext *ctx = xcalloc(1, sizeof(ReportContext));
    return ctx;
}

/*******************************************************************/

bool ReportContextAddWriter(ReportContext *context, ReportOutputType type, Writer *writer)
{
    bool replaced = false;
    if (context->report_writers[type])
    {
        WriterClose(context->report_writers[type]);
        replaced = true;
    }

    context->report_writers[type] = writer;

    return replaced;
}

/*******************************************************************/

void ReportContextDestroy(ReportContext *context)
{
    if (context)
    {
        if (context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE])
        {
            WriterWriteF(context->report_writers[REPORT_OUTPUT_TYPE_KNOWLEDGE], "}\n");
        }

        for (size_t i = 0; i < REPORT_OUTPUT_TYPE_MAX; i++)
        {
            if (context->report_writers[i])
            {
                WriterClose(context->report_writers[i]);
            }
        }
        free(context);
    }
}

/*******************************************************************/
/* Generic                                                         */
/*******************************************************************/

void ShowContext(const ReportContext *report_context)
{
    for (int i = 0; i < CF_ALPHABETSIZE; i++)
    {
        VHEAP.list[i] = SortItemListNames(VHEAP.list[i]);
    }

    for (int i = 0; i < CF_ALPHABETSIZE; i++)
    {
        VHARDHEAP.list[i] = SortItemListNames(VHARDHEAP.list[i]);
    }
    
    if (VERBOSE || DEBUG)
    {
        if (report_context->report_writers[REPORT_OUTPUT_TYPE_TEXT])
        {
            char vbuff[CF_BUFSIZE];
            snprintf(vbuff, CF_BUFSIZE, "Host %s's basic classified context", VFQNAME);
            ReportBannerText(report_context->report_writers[REPORT_OUTPUT_TYPE_TEXT], vbuff);
        }

        Writer *writer = FileWriter(stdout);

        WriterWriteF(writer, "%s>  -> Hard classes = { ", VPREFIX);

        ListAlphaList(writer, VHARDHEAP, ' ');

        WriterWriteF(writer, "}\n");

        WriterWriteF(writer, "%s>  -> Additional classes = { ", VPREFIX);

        ListAlphaList(writer, VHEAP, ' ');

        WriterWriteF(writer, "}\n");

        WriterWriteF(writer, "%s>  -> Negated Classes = { ", VPREFIX);

        for (const Item *ptr = VNEGHEAP; ptr != NULL; ptr = ptr->next)
        {
            WriterWriteF(writer, "%s ", ptr->name);
        }

        WriterWriteF(writer, "}\n");

        FileWriterDetach(writer);
    }
}

/*******************************************************************/

void ShowPromises(const ReportContext *context, ReportOutputType type, const Seq *bundles, const Seq *bodies)
{
    Writer *writer = context->report_writers[type];
    assert(writer);
    if (!writer)
    {
        return;
    }

#if defined(HAVE_NOVA)
    Nova_ShowPromises(context, type, bundles, bodies);
#else
    switch (type)
    {
    default:
    case REPORT_OUTPUT_TYPE_TEXT:
        ShowPromisesInReportText(context, bundles, bodies);
        break;
    }
#endif
}

/*******************************************************************/

static void ShowPromisesInReportText(const ReportContext *context, const Seq *bundles, const Seq *bodies)
{
    assert(context);
    Writer *writer = context->report_writers[REPORT_OUTPUT_TYPE_TEXT];
    assert(writer);
    if (!writer)
    {
        return;
    }

    ReportBannerText(writer, "Promises");

    for (size_t i = 0; i < SeqLength(bundles); i++)
    {
        Bundle *bp = SeqAt(bundles, i);

        WriterWriteF(writer, "Bundle %s in the context of %s\n\n", bp->name, bp->type);
        WriterWriteF(writer, "   ARGS:\n\n");

        for (const Rlist *rp = bp->args; rp != NULL; rp = rp->next)
        {
            WriterWriteF(writer, "   scalar arg %s\n\n", (char *) rp->item);
        }

        WriterWriteF(writer, "   {\n");

        for (size_t j = 0; j < SeqLength(bp->subtypes); j++)
        {
            const SubType *sp = SeqAt(bp->subtypes, j);

            WriterWriteF(writer, "   TYPE: %s\n\n", sp->name);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                const Promise *pp = SeqAt(sp->promises, ppi);
                ShowPromise(context, REPORT_OUTPUT_TYPE_TEXT, pp, 6);
            }
        }

        WriterWriteF(writer, "   }\n");
        WriterWriteF(writer, "\n\n");
    }

/* Now summarize the remaining bodies */

    WriterWriteF(writer, "\n\nAll Bodies\n\n");

    for (size_t i = 0; i < SeqLength(bodies); i++)
    {
        const Body *bdp = SeqAt(bodies, i);

        ShowBodyText(writer, bdp, 3);

        WriterWriteF(writer, "\n");
    }
}

void ShowPromisesInReport(const ReportContext *context, ReportOutputType type, const Seq *bundles, const Seq *bodies)
{
    switch (type)
    {
    default:
    case REPORT_OUTPUT_TYPE_TEXT:
        ShowPromisesInReportText(context, bundles, bodies);
        break;
    }
}

/*******************************************************************/

void ShowPromise(const ReportContext *context, ReportOutputType type, const Promise *pp, int indent)
{
    switch (type)
    {
    default:
    case REPORT_OUTPUT_TYPE_TEXT:
#if !defined(HAVE_NOVA)
        {
            char *v;
            Rval retval;

            if (GetVariable("control_common", "version", &retval) != DATA_TYPE_NONE)
            {
                v = (char *) retval.item;
            }
            else
            {
                v = "not specified";
            }

            ShowPromiseInReportText(context, v, pp, indent);
        }
#endif
        break;
    }
}

/*******************************************************************/

static void ShowPromiseInReportText(const ReportContext *context, const char *version, const Promise *pp, int indent)
{
    assert(context);
    Writer *writer = context->report_writers[REPORT_OUTPUT_TYPE_TEXT];
    assert(writer);
    if (!writer)
    {
        return;
    }

    IndentText(writer, indent);
    if (pp->promisee.item != NULL)
    {
        WriterWriteF(writer, "%s promise by \'%s\' -> ", pp->agentsubtype, pp->promiser);
        RvalWrite(writer, pp->promisee);
        WriterWriteF(writer, " if context is %s\n\n", pp->classes);
    }
    else
    {
        WriterWriteF(writer, "%s promise by \'%s\' (implicit) if context is %s\n\n", pp->agentsubtype, pp->promiser,
                pp->classes);
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        const Constraint *cp = SeqAt(pp->conlist, i);

        IndentText(writer, indent + 3);
        WriterWriteF(writer, "%10s => ", cp->lval);

        Policy *policy = PolicyFromPromise(pp);

        const Body *bp = NULL;
        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:
            if ((bp = IsBody(policy->bodies, pp->ns, (char *) cp->rval.item)))
            {
                ShowBodyText(writer, bp, 15);
            }
            else
            {
                RvalWrite(writer, cp->rval);        /* literal */
            }
            break;

        case RVAL_TYPE_LIST:
            {
                const Rlist *rp = (Rlist *) cp->rval.item;
                RlistWrite(writer, rp);
                break;
            }

        case RVAL_TYPE_FNCALL:
            {
                const FnCall *fp = (FnCall *) cp->rval.item;

                if ((bp = IsBody(policy->bodies, pp->ns, fp->name)))
                {
                    ShowBodyText(writer, bp, 15);
                }
                else
                {
                    RvalWrite(writer, cp->rval);        /* literal */
                }
                break;
            }

        default:
            break;
        }

        if (cp->rval.type != RVAL_TYPE_FNCALL)
        {
            IndentText(writer, indent);
            WriterWriteF(writer, " if body context %s\n", cp->classes);
        }
    }

    if (pp->audit)
    {
        IndentText(writer, indent);
    }

    if (pp->audit)
    {
        IndentText(writer, indent);
        WriterWriteF(writer,  "Promise (version %s) belongs to bundle \'%s\' (type %s) in file \'%s\' near line %zu\n",
                version, pp->bundle, pp->bundletype, pp->audit->filename, pp->offset.line);
        WriterWriteF(writer, "\n\n");
    }
    else
    {
        IndentText(writer, indent);
        WriterWriteF(writer, "Promise (version %s) belongs to bundle \'%s\' (type %s) near line %zu\n\n", version,
                pp->bundle, pp->bundletype, pp->offset.line);
    }
}

void ShowPromiseInReport(const ReportContext *context, ReportOutputType type, const char *version, const Promise *pp, int indent)
{
    switch (type)
    {
    default:
    case REPORT_OUTPUT_TYPE_TEXT:
        return ShowPromiseInReportText(context, version, pp, indent);
    }
}

/*******************************************************************/

static void PrintVariablesInScope(Writer *writer, const Scope *scope)
{
    HashIterator i = HashIteratorInit(scope->hashtable);
    CfAssoc *assoc;

    while ((assoc = HashIteratorNext(&i)))
    {
        WriterWriteF(writer, "%8s %c %s = ", CF_DATATYPES[assoc->dtype], assoc->rval.type, assoc->lval);
        RvalWrite(writer, assoc->rval);
        WriterWriteF(writer, "\n");
    }
}

/*******************************************************************/

static void ShowScopedVariablesText(Writer *writer)
{
    for (const Scope *ptr = VSCOPE; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->scope, "this") == 0)
        {
            continue;
        }

        WriterWriteF(writer, "\nScope %s:\n", ptr->scope);

        PrintVariablesInScope(writer, ptr);
    }
}

void ShowScopedVariables(const ReportContext *context, ReportOutputType type)
/* WARNING: Not thread safe (access to VSCOPE) */
{
    switch (type)
    {
    default:
    case REPORT_OUTPUT_TYPE_TEXT:
        ShowScopedVariablesText(context->report_writers[REPORT_OUTPUT_TYPE_TEXT]);
        break;
    }
}

/*******************************************************************/

void Banner(const char *s)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "***********************************************************\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " %s ", s);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "***********************************************************\n");
}

/*******************************************************************/

static void ReportBannerText(Writer *writer, const char *s)
{
    WriterWriteF(writer, "***********************************************************\n");
    WriterWriteF(writer, " %s \n", s);
    WriterWriteF(writer, "***********************************************************\n");
}

/**************************************************************/

void BannerSubType(const char *bundlename, const char *type, int pass)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   =========================================================\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   %s in bundle %s (%d)\n", type, bundlename, pass);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   =========================================================\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

/**************************************************************/

void BannerSubSubType(const char *bundlename, const char *type)
{
    if (strcmp(type, "processes") == 0)
    {

        /* Just parsed all local classes */

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "     ??? Local class context: \n");

        AlphaListIterator it = AlphaListIteratorInit(&VADDCLASSES);

        for (const Item *ip = AlphaListIteratorNext(&it); ip != NULL; ip = AlphaListIteratorNext(&it))
        {
            printf("       %s\n", ip->name);
        }

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      %s in bundle %s\n", type, bundlename);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

/*******************************************************************/

static void IndentText(Writer *writer, int i)
{
    int j;

    for (j = 0; j < i; j++)
    {
        WriterWriteChar(writer, ' ');
    }
}

/*******************************************************************/

static void ShowBodyText(Writer *writer, const Body *body, int indent)
{
    assert(writer);
    WriterWriteF(writer, "%s body for type %s", body->name, body->type);

    if (body->args == NULL)
    {
        WriterWriteF(writer, "(no parameters)\n");
    }
    else
    {
        WriterWriteF(writer, "\n");

        for (const Rlist *rp = body->args; rp != NULL; rp = rp->next)
        {
            if (rp->type != RVAL_TYPE_SCALAR)
            {
                ProgrammingError("ShowBody - non-scalar parameter container");
            }

            IndentText(writer, indent);
            WriterWriteF(writer, "arg %s\n", (char *) rp->item);
        }

        WriterWriteF(writer, "\n");
    }

    IndentText(writer, indent);
    WriterWriteF(writer, "{\n");

    for (size_t i = 0; i < SeqLength(body->conlist); i++)
    {
        const Constraint *cp = SeqAt(body->conlist, i);

        IndentText(writer, indent);
        WriterWriteF(writer, "%s => ", cp->lval);
        RvalWrite(writer, cp->rval);        /* literal */

        if (cp->classes != NULL)
        {
            WriterWriteF(writer, " if sub-body context %s\n", cp->classes);
        }
        else
        {
            WriterWriteF(writer, "\n");
        }
    }

    IndentText(writer, indent);
    WriterWriteF(writer, "}\n");
}

void ReportError(char *s)
{
    if (PARSING)
    {
        yyerror(s);
    }
    else
    {
        if (Chop(s, CF_EXPANDSIZE) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
        }
        FatalError("Validation: %s\n", s);
    }
}
