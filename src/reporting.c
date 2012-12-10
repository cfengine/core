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
#include "unix.h"
#include "cfstream.h"

#ifdef HAVE_NOVA
#include "nova-reporting.h"
#endif

#include <assert.h>

char *CFX[][2] =
{
    {"<head>", "</head>"},
    {"<bundle>", "</bundle>"},
    {"<block>", "</block>"},
    {"<blockheader>", "</blockheader>"},
    {"<blockid>", "</blockid>"},
    {"<blocktype>", "</blocktype>"},
    {"<args>", "</args>"},
    {"<promise>", "</promise>"},
    {"<class>", "</class>"},
    {"<subtype>", "</subtype>"},
    {"<object>", "</object>"},
    {"<lval>", "</lval>"},
    {"<rval>", "</rval>"},
    {"<qstring>", "</qstring>"},
    {"<rlist>", "</rlist>"},
    {"<function>", "</function>"},
    {"\n", "\n"},
    {NULL, NULL}
};

char *CFH[][2] =
{
    {"<html><head>\n<link rel=\"stylesheet\" type=\"text/css\" href=\"/cf_enterprise.css\" />\n</head>\n", "</html>"},
    {"<div id=\"bundle\"><table class=border><tr><td><h2>", "</td></tr></h2></table></div>"},
    {"<div id=\"block\"><table class=border cellpadding=5 width=800>", "</table></div>"},
    {"<tr><th>", "</th></tr>"},
    {"<span class=\"bodyname\">", "</span>"},
    {"<span class=\"bodytype\">", "</span>"},
    {"<span class=\"args\">", "</span>"},
    {"<tr><td><table class=\"border\"><tr><td>", "</td></tr></table></td></tr>"},
    {"<span class=\"class\">", "</span>"},
    {"<span class=\"subtype\">", "</span>"},
    {"<b>", "</b>"},
    {"<br><span class=\"lval\">........................", "</span>"},
    {"<span class=\"rval\">", "</span>"},
    {"<span class=\"qstring\">", "</span>"},
    {"<span class=\"rlist\">", "</span>"},
    {"", ""},
    {"<tr><td>", "</td></tr>"},
    {NULL, NULL}
};

/* Prototypes */

static void ShowControlBodies(void);
static void ReportBannerText(Writer *writer, const char *s);
static void IndentText(Writer *writer, int i);
static void ShowDataTypes(void);
static void ShowBundleTypes(void);
static void ShowPromiseTypesFor(const char *s);
static void ShowBodyText(Writer *writer, const Body *body, int indent);
static void ShowBodyHtml(Writer *writer, const Body *body, int indent);
static void ShowBodyParts(const BodySyntax *bs);
static void ShowRange(const char *s, enum cfdatatype type);
static void ShowBuiltinFunctions(void);
static void ShowPromiseInReportText(const ReportContext *context, const char *version, const Promise *pp, int indent);
static void ShowPromiseInReportHtml(const ReportContext *context, const char *version, const Promise *pp, int indent);
static void ShowPromisesInReportText(const ReportContext *context, const Bundle *bundles, const Body *bodies);
static void ShowPromisesInReportHtml(const ReportContext *context, const Bundle *bundles, const Body *bodies);

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

static void ShowControlBodies()
{
    int i;

    printf("<h1>Control bodies for cfengine components</h1>\n");

    printf("<div id=\"bundles\">");

    for (i = 0; CF_ALL_BODIES[i].bundle_type != NULL; i++)
    {
        printf("<h4>COMPONENT %s</h4>\n", CF_ALL_BODIES[i].bundle_type);

        printf("<h4>PROMISE TYPE %s</h4>\n", CF_ALL_BODIES[i].subtype);
        ShowBodyParts(CF_ALL_BODIES[i].bs);
    }
}

/*******************************************************************/

void ShowPromises(const ReportContext *context, ReportOutputType type, const Bundle *bundles, const Body *bodies)
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
    case REPORT_OUTPUT_TYPE_HTML:        
        ShowPromisesInReportHtml(context, bundles, bodies);
        break;

    default:
    case REPORT_OUTPUT_TYPE_TEXT:
        ShowPromisesInReportText(context, bundles, bodies);
        break;
    }
#endif
}

/*******************************************************************/

static void ShowPromisesInReportText(const ReportContext *context, const Bundle *bundles, const Body *bodies)
{
    assert(context);
    Writer *writer = context->report_writers[REPORT_OUTPUT_TYPE_TEXT];
    assert(writer);
    if (!writer)
    {
        return;
    }

    ReportBannerText(writer, "Promises");

    for (const Bundle *bp = bundles; bp != NULL; bp = bp->next)
    {
        WriterWriteF(writer, "Bundle %s in the context of %s\n\n", bp->name, bp->type);
        WriterWriteF(writer, "   ARGS:\n\n");

        for (const Rlist *rp = bp->args; rp != NULL; rp = rp->next)
        {
            WriterWriteF(writer, "   scalar arg %s\n\n", (char *) rp->item);
        }

        WriterWriteF(writer, "   {\n");

        for (const SubType *sp = bp->subtypes; sp != NULL; sp = sp->next)
        {
            WriterWriteF(writer, "   TYPE: %s\n\n", sp->name);

            for (const Promise *pp = sp->promiselist; pp != NULL; pp = pp->next)
            {
                ShowPromise(context, REPORT_OUTPUT_TYPE_TEXT, pp, 6);
            }
        }

        WriterWriteF(writer, "   }\n");
        WriterWriteF(writer, "\n\n");
    }

/* Now summarize the remaining bodies */

    WriterWriteF(writer, "\n\nAll Bodies\n\n");

    for (const Body *bdp = bodies; bdp != NULL; bdp = bdp->next)
    {
        ShowBodyText(writer, bdp, 3);

        WriterWriteF(writer, "\n");
    }
}

static void ShowPromisesInReportHtml(const ReportContext *context, const Bundle *bundles, const Body *bodies)
{   
    assert(context);
    Writer *writer = context->report_writers[REPORT_OUTPUT_TYPE_HTML];
    assert(writer);
    if (!writer)
    {
        return;
    }

    {
        Rval retval;
        char *v;
        char vbuff[CF_BUFSIZE];

        if (GetVariable("control_common", "version", &retval) != cf_notype)
        {
            v = (char *) retval.item;
        }
        else
        {
            v = "not specified";
        }

        snprintf(vbuff, CF_BUFSIZE - 1, "Cfengine Site Policy Summary (version %s)", v);

        CfHtmlHeader(writer, vbuff, STYLESHEET, WEBDRIVER, BANNER);
    }

    WriterWriteF(writer, "<p>");

    for (const Bundle *bp = bundles; bp != NULL; bp = bp->next)
    {
        WriterWriteF(writer, "%s Bundle %s%s%s %s%s%s\n",
                CFH[cfx_bundle][cfb],
                CFH[cfx_blocktype][cfb], bp->type, CFH[cfx_blocktype][cfe],
                CFH[cfx_blockid][cfb], bp->name, CFH[cfx_blockid][cfe]);

        WriterWriteF(writer, " %s ARGS:%s\n\n", CFH[cfx_line][cfb], CFH[cfx_line][cfe]);

        for (const Rlist *rp = bp->args; rp != NULL; rp = rp->next)
        {
            WriterWriteF(writer, "%s", CFH[cfx_line][cfb]);
            WriterWriteF(writer, "   scalar arg %s%s%s\n", CFH[cfx_args][cfb], (char *) rp->item, CFH[cfx_args][cfe]);
            WriterWriteF(writer, "%s", CFH[cfx_line][cfe]);
        }

        WriterWriteF(writer, "%s", CFH[cfx_promise][cfb]);

        for (const SubType *sp = bp->subtypes; sp != NULL; sp = sp->next)
        {
            WriterWriteF(writer, "%s", CFH[cfx_line][cfb]);
            WriterWriteF(writer, "%s", CFH[cfx_line][cfe]);

            for (const Promise *pp = sp->promiselist; pp != NULL; pp = pp->next)
            {
                ShowPromise(context, REPORT_OUTPUT_TYPE_HTML, pp, 6);
            }
        }

        WriterWriteF(writer, "%s\n", CFH[cfx_promise][cfe]);
        WriterWriteF(writer, "%s\n", CFH[cfx_line][cfe]);
        WriterWriteF(writer, "%s\n", CFH[cfx_bundle][cfe]);
    }

/* Now summarize the remaining bodies */

    WriterWriteF(writer, "<h1>All Bodies</h1>");

    for (const Body *bdp = bodies; bdp != NULL; bdp = bdp->next)
    {
        WriterWriteF(writer, "%s%s\n", CFH[cfx_line][cfb], CFH[cfx_block][cfb]);
        WriterWriteF(writer, "%s\n", CFH[cfx_promise][cfb]);

        ShowBodyHtml(writer, bdp, 3);

        WriterWriteF(writer, "%s\n", CFH[cfx_promise][cfe]);
        WriterWriteF(writer, "%s%s \n ", CFH[cfx_block][cfe], CFH[cfx_line][cfe]);
        WriterWriteF(writer, "</p>");
    }

    CfHtmlFooter(writer, FOOTER);
}

void ShowPromisesInReport(const ReportContext *context, ReportOutputType type, const Bundle *bundles, const Body *bodies)
{
    switch (type)
    {
    case REPORT_OUTPUT_TYPE_HTML:
        return ShowPromisesInReportHtml(context, bundles, bodies);

    default:
    case REPORT_OUTPUT_TYPE_TEXT:
        return ShowPromisesInReportText(context, bundles, bodies);
    }
}

/*******************************************************************/

void ShowPromise(const ReportContext *context, ReportOutputType type, const Promise *pp, int indent)
{
    char *v;
    Rval retval;

    if (GetVariable("control_common", "version", &retval) != cf_notype)
    {
        v = (char *) retval.item;
    }
    else
    {
        v = "not specified";
    }

    switch (type)
    {
    case REPORT_OUTPUT_TYPE_HTML:
        /* Ugly hack: we rely on the fact we will be called twice with different report types */
#if defined(HAVE_NOVA)
        Nova_ShowPromise(context, type, v, pp, indent);
#else
        ShowPromiseInReportHtml(context, v, pp, indent);
#endif
        break;

    default:
    case REPORT_OUTPUT_TYPE_TEXT:
#if !defined(HAVE_NOVA)
        ShowPromiseInReportText(context, v, pp, indent);
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
        RvalPrint(writer, pp->promisee);
        WriterWriteF(writer, " if context is %s\n\n", pp->classes);
    }
    else
    {
        WriterWriteF(writer, "%s promise by \'%s\' (implicit) if context is %s\n\n", pp->agentsubtype, pp->promiser,
                pp->classes);
    }

    for (const Constraint *cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        IndentText(writer, indent + 3);
        WriterWriteF(writer, "%10s => ", cp->lval);

        Policy *policy = PolicyFromPromise(pp);

        const Body *bp = NULL;
        switch (cp->rval.rtype)
        {
        case CF_SCALAR:
            if ((bp = IsBody(policy->bodies, pp->namespace, (char *) cp->rval.item)))
            {
                ShowBodyText(writer, bp, 15);
            }
            else
            {
                RvalPrint(writer, cp->rval);        /* literal */
            }
            break;

        case CF_LIST:
            {
                const Rlist *rp = (Rlist *) cp->rval.item;
                RlistPrint(writer, rp);
                break;
            }

        case CF_FNCALL:
            {
                const FnCall *fp = (FnCall *) cp->rval.item;

                if ((bp = IsBody(policy->bodies, pp->namespace, fp->name)))
                {
                    ShowBodyText(writer, bp, 15);
                }
                else
                {
                    RvalPrint(writer, cp->rval);        /* literal */
                }
                break;
            }
        }

        if (cp->rval.rtype != CF_FNCALL)
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

static void ShowPromiseInReportHtml(const ReportContext *context, const char *version, const Promise *pp, int indent)
{
    assert(context);
    Writer *writer = context->report_writers[REPORT_OUTPUT_TYPE_HTML];
    assert(writer);
    if (!writer)
    {
        return;
    }

    WriterWriteF(writer, "%s\n", CFH[cfx_line][cfb]);
    WriterWriteF(writer, "%s\n", CFH[cfx_promise][cfb]);
    WriterWriteF(writer, "Promise type is %s%s%s, ", CFH[cfx_subtype][cfb], pp->agentsubtype, CFH[cfx_subtype][cfe]);
    WriterWriteF(writer, "<a href=\"#class_context\">context</a> is %s%s%s <br><hr>\n\n", CFH[cfx_class][cfb],
            pp->classes, CFH[cfx_class][cfe]);

    if (pp->promisee.item)
    {
        WriterWriteF(writer, "Resource object %s\'%s\'%s promises %s (about %s) to", CFH[cfx_object][cfb],
                pp->promiser, CFH[cfx_object][cfe], CFH[cfx_object][cfb], pp->agentsubtype);
        RvalPrint(writer, pp->promisee);
        WriterWriteF(writer, "%s\n\n", CFH[cfx_object][cfe]);
    }
    else
    {
        WriterWriteF(writer,
                "Resource object %s\'%s\'%s make the promise to default promisee 'cf-%s' (about %s)...\n\n",
                CFH[cfx_object][cfb], pp->promiser, CFH[cfx_object][cfe], pp->bundletype, pp->agentsubtype);
    }

    for (const Constraint *cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        WriterWriteF(writer, "%s%s%s => ", CFH[cfx_lval][cfb], cp->lval, CFH[cfx_lval][cfe]);

        Policy *policy = PolicyFromPromise(pp);

        const Body *bp = NULL;
        switch (cp->rval.rtype)
        {
        case CF_SCALAR:
            if ((bp = IsBody(policy->bodies, pp->namespace, (char *) cp->rval.item)))
            {
                ShowBodyHtml(writer, bp, 15);
            }
            else
            {
                WriterWriteF(writer, "%s", CFH[cfx_rval][cfb]);
                RvalPrint(writer, cp->rval);       /* literal */
                WriterWriteF(writer, "%s", CFH[cfx_rval][cfe]);
            }
            break;

        case CF_LIST:
            {
                const Rlist *rp = (Rlist *) cp->rval.item;
                WriterWriteF(writer, "%s", CFH[cfx_rval][cfb]);
                RlistPrint(writer, rp);
                WriterWriteF(writer, "%s", CFH[cfx_rval][cfe]);
                break;
            }

        case CF_FNCALL:
            {
                const FnCall *fp = (FnCall *) cp->rval.item;

                if ((bp = IsBody(policy->bodies, pp->namespace, fp->name)))
                {
                    ShowBodyHtml(writer, bp, 15);
                }
                else
                {
                    RvalPrint(writer, cp->rval);       /* literal */
                }
                break;
            }
        }

        if (cp->rval.rtype != CF_FNCALL)
        {
            WriterWriteF(writer,
                    " , if body <a href=\"#class_context\">context</a> <span class=\"context\">%s</span>\n",
                    cp->classes);
        }
    }

    if (pp->audit)
    {
        WriterWriteF(writer,
                "<p><small>Promise (version %s) belongs to bundle <b>%s</b> (type %s) in \'<i>%s</i>\' near line %zu</small></p>\n",
                version, pp->bundle, pp->bundletype, pp->audit->filename, pp->offset.line);
    }

    WriterWriteF(writer, "%s\n", CFH[cfx_promise][cfe]);
    WriterWriteF(writer, "%s\n", CFH[cfx_line][cfe]);
}

void ShowPromiseInReport(const ReportContext *context, ReportOutputType type, const char *version, const Promise *pp, int indent)
{
    switch (type)
    {
    case REPORT_OUTPUT_TYPE_HTML:
        return ShowPromiseInReportHtml(context, version, pp, indent);

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
        WriterWriteF(writer, "%8s %c %s = ", CF_DATATYPES[assoc->dtype], assoc->rval.rtype, assoc->lval);
        RvalPrint(writer, assoc->rval);
        WriterWriteF(writer, "\n");
    }
}

/*******************************************************************/

static void PrintVariablesInScopeHtml(Writer *writer, const Scope *scope)
{
    HashIterator i = HashIteratorInit(scope->hashtable);
    CfAssoc *assoc;

    WriterWriteF(writer, "<table class=border width=600>\n");
    WriterWriteF(writer, "<tr><th>dtype</th><th>rtype</th><th>identifier</th><th>Rvalue</th></tr>\n");

    while ((assoc = HashIteratorNext(&i)))
    {
        WriterWriteF(writer, "<tr><th>%8s</th><td> %c</td><td> %s</td><td> ", CF_DATATYPES[assoc->dtype], assoc->rval.rtype,
                assoc->lval);
        RvalPrint(writer, assoc->rval);
        WriterWriteF(writer, "</td></tr>\n");
    }

    WriterWriteF(writer, "</table>\n");
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

static void ShowScopedVariablesHtml(Writer *writer)
{
    WriterWriteF(writer, "<div id=\"showvars\">");

    for (const Scope *ptr = VSCOPE; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->scope, "this") == 0)
        {
            continue;
        }

        WriterWriteF(writer, "<h4>\nScope %s:<h4>", ptr->scope);

        PrintVariablesInScopeHtml(writer, ptr);
    }

    WriterWriteF(writer, "</div>");
}

void ShowScopedVariables(const ReportContext *context, ReportOutputType type)
/* WARNING: Not thread safe (access to VSCOPE) */
{
    switch (type)
    {
    case REPORT_OUTPUT_TYPE_HTML:
        ShowScopedVariablesHtml(context->report_writers[REPORT_OUTPUT_TYPE_HTML]);
        break;

    default:
    case REPORT_OUTPUT_TYPE_TEXT:
        ShowScopedVariablesText(context->report_writers[REPORT_OUTPUT_TYPE_TEXT]);
        break;
    }
}

/*******************************************************************/

void Banner(const char *s)
{
    CfOut(cf_verbose, "", "***********************************************************\n");
    CfOut(cf_verbose, "", " %s ", s);
    CfOut(cf_verbose, "", "***********************************************************\n");
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
    CfOut(cf_verbose, "", "\n");
    CfOut(cf_verbose, "", "   =========================================================\n");
    CfOut(cf_verbose, "", "   %s in bundle %s (%d)\n", type, bundlename, pass);
    CfOut(cf_verbose, "", "   =========================================================\n");
    CfOut(cf_verbose, "", "\n");
}

/**************************************************************/

void BannerSubSubType(const char *bundlename, const char *type)
{
    if (strcmp(type, "processes") == 0)
    {

        /* Just parsed all local classes */

        CfOut(cf_verbose, "", "     ??? Local class context: \n");

        AlphaListIterator it = AlphaListIteratorInit(&VADDCLASSES);

        for (const Item *ip = AlphaListIteratorNext(&it); ip != NULL; ip = AlphaListIteratorNext(&it))
        {
            printf("       %s\n", ip->name);
        }

        CfOut(cf_verbose, "", "\n");
    }

    CfOut(cf_verbose, "", "\n");
    CfOut(cf_verbose, "", "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
    CfOut(cf_verbose, "", "      %s in bundle %s\n", type, bundlename);
    CfOut(cf_verbose, "", "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
    CfOut(cf_verbose, "", "\n");
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
            if (rp->type != CF_SCALAR)
            {
                FatalError("ShowBody - non-scalar parameter container");
            }

            IndentText(writer, indent);
            WriterWriteF(writer, "arg %s\n", (char *) rp->item);
        }

        WriterWriteF(writer, "\n");
    }

    IndentText(writer, indent);
    WriterWriteF(writer, "{\n");

    for (const Constraint *cp = body->conlist; cp != NULL; cp = cp->next)
    {
        IndentText(writer, indent);
        WriterWriteF(writer, "%s => ", cp->lval);
        RvalPrint(writer, cp->rval);        /* literal */

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

static void ShowBodyHtml(Writer *writer, const Body *body, int indent)
{
    assert(writer);
    WriterWriteF(writer, " %s%s%s ", CFH[cfx_blocktype][cfb], body->type, CFH[cfx_blocktype][cfe]);

    WriterWriteF(writer, "%s%s%s", CFH[cfx_blockid][cfb], body->name, CFH[cfx_blockid][cfe]);

    if (body->args == NULL)
    {
        WriterWriteF(writer, "%s(no parameters)%s\n", CFH[cfx_args][cfb], CFH[cfx_args][cfe]);
    }
    else
    {
        WriterWriteF(writer, "(");

        for (const Rlist *rp = body->args; rp != NULL; rp = rp->next)
        {
            if (rp->type != CF_SCALAR)
            {
                FatalError("ShowBody - non-scalar parameter container");
            }

            WriterWriteF(writer, "%s%s%s,\n", CFH[cfx_args][cfb], (char *) rp->item, CFH[cfx_args][cfe]);
        }

        WriterWriteF(writer, ")");
    }

    for (const Constraint *cp = body->conlist; cp != NULL; cp = cp->next)
    {
        WriterWriteF(writer, "%s.....%s%s => ", CFH[cfx_lval][cfb], cp->lval, CFH[cfx_lval][cfe]);

        WriterWriteF(writer, "\'%s", CFH[cfx_rval][cfb]);

        RvalPrint(writer, cp->rval);       /* literal */

        WriterWriteF(writer, "\'%s", CFH[cfx_rval][cfe]);

        if (cp->classes != NULL)
        {
            WriterWriteF(writer, " if sub-body context %s%s%s\n", CFH[cfx_class][cfb], cp->classes,
                    CFH[cfx_class][cfe]);
        }
    }
}

/*******************************************************************/

void SyntaxTree(void)
{
    printf("<h1>CFENGINE %s SYNTAX</h1><p>", Version());

    printf("<table class=\"frame\"><tr><td>\n");
    ShowDataTypes();
    ShowControlBodies();
    ShowBundleTypes();
    ShowBuiltinFunctions();
    printf("</td></tr></table>\n");
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

static void ShowDataTypes()
{
    int i;

    printf("<table class=border><tr><td><h1>Promise datatype legend</h1>\n");
    printf("<ol>\n");

    for (i = 0; strcmp(CF_DATATYPES[i], "<notype>") != 0; i++)
    {
        printf("<li>%s</li>\n", CF_DATATYPES[i]);
    }

    printf("</ol></td></tr></table>\n\n");
}

/*******************************************************************/

static void ShowBundleTypes()
{
    int i;
    const SubTypeSyntax *st;

    printf("<h1>Bundle types (software components)</h1>\n");

    printf("<div id=\"bundles\">");

    for (i = 0; CF_ALL_BODIES[i].bundle_type != NULL; i++)
    {
        printf("<h4>COMPONENT %s</h4>\n", CF_ALL_BODIES[i].bundle_type);
        ShowPromiseTypesFor(CF_ALL_BODIES[i].bundle_type);
    }

    printf("<h4>EMBEDDED BUNDLE edit_line<h4>\n");

    ShowPromiseTypesFor("*");

    st = CF_FILES_SUBTYPES;

    for (i = 0; st[i].bundle_type != NULL; i++)
    {
        if (strcmp("edit_line", st[i].bundle_type) == 0)
        {
            ShowBodyParts(st[i].bs);
        }
    }

    printf("</div>\n\n");
}

/*******************************************************************/

static void ShowPromiseTypesFor(const char *s)
{
    int i, j;
    const SubTypeSyntax *st;

    printf("<div id=\"promisetype\">");
    printf("<h4>Promise types for %s bundles</h4>\n", s);
    printf("<table class=border><tr><td>\n");

    for (i = 0; i < CF3_MODULES; i++)
    {
        st = CF_ALL_SUBTYPES[i];

        for (j = 0; st[j].bundle_type != NULL; j++)
        {
            if ((strcmp(s, st[j].bundle_type) == 0) || (strcmp("*", st[j].bundle_type) == 0))
            {
                printf("<h4>PROMISE TYPE %s</h4>\n", st[j].subtype);
                ShowBodyParts(st[j].bs);
            }
        }
    }

    printf("</td></tr></table>\n");
    printf("</div>\n\n");
}

/*******************************************************************/

static void ShowBodyParts(const BodySyntax *bs)
{
    int i;

    if (bs == NULL)
    {
        return;
    }

    printf("<div id=\"bodies\"><table class=\"border\">\n");

    for (i = 0; bs[i].lval != NULL; i++)
    {
        if (bs[i].range == (void *) CF_BUNDLE)
        {
            printf("<tr><td>%s</td><td>%s</td><td>(Separate Bundle)</td></tr>\n", bs[i].lval,
                   CF_DATATYPES[bs[i].dtype]);
        }
        else if (bs[i].dtype == cf_body)
        {
            printf("<tr><td>%s</td><td>%s</td><td>", bs[i].lval, CF_DATATYPES[bs[i].dtype]);
            ShowBodyParts((const BodySyntax *) bs[i].range);
            printf("</td></tr>\n");
        }
        else
        {
            printf("<tr><td>%s</td><td>%s</td><td>", bs[i].lval, CF_DATATYPES[bs[i].dtype]);
            ShowRange((char *) bs[i].range, bs[i].dtype);
            printf("</td><td>");
            printf("<div id=\"description\">%s</div>", bs[i].description);
            printf("</td></tr>\n");
        }
    }

    printf("</table></div>\n");
}

/*******************************************************************/

static void ShowRange(const char *s, enum cfdatatype type)
{
    if (strlen(s) == 0)
    {
        printf("(arbitrary string)");
        return;
    }

    switch (type)
    {
    case cf_opts:
    case cf_olist:

        for (const char *sp = s; *sp != '\0'; sp++)
        {
            printf("%c", *sp);
            if (*sp == ',')
            {
                printf("<br>");
            }
        }

        break;

    default:
        for (const char *sp = s; *sp != '\0'; sp++)
        {
            printf("%c", *sp);
            if (*sp == '|')
            {
                printf("<br>");
            }
        }
    }
}

/*******************************************************************/

static void ShowBuiltinFunctions()
{
    int i;

    printf("<h1>builtin functions</h1>\n");

    printf("<center><table id=functionshow>\n");
    printf("<tr><th>Return type</th><th>Function name</th><th>Arguments</th><th>Description</th></tr>\n");

    for (i = 0; CF_FNCALL_TYPES[i].name != NULL; i++)
    {
        printf("<tr><td>%s</td><td>%s()</td><td>%d args expected</td><td>%s</td></tr>\n",
               CF_DATATYPES[CF_FNCALL_TYPES[i].dtype],
               CF_FNCALL_TYPES[i].name, FnNumArgs(&CF_FNCALL_TYPES[i]), CF_FNCALL_TYPES[i].description);
    }

    printf("</table></center>\n");
}

/*******************************************************************/

void ReportError(char *s)
{
    if (PARSING)
    {
        yyerror(s);
    }
    else
    {
        Chop(s);
        FatalError("Validation: %s\n", s);
    }
}
