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

#include "files_editline.h"

#include "env_context.h"
#include "promises.h"
#include "files_names.h"
#include "vars.h"
#include "item_lib.h"
#include "sort.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "scope.h"
#include "matching.h"
#include "attributes.h"
#include "logging.h"
#include "locks.h"
#include "string_lib.h"
#include "misc_lib.h"
#include "rlist.h"
#include "policy.h"
#include "ornaments.h"

#include <assert.h>

/*****************************************************************************/

enum editlinetypesequence
{
    elp_vars,
    elp_classes,
    elp_delete,
    elp_columns,
    elp_insert,
    elp_replace,
    elp_reports,
    elp_none
};

char *EDITLINETYPESEQUENCE[] =
{
    "vars",
    "classes",
    "delete_lines",
    "field_edits",
    "insert_lines",
    "replace_patterns",
    "reports",
    NULL
};

static void KeepEditLinePromise(EvalContext *ctx, Promise *pp, void *param);
static void VerifyLineDeletions(EvalContext *ctx, Promise *pp, EditContext *edcontext);
static void VerifyColumnEdits(EvalContext *ctx, Promise *pp, EditContext *edcontext);
static void VerifyPatterns(EvalContext *ctx, Promise *pp, EditContext *edcontext);
static void VerifyLineInsertions(EvalContext *ctx, Promise *pp, EditContext *edcontext);
static int InsertMultipleLinesToRegion(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Attributes a, Promise *pp, EditContext *edcontext);
static int InsertMultipleLinesAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext);
static int DeletePromisedLinesMatching(EvalContext *ctx, Item **start, Item *begin, Item *end, Attributes a, Promise *pp, EditContext *edcontext);
static int InsertLineAtLocation(EvalContext *ctx, char *newline, Item **start, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext);
static int InsertCompoundLineAtLocation(EvalContext *ctx, char *newline, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext);
static int ReplacePatterns(EvalContext *ctx, Item *start, Item *end, Attributes a, Promise *pp, EditContext *edcontext);
static int EditColumns(EvalContext *ctx, Item *file_start, Item *file_end, Attributes a, Promise *pp, EditContext *edcontext);
static int EditLineByColumn(EvalContext *ctx, Rlist **columns, Attributes a, Promise *pp, EditContext *edcontext);
static int DoEditColumn(Rlist **columns, Attributes a, EditContext *edcontext);
static int SanityCheckInsertions(Attributes a);
static int SanityCheckDeletions(Attributes a, Promise *pp);
static int SelectLine(char *line, Attributes a);
static int NotAnchored(char *s);
static void EditClassBanner(const EvalContext *ctx, enum editlinetypesequence type);
static int SelectRegion(EvalContext *ctx, Item *start, Item **begin_ptr, Item **end_ptr, Attributes a, Promise *pp, EditContext *edcontext);
static int MultiLineString(char *s);
static int InsertFileAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext);

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ScheduleEditLineOperations(EvalContext *ctx, Bundle *bp, Attributes a, const Promise *parentp, EditContext *edcontext)
{
    enum editlinetypesequence type;
    PromiseType *sp;
    char lockname[CF_BUFSIZE];
    CfLock thislock;
    int pass;

    snprintf(lockname, CF_BUFSIZE - 1, "masterfilelock-%s", edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, parentp, true);

    if (thislock.lock == NULL)
    {
        return false;
    }

    ScopeNewSpecialScalar(ctx, "edit", "filename", edcontext->filename, DATA_TYPE_STRING);

    for (pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (type = 0; EDITLINETYPESEQUENCE[type] != NULL; type++)
        {
            EditClassBanner(ctx, type);

            if ((sp = BundleGetPromiseType(bp, EDITLINETYPESEQUENCE[type])) == NULL)
            {
                continue;
            }

            BannerSubPromiseType(ctx, bp->name, sp->name);
            ScopeSetCurrent(bp->name);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                ExpandPromise(ctx, pp, KeepEditLinePromise, edcontext);

                if (Abort())
                {
                    ScopeClear("edit");
                    YieldCurrentLock(thislock);
                    return false;
                }
            }
        }
    }

    ScopeClear("edit");
    ScopeSetCurrent(PromiseGetBundle(parentp)->name);
    YieldCurrentLock(thislock);
    return true;
}

/*****************************************************************************/

Bundle *MakeTemporaryBundleFromTemplate(EvalContext *ctx, Policy *policy, Attributes a, const Promise *pp)
{
    FILE *fp = NULL;
    if ((fp = fopen(a.template, "r" )) == NULL)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! Unable to open template file \"%s\" to make \"%s\"", a.template, pp->promiser);
        return NULL;
    }

    Bundle *bp = NULL;
    {
        char bundlename[CF_MAXVARSIZE];
        snprintf(bundlename, CF_MAXVARSIZE, "temp_cf_bundle_%s", CanonifyName(a.template));

        bp = PolicyAppendBundle(policy, "temp", bundlename, "edit_line", NULL, NULL);
    }
    assert(bp);

    {
        PromiseType *tp = BundleAppendPromiseType(bp, "insert_lines");
        Promise *np = NULL;
        Item *lines = NULL;
        char context[CF_BUFSIZE] = "any";
        int lineno = 0;
        size_t level = 0;
        char buffer[CF_BUFSIZE];

        for (;;)
        {
            if (fgets(buffer, CF_BUFSIZE, fp) == NULL)
            {
                if (ferror(fp))
                {
                    UnexpectedError("Failed to read line from stream");
                    break;
                }
                else /* feof */
                {
                    break;
                }
            }
            lineno++;

            // Check closing syntax

            // Get Action operator
            if (strncmp(buffer, "[%CFEngine", strlen("[%CFEngine")) == 0)
            {
                char op[CF_BUFSIZE], brack[CF_SMALLBUF];

                sscanf(buffer+strlen("[%CFEngine"), "%1024s %s", op, brack);

                if (strcmp(brack, "%]") != 0)
                {
                    cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! Template file \"%s\" syntax error, missing close \"%%]\" at line %d", a.template, lineno);
                    return NULL;
                }

                if (strcmp(op, "BEGIN") == 0)
                {
                    // start new buffer

                    if (++level > 1)
                    {
                        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! Template file \"%s\" contains nested blocks which are not allowed, near line %d", a.template, lineno);
                        return NULL;
                    }

                    continue;
                }

                if (strcmp(op, "END") == 0)
                {
                    // install buffer
                    level--;
                }

                if (strcmp(op + strlen(op)-2, "::") == 0)
                {
                    *(op + strlen(op)-2) = '\0';
                    strcpy(context, op);
                    continue;
                }

                size_t size = 0;
                for (const Item *ip = lines; ip != NULL; ip = ip->next)
                {
                    size += strlen(ip->name);
                }

                char *promiser = NULL;
                char *sp = promiser = xcalloc(1, size+1);

                for (const Item *ip = lines; ip != NULL; ip = ip->next)
                {
                    int len = strlen(ip->name);
                    strncpy(sp, ip->name, len);
                    sp += len;
                }

                *(sp-1) = '\0'; // StripTrailingNewline(promiser) and terminate

                np = PromiseTypeAppendPromise(tp, promiser, (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, context);
                PromiseAppendConstraint(np, "insert_type", (Rval) { xstrdup("preserve_block"), RVAL_TYPE_SCALAR }, "any", false);

                DeleteItemList(lines);
                free(promiser);
                lines = NULL;
            }
            else
            {
                if (level > 0)
                {
                    AppendItem(&lines, buffer, NULL);
                }
                else
                {
                    //install independent promise line
                    if (StripTrailingNewline(buffer, CF_EXPANDSIZE) == -1)
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "", "StripTrailingNewline was called on an overlong string");
                    }
                    np = PromiseTypeAppendPromise(tp, buffer, (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, context);
                    PromiseAppendConstraint(np, "insert_type", (Rval) { xstrdup("preserve_block"), RVAL_TYPE_SCALAR }, "any", false);
                }
            }
        }
    }

    fclose(fp);
    return bp;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

// TODO: more really ugly output
static void EditClassBanner(const EvalContext *ctx, enum editlinetypesequence type)
{
    if (type != elp_delete)     /* Just parsed all local classes */
    {
        return;
    }

    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "     ??  Private class context\n");

        StringSetIterator it = EvalContextStackFrameIteratorSoft(ctx);
        const char *context = NULL;
        while ((context = StringSetIteratorNext(&it)))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "     ??       %s\n", context);
        }

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    }
}

/***************************************************************************/

static void KeepEditLinePromise(EvalContext *ctx, Promise *pp, void *param)
{
    EditContext *edcontext = param;

    char *sp = NULL;

    if (!IsDefinedClass(ctx, pp->classes, PromiseGetNamespace(pp)))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Skipping whole next edit promise, as context %s is not relevant\n", pp->classes);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        return;
    }

    if (VarClassExcluded(ctx, pp, &sp))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole next edit promise (%s), as var-context %s is not relevant\n",
              pp->promiser, sp);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    PromiseBanner(pp);

    if (strcmp("classes", pp->parent_promise_type->name) == 0)
    {
        KeepClassContextPromise(ctx, pp, NULL);
        return;
    }

    if (strcmp("delete_lines", pp->parent_promise_type->name) == 0)
    {
        VerifyLineDeletions(ctx, pp, edcontext);
        return;
    }

    if (strcmp("field_edits", pp->parent_promise_type->name) == 0)
    {
        VerifyColumnEdits(ctx, pp, edcontext);
        return;
    }

    if (strcmp("insert_lines", pp->parent_promise_type->name) == 0)
    {
        VerifyLineInsertions(ctx, pp, edcontext);
        return;
    }

    if (strcmp("replace_patterns", pp->parent_promise_type->name) == 0)
    {
        VerifyPatterns(ctx, pp, edcontext);
        return;
    }

    if (strcmp("reports", pp->parent_promise_type->name) == 0)
    {
        VerifyReportPromise(ctx, pp);
        return;
    }
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static void VerifyLineDeletions(EvalContext *ctx, Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start);
    Attributes a = { {0} };
    Item *begin_ptr, *end_ptr;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetDeletionAttributes(ctx, pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckDeletions(a, pp))
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! The promised line deletion (%s) is inconsistent", pp->promiser);
        return;
    }

/* Are we working in a restricted region? */

    if (!a.haveregion)
    {
        begin_ptr = CF_UNDEFINED_ITEM;
        end_ptr = CF_UNDEFINED_ITEM;
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext))
    {
        if (a.region.include_end || a.region.include_start)
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, "", pp, a,
                 " !! The promised line deletion (%s) could not select an edit region in %s (this is a good thing, as policy suggests deleting the markers)",
                 pp->promiser, edcontext->filename);
        }
        else
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_INTERRUPTED, "", pp, a,
                 " !! The promised line deletion (%s) could not select an edit region in %s (but the delimiters were expected in the file)",
                 pp->promiser, edcontext->filename);
        }
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deleteline-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (DeletePromisedLinesMatching(ctx, start, begin_ptr, end_ptr, a, pp, edcontext))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyColumnEdits(EvalContext *ctx, Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start);
    Attributes a = { {0} };
    Item *begin_ptr, *end_ptr;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetColumnAttributes(ctx, pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (a.column.column_separator == NULL)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, "No field_separator in promise to edit by column for %s", pp->promiser);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return;
    }

    if (a.column.select_column <= 0)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, "No select_field in promise to edit %s", pp->promiser);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return;
    }

    if (!a.column.column_value)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, "No field_value is promised to column_edit %s", pp->promiser);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return;
    }

/* Are we working in a restricted region? */

    if (!a.haveregion)
    {
        begin_ptr = *start;
        end_ptr = NULL;         // EndOfList(*start);
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext))
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! The promised column edit (%s) could not select an edit region in %s",
             pp->promiser, edcontext->filename);
        return;
    }

/* locate and split line */

    snprintf(lockname, CF_BUFSIZE - 1, "column-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (EditColumns(ctx, begin_ptr, end_ptr, a, pp, edcontext))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyPatterns(EvalContext *ctx, Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start);
    Attributes a = { {0} };
    Item *begin_ptr, *end_ptr;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Looking at pattern %s\n", pp->promiser);

/* Are we working in a restricted region? */

    a = GetReplaceAttributes(ctx, pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!a.replace.replace_value)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! The promised pattern replace (%s) had no replacement string",
             pp->promiser);
        return;
    }

    if (!a.haveregion)
    {
        begin_ptr = *start;
        end_ptr = NULL;         //EndOfList(*start);
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext))
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a,
             " !! The promised pattern replace (%s) could not select an edit region in %s", pp->promiser,
             edcontext->filename);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "replace-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

/* Make sure back references are expanded */

    if (ReplacePatterns(ctx, begin_ptr, end_ptr, a, pp, edcontext))
    {
        (edcontext->num_edits)++;
    }

    ScopeClear("match");       // because this might pollute the parent promise in next iteration

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyLineInsertions(EvalContext *ctx, Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start), *match, *prev;
    Item *begin_ptr, *end_ptr;
    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetInsertionAttributes(ctx, pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckInsertions(a))
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! The promised line insertion (%s) breaks its own promises",
             pp->promiser);
        return;
    }

    /* Are we working in a restricted region? */

    if (!a.haveregion)
    {
        begin_ptr = *start;
        end_ptr = NULL;         //EndOfList(*start);
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext))
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a,
             " !! The promised line insertion (%s) could not select an edit region in %s", pp->promiser,
             edcontext->filename);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "insertline-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    /* Are we looking for an anchored line inside the region? */

    if (a.location.line_matching == NULL)
    {
        if (InsertMultipleLinesToRegion(ctx, start, begin_ptr, end_ptr, a, pp, edcontext))
        {
            (edcontext->num_edits)++;
        }
    }
    else
    {
        if (!SelectItemMatching(*start, a.location.line_matching, begin_ptr, end_ptr, &match, &prev, a.location.first_last))
        {
            cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! The promised line insertion (%s) could not select a locator matching regex \"%s\" in %s", pp->promiser, a.location.line_matching, edcontext->filename);
            YieldCurrentLock(thislock);
            return;
        }

        if (InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, match, prev, a, pp, edcontext))
        {
            (edcontext->num_edits)++;
        }
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int SelectRegion(EvalContext *ctx, Item *start, Item **begin_ptr, Item **end_ptr, Attributes a, Promise *pp, EditContext *edcontext)
/*

This should provide pointers to the first and last line of text that include the
delimiters, since we need to include those in case they are being deleted, etc.
It returns true if a match was identified, else false.

If no such region matches, begin_ptr and end_ptr should point to CF_UNDEFINED_ITEM

*/
{
    Item *ip, *beg = CF_UNDEFINED_ITEM, *end = CF_UNDEFINED_ITEM;

    for (ip = start; ip != NULL; ip = ip->next)
    {
        if (a.region.select_start)
        {
            if (beg == CF_UNDEFINED_ITEM && FullTextMatch(a.region.select_start, ip->name))
            {
                if (!a.region.include_start)
                {
                    if (ip->next == NULL)
                    {
                        cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, "", pp, a,
                             " !! The promised start pattern (%s) found an empty region at the end of file %s",
                             a.region.select_start, edcontext->filename);
                        return false;
                    }
                }

                beg = ip;
                continue;
            }
        }

        if (a.region.select_end && beg != CF_UNDEFINED_ITEM)
        {
            if (end == CF_UNDEFINED_ITEM && FullTextMatch(a.region.select_end, ip->name))
            {
                end = ip;
                break;
            }
        }

        if (beg != CF_UNDEFINED_ITEM && end != CF_UNDEFINED_ITEM)
        {
            break;
        }
    }

    if (beg == CF_UNDEFINED_ITEM && a.region.select_start)
    {
        cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, "", pp, a,
             " !! The promised start pattern (%s) was not found when selecting edit region in %s",
             a.region.select_start, edcontext->filename);
        return false;
    }

    if (end == CF_UNDEFINED_ITEM)
    {
        end = NULL;
    }

    *begin_ptr = beg;
    *end_ptr = end;

    return true;
}

/***************************************************************************/

static int InsertMultipleLinesToRegion(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Attributes a, Promise *pp, EditContext *edcontext)
{
    Item *ip, *prev = CF_UNDEFINED_ITEM;

    // Insert at the start of the file
    
    if (*start == NULL)
    {
        return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, *start, prev, a, pp, edcontext);
    }

    // Insert at the start of the region
    
    if (a.location.before_after == EDIT_ORDER_BEFORE)
    {
        for (ip = *start; ip != NULL; ip = ip->next)
        {
            if (ip == begin_ptr)
            {
                return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, ip, prev, a, pp, edcontext);
            }

            prev = ip;
        }
    }

    // Insert at the end of the region / else end of the file

    if (a.location.before_after == EDIT_ORDER_AFTER)
    {
        for (ip = *start; ip != NULL; ip = ip->next)
        {
            if (ip->next != NULL && ip->next == end_ptr)
            {
                return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, ip, prev, a, pp, edcontext);
            }

            if (ip->next == NULL)
            {
                return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, ip, prev, a, pp, edcontext);
            }

            prev = ip;
        }
    }

    return false;
}

/***************************************************************************/

static int InsertMultipleLinesAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext)

// Promises to insert a possibly multi-line promiser at the specificed location convergently,
// i.e. no insertion will be made if a neighbouring line matches

{
    int isfileinsert = a.sourcetype && (strcmp(a.sourcetype, "file") == 0 || strcmp(a.sourcetype, "file_preserve_block") == 0);

    if (isfileinsert)
    {
        return InsertFileAtLocation(ctx, start, begin_ptr, end_ptr, location, prev, a, pp, edcontext);
    }
    else
    {
        return InsertCompoundLineAtLocation(ctx, pp->promiser, start, begin_ptr, end_ptr, location, prev, a, pp, edcontext);
    }
}

/***************************************************************************/

static int DeletePromisedLinesMatching(EvalContext *ctx, Item **start, Item *begin, Item *end, Attributes a, Promise *pp, EditContext *edcontext)
{
    Item *ip, *np = NULL, *lp, *initiator = begin, *terminator = NULL;
    int i, retval = false, matches, noedits = true;

    if (start == NULL)
    {
        return false;
    }

// Get a pointer from before the region so we can patch the hole later

    if (begin == CF_UNDEFINED_ITEM)
    {
        initiator = *start;
    }
    else
    {
        if (a.region.include_start)
        {
            initiator = begin;
        }
        else
        {
            initiator = begin->next;
        }
    }

    if (end == CF_UNDEFINED_ITEM)
    {
        terminator = NULL;
    }
    else
    {
        if (a.region.include_end)
        {
            terminator = end->next;
        }
        else
        {
            terminator = end;
        }
    }

// Now do the deletion

    for (ip = initiator; ip != terminator && ip != NULL; ip = np)
    {
        if (a.not_matching)
        {
            matches = !MatchRegion(pp->promiser, ip, terminator, true);
        }
        else
        {
            matches = MatchRegion(pp->promiser, ip, terminator, true);
        }

        if (matches)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Multi-line region (%d lines) matched text in the file", matches);
        }
        else
        {
            CfDebug(" -> Multi-line region didn't match text in the file");
        }

        if (!SelectLine(ip->name, a))       // Start search from location
        {
            np = ip->next;
            continue;
        }

        if (matches)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Delete chunk of %d lines\n", matches);

            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                     " -> Need to delete line \"%s\" from %s - but only a warning was promised", ip->name,
                     edcontext->filename);
                np = ip->next;
                noedits = false;
            }
            else
            {
                for (i = 1; i <= matches; i++)
                {
                    cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, " -> Deleting the promised line %d \"%s\" from %s", i, ip->name,
                         edcontext->filename);
                    retval = true;
                    noedits = false;

                    if (ip->name != NULL)
                    {
                        free(ip->name);
                    }

                    np = ip->next;
                    free((char *) ip);

                    lp = ip;

                    if (ip == *start)
                    {
                        if (initiator == *start)
                        {
                            initiator = np;
                        }
                        *start = np;
                    }
                    else
                    {
                        if (ip == initiator)
                        {
                            initiator = *start;
                        }

                        for (lp = initiator; lp->next != ip; lp = lp->next)
                        {
                        }

                        lp->next = np;
                    }

                    (edcontext->num_edits)++;

                    ip = np;
                }
            }
        }
        else
        {
            np = ip->next;
        }
    }

    if (noedits)
    {
        cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a, " -> No need to delete lines from %s, ok", edcontext->filename);
    }

    return retval;
}

/********************************************************************/

static int ReplacePatterns(EvalContext *ctx, Item *file_start, Item *file_end, Attributes a, Promise *pp, EditContext *edcontext)
{
    char replace[CF_EXPANDSIZE], line_buff[CF_EXPANDSIZE];
    char before[CF_BUFSIZE], after[CF_BUFSIZE];
    int match_len, start_off, end_off, once_only = false, retval = false;
    Item *ip;
    int notfound = true, cutoff = 1, replaced = false;

    if (a.replace.occurrences && (strcmp(a.replace.occurrences, "first") == 0))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "WARNING! Setting replace-occurrences policy to \"first\" is not convergent");
        once_only = true;
    }

    for (ip = file_start; ip != NULL && ip != file_end; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        cutoff = 1;
        strncpy(line_buff, ip->name, CF_BUFSIZE);
        replaced = false;
        match_len = 0;

        while (BlockTextMatch(pp->promiser, line_buff, &start_off, &end_off))
        {
            if (match_len == strlen(line_buff))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Improper convergent expression matches defacto convergence, so accepting");
                break;
            }

            if (cutoff++ > CF_MAX_REPLACE)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Too many replacements on this line");
                break;
            }

            match_len = end_off - start_off;
            ExpandScalar(ctx, PromiseGetBundle(pp)->name, a.replace.replace_value, replace);

            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verifying replacement of \"%s\" with \"%s\" (%d)\n", pp->promiser, replace,
                  cutoff);

            before[0] = after[0] = '\0';

            // Model the partial substitution in line_buff to check convergence

            strncat(before, line_buff, start_off);
            strncat(after, line_buff + end_off, sizeof(after) - 1);
            snprintf(line_buff, CF_EXPANDSIZE - 1, "%s%s", before, replace);
            notfound = false;
            replaced = true;

            // Model the full substitution in line_buff

            snprintf(line_buff, CF_EXPANDSIZE - 1, "%s%s%s", before, replace, after);

            if (once_only)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Replace first occurrence only (warning, this is not a convergent policy)");
                break;
            }
        }

        if (NotAnchored(pp->promiser) && BlockTextMatch(pp->promiser, line_buff, &start_off, &end_off))
        {
            cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a,
                 " -> Promised replacement \"%s\" on line \"%s\" for pattern \"%s\" is not convergent while editing %s",
                 line_buff, ip->name, pp->promiser, edcontext->filename);
            CfOut(OUTPUT_LEVEL_ERROR, "", "Because the regular expression \"%s\" still matches the replacement string \"%s\"",
                  pp->promiser, line_buff);
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            break;
        }

        if (a.transaction.action == cfa_warn)
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_WARN, "", pp, a,
                 " -> Need to replace line \"%s\" in %s - but only a warning was promised", pp->promiser,
                 edcontext->filename);
            continue;
        }
        else if (replaced)
        {
            free(ip->name);
            ip->name = xstrdup(line_buff);
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, " -> Replaced pattern \"%s\" in %s", pp->promiser, edcontext->filename);
            (edcontext->num_edits)++;
            retval = true;

            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> << (%d)\"%s\"\n", cutoff, ip->name);
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> >> (%d)\"%s\"\n", cutoff, line_buff);

            if (once_only)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Replace first occurrence only (warning, this is not a convergent policy)");
                break;
            }

            if (BlockTextMatch(pp->promiser, ip->name, &start_off, &end_off))
            {
                cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_INTERRUPTED, "", pp, a,
                     " -> Promised replacement \"%s\" for pattern \"%s\" is not properly convergent while editing %s",
                     ip->name, pp->promiser, edcontext->filename);
                CfOut(OUTPUT_LEVEL_INFORM, "",
                      "Because the regular expression \"%s\" still matches the end-state replacement string \"%s\"",
                      pp->promiser, line_buff);
                PromiseRef(OUTPUT_LEVEL_INFORM, pp);
            }
        }
    }

    if (notfound)
    {
        cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a, " -> No pattern \"%s\" in %s", pp->promiser, edcontext->filename);
    }

    return retval;
}

/********************************************************************/

static int EditColumns(EvalContext *ctx, Item *file_start, Item *file_end, Attributes a, Promise *pp, EditContext *edcontext)
{
    char separator[CF_MAXVARSIZE];
    int s, e, retval = false;
    Item *ip;
    Rlist *columns = NULL;

    if (!ValidateRegEx(pp->promiser))
    {
        return false;
    }

    for (ip = file_start; ip != file_end; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        if (!FullTextMatch(pp->promiser, ip->name))
        {
            continue;
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " - Matched line (%s)\n", ip->name);
        }

        if (!BlockTextMatch(a.column.column_separator, ip->name, &s, &e))
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, "", pp, a, " ! Field edit - no fields found by promised pattern %s in %s",
                 a.column.column_separator, edcontext->filename);
            return false;
        }

        if (e - s > CF_MAXVARSIZE / 2)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Line split criterion matches a huge part of the line -- seems to be in error");
            return false;
        }

        strncpy(separator, ip->name + s, e - s);
        separator[e - s] = '\0';

        columns = RlistFromSplitRegex(ip->name, a.column.column_separator, CF_INFINITY, a.column.blanks_ok);
        retval = EditLineByColumn(ctx, &columns, a, pp, edcontext);

        if (retval)
        {
            free(ip->name);
            ip->name = Rlist2String(columns, separator);
        }

        RlistDestroy(columns);
    }

    return retval;
}

/***************************************************************************/

static int SanityCheckInsertions(Attributes a)
{
    long not = 0;
    long with = 0;
    long ok = true;
    Rlist *rp;
    InsertMatchType opt;
    int exact = false, ignore_something = false;
    int preserve_block = a.sourcetype && strcmp(a.sourcetype, "preserve_block") == 0;

    if (a.line_select.startwith_from_list)
    {
        with++;
    }

    if (a.line_select.not_startwith_from_list)
    {
        not++;
    }

    if (a.line_select.match_from_list)
    {
        with++;
    }

    if (a.line_select.not_match_from_list)
    {
        not++;
    }

    if (a.line_select.contains_from_list)
    {
        with++;
    }

    if (a.line_select.not_contains_from_list)
    {
        not++;
    }

    if (not > 1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Line insertion selection promise is meaningless - the alternatives are mutually exclusive (only one is allowed)");
        ok = false;
    }

    if (with && not)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Line insertion selection promise is meaningless - cannot mix positive and negative constraints");
        ok = false;
    }

    for (rp = a.insert_match; rp != NULL; rp = rp->next)
    {
        opt = InsertMatchTypeFromString(rp->item);

        switch (opt)
        {
        case INSERT_MATCH_TYPE_EXACT:
            exact = true;
            break;
        default:
            ignore_something = true;
            if (preserve_block)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Line insertion should not use whitespace policy with preserve_block");
                ok = false;
            }
            break;
        }
    }

    if (exact && ignore_something)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Line insertion selection promise is meaningless - cannot mix exact_match with other ignore whitespace options");
        ok = false;
    }

    return ok;
}

/***************************************************************************/

static int SanityCheckDeletions(Attributes a, Promise *pp)
{
    if (MultiLineString(pp->promiser))
    {
        if (a.not_matching)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "",
                  " !! Makes no sense to promise multi-line delete with not_matching. Cannot be satisfied for all lines as a block.");
        }
    }

    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int InsertFileAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext)
{
    FILE *fin;
    char buf[CF_BUFSIZE], exp[CF_EXPANDSIZE];
    int retval = false;
    Item *loc = NULL;
    int preserve_block = a.sourcetype && strcmp(a.sourcetype, "file_preserve_block") == 0;

    if ((fin = fopen(pp->promiser, "r")) == NULL)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "fopen", pp, a, "Could not read file %s", pp->promiser);
        return false;
    }
    
    loc = location;

    for(;;)
    {
        if (fgets(buf, CF_BUFSIZE, fin) == NULL)
        {
            if (ferror(fin)) {
                if (errno == EISDIR)
                {
                    cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a, "Could not read file %s: Is a directory", pp->promiser);
                    break;
                }
                else
                {
                    UnexpectedError("Failed to read line from stream");
                    break;
                }
            }
            else /* feof */
            {
                break;
            }
        }
        if (StripTrailingNewline(buf, CF_EXPANDSIZE) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "StripTrailingNewline was called on an overlong string");
        }
        
        if (feof(fin) && strlen(buf) == 0)
        {
            break;
        }
        
        if (a.expandvars)
        {
            ExpandScalar(ctx, PromiseGetBundle(pp)->name, buf, exp);
        }
        else
        {
            strcpy(exp, buf);
        }
        
        if (!SelectLine(exp, a))
        {
            continue;
        }
        
        if (!preserve_block && IsItemInRegion(exp, begin_ptr, end_ptr, a.insert_match, pp))
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a,
                 " -> Promised file line \"%s\" exists within file %s (promise kept)", exp, edcontext->filename);
            continue;
        }
        
        // Need to call CompoundLine here in case ExpandScalar has inserted \n into a string
        
        retval |= InsertCompoundLineAtLocation(ctx, exp, start, begin_ptr, end_ptr, loc, prev, a, pp, edcontext);

        if (preserve_block && prev == CF_UNDEFINED_ITEM)
           {
           // If we are inserting a preserved block before, need to flip the implied order after the first insertion
           // to get the order of the block right
           //a.location.before_after = cfe_after;
           }
        
        if (prev && prev != CF_UNDEFINED_ITEM)
        {
            prev = prev->next;
        }
        else
        {
            prev = *start;
        }
        
        if (loc)
        {
            loc = loc->next;
        }
        else
        {
            location = *start;
        }
    }
    
    fclose(fin);
    return retval;
    
}

/***************************************************************************/
    
static int InsertCompoundLineAtLocation(EvalContext *ctx, char *chunk, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext)
{
    int result = false;
    char buf[CF_EXPANDSIZE];
    char *sp;
    int preserve_block = a.sourcetype && (strcmp(a.sourcetype, "preserve_block") == 0 || strcmp(a.sourcetype, "file_preserve_block") == 0);

    if (MatchRegion(chunk, location, NULL, false))
    {
        return false;
    }

    // Iterate over any lines within the chunk

    for (sp = chunk; sp <= chunk + strlen(chunk); sp++)
    {
        memset(buf, 0, CF_BUFSIZE);
        sscanf(sp, "%2048[^\n]", buf);
        sp += strlen(buf);

        if (!SelectLine(buf, a))
        {
            continue;
        }

        if (!preserve_block && IsItemInRegion(buf, begin_ptr, end_ptr, a.insert_match, pp))
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a, " -> Promised chunk \"%s\" exists within selected region of %s (promise kept)", pp->promiser, edcontext->filename);
            continue;
        }

        result |= InsertLineAtLocation(ctx, buf, start, location, prev, a, pp, edcontext);

        if (preserve_block && a.location.before_after == EDIT_ORDER_BEFORE && location == NULL && prev == CF_UNDEFINED_ITEM)
        {
            // If we are inserting a preserved block before, need to flip the implied order after the first insertion
            // to get the order of the block right
            // a.location.before_after = cfe_after;
            location = *start;
        }
        
        if (prev && prev != CF_UNDEFINED_ITEM)
        {
            prev = prev->next;
        }
        else
        {
            prev = *start;
        }
        
        if (location)
        {
            location = location->next;
        }
        else
        {
            location = *start;
        }
    }
    
    return result;
}

/***************************************************************************/

static int InsertLineAtLocation(EvalContext *ctx, char *newline, Item **start, Item *location, Item *prev, Attributes a, Promise *pp, EditContext *edcontext)

/* Check line neighbourhood in whole file to avoid edge effects, iff we are not preseving block structure */

{   int preserve_block = a.sourcetype && strcmp(a.sourcetype, "preserve_block") == 0;

    if (prev == CF_UNDEFINED_ITEM)      /* Insert at first line */
    {
        if (a.location.before_after == EDIT_ORDER_BEFORE)
        {
            if (*start == NULL)
            {
                if (a.transaction.action == cfa_warn)
                {
                    cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                         " -> Need to insert the promised line \"%s\" in %s - but only a warning was promised", newline,
                         edcontext->filename);
                    return true;
                }
                else
                {
                    PrependItemList(start, newline);
                    (edcontext->num_edits)++;
                    cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, " -> Inserting the promised line \"%s\" into %s", newline,
                         edcontext->filename);
                    return true;
                }
            }

            if (strcmp((*start)->name, newline) != 0)
            {
                if (a.transaction.action == cfa_warn)
                {
                    cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                         " -> Need to prepend the promised line \"%s\" to %s - but only a warning was promised",
                         newline, edcontext->filename);
                    return true;
                }
                else
                {
                    PrependItemList(start, newline);
                    (edcontext->num_edits)++;
                    cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, " -> Prepending the promised line \"%s\" to %s", newline,
                         edcontext->filename);
                    return true;
                }
            }
            else
            {
                cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a,
                     " -> Promised line \"%s\" exists at start of file %s (promise kept)", newline, edcontext->filename);
                return false;
            }
        }
    }

    if (a.location.before_after == EDIT_ORDER_BEFORE)
    {    
        if (!preserve_block && NeighbourItemMatches(*start, location, newline, EDIT_ORDER_BEFORE, a.insert_match, pp))
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a, " -> Promised line \"%s\" exists before locator in (promise kept)",
                 newline);
            return false;
        }
        else
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                     " -> Need to insert line \"%s\" into %s but only a warning was promised", newline,
                     edcontext->filename);
                return true;
            }
            else
            {
                InsertAfter(start, prev, newline);
                (edcontext->num_edits)++;
                cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, " -> Inserting the promised line \"%s\" into %s before locator",
                     newline, edcontext->filename);
                return true;
            }
        }
    }
    else
    {
        if (!preserve_block && NeighbourItemMatches(*start, location, newline, EDIT_ORDER_AFTER, a.insert_match, pp))
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a, " -> Promised line \"%s\" exists after locator (promise kept)",
                 newline);
                        printf("NEIGHMATCH %s\n",newline);
            return false;
        }
        else
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                     " -> Need to insert line \"%s\" in %s but only a warning was promised", newline, edcontext->filename);
                return true;
            }
            else
            {
                InsertAfter(start, location, newline);
                cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, " -> Inserting the promised line \"%s\" into %s after locator",
                     newline, edcontext->filename);
                (edcontext->num_edits)++;
                return true;
            }
        }
    }
}

/***************************************************************************/

static int EditLineByColumn(EvalContext *ctx, Rlist **columns, Attributes a, Promise *pp, EditContext *edcontext)
{
    Rlist *rp, *this_column = NULL;
    char sep[CF_MAXVARSIZE];
    int i, count = 0, retval = false;

/* Now break up the line into a list - not we never remove an item/column */

    for (rp = *columns; rp != NULL; rp = rp->next)
    {
        count++;

        if (count == a.column.select_column)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Stopped at field %d\n", count);
            break;
        }
    }

    if (a.column.select_column > count)
    {
        if (!a.column.extend_columns)
        {
            cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "", pp, a,
                 " !! The file %s has only %d fields, but there is a promise for field %d", edcontext->filename, count,
                 a.column.select_column);
            return false;
        }
        else
        {
            for (i = 0; i < (a.column.select_column - count); i++)
            {
                RlistAppendScalar(columns, xstrdup(""));
            }

            count = 0;

            for (rp = *columns; rp != NULL; rp = rp->next)
            {
                count++;
                if (count == a.column.select_column)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Stopped at column/field %d\n", count);
                    break;
                }
            }
        }
    }

    if (a.column.value_separator != '\0')
    {
        /* internal separator, single char so split again */

        if (strcmp(rp->item, a.column.column_value) == 0)
        {
            retval = false;
        }
        else
        {
            this_column = RlistFromSplitString(rp->item, a.column.value_separator);
            retval = DoEditColumn(&this_column, a, edcontext);
        }

        if (retval)
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a, " -> Need to edit field in %s but only warning promised",
                     edcontext->filename);
                retval = false;
            }
            else
            {
                cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, " -> Edited field inside file object %s", edcontext->filename);
                (edcontext->num_edits)++;
                free(rp->item);
                sep[0] = a.column.value_separator;
                sep[1] = '\0';
                rp->item = Rlist2String(this_column, sep);
            }
        }
        else
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a, " -> No need to edit field in %s", edcontext->filename);
        }

        RlistDestroy(this_column);
        return retval;
    }
    else
    {
        /* No separator, so we set the whole field to the value */

        if (a.column.column_operation && strcmp(a.column.column_operation, "delete") == 0)
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                     " -> Need to delete field field value %s in %s but only a warning was promised", RlistScalarValue(rp),
                     edcontext->filename);
                return false;
            }
            else
            {
                cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, " -> Deleting column field value %s in %s", RlistScalarValue(rp),
                     edcontext->filename);
                (edcontext->num_edits)++;
                free(rp->item);
                rp->item = xstrdup("");
                return true;
            }
        }
        else
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_WARN, "", pp, a,
                     " -> Need to set column field value %s to %s in %s but only a warning was promised",
                     RlistScalarValue(rp), a.column.column_value, edcontext->filename);
                return false;
            }
            else
            {
                cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, " -> Setting whole column field value %s to %s in %s",
                     RlistScalarValue(rp), a.column.column_value, edcontext->filename);
                free(rp->item);
                rp->item = xstrdup(a.column.column_value);
                (edcontext->num_edits)++;
                return true;
            }
        }
    }

    cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, "", pp, a, " -> No need to edit column field value %s in %s", a.column.column_value,
         edcontext->filename);

    return false;
}

/***************************************************************************/

static int SelectLine(char *line, Attributes a)
{
    Rlist *rp, *c;
    int s, e;
    char *selector;

    if ((c = a.line_select.startwith_from_list))
    {
        for (rp = c; rp != NULL; rp = rp->next)
        {
            selector = (char *) (rp->item);

            if (strncmp(selector, line, strlen(selector)) == 0)
            {
                return true;
            }
        }

        return false;
    }

    if ((c = a.line_select.not_startwith_from_list))
    {
        for (rp = c; rp != NULL; rp = rp->next)
        {
            selector = (char *) (rp->item);

            if (strncmp(selector, line, strlen(selector)) == 0)
            {
                return false;
            }
        }

        return true;
    }

    if ((c = a.line_select.match_from_list))
    {
        for (rp = c; rp != NULL; rp = rp->next)
        {
            selector = (char *) (rp->item);

            if (FullTextMatch(selector, line))
            {
                return true;
            }
        }

        return false;
    }

    if ((c = a.line_select.not_match_from_list))
    {
        for (rp = c; rp != NULL; rp = rp->next)
        {
            selector = (char *) (rp->item);

            if (FullTextMatch(selector, line))
            {
                return false;
            }
        }

        return true;
    }

    if ((c = a.line_select.contains_from_list))
    {
        for (rp = c; rp != NULL; rp = rp->next)
        {
            selector = (char *) (rp->item);

            if (BlockTextMatch(selector, line, &s, &e))
            {
                return true;
            }
        }

        return false;
    }

    if ((c = a.line_select.not_contains_from_list))
    {
        for (rp = c; rp != NULL; rp = rp->next)
        {
            selector = (char *) (rp->item);

            if (BlockTextMatch(selector, line, &s, &e))
            {
                return false;
            }
        }

        return true;
    }

    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int DoEditColumn(Rlist **columns, Attributes a, EditContext *edcontext)
{
    Rlist *rp, *found;
    int retval = false;

    if (a.column.column_operation && strcmp(a.column.column_operation, "delete") == 0)
    {
        if ((found = RlistKeyIn(*columns, a.column.column_value)))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " -> Deleting column field sub-value %s in %s", a.column.column_value,
                  edcontext->filename);
            RlistDestroyEntry(columns, found);
            return true;
        }
        else
        {
            return false;
        }
    }

    if (a.column.column_operation && strcmp(a.column.column_operation, "set") == 0)
    {
        if (RlistLen(*columns) == 1)
        {
            if (strcmp((*columns)->item, a.column.column_value) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Field sub-value set as promised\n");
                return false;
            }
        }

        CfOut(OUTPUT_LEVEL_INFORM, "", " -> Setting field sub-value %s in %s", a.column.column_value, edcontext->filename);
        RlistDestroy(*columns);
        *columns = NULL;
        RlistPrependScalarIdemp(columns, a.column.column_value);

        return true;
    }

    if (a.column.column_operation && strcmp(a.column.column_operation, "prepend") == 0)
    {
        if (RlistPrependScalarIdemp(columns, a.column.column_value))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " -> Prepending field sub-value %s in %s", a.column.column_value, edcontext->filename);
            return true;
        }
        else
        {
            return false;
        }
    }

    if (a.column.column_operation && strcmp(a.column.column_operation, "alphanum") == 0)
    {
        if (RlistPrependScalarIdemp(columns, a.column.column_value))
        {
            retval = true;
        }

        rp = AlphaSortRListNames(*columns);
        *columns = rp;
        return retval;
    }

/* default operation is append */

    if (RlistAppendScalarIdemp(columns, a.column.column_value))
    {
        return true;
    }
    else
    {
        return false;
    }

    return false;
}

/********************************************************************/

static int NotAnchored(char *s)
{
    if (*s != '^')
    {
        return true;
    }

    if (*(s + strlen(s) - 1) != '$')
    {
        return true;
    }

    return false;
}

/********************************************************************/

static int MultiLineString(char *s)
{
    return (strchr(s, '\n') != NULL);
}
