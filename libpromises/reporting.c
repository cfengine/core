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
#include "policy.h"

#ifdef HAVE_NOVA
#include "nova_reporting.h"
#endif

#include <assert.h>

static void ReportBannerText(Writer *writer, const char *s);

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

void ShowContext(EvalContext *ctx, const ReportContext *report_context)
{
    if (VERBOSE || DEBUG)
    {
        if (report_context->report_writers[REPORT_OUTPUT_TYPE_TEXT])
        {
            char vbuff[CF_BUFSIZE];
            snprintf(vbuff, CF_BUFSIZE, "Host %s's basic classified context", VFQNAME);
            ReportBannerText(report_context->report_writers[REPORT_OUTPUT_TYPE_TEXT], vbuff);
        }

        Writer *writer = FileWriter(stdout);

        {
            WriterWriteF(writer, "%s>  -> Hard classes = { ", VPREFIX);

            Seq *hard_contexts = SeqNew(1000, NULL);
            SetIterator it = EvalContextHeapIteratorHard(ctx);
            char *context = NULL;
            while ((context = SetIteratorNext(&it)))
            {
                if (!EvalContextHeapContainsNegated(ctx, context))
                {
                    SeqAppend(hard_contexts, context);
                }
            }

            SeqSort(hard_contexts, (SeqItemComparator)strcmp, NULL);

            for (size_t i = 0; i < SeqLength(hard_contexts); i++)
            {
                const char *context = SeqAt(hard_contexts, i);
                WriterWriteF(writer, "%s ", context);
            }

            WriterWriteF(writer, "}\n");
            SeqDestroy(hard_contexts);
        }

        {
            WriterWriteF(writer, "%s>  -> Additional classes = { ", VPREFIX);

            Seq *soft_contexts = SeqNew(1000, NULL);
            SetIterator it = EvalContextHeapIteratorSoft(ctx);
            char *context = NULL;
            while ((context = SetIteratorNext(&it)))
            {
                if (!EvalContextHeapContainsNegated(ctx, context))
                {
                    SeqAppend(soft_contexts, context);
                }
            }

            SeqSort(soft_contexts, (SeqItemComparator)strcmp, NULL);

            for (size_t i = 0; i < SeqLength(soft_contexts); i++)
            {
                const char *context = SeqAt(soft_contexts, i);
                WriterWriteF(writer, "%s ", context);
            }

            WriterWriteF(writer, "}\n");
            SeqDestroy(soft_contexts);
        }

        {
            WriterWriteF(writer, "%s>  -> Negated Classes = { ", VPREFIX);

            StringSetIterator it = EvalContextHeapIteratorNegated(ctx);
            const char *context = NULL;
            while ((context = StringSetIteratorNext(&it)))
            {
                WriterWriteF(writer, "%s ", context);
            }

            WriterWriteF(writer, "}\n");
        }

        FileWriterDetach(writer);
    }
}

/*******************************************************************/

void ShowPromises(EvalContext *ctx, const ReportContext *context, const Seq *bundles, const Seq *bodies)
{
#if defined(HAVE_NOVA)
    Nova_ShowPromises(ctx, context, bundles, bodies);
#endif
}

void ShowPromise(EvalContext *ctx, const ReportContext *context, const Promise *pp, int indent)
{
#if defined(HAVE_NOVA)
    Nova_ShowPromise(ctx, context, NULL, pp, indent);
#endif
}

static void PrintVariablesInScope(Writer *writer, const Scope *scope)
{
    AssocHashTableIterator i = HashIteratorInit(scope->hashtable);
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

// TODO: this output looks ugly as hell
void BannerSubSubType(EvalContext *ctx, const char *bundlename, const char *type)
{
    if (strcmp(type, "processes") == 0)
    {
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "     ??? Local class context: \n");

            StringSetIterator it = EvalContextStackFrameIteratorSoft(ctx);
            const char *context = NULL;
            while ((context = StringSetIteratorNext(&it)))
            {
                printf("       %s\n", context);
            }

            CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      %s in bundle %s\n", type, bundlename);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
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
