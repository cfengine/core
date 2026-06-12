/*
  Copyright 2024 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <verify_classes.h>

#include <attributes.h>
#include <matching.h>
#include <files_names.h>
#include <fncall.h>
#include <rlist.h>
#include <expand.h>
#include <promises.h>
#include <conversion.h>
#include <logic_expressions.h>
#include <string_lib.h>                                  /* StringHash */
#include <regex.h>                                       /* StringMatchFull */


static bool EvalClassExpression(EvalContext *ctx, Constraint *cp, const Promise *pp);
static bool EvalClassConstraintRval(EvalContext *ctx, Constraint *cp, const Promise *pp);
static PromiseResult VerifyClassCancel(EvalContext *ctx, const Promise *pp, Attributes *a);

static bool ValidClassName(const char *str)
{
    ParseResult res = ParseExpression(str, 0, strlen(str));

    if (res.result)
    {
        FreeExpression(res.result);
    }

    assert(res.position >= 0);
    return res.result && (size_t) res.position == strlen(str);
}

PromiseResult VerifyClassPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(pp != NULL);
    assert(param == NULL);

    Log(LOG_LEVEL_DEBUG, "Evaluating classes promise: %s", pp->promiser);

    Attributes a = GetClassContextAttributes(ctx, pp);

    if (!StringMatchFull("[a-zA-Z0-9_]+", pp->promiser))
    {
        Log(LOG_LEVEL_VERBOSE, "Class identifier '%s' contains illegal characters - canonifying", pp->promiser);
        CanonifyNameInPlace(pp->promiser);
    }

    if (a.context.nconstraints > 1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, &a, "Irreconcilable constraints in classes for '%s'", pp->promiser);
        return PROMISE_RESULT_FAIL;
    }

    /* The 'cancel' attribute undefines the promiser class when its expression
     * evaluates true. It must be handled before the class-defining path
     * because, by design, it operates on a class that is already defined and
     * would otherwise be skipped (see EvalClassExpression()). */
    if (a.context.expression != NULL &&
        strcmp(a.context.expression->lval, "cancel") == 0)
    {
        return VerifyClassCancel(ctx, pp, &a);
    }

    if (a.context.expression == NULL ||
        EvalClassExpression(ctx, a.context.expression, pp))
    {
        if (a.context.expression == NULL)
        {
            Log(LOG_LEVEL_DEBUG, "Setting class '%s' without an expression, implying 'any'", pp->promiser);
        }

        if (!ValidClassName(pp->promiser))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, &a,
                 "Attempted to name a class '%s', which is an illegal class identifier", pp->promiser);
            return PROMISE_RESULT_FAIL;
        }
        else
        {
            StringSet *tags = StringSetNew();
            StringSetAdd(tags, xstrdup("source=promise"));

            for (const Rlist *rp = PromiseGetConstraintAsList(ctx, "meta", pp); rp; rp = rp->next)
            {
                StringSetAdd(tags, xstrdup(RlistScalarValue(rp)));
            }

            const char *comment = PromiseGetConstraintAsRval(pp, "comment", RVAL_TYPE_SCALAR);

            bool inserted = false;
            if (/* Persistent classes are always global: */
                a.context.persistent > 0 ||
                /* Namespace-scope is global: */
                a.context.scope == CONTEXT_SCOPE_NAMESPACE ||
                /* If there is no explicit scope, common bundles define global
                 * classes, other bundles define local classes: */
                (a.context.scope == CONTEXT_SCOPE_NONE &&
                 strcmp(PromiseGetBundle(pp)->type, "common") == 0))
            {
                Log(LOG_LEVEL_VERBOSE, "C:     +  Global class: %s",
                    pp->promiser);
                inserted = EvalContextClassPutSoftTagsSetWithComment(ctx, pp->promiser,
                                                                     CONTEXT_SCOPE_NAMESPACE,
                                                                     tags, comment);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "C:     +  Private class: %s",
                    pp->promiser);
                inserted = EvalContextClassPutSoftTagsSetWithComment(ctx, pp->promiser,
                                                                     CONTEXT_SCOPE_BUNDLE,
                                                                     tags, comment);
            }

            if (a.context.persistent > 0)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "C:     +  Persistent class: '%s' (%d minutes)",
                    pp->promiser, a.context.persistent);
                Buffer *buf = StringSetToBuffer(tags, ',');
                EvalContextHeapPersistentSave(ctx, pp->promiser, a.context.persistent,
                                              CONTEXT_STATE_POLICY_RESET, BufferData(buf));
                BufferDestroy(buf);
            }
            if (inserted && (comment != NULL))
            {
                Log(LOG_LEVEL_VERBOSE, "Added class '%s' with comment '%s'", pp->promiser, comment);
            }


            if (!inserted)
            {
                StringSetDestroy(tags);
            }

            return PROMISE_RESULT_NOOP;
        }
    }

    return PROMISE_RESULT_NOOP;
}

static bool SelectClass(EvalContext *ctx, const Rlist *list, const Promise *pp)
{
    int count = RlistLen(list);

    if (count == 0)
    {
        Log(LOG_LEVEL_ERR, "No classes to select on RHS");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }
    else if (count == 1 && IsVarList(RlistScalarValue(list)))
    {
        Log(LOG_LEVEL_VERBOSE,
            "select_class: Can not expand list '%s' for setting class.",
            RlistScalarValue(list));
        PromiseRef(LOG_LEVEL_VERBOSE, pp);
        return false;
    }

    assert(list);

    // Max:    (strlen of VFQNAME)   (strlen of VIPADDRESS)   (strlen of max 64 bit integer    )   '++\0'
    char splay[sizeof(VFQNAME) - 1 + sizeof(VIPADDRESS) - 1 + sizeof("18446744073709551615") - 1 + 2 + 1];
    snprintf(splay, sizeof(splay), "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());
    unsigned int hash = StringHash(splay, 0);
    int n = hash % count;

    while (n > 0 && list->next != NULL)
    {
        n--;
        list = list->next;
    }

    /* We are not having expanded variable or list at this point,
     * so we can not set select_class. */
    if (IsExpandable(RlistScalarValue(list)))
    {
        Log(LOG_LEVEL_VERBOSE,
            "select_class: Can not use not expanded element '%s' for setting class.",
            RlistScalarValue(list));
        PromiseRef(LOG_LEVEL_VERBOSE, pp);
        return false;
    }

    EvalContextClassPutSoft(ctx, RlistScalarValue(list),
                            CONTEXT_SCOPE_NAMESPACE, "source=promise");
    return true;
}

static bool DistributeClass(EvalContext *ctx, const Rlist *dist, const Promise *pp)
{
    int total = 0;
    const Rlist *rp;

    for (rp = dist; rp != NULL; rp = rp->next)
    {
        int result = IntFromString(RlistScalarValue(rp));

        if (result < 0)
        {
            Log(LOG_LEVEL_ERR, "Negative integer in class distribution");
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        total += result;
    }

    if (total == 0)
    {
        Log(LOG_LEVEL_ERR, "An empty distribution was specified on RHS");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    double fluct = drand48() * total;
    assert(0 <= fluct && fluct < total);

    for (rp = dist; rp != NULL; rp = rp->next)
    {
        fluct -= IntFromString(RlistScalarValue(rp));
        if (fluct < 0)
        {
            break;
        }
    }
    assert(rp);

    char buffer[CF_MAXVARSIZE];
    snprintf(buffer, CF_MAXVARSIZE, "%s_%s", pp->promiser, RlistScalarValue(rp));

    if (strcmp(PromiseGetBundle(pp)->type, "common") == 0)
    {
        EvalContextClassPutSoft(ctx, buffer, CONTEXT_SCOPE_NAMESPACE,
                                "source=promise");
    }
    else
    {
        EvalContextClassPutSoft(ctx, buffer, CONTEXT_SCOPE_BUNDLE,
                                "source=promise");
    }

    return true;
}

enum combine_t { c_or, c_and, c_xor }; // Class combinations
static bool EvalBoolCombination(EvalContext *ctx, const Rlist *list,
                                enum combine_t logic)
{
    bool result = (logic == c_and);

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        // tolerate unexpanded entries here and interpret as "class not set"
        bool here = (rp->val.type == RVAL_TYPE_SCALAR &&
                     IsDefinedClass(ctx, RlistScalarValue(rp)));

        // shortcut "and" and "or"
        switch (logic)
        {
        case c_or:
            if (here)
            {
                return true;
            }
            break;

        case c_and:
            if (!here)
            {
                return false;
            }
            break;

        default:
            result ^= here;
            break;
        }
    }

    return result;
}

/*
  Expand the RHS of a class-context constraint in place and evaluate it as a
  boolean. Shared by the class-defining attributes (expression, not, and, or,
  xor, select_class, dist) and by the class-cancelling 'cancel' attribute,
  whose trigger is evaluated exactly like 'expression'. This deliberately does
  NOT consider whether the promiser class is already defined; that skip logic
  is specific to class definition and lives in EvalClassExpression().
*/
static bool EvalClassConstraintRval(EvalContext *ctx, Constraint *cp, const Promise *pp)
{
    assert(cp != NULL);
    assert(pp != NULL);

    switch (cp->rval.type)
    {
        Rval rval;
        FnCall *fp;

    case RVAL_TYPE_FNCALL:
        fp = RvalFnCallValue(cp->rval);
        /* Special expansion of functions for control, best effort only: */
        FnCallResult res = FnCallEvaluate(ctx, PromiseGetPolicy(pp), fp, pp);

        FnCallDestroy(fp);
        cp->rval = res.rval;
        break;

    case RVAL_TYPE_LIST:
        for (Rlist *rp = cp->rval.item; rp != NULL; rp = rp->next)
        {
            rval = EvaluateFinalRval(ctx, PromiseGetPolicy(pp), NULL,
                                     "this", rp->val, true, pp);
            RvalDestroy(rp->val);
            rp->val = rval;
        }
        break;

    default:
        rval = ExpandPrivateRval(ctx, NULL, "this", cp->rval.item, cp->rval.type);
        RvalDestroy(cp->rval);
        cp->rval = rval;
        break;
    }

    /* The 'cancel' trigger is a plain class expression, evaluated like
     * 'expression': true when the named class(es) resolve to a defined
     * context. */
    if (strcmp(cp->lval, "expression") == 0 ||
        strcmp(cp->lval, "cancel") == 0)
    {
        return (cp->rval.type == RVAL_TYPE_SCALAR &&
                IsDefinedClass(ctx, RvalScalarValue(cp->rval)));
    }

    if (strcmp(cp->lval, "not") == 0)
    {
        return (cp->rval.type == RVAL_TYPE_SCALAR &&
                !IsDefinedClass(ctx, RvalScalarValue(cp->rval)));
    }

    /* If we get here, anything remaining on the RHS must be a clist */
    if (cp->rval.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_ERR, "RHS of promise body attribute '%s' is not a list", cp->lval);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return true;
    }

    // Class selection
    if (strcmp(cp->lval, "select_class") == 0)
    {
        return SelectClass(ctx, cp->rval.item, pp);
    }

    // Class distributions
    if (strcmp(cp->lval, "dist") == 0)
    {
        return DistributeClass(ctx, cp->rval.item, pp);
    }

    /* Combine with and/or/xor: */
    if (strcmp(cp->lval, "or") == 0)
    {
        return EvalBoolCombination(ctx, cp->rval.item, c_or);
    }
    else if (strcmp(cp->lval, "and") == 0)
    {
        return EvalBoolCombination(ctx, cp->rval.item, c_and);
    }
    else if (strcmp(cp->lval, "xor") == 0)
    {
        return EvalBoolCombination(ctx, cp->rval.item, c_xor);
    }

    return false;
}

static bool EvalClassExpression(EvalContext *ctx, Constraint *cp, const Promise *pp)
{
    assert(pp != NULL);
    assert(cp != NULL);

    /* assert() is compiled out under NDEBUG, so guard the pointer
     * dereferences below with an explicit runtime check as well. */
    if (pp == NULL || cp == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "EvalClassExpression internal diagnostic discovered an ill-formed condition");
        return false;
    }

    if (!IsDefinedClass(ctx, pp->classes))
    {
        return false;
    }

    if (IsDefinedClass(ctx, pp->promiser))
    {
        if (PromiseGetConstraintAsInt(ctx, "persistence", pp) == 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                " ?> Cancelling cached persistent class %s",
                pp->promiser);
            EvalContextHeapPersistentRemove(pp->promiser);
        }
        return false;
    }

    return EvalClassConstraintRval(ctx, cp, pp);
}

/*
  Handle a classes promise that uses the 'cancel' attribute. Unlike the
  class-defining attributes, a cancel promise must be evaluated even when the
  promiser class is already defined - that is the whole point - so it bypasses
  the "already defined" skip in EvalClassExpression(). When the cancel
  expression evaluates true and the promiser class is currently defined, the
  class is undefined, mirroring how classes bodies cancel classes via
  DeleteAllClasses().
*/
static PromiseResult VerifyClassCancel(EvalContext *ctx, const Promise *pp, Attributes *a)
{
    assert(ctx != NULL);
    assert(pp != NULL);
    assert(a != NULL);

    Constraint *cp = a->context.expression;
    assert(cp != NULL);

    /* Respect the promise's class context guard. */
    if (!IsDefinedClass(ctx, pp->classes))
    {
        return PROMISE_RESULT_NOOP;
    }

    if (!ValidClassName(pp->promiser))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "Attempted to cancel a class '%s', which is an illegal class identifier",
             pp->promiser);
        return PROMISE_RESULT_FAIL;
    }

    /* Hard classes describe the running system and must never be undefined
     * (consistent with how the cancel_* class body attributes ignore them).
     * Warn and leave the class in place rather than failing the promise. */
    const Class *promiser_class = EvalContextClassGet(ctx, NULL, pp->promiser);
    if (promiser_class != NULL && !promiser_class->is_soft)
    {
        Log(LOG_LEVEL_WARNING,
            "Cannot cancel reserved hard class '%s'", pp->promiser);
        return PROMISE_RESULT_NOOP;
    }

    /* Evaluate the cancel trigger. If it is false, leave the class as-is. */
    if (!EvalClassConstraintRval(ctx, cp, pp))
    {
        return PROMISE_RESULT_NOOP;
    }

    /* Trigger is true: undefine the promiser class if it is defined. */
    if (!IsDefinedClass(ctx, pp->promiser))
    {
        Log(LOG_LEVEL_VERBOSE,
            "C:     -  Class '%s' targeted by cancel is not defined, nothing to do",
            pp->promiser);
        return PROMISE_RESULT_NOOP;
    }

    Log(LOG_LEVEL_VERBOSE, "C:     -  Cancelling class: %s", pp->promiser);

    EvalContextHeapPersistentRemove(pp->promiser);
    {
        ClassRef ref = ClassRefParse(pp->promiser);
        EvalContextClassRemove(ctx, ref.ns, ref.name);
        ClassRefDestroy(ref);
    }
    EvalContextStackFrameRemoveSoft(ctx, pp->promiser);

    return PROMISE_RESULT_NOOP;
}
