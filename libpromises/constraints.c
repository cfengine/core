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

#include "constraints.h"

#include "env_context.h"
#include "promises.h"
#include "syntax.h"
#include "item_lib.h"
#include "files_names.h"
#include "conversion.h"
#include "reporting.h"
#include "attributes.h"
#include "cfstream.h"
#include "string_lib.h"
#include "transaction.h"
#include "logging.h"
#include "misc_lib.h"

static PromiseIdent *PromiseIdExists(char *namespace, char *handle);
static void DeleteAllPromiseIdsRecurse(PromiseIdent *key);
static int VerifyConstraintName(const char *lval);

/*******************************************************************/

// FIX: copied nearly verbatim from AppendConstraint, needs review


Seq *ConstraintGetFromBody(Body *body, const char *lval)
{
    Seq *matches = SeqNew(5, NULL);

    for (size_t i = 0; i < SeqLength(body->conlist); i++)
    {
        Constraint *cp = SeqAt(body->conlist, i);
        if (strcmp(cp->lval, lval) == 0)
        {
            SeqAppend(matches, cp);
        }
    }

    return matches;
}

const char *ConstraintContext(const Constraint *cp)
{
    switch (cp->type)
    {
    case POLICY_ELEMENT_TYPE_BODY:
        return cp->classes;

    case POLICY_ELEMENT_TYPE_BUNDLE:
        return cp->parent.promise->classes;

    default:
        ProgrammingError("Constraint had parent element type: %d", cp->type);
        return NULL;
    }
}

Constraint *EffectiveConstraint(Seq *constraints)
{
    for (size_t i = 0; i < SeqLength(constraints); i++)
    {
        Constraint *cp = SeqAt(constraints, i);

        const char *context = ConstraintContext(cp);
        const char *ns = NamespaceFromConstraint(cp);

        if (IsDefinedClass(context, ns))
        {
            return cp;
        }
    }

    return NULL;
}

/*****************************************************************************/

void EditScalarConstraint(Seq *conlist, const char *lval, const char *rval)
{
    for (size_t i = 0; i < SeqLength(conlist); i++)
    {
        Constraint *cp = SeqAt(conlist, i);

        if (strcmp(lval, cp->lval) == 0)
        {
            DeleteRvalItem(cp->rval);
            cp->rval = (Rval) { xstrdup(rval), RVAL_TYPE_SCALAR };
            return;
        }
    }
}

void ConstraintDestroy(Constraint *cp)
{
    if (cp)
    {
        DeleteRvalItem(cp->rval);
        free(cp->lval);
        free(cp->classes);

        free(cp);
    }
}

/*****************************************************************************/

int GetBooleanConstraint(const char *lval, const Promise *pp)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" (boolean) constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(cf_error, "", " !! Type mismatch on rhs - expected type (%c) for boolean constraint \"%s\"\n",
                      cp->rval.type, lval);
                PromiseRef(cf_error, pp);
                FatalError("Aborted");
            }

            if (strcmp(cp->rval.item, "true") == 0 || strcmp(cp->rval.item, "yes") == 0)
            {
                retval = true;
                continue;
            }

            if (strcmp(cp->rval.item, "false") == 0 || strcmp(cp->rval.item, "no") == 0)
            {
                retval = false;
            }
        }
    }

    if (retval == CF_UNDEFINED)
    {
        retval = false;
    }

    return retval;
}

/*****************************************************************************/

int GetRawBooleanConstraint(const char *lval, const Seq *constraints)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(constraints); i++)
    {
        Constraint *cp = SeqAt(constraints, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, NULL))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" (boolean) body constraints break this promise\n", lval);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(cf_error, "", " !! Type mismatch - expected type (%c) for boolean constraint \"%s\"\n",
                      cp->rval.type, lval);
                FatalError("Aborted");
            }

            if (strcmp(cp->rval.item, "true") == 0 || strcmp(cp->rval.item, "yes") == 0)
            {
                retval = true;
                continue;
            }

            if (strcmp(cp->rval.item, "false") == 0 || strcmp(cp->rval.item, "no") == 0)
            {
                retval = false;
            }
        }
    }

    if (retval == CF_UNDEFINED)
    {
        retval = false;
    }

    return retval;
}

/*****************************************************************************/

int GetBundleConstraint(const char *lval, const Promise *pp)
{
    int retval = CF_UNDEFINED;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        const Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }
            }
            else
            {
                continue;
            }

            if (!(cp->rval.type == RVAL_TYPE_FNCALL || cp->rval.type == RVAL_TYPE_SCALAR))
            {
                CfOut(cf_error, "",
                      "Anomalous type mismatch - type (%c) for bundle constraint %s did not match internals\n",
                      cp->rval.type, lval);
                PromiseRef(cf_error, pp);
                FatalError("Aborted");
            }

            return true;
        }
    }

    return false;
}

/*****************************************************************************/

int GetIntConstraint(const char *lval, const Promise *pp)
{
    int retval = CF_NOINT;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != CF_NOINT)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" (int) constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(cf_error, "",
                      "Anomalous type mismatch - expected type for int constraint %s did not match internals\n", lval);
                PromiseRef(cf_error, pp);
                FatalError("Aborted");
            }

            retval = (int) Str2Int((char *) cp->rval.item);
        }
    }

    return retval;
}

/*****************************************************************************/

double GetRealConstraint(const char *lval, const Promise *pp)
{
    double retval = CF_NODOUBLE;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != CF_NODOUBLE)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" (real) constraints break this promise\n", lval);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(cf_error, "",
                      "Anomalous type mismatch - expected type for int constraint %s did not match internals\n", lval);
                FatalError("Aborted");
            }

            retval = Str2Double((char *) cp->rval.item);
        }
    }

    return retval;
}

/*****************************************************************************/

static mode_t Str2Mode(const char *s)
{
    int a = CF_UNDEFINED;
    char output[CF_BUFSIZE];

    if (s == NULL)
    {
        return 0;
    }

    sscanf(s, "%o", &a);

    if (a == CF_UNDEFINED)
    {
        snprintf(output, CF_BUFSIZE, "Error reading assumed octal value %s\n", s);
        ReportError(output);
    }

    return (mode_t) a;
}

mode_t GetOctalConstraint(const char *lval, const Promise *pp)
{
    mode_t retval = 077;

// We could handle units here, like kb,b,mb

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != 077)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" (int,octal) constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(cf_error, "",
                      "Anomalous type mismatch - expected type for int constraint %s did not match internals\n", lval);
                PromiseRef(cf_error, pp);
                FatalError("Aborted");
            }

            retval = Str2Mode((char *) cp->rval.item);
        }
    }

    return retval;
}

/*****************************************************************************/

#ifdef __MINGW32__

uid_t GetUidConstraint(const char *lval, const Promise *pp)
{                               // we use sids on windows instead
    return CF_SAME_OWNER;
}

#else /* !__MINGW32__ */

uid_t GetUidConstraint(const char *lval, const Promise *pp)
{
    int retval = CF_SAME_OWNER;
    char buffer[CF_MAXVARSIZE];

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" (owner/uid) constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(cf_error, "",
                      "Anomalous type mismatch - expected type for owner constraint %s did not match internals\n",
                      lval);
                PromiseRef(cf_error, pp);
                FatalError("Aborted");
            }

            retval = Str2Uid((char *) cp->rval.item, buffer, pp);
        }
    }

    return retval;
}

#endif /* !__MINGW32__ */

/*****************************************************************************/

#ifdef __MINGW32__

gid_t GetGidConstraint(char *lval, const Promise *pp)
{                               // not applicable on windows: processes have no group
    return CF_SAME_GROUP;
}

#else /* !__MINGW32__ */

gid_t GetGidConstraint(char *lval, const Promise *pp)
{
    int retval = CF_SAME_GROUP;
    char buffer[CF_MAXVARSIZE];

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != CF_UNDEFINED)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\"  (group/gid) constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_SCALAR)
            {
                CfOut(cf_error, "",
                      "Anomalous type mismatch - expected type for group constraint %s did not match internals\n",
                      lval);
                PromiseRef(cf_error, pp);
                FatalError("Aborted");
            }

            retval = Str2Gid((char *) cp->rval.item, buffer, pp);
        }
    }

    return retval;
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

Rlist *GetListConstraint(const char *lval, const Promise *pp)
{
    Rlist *retval = NULL;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != NULL)
                {
                    CfOut(cf_error, "", " !! Multiple \"%s\" int constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }
            }
            else
            {
                continue;
            }

            if (cp->rval.type != RVAL_TYPE_LIST)
            {
                CfOut(cf_error, "", " !! Type mismatch on rhs - expected type for list constraint \"%s\" \n", lval);
                PromiseRef(cf_error, pp);
                FatalError("Aborted");
            }

            retval = (Rlist *) cp->rval.item;
            break;
        }
    }

    return retval;
}

/*****************************************************************************/

Constraint *GetConstraint(const Promise *pp, const char *lval)
{
    Constraint *retval = NULL;

    if (pp == NULL)
    {
        return NULL;
    }

    if (!VerifyConstraintName(lval))
    {
        CfOut(cf_error, "", " !! Self-diagnostic: Constraint type \"%s\" is not a registered type\n", lval);
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, lval) == 0)
        {
            if (IsDefinedClass(cp->classes, pp->ns))
            {
                if (retval != NULL)
                {
                    CfOut(cf_error, "", " !! Inconsistent \"%s\" constraints break this promise\n", lval);
                    PromiseRef(cf_error, pp);
                }

                retval = cp;
                break;
            }
        }
    }

    return retval;
}

/*****************************************************************************/

void *GetConstraintValue(const char *lval, const Promise *pp, RvalType rtype)
{
    const Constraint *constraint = GetConstraint(pp, lval);

    if (constraint && constraint->rval.type == rtype)
    {
        return constraint->rval.item;
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************/

void ReCheckAllConstraints(Promise *pp)
{
    char *sp, *handle = GetConstraintValue("handle", pp, RVAL_TYPE_SCALAR);
    PromiseIdent *prid;
    Item *ptr;
    int in_class_any = false;

    if (strcmp(pp->parent_subtype->name, "reports") == 0 && strcmp(pp->classes, "any") == 0)
    {
        char *cl = GetConstraintValue("ifvarclass", pp, RVAL_TYPE_SCALAR);

        if (cl == NULL || strcmp(cl, "any") == 0)
        {
            in_class_any = true;
        }

        if (in_class_any)
        {
        Attributes a = GetReportsAttributes(pp);
        cfPS(cf_error, CF_INTERPT, "", pp, a, "reports promises may not be in class \'any\' - risk of a notification explosion");
        PromiseRef(cf_error, pp);
        ERRORCOUNT++;
        }
    }

/* Special promise type checks */

    if (SHOWREPORTS)
    {
        NewPromiser(pp);
    }

    if (!IsDefinedClass(pp->classes, pp->ns))
    {
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        return;
    }

    if (handle)
    {
        if (!ThreadLock(cft_policy))
        {
            CfOut(cf_error, "", "!! Could not lock cft_policy in ReCheckAllConstraints() -- aborting");
            return;
        }

        if ((prid = PromiseIdExists(pp->ns, handle)))
        {
            if ((strcmp(prid->filename, pp->audit->filename) != 0) || (prid->line_number != pp->offset.line))
            {
                CfOut(cf_error, "", " !! Duplicate promise handle '%s' -- previously used in file %s near line %d",
                      handle, prid->filename, prid->line_number);
                PromiseRef(cf_error, pp);
            }
        }
        else
        {
            NewPromiseId(handle, pp);
        }

        prid = NULL;            // we can't access this after unlocking
        ThreadUnlock(cft_policy);
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);
        PostCheckConstraint(pp->parent_subtype->name, pp->bundle, cp->lval, cp->rval);
    }

    if (strcmp(pp->parent_subtype->name, "insert_lines") == 0)
    {
        /* Multiple additions with same criterion will not be convergent -- but ignore for empty file baseline */

        if ((sp = GetConstraintValue("select_line_matching", pp, RVAL_TYPE_SCALAR)))
        {
            if ((ptr = ReturnItemIn(EDIT_ANCHORS, sp)))
            {
                if (strcmp(ptr->classes, pp->bundle) == 0)
                {
                    CfOut(cf_inform, "",
                          " !! insert_lines promise uses the same select_line_matching anchor (\"%s\") as another promise. This will lead to non-convergent behaviour unless \"empty_file_before_editing\" is set.",
                          sp);
                    PromiseRef(cf_inform, pp);
                }
            }
            else
            {
                PrependItem(&EDIT_ANCHORS, sp, pp->bundle);
            }
        }
    }

    PreSanitizePromise(pp);
}

/*****************************************************************************/

void PostCheckConstraint(const char *type, const char *bundle, const char *lval, Rval rval)
{
    SubTypeSyntax ss;
    int i, j, l, m;
    const BodySyntax *bs, *bs2;
    const SubTypeSyntax *ssp;

    CfDebug("  Post Check Constraint %s: %s =>", type, lval);

    if (DEBUG)
    {
        ShowRval(stdout, rval);
        printf("\n");
    }

// Check class

    for (i = 0; CF_CLASSBODY[i].lval != NULL; i++)
    {
        if (strcmp(lval, CF_CLASSBODY[i].lval) == 0)
        {
            CheckConstraintTypeMatch(lval, rval, CF_CLASSBODY[i].dtype, CF_CLASSBODY[i].range, 0);
        }
    }

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ssp = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ssp[j].bundle_type != NULL; j++)
        {
            ss = ssp[j];

            if (ss.subtype != NULL)
            {
                if (strcmp(ss.subtype, type) == 0)
                {
                    bs = ss.bs;

                    for (l = 0; bs[l].lval != NULL; l++)
                    {
                        if (bs[l].dtype == cf_bundle)
                        {
                        }
                        else if (bs[l].dtype == cf_body)
                        {
                            bs2 = (BodySyntax *) bs[l].range;

                            for (m = 0; bs2[m].lval != NULL; m++)
                            {
                                if (strcmp(lval, bs2[m].lval) == 0)
                                {
                                    CheckConstraintTypeMatch(lval, rval, bs2[m].dtype, (char *) (bs2[m].range), 0);
                                    return;
                                }
                            }
                        }

                        if (strcmp(lval, bs[l].lval) == 0)
                        {
                            CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *) (bs[l].range), 0);
                            return;
                        }
                    }
                }
            }
        }
    }

/* Now check the functional modules - extra level of indirection */

    for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        if (CF_COMMON_BODIES[i].dtype == cf_body)
        {
            continue;
        }

        if (strcmp(lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common constraint attributes\n", lval);
            CheckConstraintTypeMatch(lval, rval, CF_COMMON_BODIES[i].dtype, (char *) (CF_COMMON_BODIES[i].range), 0);
            return;
        }
    }
}

/*****************************************************************************/

static int VerifyConstraintName(const char *lval)
{
    SubTypeSyntax ss;
    int i, j, l, m;
    const BodySyntax *bs, *bs2;
    const SubTypeSyntax *ssp;

    CfDebug("  Verify Constrant name %s\n", lval);

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ssp = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ssp[j].bundle_type != NULL; j++)
        {
            ss = ssp[j];

            if (ss.subtype != NULL)
            {
                bs = ss.bs;

                for (l = 0; bs[l].lval != NULL; l++)
                {
                    if (bs[l].dtype == cf_bundle)
                    {
                    }
                    else if (bs[l].dtype == cf_body)
                    {
                        bs2 = (BodySyntax *) bs[l].range;

                        for (m = 0; bs2[m].lval != NULL; m++)
                        {
                            if (strcmp(lval, bs2[m].lval) == 0)
                            {
                                return true;
                            }
                        }
                    }

                    if (strcmp(lval, bs[l].lval) == 0)
                    {
                        return true;
                    }
                }
            }
        }
    }

/* Now check the functional modules - extra level of indirection */

    for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        if (strcmp(lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            return true;
        }
    }

    return false;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

// NOTE: PROMISE_ID_LIST must be thread-safe here (locked by caller)

PromiseIdent *NewPromiseId(char *handle, Promise *pp)
{
    PromiseIdent *ptr;
    char name[CF_BUFSIZE];
    ptr = xmalloc(sizeof(PromiseIdent));

    snprintf(name, CF_BUFSIZE, "%s%c%s", pp->ns, CF_NS, handle);
    ptr->filename = xstrdup(pp->audit->filename);
    ptr->line_number = pp->offset.line;
    ptr->handle = xstrdup(name);
    ptr->next = PROMISE_ID_LIST;
    PROMISE_ID_LIST = ptr;
    return ptr;
}

/*****************************************************************************/

static void DeleteAllPromiseIdsRecurse(PromiseIdent *key)
{
    if (key->next != NULL)
    {
        DeleteAllPromiseIdsRecurse(key->next);
    }

    free(key->filename);
    free(key->handle);
    free(key);
}

/*****************************************************************************/

void DeleteAllPromiseIds(void)
{
    if (!ThreadLock(cft_policy))
    {
        CfOut(cf_error, "", "!! Could not lock cft_policy in DelteAllPromiseIds() -- aborting");
        return;
    }

    if (PROMISE_ID_LIST)
    {
        DeleteAllPromiseIdsRecurse(PROMISE_ID_LIST);
        PROMISE_ID_LIST = NULL;
    }

    ThreadUnlock(cft_policy);
}

/*****************************************************************************/

static PromiseIdent *PromiseIdExists(char *namespace, char *handle)
{
    PromiseIdent *key;
    char name[CF_BUFSIZE];

    snprintf(name, CF_BUFSIZE, "%s%c%s", namespace, CF_NS, handle);
    
    for (key = PROMISE_ID_LIST; key != NULL; key = key->next)
    {
        if (strcmp(name, key->handle) == 0)
        {
            return key;
        }
    }

    return NULL;
}
