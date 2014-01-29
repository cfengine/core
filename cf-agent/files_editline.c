/*
   Copyright (C) CFEngine AS

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

#include <files_editline.h>

#include <actuator.h>
#include <eval_context.h>
#include <promises.h>
#include <files_names.h>
#include <vars.h>
#include <item_lib.h>
#include <sort.h>
#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <matching.h>
#include <match_scope.h>
#include <attributes.h>
#include <locks.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <file_lib.h>
#include <rlist.h>
#include <policy.h>
#include <ornaments.h>
#include <verify_classes.h>


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

static const char *const EDITLINETYPESEQUENCE[] =
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

static PromiseResult KeepEditLinePromise(EvalContext *ctx, const Promise *pp, void *param);
static PromiseResult VerifyLineDeletions(EvalContext *ctx, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyColumnEdits(EvalContext *ctx, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyPatterns(EvalContext *ctx, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyLineInsertions(EvalContext *ctx, const Promise *pp, EditContext *edcontext);
static int InsertMultipleLinesToRegion(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int InsertMultipleLinesAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int DeletePromisedLinesMatching(EvalContext *ctx, Item **start, Item *begin, Item *end, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int InsertLineAtLocation(EvalContext *ctx, char *newline, Item **start, Item *location, Item *prev, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int InsertCompoundLineAtLocation(EvalContext *ctx, char *newline, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int ReplacePatterns(EvalContext *ctx, Item *start, Item *end, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int EditColumns(EvalContext *ctx, Item *file_start, Item *file_end, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int EditLineByColumn(EvalContext *ctx, Rlist **columns, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int DoEditColumn(Rlist **columns, Attributes a, EditContext *edcontext);
static int SanityCheckInsertions(Attributes a);
static int SanityCheckDeletions(Attributes a, const Promise *pp);
static int SelectLine(EvalContext *ctx, const char *line, Attributes a);
static int NotAnchored(char *s);
static int SelectRegion(EvalContext *ctx, Item *start, Item **begin_ptr, Item **end_ptr, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static int MultiLineString(char *s);
static int InsertFileAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location, Item *prev, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);

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

    assert(strcmp(bp->type, "edit_line") == 0);

    snprintf(lockname, CF_BUFSIZE - 1, "masterfilelock-%s", edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, parentp, true);

    if (thislock.lock == NULL)
    {
        return false;
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_EDIT, "filename", edcontext->filename, DATA_TYPE_STRING, "source=promise");

    for (pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (type = 0; EDITLINETYPESEQUENCE[type] != NULL; type++)
        {
            if ((sp = BundleGetPromiseType(bp, EDITLINETYPESEQUENCE[type])) == NULL)
            {
                continue;
            }

            BannerSubPromiseType(ctx, bp->name, sp->name);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                ExpandPromise(ctx, pass, pp, KeepEditLinePromise, edcontext);

                if (Abort(ctx))
                {
                    YieldCurrentLock(thislock);
                    return false;
                }
            }
        }
    }

    YieldCurrentLock(thislock);
    return true;
}

/*****************************************************************************/

Bundle *MakeTemporaryBundleFromTemplate(EvalContext *ctx, Policy *policy, Attributes a, const Promise *pp, PromiseResult *result)
{
    FILE *fp = NULL;
    if ((fp = safe_fopen(a.edit_template, "r" )) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to open template file '%s' to make '%s'", a.edit_template, pp->promiser);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return NULL;
    }

    Bundle *bp = NULL;
    {
        char bundlename[CF_MAXVARSIZE];
        snprintf(bundlename, CF_MAXVARSIZE, "temp_cf_bundle_%s", CanonifyName(a.edit_template));

        bp = PolicyAppendBundle(policy, "default", bundlename, "edit_line", NULL, NULL);
    }
    assert(bp);

    {
        PromiseType *tp = BundleAppendPromiseType(bp, "insert_lines");
        Promise *np = NULL;
        Item *lines = NULL;
        Item *stack = NULL;
        char context[CF_BUFSIZE] = "any";
        int lineno = 0;
        size_t level = 0;
        char buffer[CF_BUFSIZE];

        for (;;)
        {
            if (fgets(buffer, sizeof(buffer), fp) == NULL)
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
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Template file '%s' syntax error, missing close \"%%]\" at line %d", a.edit_template, lineno);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
                    return NULL;
                }

                if (strcmp(op, "BEGIN") == 0)
                {
                    PrependItem(&stack, context, NULL);
                    if (++level > 1)
                    {
                        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Template file '%s' contains nested blocks which are not allowed, near line %d", a.edit_template, lineno);
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
                        return NULL;
                    }

                    continue;
                }

                if (strcmp(op, "END") == 0)
                {
                    level--;
                    if (stack != NULL)
                       {
                       strcpy(context, stack->name);
                       DeleteItem(&stack, stack);
                       }
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
                PromiseAppendConstraint(np, "insert_type", RvalNew("preserve_all_lines", RVAL_TYPE_SCALAR), false);

                DeleteItemList(lines);
                free(promiser);
                lines = NULL;
            }
            else
            {
                if (IsDefinedClass(ctx, context))
                {
                    if (level > 0)
                    {
                        AppendItem(&lines, buffer, context);
                    }
                    else
                    {
                        //install independent promise line
                        if (StripTrailingNewline(buffer, CF_EXPANDSIZE) == -1)
                        {
                            Log(LOG_LEVEL_ERR, "StripTrailingNewline was called on an overlong string");
                        }
                        np = PromiseTypeAppendPromise(tp, buffer, (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, context);
                        PromiseAppendConstraint(np, "insert_type", RvalNew("preserve_all_lines", RVAL_TYPE_SCALAR), false);
                    }
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

static PromiseResult KeepEditLinePromise(EvalContext *ctx, const Promise *pp, void *param)
{
    EditContext *edcontext = param;

    char *sp = NULL;

    if (!IsDefinedClass(ctx, pp->classes))
    {
        if (LEGACY_OUTPUT)
        {
            Log(LOG_LEVEL_VERBOSE, "\n");
            Log(LOG_LEVEL_VERBOSE, "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . ");
            Log(LOG_LEVEL_VERBOSE, "   Skipping whole next edit promise, as context %s is not relevant", pp->classes);
            Log(LOG_LEVEL_VERBOSE, "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . ");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping next edit promise, as context '%s' is not relevant", pp->classes);
        }
        return PROMISE_RESULT_NOOP;
    }

    if (VarClassExcluded(ctx, pp, &sp))
    {
        if (LEGACY_OUTPUT)
        {
            Log(LOG_LEVEL_VERBOSE, "\n");
            Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
            Log(LOG_LEVEL_VERBOSE, "Skipping whole next edit promise (%s), as var-context %s is not relevant",
                  pp->promiser, sp);
            Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping whole next edit promise '%s', as var-context '%s' is not relevant",
                  pp->promiser, sp);
        }
        return PROMISE_RESULT_NOOP;
    }

    PromiseBanner(pp);

    if (strcmp("classes", pp->parent_promise_type->name) == 0)
    {
        return VerifyClassPromise(ctx, pp, NULL);
    }
    else if (strcmp("delete_lines", pp->parent_promise_type->name) == 0)
    {
        return VerifyLineDeletions(ctx, pp, edcontext);
    }
    else if (strcmp("field_edits", pp->parent_promise_type->name) == 0)
    {
        return VerifyColumnEdits(ctx, pp, edcontext);
    }
    else if (strcmp("insert_lines", pp->parent_promise_type->name) == 0)
    {
        return VerifyLineInsertions(ctx, pp, edcontext);
    }
    else if (strcmp("replace_patterns", pp->parent_promise_type->name) == 0)
    {
        return VerifyPatterns(ctx, pp, edcontext);
    }
    else if (strcmp("reports", pp->parent_promise_type->name) == 0)
    {
        return VerifyReportPromise(ctx, pp);
    }

    return PROMISE_RESULT_NOOP;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static PromiseResult VerifyLineDeletions(EvalContext *ctx, const Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start);
    Item *begin_ptr, *end_ptr;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetDeletionAttributes(ctx, pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckDeletions(a, pp))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The promised line deletion '%s' is inconsistent", pp->promiser);
        return PROMISE_RESULT_INTERRUPTED;
    }

/* Are we working in a restricted region? */

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!a.haveregion)
    {
        begin_ptr = CF_UNDEFINED_ITEM;
        end_ptr = CF_UNDEFINED_ITEM;
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext, &result))
    {
        if (a.region.include_end || a.region.include_start)
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a,
                 "The promised line deletion '%s' could not select an edit region in '%s' (this is a good thing, as policy suggests deleting the markers)",
                 pp->promiser, edcontext->filename);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp, a,
                 "The promised line deletion '%s' could not select an edit region in '%s' (but the delimiters were expected in the file)",
                 pp->promiser, edcontext->filename);
        }
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deleteline-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    if (DeletePromisedLinesMatching(ctx, start, begin_ptr, end_ptr, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);

    return result;
}

/***************************************************************************/

static PromiseResult VerifyColumnEdits(EvalContext *ctx, const Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start);
    Item *begin_ptr, *end_ptr;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetColumnAttributes(ctx, pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (a.column.column_separator == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a, "No field_separator in promise to edit by column for '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return PROMISE_RESULT_WARN;
    }

    if (a.column.select_column <= 0)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a, "No select_field in promise to edit '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return PROMISE_RESULT_WARN;
    }

    if (!a.column.column_value)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a, "No field_value is promised to column_edit '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return PROMISE_RESULT_WARN;
    }

/* Are we working in a restricted region? */

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!a.haveregion)
    {
        begin_ptr = *start;
        end_ptr = NULL;         // EndOfList(*start);
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The promised column edit '%s' could not select an edit region in '%s'",
             pp->promiser, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

/* locate and split line */

    snprintf(lockname, CF_BUFSIZE - 1, "column-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    if (EditColumns(ctx, begin_ptr, end_ptr, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);

    return result;
}

/***************************************************************************/

static PromiseResult VerifyPatterns(EvalContext *ctx, const Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start);
    Item *begin_ptr, *end_ptr;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Looking at pattern '%s'", pp->promiser);

/* Are we working in a restricted region? */

    Attributes a = GetReplaceAttributes(ctx, pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!a.replace.replace_value)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The promised pattern replace '%s' had no replacement string",
             pp->promiser);
        return PROMISE_RESULT_INTERRUPTED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!a.haveregion)
    {
        begin_ptr = *start;
        end_ptr = NULL;         //EndOfList(*start);
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised pattern replace '%s' could not select an edit region in '%s'", pp->promiser,
             edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "replace-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

/* Make sure back references are expanded */

    if (ReplacePatterns(ctx, begin_ptr, end_ptr, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    EvalContextVariableClearMatch(ctx);

    YieldCurrentLock(thislock);

    return result;
}

/***************************************************************************/

static int SelectNextItemMatching(EvalContext *ctx, const char *regexp, Item *begin, Item *end, Item **match, Item **prev)
{
    Item *ip_prev = CF_UNDEFINED_ITEM;

    *match = CF_UNDEFINED_ITEM;
    *prev = CF_UNDEFINED_ITEM;

    for (Item *ip = begin; ip != end; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        if (FullTextMatch(ctx, regexp, ip->name))
        {
            *match = ip;
            *prev = ip_prev;
            return true;
        }

        ip_prev = ip;
    }

    return false;
}

/***************************************************************************/

static int SelectLastItemMatching(EvalContext *ctx, const char *regexp, Item *begin, Item *end, Item **match, Item **prev)
{
    Item *ip, *ip_last = NULL, *ip_prev = CF_UNDEFINED_ITEM;

    *match = CF_UNDEFINED_ITEM;
    *prev = CF_UNDEFINED_ITEM;

    for (ip = begin; ip != end; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        if (FullTextMatch(ctx, regexp, ip->name))
        {
            *prev = ip_prev;
            ip_last = ip;
        }

        ip_prev = ip;
    }

    if (ip_last)
    {
        *match = ip_last;
        return true;
    }

    return false;
}

/***************************************************************************/

static int SelectItemMatching(EvalContext *ctx, Item *start, char *regex, Item *begin_ptr, Item *end_ptr, Item **match, Item **prev, char *fl)
{
    Item *ip;
    int ret = false;

    *match = CF_UNDEFINED_ITEM;
    *prev = CF_UNDEFINED_ITEM;

    if (regex == NULL)
    {
        return false;
    }

    if (fl && (strcmp(fl, "first") == 0))
    {
        if (SelectNextItemMatching(ctx, regex, begin_ptr, end_ptr, match, prev))
        {
            ret = true;
        }
    }
    else
    {
        if (SelectLastItemMatching(ctx, regex, begin_ptr, end_ptr, match, prev))
        {
            ret = true;
        }
    }

    if ((*match != CF_UNDEFINED_ITEM) && (*prev == CF_UNDEFINED_ITEM))
    {
        for (ip = start; (ip != NULL) && (ip != *match); ip = ip->next)
        {
            *prev = ip;
        }
    }

    return ret;
}

/***************************************************************************/

static PromiseResult VerifyLineInsertions(EvalContext *ctx, const Promise *pp, EditContext *edcontext)
{
    Item **start = &(edcontext->file_start), *match, *prev;
    Item *begin_ptr, *end_ptr;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetInsertionAttributes(ctx, pp);
    int allow_multi_lines = a.sourcetype && strcmp(a.sourcetype, "preserve_all_lines") == 0;
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckInsertions(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The promised line insertion '%s' breaks its own promises",
             pp->promiser);
        return PROMISE_RESULT_INTERRUPTED;
    }

    /* Are we working in a restricted region? */

    PromiseResult result = PROMISE_RESULT_NOOP;

    if (!a.haveregion)
    {
        begin_ptr = *start;
        end_ptr = NULL;         //EndOfList(*start);
    }
    else if (!SelectRegion(ctx, *start, &begin_ptr, &end_ptr, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised line insertion '%s' could not select an edit region in '%s'", pp->promiser,
             edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (allow_multi_lines)
    {
        // promise to insert duplicates on first pass only
        snprintf(lockname, CF_BUFSIZE - 1, "insertline-%s-%s-%lu", pp->promiser, edcontext->filename, (long unsigned int) pp->org_pp);
    }
    else
    {
        snprintf(lockname, CF_BUFSIZE - 1, "insertline-%s-%s", pp->promiser, edcontext->filename);
    }

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    /* Are we looking for an anchored line inside the region? */

    if (a.location.line_matching == NULL)
    {
        if (InsertMultipleLinesToRegion(ctx, start, begin_ptr, end_ptr, a, pp, edcontext, &result))
        {
            (edcontext->num_edits)++;
        }
    }
    else
    {
        if (!SelectItemMatching(ctx, *start, a.location.line_matching, begin_ptr, end_ptr, &match, &prev, a.location.first_last))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The promised line insertion '%s' could not select a locator matching regex '%s' in '%s'", pp->promiser, a.location.line_matching, edcontext->filename);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            YieldCurrentLock(thislock);
            return result;
        }

        if (InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, match, prev, a, pp, edcontext, &result))
        {
            (edcontext->num_edits)++;
        }
    }

    YieldCurrentLock(thislock);

    return result;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int SelectRegion(EvalContext *ctx, Item *start, Item **begin_ptr, Item **end_ptr, Attributes a,
                        const Promise *pp, EditContext *edcontext, PromiseResult *result)
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
            if (beg == CF_UNDEFINED_ITEM && FullTextMatch(ctx, a.region.select_start, ip->name))
            {
                if (!a.region.include_start)
                {
                    if (ip->next == NULL)
                    {
                        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a,
                             "The promised start pattern '%s' found an empty region at the end of file '%s'",
                             a.region.select_start, edcontext->filename);
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
                        return false;
                    }
                }

                beg = ip;
                continue;
            }
        }

        if (a.region.select_end && beg != CF_UNDEFINED_ITEM)
        {
            if (end == CF_UNDEFINED_ITEM && FullTextMatch(ctx, a.region.select_end, ip->name))
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
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised start pattern '%s' was not found when selecting edit region in '%s'",
             a.region.select_start, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
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

/*****************************************************************************/

static int MatchRegion(EvalContext *ctx, const char *chunk, const Item *begin, const Item *end, bool regex)
/*
  Match a region in between the selection delimiters. It is
  called after SelectRegion. The end delimiter will be visible
  here so we have to check for it. Can handle multi-line chunks
*/
{
    const Item *ip = begin;
    char buf[CF_BUFSIZE];
    int lines = 0;

    for (const char *sp = chunk; sp <= chunk + strlen(chunk); sp++)
    {
        memset(buf, 0, CF_BUFSIZE);
        sscanf(sp, "%[^\n]", buf);
        sp += strlen(buf);

        if (ip == NULL)
        {
            return false;
        }

        if (!regex && strcmp(buf, ip->name) != 0)
        {
            return false;
        }
        if (regex && !FullTextMatch(ctx, buf, ip->name))
        {
            return false;
        }

        lines++;

        // We have to manually exclude the marked terminator

        if (ip == end)
        {
            return false;
        }

        // Now see if there is more

        if (ip->next)
        {
            ip = ip->next;
        }
        else                    // if the region runs out before the end
        {
            if (++sp <= chunk + strlen(chunk))
            {
                return false;
            }

            break;
        }
    }

    return lines;
}

/*****************************************************************************/

static int InsertMultipleLinesToRegion(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Attributes a,
                                       const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    Item *ip, *prev = CF_UNDEFINED_ITEM;
    int allow_multi_lines = a.sourcetype && strcmp(a.sourcetype, "preserve_all_lines") == 0;

    // Insert at the start of the file

    if (*start == NULL)
    {
        return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, *start, prev, a, pp, edcontext, result);
    }

    // Insert at the start of the region

    if (a.location.before_after == EDIT_ORDER_BEFORE)
    {
        for (ip = *start; ip != NULL; ip = ip->next)
        {
            if (ip == begin_ptr)
            {
                return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, ip, prev, a, pp, edcontext, result);
            }

            prev = ip;
        }
    }

    // Insert at the end of the region / else end of the file

    if (a.location.before_after == EDIT_ORDER_AFTER)
    {
        for (ip = *start; ip != NULL; ip = ip->next)
        {
            if (!allow_multi_lines && MatchRegion(ctx, pp->promiser, ip, end_ptr, true))
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Promised chunk '%s' exists within selected region of %s (promise kept)", pp->promiser, edcontext->filename);
                return false;
            }

            if (ip->next != NULL && ip->next == end_ptr)
            {
                return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, ip, prev, a, pp, edcontext, result);
            }

            if (ip->next == NULL)
            {
                return InsertMultipleLinesAtLocation(ctx, start, begin_ptr, end_ptr, ip, prev, a, pp, edcontext, result);
            }

            prev = ip;
        }
    }

    return false;
}

/***************************************************************************/

static int InsertMultipleLinesAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location,
                                         Item *prev, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result)

// Promises to insert a possibly multi-line promiser at the specificed location convergently,
// i.e. no insertion will be made if a neighbouring line matches

{
    int isfileinsert = a.sourcetype && (strcmp(a.sourcetype, "file") == 0 || strcmp(a.sourcetype, "file_preserve_block") == 0);

    if (isfileinsert)
    {
        return InsertFileAtLocation(ctx, start, begin_ptr, end_ptr, location, prev, a, pp, edcontext, result);
    }
    else
    {
        return InsertCompoundLineAtLocation(ctx, pp->promiser, start, begin_ptr, end_ptr, location,
                                            prev, a, pp, edcontext, result);
    }
}

/***************************************************************************/

static int DeletePromisedLinesMatching(EvalContext *ctx, Item **start, Item *begin, Item *end, Attributes a,
                                       const Promise *pp, EditContext *edcontext, PromiseResult *result)
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
            matches = !MatchRegion(ctx, pp->promiser, ip, terminator, true);
        }
        else
        {
            matches = MatchRegion(ctx, pp->promiser, ip, terminator, true);
        }

        if (matches)
        {
            Log(LOG_LEVEL_VERBOSE, "Multi-line region (%d lines) matched text in the file", matches);
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Multi-line region didn't match text in the file");
        }

        if (!SelectLine(ctx, ip->name, a))       // Start search from location
        {
            np = ip->next;
            continue;
        }

        if (matches)
        {
            Log(LOG_LEVEL_VERBOSE, "Delete chunk of %d lines", matches);

            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                     "Need to delete line '%s' from %s - but only a warning was promised", ip->name,
                     edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                np = ip->next;
                noedits = false;
            }
            else
            {
                for (i = 1; i <= matches; i++)
                {
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Deleting the promised line %d '%s' from %s", i, ip->name,
                         edcontext->filename);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
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
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "No need to delete lines from %s, ok", edcontext->filename);
    }

    return retval;
}

/********************************************************************/

static int ReplacePatterns(EvalContext *ctx, Item *file_start, Item *file_end, Attributes a,
                           const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    char line_buff[CF_EXPANDSIZE];
    char after[CF_BUFSIZE];
    int match_len, start_off, end_off, once_only = false, retval = false;
    Item *ip;
    int notfound = true, cutoff = 1, replaced = false;

    if (a.replace.occurrences && (strcmp(a.replace.occurrences, "first") == 0))
    {
        Log(LOG_LEVEL_WARNING, "Setting replace-occurrences policy to 'first' is not convergent");
        once_only = true;
    }

    Buffer *replace = BufferNew();
    for (ip = file_start; ip != NULL && ip != file_end; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        cutoff = 1;
        strlcpy(line_buff, ip->name, sizeof(line_buff));
        replaced = false;
        match_len = 0;

        while (BlockTextMatch(ctx, pp->promiser, line_buff, &start_off, &end_off))
        {
            if (match_len == strlen(line_buff))
            {
                Log(LOG_LEVEL_VERBOSE, "Improper convergent expression matches defacto convergence, so accepting");
                break;
            }

            if (cutoff++ > CF_MAX_REPLACE)
            {
                Log(LOG_LEVEL_VERBOSE, "Too many replacements on this line");
                break;
            }

            match_len = end_off - start_off;
            BufferZero(replace);
            ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, a.replace.replace_value, replace);

            Log(LOG_LEVEL_VERBOSE, "Verifying replacement of '%s' with '%s', cutoff %d", pp->promiser, BufferData(replace),
                  cutoff);

            // Save portion of line after substitution:
            after[0] = '\0'; // Do what strncpy() should, using strncat():
            strncat(after, line_buff + end_off, sizeof(after) - 1);
            // TODO: gripe if that truncated !

            // Substitute into line_buff:
            snprintf(line_buff + start_off, sizeof(line_buff) - start_off,
                     "%s%s", BufferData(replace), after);
            // TODO: gripe if that truncated or failed !
            notfound = false;
            replaced = true;

            if (once_only)
            {
                Log(LOG_LEVEL_VERBOSE, "Replace first occurrence only (warning, this is not a convergent policy)");
                break;
            }
        }

        if (NotAnchored(pp->promiser) && BlockTextMatch(ctx, pp->promiser, line_buff, &start_off, &end_off))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
                 "Promised replacement '%s' on line '%s' for pattern '%s' is not convergent while editing '%s'",
                 line_buff, ip->name, pp->promiser, edcontext->filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
            Log(LOG_LEVEL_ERR, "Because the regular expression '%s' still matches the replacement string '%s'",
                  pp->promiser, line_buff);
            PromiseRef(LOG_LEVEL_ERR, pp);
            break;
        }

        if (a.transaction.action == cfa_warn)
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_WARN, pp, a,
                 "Need to replace line '%s' in '%s' - but only a warning was promised", pp->promiser,
                 edcontext->filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
            continue;
        }
        else if (replaced)
        {
            free(ip->name);
            ip->name = xstrdup(line_buff);
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Replaced pattern '%s' in '%s'", pp->promiser, edcontext->filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            (edcontext->num_edits)++;
            retval = true;

            Log(LOG_LEVEL_VERBOSE, "cutoff %d, '%s'", cutoff, ip->name);
            Log(LOG_LEVEL_VERBOSE, "cutoff %d, '%s'", cutoff, line_buff);

            if (once_only)
            {
                Log(LOG_LEVEL_VERBOSE, "Replace first occurrence only (warning, this is not a convergent policy)");
                break;
            }

            if (BlockTextMatch(ctx, pp->promiser, ip->name, &start_off, &end_off))
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp, a,
                     "Promised replacement '%s' for pattern '%s' is not properly convergent while editing '%s'",
                     ip->name, pp->promiser, edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
                Log(LOG_LEVEL_INFO,
                      "Because the regular expression '%s' still matches the end-state replacement string '%s'",
                      pp->promiser, line_buff);
                PromiseRef(LOG_LEVEL_INFO, pp);
            }
        }
    }

    BufferDestroy(replace);

    if (notfound)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "No pattern '%s' in '%s'", pp->promiser, edcontext->filename);
    }

    return retval;
}

/********************************************************************/

static int EditColumns(EvalContext *ctx, Item *file_start, Item *file_end, Attributes a,
                       const Promise *pp, EditContext *edcontext, PromiseResult *result)
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

        if (!FullTextMatch(ctx, pp->promiser, ip->name))
        {
            continue;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Matched line '%s'", ip->name);
        }

        if (!BlockTextMatch(ctx, a.column.column_separator, ip->name, &s, &e))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Field edit, no fields found by promised pattern '%s' in '%s'",
                 a.column.column_separator, edcontext->filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
            return false;
        }

        if (e - s > CF_MAXVARSIZE / 2)
        {
            Log(LOG_LEVEL_ERR, "Line split criterion matches a huge part of the line, seems to be in error");
            return false;
        }

        strncpy(separator, ip->name + s, e - s);
        separator[e - s] = '\0';

        columns = RlistFromSplitRegex(ip->name, a.column.column_separator, CF_INFINITY, a.column.blanks_ok);
        retval = EditLineByColumn(ctx, &columns, a, pp, edcontext, result);

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
        Log(LOG_LEVEL_ERR,
              "Line insertion selection promise is meaningless - the alternatives are mutually exclusive (only one is allowed)");
        ok = false;
    }

    if (with && not)
    {
        Log(LOG_LEVEL_ERR,
              "Line insertion selection promise is meaningless - cannot mix positive and negative constraints");
        ok = false;
    }

    for (rp = a.insert_match; rp != NULL; rp = rp->next)
    {
        opt = InsertMatchTypeFromString(RlistScalarValue(rp));

        switch (opt)
        {
        case INSERT_MATCH_TYPE_EXACT:
            exact = true;
            break;
        default:
            ignore_something = true;
            if (preserve_block)
            {
                Log(LOG_LEVEL_ERR, "Line insertion should not use whitespace policy with preserve_block");
                ok = false;
            }
            break;
        }
    }

    if (exact && ignore_something)
    {
        Log(LOG_LEVEL_ERR,
              "Line insertion selection promise is meaningless - cannot mix exact_match with other ignore whitespace options");
        ok = false;
    }

    return ok;
}

/***************************************************************************/

static int SanityCheckDeletions(Attributes a, const Promise *pp)
{
    if (MultiLineString(pp->promiser))
    {
        if (a.not_matching)
        {
            Log(LOG_LEVEL_ERR,
                  "Makes no sense to promise multi-line delete with not_matching. Cannot be satisfied for all lines as a block.");
        }
    }

    return true;
}

/***************************************************************************/

static int MatchPolicy(EvalContext *ctx, const char *camel, const char *haystack, Rlist *insert_match, const Promise *pp)
{
    Rlist *rp;
    char *sp, *spto, *firstchar, *lastchar;
    InsertMatchType opt;
    char work[CF_BUFSIZE], final[CF_BUFSIZE];
    Item *list = SplitString(camel, '\n'), *ip;
    int direct_cmp = false, ok = false, escaped = false;

//Split into separate lines first

    for (ip = list; ip != NULL; ip = ip->next)
    {
        ok = false;
        direct_cmp = (strcmp(camel, haystack) == 0);

        if (insert_match == NULL)
        {
            // No whitespace policy means exact_match
            ok = ok || direct_cmp;
            break;
        }

        memset(final, 0, CF_BUFSIZE);
        strncpy(final, ip->name, CF_BUFSIZE - 1);

        for (rp = insert_match; rp != NULL; rp = rp->next)
        {
            opt = InsertMatchTypeFromString(RlistScalarValue(rp));

            /* Exact match can be done immediately */

            if (opt == INSERT_MATCH_TYPE_EXACT)
            {
                if ((rp->next != NULL) || (rp != insert_match))
                {
                    Log(LOG_LEVEL_ERR, "Multiple policies conflict with \"exact_match\", using exact match");
                    PromiseRef(LOG_LEVEL_ERR, pp);
                }

                ok = ok || direct_cmp;
                break;
            }

            if (!escaped)
            {    
            // Need to escape the original string once here in case it contains regex chars when non-exact match
            EscapeRegexChars(ip->name, final, CF_BUFSIZE - 1);
            escaped = true;
            }
            
            if (opt == INSERT_MATCH_TYPE_IGNORE_EMBEDDED)
            {
                memset(work, 0, CF_BUFSIZE);

                // Strip initial and final first

                for (firstchar = final; isspace((int)*firstchar); firstchar++)
                {
                }

                for (lastchar = final + strlen(final) - 1; (lastchar > firstchar) && (isspace((int)*lastchar)); lastchar--)
                {
                }

                for (sp = final, spto = work; *sp != '\0'; sp++)
                {
                    if ((sp > firstchar) && (sp < lastchar))
                    {
                        if (isspace((int)*sp))
                        {
                            while (isspace((int)*(sp + 1)))
                            {
                                sp++;
                            }

                            strcat(spto, "\\s+");
                            spto += 3;
                        }
                        else
                        {
                            *spto++ = *sp;
                        }
                    }
                    else
                    {
                        *spto++ = *sp;
                    }
                }

                strcpy(final, work);
            }

            if (opt == INSERT_MATCH_TYPE_IGNORE_LEADING)
            {
                if (strncmp(final, "\\s*", 3) != 0)
                {
                    for (sp = final; isspace((int)*sp); sp++)
                    {
                    }
                    strcpy(work, sp);
                    snprintf(final, CF_BUFSIZE, "\\s*%s", work);
                }
            }

            if (opt == INSERT_MATCH_TYPE_IGNORE_TRAILING)
            {
                if (strncmp(final + strlen(final) - 4, "\\s*", 3) != 0)
                {
                    strcpy(work, final);
                    snprintf(final, CF_BUFSIZE, "%s\\s*", work);
                }
            }

            ok = ok || (FullTextMatch(ctx, final, haystack));
        }

        if (!ok)                // All lines in region need to match to avoid insertions
        {
            break;
        }
    }

    DeleteItemList(list);
    return ok;
}

static int IsItemInRegion(EvalContext *ctx, const char *item, const Item *begin_ptr, const Item *end_ptr, Rlist *insert_match, const Promise *pp)
{
    for (const Item *ip = begin_ptr; ((ip != end_ptr) && (ip != NULL)); ip = ip->next)
    {
        if (MatchPolicy(ctx, item, ip->name, insert_match, pp))
        {
            return true;
        }
    }

    return false;
}

/***************************************************************************/

static int InsertFileAtLocation(EvalContext *ctx, Item **start, Item *begin_ptr, Item *end_ptr, Item *location,
                                Item *prev, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    FILE *fin;
    char buf[CF_BUFSIZE];
    int retval = false;
    Item *loc = NULL;
    int preserve_block = a.sourcetype && strcmp(a.sourcetype, "file_preserve_block") == 0;

    if ((fin = safe_fopen(pp->promiser, "r")) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Could not read file '%s'. (fopen: %s)", pp->promiser, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    loc = location;
    Buffer *exp = BufferNew();

    for(;;)
    {
        if (fgets(buf, sizeof(buf), fin) == NULL)
        {
            if (ferror(fin)) {
                if (errno == EISDIR)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Could not read file %s: Is a directory", pp->promiser);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
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
            Log(LOG_LEVEL_ERR, "StripTrailingNewline was called on an overlong string");
        }

        if (feof(fin) && strlen(buf) == 0)
        {
            break;
        }

        BufferZero(exp);
        if (a.expandvars)
        {
            ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, buf, exp);
        }
        else
        {
            BufferAppend(exp, buf, strlen(buf));
        }

        if (!SelectLine(ctx, BufferData(exp), a))
        {
            continue;
        }

        if (!preserve_block && IsItemInRegion(ctx, BufferData(exp), begin_ptr, end_ptr, a.insert_match, pp))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                 "Promised file line '%s' exists within file %s (promise kept)", BufferData(exp), edcontext->filename);
            continue;
        }

        // Need to call CompoundLine here in case ExpandScalar has inserted \n into a string

        retval |= InsertCompoundLineAtLocation(ctx, BufferGet(exp), start, begin_ptr, end_ptr, loc, prev, a, pp, edcontext, result);

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
    BufferDestroy(exp);
    return retval;

}

/***************************************************************************/

static int InsertCompoundLineAtLocation(EvalContext *ctx, char *chunk, Item **start, Item *begin_ptr, Item *end_ptr,
                                        Item *location, Item *prev, Attributes a, const Promise *pp, EditContext *edcontext,
                                        PromiseResult *result)
{
    bool retval = false;
    char buf[CF_EXPANDSIZE];
    char *sp;
    int preserve_block = a.sourcetype && (strcmp(a.sourcetype, "preserve_all_lines") == 0 || strcmp(a.sourcetype, "preserve_block") == 0 || strcmp(a.sourcetype, "file_preserve_block") == 0);

    if (!preserve_block && MatchRegion(ctx, chunk, location, NULL, false))
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Promised chunk '%s' exists within selected region of %s (promise kept)", pp->promiser, edcontext->filename);
        return false;
    }

    // Iterate over any lines within the chunk

    for (sp = chunk; sp <= chunk + strlen(chunk); sp++)
    {
        memset(buf, 0, CF_BUFSIZE);
        StringNotMatchingSetCapped(sp, CF_BUFSIZE, "\n", buf);
        sp += strlen(buf);

        if (!SelectLine(ctx, buf, a))
        {
            continue;
        }

        if (!preserve_block && IsItemInRegion(ctx, buf, begin_ptr, end_ptr, a.insert_match, pp))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Promised chunk '%s' exists within selected region of %s (promise kept)", pp->promiser, edcontext->filename);
            continue;
        }

        retval |= InsertLineAtLocation(ctx, buf, start, location, prev, a, pp, edcontext, result);

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

    return retval;
}

static int NeighbourItemMatches(EvalContext *ctx, const Item *file_start, const Item *location, const char *string, EditOrder pos, Rlist *insert_match,
                         const Promise *pp)
{
/* Look for a line matching proposed insert before or after location */

    for (const Item *ip = file_start; ip != NULL; ip = ip->next)
    {
        if (pos == EDIT_ORDER_BEFORE)
        {
            if ((ip->next) && (ip->next == location))
            {
                if (MatchPolicy(ctx, string, ip->name, insert_match, pp))
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }

        if (pos == EDIT_ORDER_AFTER)
        {
            if (ip == location)
            {
                if ((ip->next) && (MatchPolicy(ctx, string, ip->next->name, insert_match, pp)))
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
    }

    return false;
}

static int InsertLineAtLocation(EvalContext *ctx, char *newline, Item **start, Item *location, Item *prev, Attributes a,
                                const Promise *pp, EditContext *edcontext, PromiseResult *result)

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
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                         "Need to insert the promised line '%s' in %s - but only a warning was promised", newline,
                         edcontext->filename);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                    return true;
                }
                else
                {
                    PrependItemList(start, newline);
                    (edcontext->num_edits)++;
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Inserting the promised line '%s' into %s", newline,
                         edcontext->filename);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                    return true;
                }
            }

            if (strcmp((*start)->name, newline) != 0)
            {
                if (a.transaction.action == cfa_warn)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                         "Need to prepend the promised line '%s' to %s - but only a warning was promised",
                         newline, edcontext->filename);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                    return true;
                }
                else
                {
                    PrependItemList(start, newline);
                    (edcontext->num_edits)++;
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Prepending the promised line '%s' to %s", newline,
                         edcontext->filename);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                    return true;
                }
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                     "Promised line '%s' exists at start of file %s (promise kept)", newline, edcontext->filename);
                return false;
            }
        }
    }

    if (a.location.before_after == EDIT_ORDER_BEFORE)
    {
        if (!preserve_block && NeighbourItemMatches(ctx, *start, location, newline, EDIT_ORDER_BEFORE, a.insert_match, pp))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Promised line '%s' exists before locator in (promise kept)",
                 newline);
            return false;
        }
        else
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                     "Need to insert line '%s' into '%s' but only a warning was promised", newline,
                     edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                return true;
            }
            else
            {
                InsertAfter(start, prev, newline);
                (edcontext->num_edits)++;
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Inserting the promised line '%s' into '%s' before locator",
                     newline, edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                return true;
            }
        }
    }
    else
    {
        if (!preserve_block && NeighbourItemMatches(ctx, *start, location, newline, EDIT_ORDER_AFTER, a.insert_match, pp))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Promised line '%s' exists after locator (promise kept)",
                 newline);
            return false;
        }
        else
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                     "Need to insert line '%s' in '%s' but only a warning was promised", newline, edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                return true;
            }
            else
            {
                InsertAfter(start, location, newline);
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Inserting the promised line '%s' into '%s' after locator",
                     newline, edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                (edcontext->num_edits)++;
                return true;
            }
        }
    }
}

/***************************************************************************/

static int EditLineByColumn(EvalContext *ctx, Rlist **columns, Attributes a,
                            const Promise *pp, EditContext *edcontext, PromiseResult *result)
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
            Log(LOG_LEVEL_VERBOSE, "Stopped at field %d", count);
            break;
        }
    }

    if (a.column.select_column > count)
    {
        if (!a.column.extend_columns)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
                 "The file %s has only %d fields, but there is a promise for field %d", edcontext->filename, count,
                 a.column.select_column);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
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
                    Log(LOG_LEVEL_VERBOSE, "Stopped at column/field %d", count);
                    break;
                }
            }
        }
    }

    if (a.column.value_separator != '\0')
    {
        /* internal separator, single char so split again */

        if (strstr(RlistScalarValue(rp), a.column.column_value) || strcmp(RlistScalarValue(rp), a.column.column_value) != 0) 
        {
            this_column = RlistFromSplitString(RlistScalarValue(rp), a.column.value_separator);
            retval = DoEditColumn(&this_column, a, edcontext);
        }
        else
        {
            retval = false;
        }

        if (retval)
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a, "Need to edit field in %s but only warning promised",
                     edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                retval = false;
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Edited field inside file object %s", edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                (edcontext->num_edits)++;
                free(RlistScalarValue(rp));
                sep[0] = a.column.value_separator;
                sep[1] = '\0';
                rp->val.item = Rlist2String(this_column, sep);
            }
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "No need to edit field in %s", edcontext->filename);
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
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                     "Need to delete field field value %s in %s but only a warning was promised", RlistScalarValue(rp),
                     edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                return false;
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Deleting column field value %s in %s", RlistScalarValue(rp),
                     edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                (edcontext->num_edits)++;
                free(rp->val.item);
                rp->val.item = xstrdup("");
                return true;
            }
        }
        else
        {
            if (a.transaction.action == cfa_warn)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                     "Need to set column field value %s to %s in %s but only a warning was promised",
                     RlistScalarValue(rp), a.column.column_value, edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                return false;
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Setting whole column field value %s to %s in %s",
                     RlistScalarValue(rp), a.column.column_value, edcontext->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                free(rp->val.item);
                rp->val.item = xstrdup(a.column.column_value);
                (edcontext->num_edits)++;
                return true;
            }
        }
    }

    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "No need to edit column field value %s in %s", a.column.column_value,
         edcontext->filename);

    return false;
}

/***************************************************************************/

static int SelectLine(EvalContext *ctx, const char *line, Attributes a)
{
    Rlist *rp, *c;
    int s, e;
    char *selector;

    if ((c = a.line_select.startwith_from_list))
    {
        for (rp = c; rp != NULL; rp = rp->next)
        {
            selector = RlistScalarValue(rp);

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
            selector = RlistScalarValue(rp);

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
            selector = RlistScalarValue(rp);

            if (FullTextMatch(ctx, selector, line))
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
            selector = RlistScalarValue(rp);

            if (FullTextMatch(ctx, selector, line))
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
            selector = RlistScalarValue(rp);

            if (BlockTextMatch(ctx, selector, line, &s, &e))
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
            selector = RlistScalarValue(rp);

            if (BlockTextMatch(ctx, selector, line, &s, &e))
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

    if (a.column.column_operation && (strcmp(a.column.column_operation, "delete") == 0))
    {
        while ((found = RlistKeyIn(*columns, a.column.column_value)))
        {
            Log(LOG_LEVEL_INFO, "Deleting column field sub-value '%s' in '%s'", a.column.column_value,
                  edcontext->filename);
            RlistDestroyEntry(columns, found);
            retval = true;
        }

        return retval;
    }

    if (a.column.column_operation && strcmp(a.column.column_operation, "set") == 0)
    {
        if (RlistLen(*columns) == 1)
        {
            if (strcmp(RlistScalarValue(*columns), a.column.column_value) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Field sub-value set as promised");
                return false;
            }
        }

        Log(LOG_LEVEL_INFO, "Setting field sub-value '%s' in '%s'", a.column.column_value, edcontext->filename);
        RlistDestroy(*columns);
        *columns = NULL;
        RlistPrependScalarIdemp(columns, a.column.column_value);

        return true;
    }

    if (a.column.column_operation && strcmp(a.column.column_operation, "prepend") == 0)
    {
        if (RlistPrependScalarIdemp(columns, a.column.column_value))
        {
            Log(LOG_LEVEL_INFO, "Prepending field sub-value '%s' in '%s'", a.column.column_value, edcontext->filename);
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
