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

#include "env_context.h"

#include "policy.h"
#include "promises.h"
#include "files_names.h"
#include "logic_expressions.h"
#include "dbm_api.h"
#include "syntax.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "matching.h"
#include "hashes.h"
#include "attributes.h"
#include "cfstream.h"
#include "fncall.h"
#include "string_lib.h"
#include "logging.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

#include <assert.h>

/*****************************************************************************/

static bool ValidClassName(const char *str);
static int GetORAtom(const char *start, char *buffer);
static int HasBrackets(const char *s, Promise *pp);
static int IsBracketed(const char *s);

static bool EvalContextStackFrameContainsNegated(EvalContext *ctx, const char *context);

/*****************************************************************************/

Item *ABORTBUNDLEHEAP = NULL;

static Item *ABORTHEAP = NULL;

static bool ABORTBUNDLE = false;

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static int EvalClassExpression(EvalContext *ctx, Constraint *cp, Promise *pp)
{
    int result_and = true;
    int result_or = false;
    int result_xor = 0;
    int result = 0, total = 0;
    char buffer[CF_MAXVARSIZE];
    Rlist *rp;
    double prob, cum = 0, fluct;
    FnCall *fp;
    Rval rval;

    if (cp == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! EvalClassExpression internal diagnostic discovered an ill-formed condition");
    }

    if (!IsDefinedClass(ctx, pp->classes, pp->ns))
    {
        return false;
    }

    if (pp->done)
    {
        return false;
    }

    if (IsDefinedClass(ctx, pp->promiser, pp->ns))
    {
        if (PromiseGetConstraintAsInt(ctx, "persistence", pp) == 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> Cancelling cached persistent class %s", pp->promiser);
            DeletePersistentContext(pp->promiser);
        }
        return false;
    }

    switch (cp->rval.type)
    {
    case RVAL_TYPE_FNCALL:

        fp = (FnCall *) cp->rval.item;  /* Special expansion of functions for control, best effort only */
        FnCallResult res = FnCallEvaluate(ctx, fp, pp);

        FnCallDestroy(fp);
        cp->rval = res.rval;
        break;

    case RVAL_TYPE_LIST:
        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            rval = EvaluateFinalRval(ctx, "this", (Rval) {rp->item, rp->type}, true, pp);
            RvalDestroy((Rval) {rp->item, rp->type});
            rp->item = rval.item;
            rp->type = rval.type;
        }
        break;

    default:

        rval = ExpandPrivateRval("this", cp->rval);
        RvalDestroy(cp->rval);
        cp->rval = rval;
        break;
    }

    if (strcmp(cp->lval, "expression") == 0)
    {
        if (cp->rval.type != RVAL_TYPE_SCALAR)
        {
            return false;
        }

        if (IsDefinedClass(ctx, (char *) cp->rval.item, pp->ns))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    if (strcmp(cp->lval, "not") == 0)
    {
        if (cp->rval.type != RVAL_TYPE_SCALAR)
        {
            return false;
        }

        if (IsDefinedClass(ctx, (char *) cp->rval.item, pp->ns))
        {
            return false;
        }
        else
        {
            return true;
        }
    }

// Class selection

    if (strcmp(cp->lval, "select_class") == 0)
    {
        char splay[CF_MAXVARSIZE];
        int i, n;
        double hash;

        total = 0;

        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            total++;
        }

        if (total == 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! No classes to select on RHS");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            return false;
        }

        snprintf(splay, CF_MAXVARSIZE, "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());
        hash = (double) GetHash(splay, CF_HASHTABLESIZE);
        n = (int) (total * hash / (double) CF_HASHTABLESIZE);

        for (rp = (Rlist *) cp->rval.item, i = 0; rp != NULL; rp = rp->next, i++)
        {
            if (i == n)
            {
                NewClass(ctx, rp->item, pp->ns);
                return true;
            }
        }
    }

// Class distributions

    if (strcmp(cp->lval, "dist") == 0)
    {
        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            result = IntFromString(rp->item);

            if (result < 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Non-positive integer in class distribution");
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                return false;
            }

            total += result;
        }

        if (total == 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! An empty distribution was specified on RHS");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            return false;
        }
    }

    fluct = drand48();          /* Get random number 0-1 */
    cum = 0.0;

/* If we get here, anything remaining on the RHS must be a clist */

    if (cp->rval.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! RHS of promise body attribute \"%s\" is not a list\n", cp->lval);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return true;
    }

    for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
    {
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            return false;
        }

        result = IsDefinedClass(ctx, (char *) (rp->item), pp->ns);

        result_and = result_and && result;
        result_or = result_or || result;
        result_xor ^= result;

        if (total > 0)          // dist class
        {
            prob = ((double) IntFromString(rp->item)) / ((double) total);
            cum += prob;

            if ((fluct < cum) || rp->next == NULL)
            {
                snprintf(buffer, CF_MAXVARSIZE - 1, "%s_%s", pp->promiser, (char *) rp->item);
                *(pp->donep) = true;

                if (strcmp(pp->bundletype, "common") == 0)
                {
                    NewClass(ctx, buffer, pp->ns);
                }
                else
                {
                    NewBundleClass(ctx, buffer, pp->bundle, pp->ns);
                }

                CfDebug(" ?? \'Strategy\' distribution class interval -> %s\n", buffer);
                return true;
            }
        }
    }

// Class combinations

    if (strcmp(cp->lval, "or") == 0)
    {
        return result_or;
    }

    if (strcmp(cp->lval, "xor") == 0)
    {
        return (result_xor == 1) ? true : false;
    }

    if (strcmp(cp->lval, "and") == 0)
    {
        return result_and;
    }

    return false;
}

/*******************************************************************/

void KeepClassContextPromise(EvalContext *ctx, Promise *pp)
{
    Attributes a;

    a = GetClassContextAttributes(ctx, pp);

    if (!FullTextMatch("[a-zA-Z0-9_]+", pp->promiser))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Class identifier \"%s\" contains illegal characters - canonifying", pp->promiser);
        snprintf(pp->promiser, strlen(pp->promiser) + 1, "%s", CanonifyName(pp->promiser));
    }

    if (a.context.nconstraints == 0)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "No constraints for class promise %s", pp->promiser);
        return;
    }

    if (a.context.nconstraints > 1)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Irreconcilable constraints in classes for %s", pp->promiser);
        return;
    }

// If this is a common bundle ...

    if (strcmp(pp->bundletype, "common") == 0)
    {
        if (EvalClassExpression(ctx, a.context.expression, pp))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining additional global class %s\n", pp->promiser);

            if (!ValidClassName(pp->promiser))
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                     " !! Attempted to name a class \"%s\", which is an illegal class identifier", pp->promiser);
            }
            else
            {
                if (a.context.persistent > 0)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining explicit persistent class %s (%d mins)\n", pp->promiser,
                          a.context.persistent);
                    NewPersistentContext(pp->promiser, pp->ns, a.context.persistent, CONTEXT_STATE_POLICY_RESET);
                    NewClass(ctx, pp->promiser, pp->ns);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining explicit global class %s\n", pp->promiser);
                    NewClass(ctx, pp->promiser, pp->ns);
                }
            }
        }

        /* These are global and loaded once */
        /* *(pp->donep) = true; */

        return;
    }

// If this is some other kind of bundle (else here??)

    if (strcmp(pp->bundletype, CF_AGENTTYPES[THIS_AGENT_TYPE]) == 0 || FullTextMatch("edit_.*", pp->bundletype))
    {
        if (EvalClassExpression(ctx, a.context.expression, pp))
        {
            if (!ValidClassName(pp->promiser))
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                     " !! Attempted to name a class \"%s\", which is an illegal class identifier", pp->promiser);
            }
            else
            {
                if (a.context.persistent > 0)
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining explicit persistent class %s (%d mins)\n", pp->promiser,
                          a.context.persistent);
                    CfOut(OUTPUT_LEVEL_VERBOSE, "",
                          " ?> Warning: persistent classes are global in scope even in agent bundles\n");
                    NewPersistentContext(pp->promiser, pp->ns, a.context.persistent, CONTEXT_STATE_POLICY_RESET);
                    NewClass(ctx, pp->promiser, pp->ns);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining explicit local bundle class %s\n", pp->promiser);
                    NewBundleClass(ctx, pp->promiser, pp->bundle, pp->ns);
                }
            }
        }

        // Private to bundle, can be reloaded

        *(pp->donep) = false;
        return;
    }
}

/*******************************************************************/

void NewClass(EvalContext *ctx, const char *oclass, const char *ns)
{
    Item *ip;
    char context[CF_MAXVARSIZE];
    char canonclass[CF_MAXVARSIZE];

    strcpy(canonclass, oclass);
    if (Chop(canonclass, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }
    CanonifyNameInPlace(canonclass);
    
    if (ns && strcmp(ns, "default") != 0)
       {
       snprintf(context, CF_MAXVARSIZE, "%s:%s", ns, canonclass);
       }
    else
       {
       strncpy(context, canonclass, CF_MAXVARSIZE);
       }
    
    CfDebug("NewClass(%s)\n", context);

    if (strlen(context) == 0)
    {
        return;
    }

    if (IsRegexItemIn(ctx, ABORTBUNDLEHEAP, context))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Bundle aborted on defined class \"%s\"\n", context);
        ABORTBUNDLE = true;
    }

    if (IsRegexItemIn(ctx, ABORTHEAP, context))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "cf-agent aborted on defined class \"%s\"\n", context);
        exit(1);
    }

    if (EvalContextHeapContainsSoft(ctx, context))
    {
        return;
    }

    EvalContextHeapAddSoft(ctx, context);

    for (ip = ABORTHEAP; ip != NULL; ip = ip->next)
    {
        if (IsDefinedClass(ctx, ip->name, ns))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "cf-agent aborted on defined class \"%s\" defined in bundle %s\n", ip->name, THIS_BUNDLE);
            exit(1);
        }
    }

    if (!ABORTBUNDLE)
    {
        for (ip = ABORTBUNDLEHEAP; ip != NULL; ip = ip->next)
        {
            if (IsDefinedClass(ctx, ip->name, ns))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " -> Setting abort for \"%s\" when setting \"%s\"", ip->name, context);
                ABORTBUNDLE = true;
                break;
            }
        }
    }
}

/*********************************************************************/

void DeleteClass(EvalContext *ctx, const char *oclass, const char *ns)
{
    char context[CF_MAXVARSIZE];
 
    if (strchr(oclass, ':'))
    {
        strncpy(context, oclass, CF_MAXVARSIZE);
    }
    else
    {
        if (ns && strcmp(ns, "default") != 0)
        {
            snprintf(context, CF_MAXVARSIZE, "%s:%s", ns, oclass);
        }
        else
        {
            strncpy(context, oclass, CF_MAXVARSIZE);
        }
    }

    EvalContextHeapRemoveSoft(ctx, context);
    EvalContextStackFrameRemoveSoft(ctx, context);
}

/*******************************************************************/

void HardClass(EvalContext *ctx, const char *oclass)
{
    Item *ip;
    char context[CF_MAXVARSIZE];

    strcpy(context, oclass);
    if (Chop(context, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }
    CanonifyNameInPlace(context);

    CfDebug("HardClass(%s)\n", context);

    if (strlen(context) == 0)
    {
        return;
    }

    if (IsRegexItemIn(ctx, ABORTBUNDLEHEAP, context))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Bundle aborted on defined class \"%s\"\n", context);
        ABORTBUNDLE = true;
    }

    if (IsRegexItemIn(ctx, ABORTHEAP, context))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "cf-agent aborted on defined class \"%s\"\n", context);
        exit(1);
    }

    if (EvalContextHeapContainsHard(ctx, context))
    {
        return;
    }

    EvalContextHeapAddHard(ctx, context);

    for (ip = ABORTHEAP; ip != NULL; ip = ip->next)
    {
        if (IsDefinedClass(ctx, ip->name, NULL))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "cf-agent aborted on defined class \"%s\" defined in bundle %s\n", ip->name, THIS_BUNDLE);
            exit(1);
        }
    }

    if (!ABORTBUNDLE)
    {
        for (ip = ABORTBUNDLEHEAP; ip != NULL; ip = ip->next)
        {
            if (IsDefinedClass(ctx, ip->name, NULL))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " -> Setting abort for \"%s\" when setting \"%s\"", ip->name, context);
                ABORTBUNDLE = true;
                break;
            }
        }
    }
}

void NewBundleClass(EvalContext *ctx, const char *context, const char *bundle, const char *ns)
{
    char copy[CF_BUFSIZE];
    Item *ip;

    if (ns && strcmp(ns, "default") != 0)
    {
        snprintf(copy, CF_MAXVARSIZE, "%s:%s", ns, context);
    }
    else
    {
        strncpy(copy, context, CF_MAXVARSIZE);
    }

    if (Chop(copy, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }

    if (strlen(copy) == 0)
    {
        return;
    }

    CfDebug("NewBundleClass(%s)\n", copy);
    
    if (IsRegexItemIn(ctx, ABORTBUNDLEHEAP, copy))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Bundle %s aborted on defined class \"%s\"\n", bundle, copy);
        ABORTBUNDLE = true;
    }

    if (IsRegexItemIn(ctx, ABORTHEAP, copy))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "cf-agent aborted on defined class \"%s\" defined in bundle %s\n", copy, bundle);
        exit(1);
    }

    if (EvalContextHeapContainsSoft(ctx, copy))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "WARNING - private class \"%s\" in bundle \"%s\" shadows a global class - you should choose a different name to avoid conflicts", copy, bundle);
    }

    if (EvalContextStackFrameContainsSoft(ctx, copy))
    {
        return;
    }

    EvalContextStackFrameAddSoft(ctx, copy);

    for (ip = ABORTHEAP; ip != NULL; ip = ip->next)
    {
        if (IsDefinedClass(ctx, ip->name, ns))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "cf-agent aborted on defined class \"%s\" defined in bundle %s\n", copy, bundle);
            exit(1);
        }
    }

    if (!ABORTBUNDLE)
    {
        for (ip = ABORTBUNDLEHEAP; ip != NULL; ip = ip->next)
        {
            if (IsDefinedClass(ctx, ip->name, ns))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " -> Setting abort for \"%s\" when setting \"%s\"", ip->name, context);
                ABORTBUNDLE = true;
                break;
            }
        }
    }
}

/*********************************************************************/

Rlist *SplitContextExpression(const char *context, Promise *pp)
{
    Rlist *list = NULL;
    char cbuff[CF_MAXVARSIZE];

    if (context == NULL)
    {
        RlistPrependScalar(&list, "any");
    }
    else
    {
        for (const char *sp = context; *sp != '\0'; sp++)
        {
            while (*sp == '|')
            {
                sp++;
            }

            memset(cbuff, 0, CF_MAXVARSIZE);

            sp += GetORAtom(sp, cbuff);

            if (strlen(cbuff) == 0)
            {
                break;
            }

            if (IsBracketed(cbuff))
            {
                // Fully bracketed atom (protected)
                cbuff[strlen(cbuff) - 1] = '\0';
                RlistPrependScalar(&list, cbuff + 1);
            }
            else
            {
                if (HasBrackets(cbuff, pp))
                {
                    Rlist *andlist = RlistFromSplitRegex(cbuff, "[.&]+", 99, false);
                    Rlist *rp, *orlist = NULL;
                    char buff[CF_MAXVARSIZE];
                    char orstring[CF_MAXVARSIZE] = { 0 };
                    char andstring[CF_MAXVARSIZE] = { 0 };

                    // Apply distribution P.(A|B) -> P.A|P.B

                    for (rp = andlist; rp != NULL; rp = rp->next)
                    {
                        if (IsBracketed(rp->item))
                        {
                            // This must be an OR string to be ORed and split into a list
                            *((char *) rp->item + strlen((char *) rp->item) - 1) = '\0';

                            if (strlen(orstring) > 0)
                            {
                                strcat(orstring, "|");
                            }

                            Join(orstring, (char *) (rp->item) + 1, CF_MAXVARSIZE);
                        }
                        else
                        {
                            if (strlen(andstring) > 0)
                            {
                                strcat(andstring, ".");
                            }

                            Join(andstring, rp->item, CF_MAXVARSIZE);
                        }

                        // foreach ORlist, AND with AND string
                    }

                    if (strlen(orstring) > 0)
                    {
                        orlist = RlistFromSplitRegex(orstring, "[|]+", 99, false);

                        for (rp = orlist; rp != NULL; rp = rp->next)
                        {
                            snprintf(buff, CF_MAXVARSIZE, "%s.%s", (char *) rp->item, andstring);
                            RlistPrependScalar(&list, buff);
                        }
                    }
                    else
                    {
                        RlistPrependScalar(&list, andstring);
                    }

                    RlistDestroy(orlist);
                    RlistDestroy(andlist);
                }
                else
                {
                    // Clean atom
                    RlistPrependScalar(&list, cbuff);
                }
            }

            if (*sp == '\0')
            {
                break;
            }
        }
    }

    return list;
}

/*********************************************************************/

static int IsBracketed(const char *s)
 /* return true if the entire string is bracketed, not just if
    if contains brackets */
{
    int i, level = 0, yes = 0;

    if (*s != '(')
    {
        return false;
    }

    if (*(s + strlen(s) - 1) != ')')
    {
        return false;
    }

    if (strstr(s, ")("))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has broken brackets", s);
        return false;
    }

    for (i = 0; i < strlen(s); i++)
    {
        if (s[i] == '(')
        {
            yes++;
            level++;
            if (i > 0 && !strchr(".&|!(", s[i - 1]))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has a missing operator in front of '(' at position %d", s, i);
            }
        }

        if (s[i] == ')')
        {
            yes++;
            level--;
            if (i < strlen(s) - 1 && !strchr(".&|!)", s[i + 1]))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has a missing operator after of ')'", s);
            }
        }
    }

    if (level != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has broken brackets", s);
        return false;           /* premature ) */
    }

    if (yes > 2)
    {
        // e.g. (a|b).c.(d|e)
        return false;
    }

    return true;
}

/*********************************************************************/

static int GetORAtom(const char *start, char *buffer)
{
    const char *sp = start;
    char *spc = buffer;
    int bracklevel = 0, len = 0;

    while ((*sp != '\0') && !((*sp == '|') && (bracklevel == 0)))
    {
        if (*sp == '(')
        {
            CfDebug("+(\n");
            bracklevel++;
        }

        if (*sp == ')')
        {
            CfDebug("-)\n");
            bracklevel--;
        }

        CfDebug("(%c)", *sp);
        *spc++ = *sp++;
        len++;
    }

    *spc = '\0';

    CfDebug("\nGetORATom(%s)->%s\n", start, buffer);
    return len;
}

/*********************************************************************/

static int HasBrackets(const char *s, Promise *pp)
 /* return true if contains brackets */
{
    int i, level = 0, yes = 0;

    for (i = 0; i < strlen(s); i++)
    {
        if (s[i] == '(')
        {
            yes++;
            level++;
            if (i > 0 && !strchr(".&|!(", s[i - 1]))
            {
                 CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has a missing operator in front of '(' at position %d", s, i);
            }
        }

        if (s[i] == ')')
        {
            level--;
            if (i < strlen(s) - 1 && !strchr(".&|!)", s[i + 1]))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has a missing operator after ')'", s);
            }
        }
    }

    if (level != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has unbalanced brackets", s);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return true;
    }

    if (yes > 1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Class expression \"%s\" has multiple brackets", s);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }
    else if (yes)
    {
        return true;
    }

    return false;
}

/**********************************************************************/
/* Utilities */
/**********************************************************************/

/* Return expression with error position highlighted. Result is on the heap. */

static char *HighlightExpressionError(const char *str, int position)
{
    char *errmsg = xmalloc(strlen(str) + 3);
    char *firstpart = xstrndup(str, position);
    char *secondpart = xstrndup(str + position, strlen(str) - position);

    sprintf(errmsg, "%s->%s", firstpart, secondpart);

    free(secondpart);
    free(firstpart);

    return errmsg;
}

/**********************************************************************/
/* Debugging output */

static void IndentL(int level)
{
    int i;

    if (level > 0)
    {
        putc('\n', stderr);
        for (i = 0; i < level; ++i)
        {
            putc(' ', stderr);
        }
    }
}

/**********************************************************************/

static int IncIndent(int level, int inc)
{
    if (level < 0)
    {
        return -level + inc;
    }
    else
    {
        return level + inc;
    }
}

/**********************************************************************/

static void EmitStringExpression(StringExpression *e, int level)
{
    if (!e)
    {
        return;
    }

    switch (e->op)
    {
    case CONCAT:
        IndentL(level);
        fputs("(concat ", stderr);
        EmitStringExpression(e->val.concat.lhs, -IncIndent(level, 8));
        EmitStringExpression(e->val.concat.rhs, IncIndent(level, 8));
        fputs(")", stderr);
        break;
    case LITERAL:
        IndentL(level);
        fprintf(stderr, "\"%s\"", e->val.literal.literal);
        break;
    case VARREF:
        IndentL(level);
        fputs("($ ", stderr);
        EmitStringExpression(e->val.varref.name, -IncIndent(level, 3));
        break;
    default:
        FatalError("Unknown type of string expression: %d\n", e->op);
        break;
    }
}

/**********************************************************************/

static void EmitExpression(Expression *e, int level)
{
    if (!e)
    {
        return;
    }

    switch (e->op)
    {
    case OR:
    case AND:
        IndentL(level);
        fprintf(stderr, "(%s ", e->op == OR ? "|" : "&");
        EmitExpression(e->val.andor.lhs, -IncIndent(level, 3));
        EmitExpression(e->val.andor.rhs, IncIndent(level, 3));
        fputs(")", stderr);
        break;
    case NOT:
        IndentL(level);
        fputs("(- ", stderr);
        EmitExpression(e->val.not.arg, -IncIndent(level, 3));
        fputs(")", stderr);
        break;
    case EVAL:
        IndentL(level);
        fputs("(eval ", stderr);
        EmitStringExpression(e->val.eval.name, -IncIndent(level, 6));
        fputs(")", stderr);
        break;
    default:
        FatalError("Unknown logic expression type: %d\n", e->op);
    }
}

/*****************************************************************************/
/* Syntax-checking and evaluating various expressions */
/*****************************************************************************/

static void EmitParserError(const char *str, int position)
{
    char *errmsg = HighlightExpressionError(str, position);

    yyerror(errmsg);
    free(errmsg);
}

/**********************************************************************/

/* To be used from parser only (uses yyerror) */
void ValidateClassSyntax(const char *str)
{
    ParseResult res = ParseExpression(str, 0, strlen(str));

    if (DEBUG)
    {
        EmitExpression(res.result, 0);
        putc('\n', stderr);
    }

    if (res.result)
    {
        FreeExpression(res.result);
    }

    if (!res.result || res.position != strlen(str))
    {
        EmitParserError(str, res.position);
    }
}

/**********************************************************************/

static bool ValidClassName(const char *str)
{
    ParseResult res = ParseExpression(str, 0, strlen(str));

    if (res.result)
    {
        FreeExpression(res.result);
    }

    return res.result && res.position == strlen(str);
}

/**********************************************************************/

static ExpressionValue EvalTokenAsClass(EvalContext *ctx, const char *classname, void *ns)
{
    char qualified_class[CF_MAXVARSIZE];

    if (strcmp(classname, "any") == 0)
       {
       return true;
       }
    
    if (strchr(classname, ':'))
    {
        if (strncmp(classname, "default:", strlen("default:")) == 0)
        {
            snprintf(qualified_class, CF_MAXVARSIZE, "%s", classname + strlen("default:"));
        }
        else
        {
            snprintf(qualified_class, CF_MAXVARSIZE, "%s", classname);
        }
    }
    else if (ns != NULL && strcmp(ns, "default") != 0)
    {
        snprintf(qualified_class, CF_MAXVARSIZE, "%s:%s", (char *)ns, (char *)classname);
    }
    else
    {
        snprintf(qualified_class, CF_MAXVARSIZE, "%s", classname);
    }

    if (EvalContextHeapContainsNegated(ctx, qualified_class))
    {
        return false;
    }
    if (EvalContextStackFrameContainsNegated(ctx, qualified_class))
    {
        return false;
    }
    if (EvalContextHeapContainsHard(ctx, classname))  // Hard classes are always unqualified
    {
        return true;
    }
    if (EvalContextHeapContainsSoft(ctx, qualified_class))
    {
        return true;
    }
    if (EvalContextStackFrameContainsSoft(ctx, qualified_class))
    {
        return true;
    }
    return false;
}

/**********************************************************************/

static char *EvalVarRef(const char *varname, void *param)
{
/*
 * There should be no unexpanded variables when we evaluate any kind of
 * logic expressions, until parsing of logic expression changes and they are
 * not pre-expanded before evaluation.
 */
    return NULL;
}

/**********************************************************************/

bool IsDefinedClass(EvalContext *ctx, const char *context, const char *ns)
{
    ParseResult res;

    if (!context)
    {
        return true;
    }

    res = ParseExpression(context, 0, strlen(context));

    if (!res.result)
    {
        char *errexpr = HighlightExpressionError(context, res.position);

        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to parse class expression: %s", errexpr);
        free(errexpr);
        return false;
    }
    else
    {
        ExpressionValue r = EvalExpression(ctx, res.result,
                                           &EvalTokenAsClass, &EvalVarRef,
                                           (void *)ns);

        FreeExpression(res.result);

        CfDebug("Evaluate(%s) -> %d\n", context, r);

        /* r is EvalResult which could be ERROR */
        return r == true;
    }
}

/**********************************************************************/

bool IsExcluded(EvalContext *ctx, const char *exception, const char *ns)
{
    return !IsDefinedClass(ctx, exception, ns);
}

/**********************************************************************/

static ExpressionValue EvalTokenFromList(EvalContext *ctx, const char *token, void *param)
{
    return InAlphaList((AlphaList *) param, token);
}

/**********************************************************************/

static bool EvalWithTokenFromList(EvalContext *ctx, const char *expr, AlphaList *token_list)
{
    ParseResult res = ParseExpression(expr, 0, strlen(expr));

    if (!res.result)
    {
        char *errexpr = HighlightExpressionError(expr, res.position);

        CfOut(OUTPUT_LEVEL_ERROR, "", "Syntax error in expression: %s", errexpr);
        free(errexpr);
        return false;           /* FIXME: return error */
    }
    else
    {
        ExpressionValue r = EvalExpression(ctx,
                                           res.result,
                                           &EvalTokenFromList,
                                           &EvalVarRef,
                                           token_list);

        FreeExpression(res.result);

        /* r is EvalResult which could be ERROR */
        return r == true;
    }
}

/**********************************************************************/

/* Process result expression */

bool EvalProcessResult(EvalContext *ctx, const char *process_result, AlphaList *proc_attr)
{
    return EvalWithTokenFromList(ctx, process_result, proc_attr);
}

/**********************************************************************/

/* File result expressions */

bool EvalFileResult(EvalContext *ctx, const char *file_result, AlphaList *leaf_attr)
{
    return EvalWithTokenFromList(ctx, file_result, leaf_attr);
}

/*****************************************************************************/

void NewPersistentContext(char *unqualifiedname, const char *ns, unsigned int ttl_minutes, ContextStatePolicy policy)
{
    CF_DB *dbp;
    CfState state;
    time_t now = time(NULL);
    char name[CF_BUFSIZE];

    if (!OpenDB(&dbp, dbid_state))
    {
        return;
    }

    snprintf(name, CF_BUFSIZE, "%s%c%s", ns, CF_NS, unqualifiedname);
    
    if (ReadDB(dbp, name, &state, sizeof(state)))
    {
        if (state.policy == CONTEXT_STATE_POLICY_PRESERVE)
        {
            if (now < state.expires)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Persisent state %s is already in a preserved state --  %jd minutes to go\n",
                      name, (intmax_t)((state.expires - now) / 60));
                CloseDB(dbp);
                return;
            }
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> New persistent state %s\n", name);
    }

    state.expires = now + ttl_minutes * 60;
    state.policy = policy;

    WriteDB(dbp, name, &state, sizeof(state));
    CloseDB(dbp);
}

/*****************************************************************************/

void DeletePersistentContext(const char *name)
{
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_state))
    {
        return;
    }

    DeleteDB(dbp, name);
    CfDebug("Deleted any persistent state %s\n", name);
    CloseDB(dbp);
}

/*****************************************************************************/

void LoadPersistentContext(EvalContext *ctx)
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    int ksize, vsize;
    char *key;
    void *value;
    time_t now = time(NULL);
    CfState q;

    if (LOOKUP)
    {
        return;
    }

    Banner("Loading persistent classes");

    if (!OpenDB(&dbp, dbid_state))
    {
        return;
    }

/* Acquire a cursor for the database. */

    if (!NewDBCursor(dbp, &dbcp))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unable to scan persistence cache");
        return;
    }

    while (NextDB(dbp, dbcp, &key, &ksize, &value, &vsize))
    {
        memcpy((void *) &q, value, sizeof(CfState));

        CfDebug(" - Found key %s...\n", key);

        if (now > q.expires)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " Persistent class %s expired\n", key);
            DBCursorDeleteEntry(dbcp);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " Persistent class %s for %jd more minutes\n", key, (intmax_t)((q.expires - now) / 60));
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " Adding persistent class %s to heap\n", key);
            if (strchr(key, CF_NS))
               {
               char ns[CF_MAXVARSIZE], name[CF_MAXVARSIZE];
               ns[0] = '\0';
               name[0] = '\0';
               sscanf(key, "%[^:]:%[^\n]", ns, name);
               NewClass(ctx, name, ns);
               }
            else
               {
               NewClass(ctx, key, NULL);
               }
        }
    }

    DeleteDBCursor(dbp, dbcp);
    CloseDB(dbp);

    Banner("Loaded persistent memory");
}

/*****************************************************************************/

void AddEphemeralClasses(EvalContext *ctx, const Rlist *classlist, const char *ns)
{
    for (const Rlist *rp = classlist; rp != NULL; rp = rp->next)
    {
        if (!EvalContextHeapContainsSoft(ctx, rp->item))
        {
            NewClass(ctx, rp->item, ns);
        }
    }
}

/*********************************************************************/

void NewClassesFromString(EvalContext *ctx, const char *classlist)
{
    char *sp, currentitem[CF_MAXVARSIZE], local[CF_MAXVARSIZE];

    if ((classlist == NULL) || strlen(classlist) == 0)
    {
        return;
    }

    memset(local, 0, CF_MAXVARSIZE);
    strncpy(local, classlist, CF_MAXVARSIZE - 1);

    for (sp = local; *sp != '\0'; sp++)
    {
        memset(currentitem, 0, CF_MAXVARSIZE);

        sscanf(sp, "%250[^,]", currentitem);

        sp += strlen(currentitem);

        if (EvalContextHeapContainsHard(ctx, currentitem))
        {
            FatalError("cfengine: You cannot use -D to define a reserved class!");
        }

        NewClass(ctx, currentitem, NULL);
    }
}

/*********************************************************************/

void NegateClassesFromString(EvalContext *ctx, const char *classlist)
{
    char *sp, currentitem[CF_MAXVARSIZE], local[CF_MAXVARSIZE];

    if ((classlist == NULL) || strlen(classlist) == 0)
    {
        return;
    }

    memset(local, 0, CF_MAXVARSIZE);
    strncpy(local, classlist, CF_MAXVARSIZE - 1);

    for (sp = local; *sp != '\0'; sp++)
    {
        memset(currentitem, 0, CF_MAXVARSIZE);

        sscanf(sp, "%250[^,]", currentitem);

        sp += strlen(currentitem);

        if (EvalContextHeapContainsHard(ctx, currentitem))
        {
            FatalError("Cannot negate the reserved class [%s]\n", currentitem);
        }

        EvalContextHeapAddNegated(ctx, currentitem);
    }
}

/*********************************************************************/

bool IsSoftClass(EvalContext *ctx, const char *sp)
{
    return !EvalContextHeapContainsHard(ctx, sp);
}

/***************************************************************************/

bool IsTimeClass(const char *sp)
{

    if (IsStrIn(sp, DAY_TEXT))
    {
        return true;
    }

    if (IsStrIn(sp, MONTH_TEXT))
    {
        return true;
    }

    if (IsStrIn(sp, SHIFT_TEXT))
    {
        return true;
    }

    if (strncmp(sp, "Min", 3) == 0 && isdigit((int)*(sp + 3)))
    {
        return true;
    }

    if (strncmp(sp, "Hr", 2) == 0 && isdigit((int)*(sp + 2)))
    {
        return true;
    }

    if (strncmp(sp, "Yr", 2) == 0 && isdigit((int)*(sp + 2)))
    {
        return true;
    }

    if (strncmp(sp, "Day", 3) == 0 && isdigit((int)*(sp + 3)))
    {
        return true;
    }

    if (strncmp(sp, "GMT", 3) == 0 && *(sp + 3) == '_')
    {
        return true;
    }

    if (strncmp(sp, "Lcycle", strlen("Lcycle")) == 0)
    {
        return true;
    }

    const char *quarters[] = { "Q1", "Q2", "Q3", "Q4", NULL };

    if (IsStrIn(sp, quarters))
    {
        return true;
    }

    return false;
}

/***************************************************************************/

int Abort()
{
    if (ABORTBUNDLE)
    {
        ABORTBUNDLE = false;
        return true;
    }

    return false;
}

/*****************************************************************************/

int VarClassExcluded(EvalContext *ctx, Promise *pp, char **classes)
{
    Constraint *cp = PromiseGetConstraint(ctx, pp, "ifvarclass");

    if (cp == NULL)
    {
        return false;
    }

    *classes = (char *) ConstraintGetRvalValue(ctx, "ifvarclass", pp, RVAL_TYPE_SCALAR);

    if (*classes == NULL)
    {
        return true;
    }

    if (strchr(*classes, '$') || strchr(*classes, '@'))
    {
        CfDebug("Class expression did not evaluate");
        return true;
    }

    if (*classes && IsDefinedClass(ctx, *classes, pp->ns))
    {
        return false;
    }
    else
    {
        return true;
    }
}

/*******************************************************************/

void SaveClassEnvironment(EvalContext *ctx)
{
    char file[CF_BUFSIZE];
    FILE *fp;

    snprintf(file, CF_BUFSIZE, "%s/state/allclasses.txt", CFWORKDIR);
    if ((fp = fopen(file, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Could not open allclasses cache file");
        return;
    }

    Writer *writer = FileWriter(fp);

    {
        SetIterator it = EvalContextHeapIteratorHard(ctx);
        const char *context = NULL;
        while ((context = SetIteratorNext(&it)))
        {
            if (!EvalContextHeapContainsNegated(ctx, context))
            {
                WriterWriteF(writer, "%s\n", context);
            }
        }
    }

    {
        SetIterator it = EvalContextHeapIteratorSoft(ctx);
        const char *context = NULL;
        while ((context = SetIteratorNext(&it)))
        {
            if (!EvalContextHeapContainsNegated(ctx, context))
            {
                WriterWriteF(writer, "%s\n", context);
            }
        }
    }

    {
        SetIterator it = EvalContextStackFrameIteratorSoft(ctx);
        const char *context = NULL;
        while ((context = SetIteratorNext(&it)))
        {
            if (!EvalContextHeapContainsNegated(ctx, context))
            {
                WriterWriteF(writer, "%s\n", context);
            }
        }
    }

    WriterClose(writer);
}

/**********************************************************************/

void DeleteAllClasses(EvalContext *ctx, const Rlist *list)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (CheckParseContext((char *) rp->item, CF_IDRANGE) != SYNTAX_TYPE_MATCH_OK)
        {
            return; // TODO: interesting course of action, but why is the check there in the first place?
        }

        if (EvalContextHeapContainsHard(ctx, (char *) rp->item))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! You cannot cancel a reserved hard class \"%s\" in post-condition classes",
                  RlistScalarValue(rp));
        }

        const char *string = (char *) (rp->item);

        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Cancelling class %s\n", string);

        DeletePersistentContext(string);

        EvalContextHeapRemoveSoft(ctx, CanonifyName(string));

        EvalContextStackFrameAddNegated(ctx, CanonifyName(string));
    }
}

/*****************************************************************************/

void AddAllClasses(EvalContext *ctx, const char *ns, const Rlist *list, bool persist, ContextStatePolicy policy, ContextScope context_scope)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        char *classname = xstrdup(rp->item);

        CanonifyNameInPlace(classname);

        if (EvalContextHeapContainsHard(ctx, classname))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! You cannot use reserved hard class \"%s\" as post-condition class", classname);
            // TODO: ok.. but should we take any action? continue; maybe?
        }

        if (persist > 0)
        {
            if (context_scope != CONTEXT_SCOPE_NAMESPACE)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "Automatically promoting context scope for '%s' to namespace visibility, due to persistence", classname);
            }

            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining persistent promise result class %s\n", classname);
            NewPersistentContext(CanonifyName(rp->item), ns, persist, policy);
            NewClass(ctx, classname, ns);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining promise result class %s\n", classname);

            switch (context_scope)
            {
            case CONTEXT_SCOPE_BUNDLE:
                NewBundleClass(ctx, classname, THIS_BUNDLE, ns);
                break;

            default:
            case CONTEXT_SCOPE_NAMESPACE:
                NewClass(ctx, classname, ns);
                break;
            }
        }
    }
}

/*****************************************************************************/

void ListAlphaList(EvalContext *ctx, Writer *writer, AlphaList al, char sep)
{
    AlphaListIterator i = AlphaListIteratorInit(&al);

    for (const Item *ip = AlphaListIteratorNext(&i); ip != NULL; ip = AlphaListIteratorNext(&i))
    {
        if (!EvalContextHeapContainsNegated(ctx, ip->name))
        {
            WriterWriteF(writer, "%s%c", ip->name, sep);
        }
    }
}

/*****************************************************************************/

void AddAbortClass(const char *name, const char *classes)
{
    if (!IsItemIn(ABORTHEAP, name))
    {
        AppendItem(&ABORTHEAP, name, classes);
    }
}

/*****************************************************************************/

void MarkPromiseHandleDone(EvalContext *ctx, const Promise *pp)
{
    if (pp == NULL)
    {
        return;
    }

    char name[CF_BUFSIZE];
    char *handle = ConstraintGetRvalValue(ctx, "handle", pp, RVAL_TYPE_SCALAR);

    if (handle == NULL)
    {
       return;
    }
    
    snprintf(name, CF_BUFSIZE, "%s:%s", pp->ns, handle);
    StringSetAdd(ctx->dependency_handles, xstrdup(name));
}

/*****************************************************************************/

int MissingDependencies(EvalContext *ctx, const Promise *pp)
{
    if (pp == NULL)
    {
        return false;
    }

    char name[CF_BUFSIZE], *d;
    Rlist *rp, *deps = PromiseGetConstraintAsList(ctx, "depends_on", pp);
    
    for (rp = deps; rp != NULL; rp = rp->next)
    {
        if (strchr(rp->item, ':'))
        {
            d = (char *)rp->item;
        }
        else
        {
            snprintf(name, CF_BUFSIZE, "%s:%s", pp->ns, (char *)rp->item);
            d = name;
        }

        if (!StringSetContains(ctx->dependency_handles, d))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
            CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole next promise (%s), as promise dependency %s has not yet been kept\n", pp->promiser, d);
            CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");

            return true;
        }
    }

    return false;
}

static void StackFrameDestroy(StackFrame *frame)
{
    if (frame)
    {
        StringSetDestroy(frame->contexts);
        StringSetDestroy(frame->contexts_negated);
    }
}

EvalContext *EvalContextNew(void)
{
    EvalContext *ctx = xmalloc(sizeof(EvalContext));

    ctx->heap_soft = StringSetNew();
    ctx->heap_hard = StringSetNew();
    ctx->heap_negated = StringSetNew();

    ctx->stack = SeqNew(10, StackFrameDestroy);

    // TODO: this should probably rather be done when evaluating a new bundle, not just when
    // bundles call other bundles. We should not need a 'base frame' like this.
    EvalContextStackPushFrame(ctx, false);

    ctx->dependency_handles = StringSetNew();

    return ctx;
}

void EvalContextDestroy(EvalContext *ctx)
{
    if (ctx)
    {
        StringSetDestroy(ctx->heap_soft);
        StringSetDestroy(ctx->heap_hard);
        StringSetDestroy(ctx->heap_negated);

        SeqDestroy(ctx->stack);

        StringSetDestroy(ctx->dependency_handles);
    }
}

void EvalContextHeapAddSoft(EvalContext *ctx, const char *context)
{
    StringSetAdd(ctx->heap_soft, xstrdup(context));
}

void EvalContextHeapAddHard(EvalContext *ctx, const char *context)
{
    StringSetAdd(ctx->heap_hard, xstrdup(context));
}

void EvalContextHeapAddNegated(EvalContext *ctx, const char *context)
{
    StringSetAdd(ctx->heap_negated, xstrdup(context));
}

static StackFrame *EvalContextStackFrame(const EvalContext *ctx)
{
    assert(SeqLength(ctx->stack) > 0);
    return SeqAt(ctx->stack, SeqLength(ctx->stack) - 1);
}

void EvalContextStackFrameAddSoft(EvalContext *ctx, const char *context)
{
    StringSetAdd(EvalContextStackFrame(ctx)->contexts, xstrdup(context));
}

void EvalContextStackFrameAddNegated(EvalContext *ctx, const char *context)
{
    StringSetAdd(EvalContextStackFrame(ctx)->contexts_negated, xstrdup(context));
}

bool EvalContextHeapContainsSoft(EvalContext *ctx, const char *context)
{
    return StringSetContains(ctx->heap_soft, context);
}

bool EvalContextHeapContainsHard(EvalContext *ctx, const char *context)
{
    return StringSetContains(ctx->heap_hard, context);
}

bool EvalContextHeapContainsNegated(EvalContext *ctx, const char *context)
{
    return StringSetContains(ctx->heap_negated, context);
}

bool StackFrameContainsSoftRecursive(EvalContext *ctx, const char *context, size_t stack_index)
{
    StackFrame *frame = SeqAt(ctx->stack, stack_index);
    if (StringSetContains(frame->contexts, context))
    {
        return true;
    }
    else if (stack_index > 0 && frame->inherits_previous)
    {
        return StackFrameContainsSoftRecursive(ctx, context, stack_index - 1);
    }
    else
    {
        return false;
    }
}

bool EvalContextStackFrameContainsSoft(EvalContext *ctx, const char *context)
{
    assert(SeqLength(ctx->stack) > 0);

    size_t stack_index = SeqLength(ctx->stack) - 1;
    return StackFrameContainsSoftRecursive(ctx, context, stack_index);
}

static bool EvalContextStackFrameContainsNegated(EvalContext *ctx, const char *context)
{
    return StringSetContains(EvalContextStackFrame(ctx)->contexts_negated, context);
}

bool EvalContextHeapRemoveSoft(EvalContext *ctx, const char *context)
{
    return StringSetRemove(ctx->heap_soft, context);
}

bool EvalContextHeapRemoveHard(EvalContext *ctx, const char *context)
{
    return StringSetRemove(ctx->heap_hard, context);
}

void EvalContextHeapClear(EvalContext *ctx)
{
    StringSetClear(ctx->heap_soft);
    StringSetClear(ctx->heap_hard);
    StringSetClear(ctx->heap_negated);
}

static size_t StringSetMatchCount(StringSet *set, const char *regex)
{
    size_t count = 0;
    StringSetIterator it = StringSetIteratorInit(set);
    const char *context = NULL;
    while ((context = SetIteratorNext(&it)))
    {
        // TODO: used FullTextMatch to avoid regressions, investigate whether StringMatch can be used
        if (FullTextMatch(regex, context))
        {
            count++;
        }
    }
    return count;
}

size_t EvalContextHeapMatchCountSoft(const EvalContext *ctx, const char *context_regex)
{
    return StringSetMatchCount(ctx->heap_soft, context_regex);
}

size_t EvalContextHeapMatchCountHard(const EvalContext *ctx, const char *context_regex)
{
    return StringSetMatchCount(ctx->heap_hard, context_regex);
}

size_t EvalContextStackFrameMatchCountSoft(const EvalContext *ctx, const char *context_regex)
{
    return StringSetMatchCount(EvalContextStackFrame(ctx)->contexts, context_regex);
}

StringSetIterator EvalContextHeapIteratorSoft(const EvalContext *ctx)
{
    return StringSetIteratorInit(ctx->heap_soft);
}

StringSetIterator EvalContextHeapIteratorHard(const EvalContext *ctx)
{
    return StringSetIteratorInit(ctx->heap_hard);
}

StringSetIterator EvalContextHeapIteratorNegated(const EvalContext *ctx)
{
    return StringSetIteratorInit(ctx->heap_negated);
}

static StackFrame *StackFrameNew(bool inherit_previous)
{
    StackFrame *frame = xmalloc(sizeof(StackFrame));

    frame->contexts = StringSetNew();
    frame->contexts_negated = StringSetNew();

    frame->inherits_previous = inherit_previous;

    return frame;
}

void EvalContextStackFrameRemoveSoft(EvalContext *ctx, const char *context)
{
    StringSetRemove(EvalContextStackFrame(ctx)->contexts, context);
}

void EvalContextStackPushFrame(EvalContext *ctx, bool inherits_previous)
{
    StackFrame *frame = StackFrameNew(inherits_previous);
    SeqAppend(ctx->stack, frame);
}

void EvalContextStackPopFrame(EvalContext *ctx)
{
    assert(SeqLength(ctx->stack) > 0);
}

void EvalContextStackFrameClear(EvalContext *ctx)
{
    StackFrame *frame = EvalContextStackFrame(ctx);
    StringSetClear(frame->contexts);
    StringSetClear(frame->contexts_negated);
}

StringSetIterator EvalContextStackFrameIteratorSoft(const EvalContext *ctx)
{
    StackFrame *frame = EvalContextStackFrame(ctx);
    return StringSetIteratorInit(frame->contexts);
}
