/*
   Copyright 2017 Northern.tech AS

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

#include <ornaments.h>
#include <rlist.h>
#include <logging.h>
#include <fncall.h>
#include <string_lib.h>

/****************************************************************************************/

void SpecialTypeBanner(TypeSequence type, int pass)
{
    if (type == TYPE_SEQUENCE_CONTEXTS)
    {
        Log(LOG_LEVEL_VERBOSE, "C: .........................................................");
        Log(LOG_LEVEL_VERBOSE, "C: BEGIN classes / conditions (pass %d)", pass);
        Log(LOG_LEVEL_VERBOSE, "C: .........................................................");
    }
    if (type == TYPE_SEQUENCE_VARS)
    {
        Log(LOG_LEVEL_VERBOSE, "V: .........................................................");
        Log(LOG_LEVEL_VERBOSE, "V: BEGIN variables (pass %d)", pass);
        Log(LOG_LEVEL_VERBOSE, "V: .........................................................");
    }
}

/****************************************************************************************/

void PromiseBanner(EvalContext *ctx, const Promise *pp)
{
    char handle[CF_MAXVARSIZE];
    const char *sp;

    if ((sp = PromiseGetHandle(pp)) || (sp = PromiseID(pp)))
    {
        strlcpy(handle, sp, CF_MAXVARSIZE);
    }
    else
    {
        strcpy(handle, "");
    }

    Log(LOG_LEVEL_VERBOSE, "P: .........................................................");

    if (strlen(handle) > 0)
    {
        Log(LOG_LEVEL_VERBOSE, "P: BEGIN promise '%s' of type \"%s\" (pass %d)", handle, pp->parent_promise_type->name, EvalContextGetPass(ctx));
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "P: BEGIN un-named promise of type \"%s\" (pass %d)", pp->parent_promise_type->name, EvalContextGetPass(ctx));
    }

    const size_t n = 2*CF_MAXFRAGMENT + 3;
    char pretty_promise_name[n+1];
    pretty_promise_name[0] = '\0';
    StringAppendAbbreviatedPromise(pretty_promise_name, pp->promiser, n, CF_MAXFRAGMENT);
    Log(LOG_LEVEL_VERBOSE, "P:    Promiser/affected object: '%s'", pretty_promise_name);

    Rlist *params = NULL;
    char *varclass;
    FnCall *fp;

    if ((params = EvalContextGetBundleArgs(ctx)))
    {
        Writer *w = StringWriter();
        RlistWrite(w, params);
        Log(LOG_LEVEL_VERBOSE, "P:    From parameterized bundle: %s(%s)", PromiseGetBundle(pp)->name, StringWriterData(w));
        WriterClose(w);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "P:    Part of bundle: %s", PromiseGetBundle(pp)->name);
    }

    Log(LOG_LEVEL_VERBOSE, "P:    Base context class: %s", pp->classes);

    if ((varclass = PromiseGetConstraintAsRval(pp, "if", RVAL_TYPE_SCALAR)) || (varclass = PromiseGetConstraintAsRval(pp, "ifvarclass", RVAL_TYPE_SCALAR)))
    {
        Log(LOG_LEVEL_VERBOSE, "P:    \"if\" class condition: %s", varclass);
    }
    else if ((fp = (FnCall *)PromiseGetConstraintAsRval(pp, "if", RVAL_TYPE_FNCALL)) || (fp = (FnCall *)PromiseGetConstraintAsRval(pp, "ifvarclass", RVAL_TYPE_FNCALL)))
    {
        Writer *w = StringWriter();
        FnCallWrite(w, fp);
        Log(LOG_LEVEL_VERBOSE, "P:    \"if\" class condition: %s", StringWriterData(w));
    }
    else if ((varclass = PromiseGetConstraintAsRval(pp, "unless", RVAL_TYPE_SCALAR)))
    {
        Log(LOG_LEVEL_VERBOSE, "P:    \"unless\" class condition: %s", varclass);
    }
    else if ((fp = (FnCall *)PromiseGetConstraintAsRval(pp, "unless", RVAL_TYPE_FNCALL)))
    {
        Writer *w = StringWriter();
        FnCallWrite(w, fp);
        Log(LOG_LEVEL_VERBOSE, "P:    \"unless\" class condition: %s", StringWriterData(w));
    }

    Log(LOG_LEVEL_VERBOSE, "P:    Container path : '%s'", EvalContextStackToString(ctx));

    if (pp->comment)
    {
        Log(LOG_LEVEL_VERBOSE, "P:\n");
        Log(LOG_LEVEL_VERBOSE, "P:    Comment:  %s", pp->comment);
    }

    Log(LOG_LEVEL_VERBOSE, "P: .........................................................");
    Log(LOG_LEVEL_VERBOSE, "\n");
}

/****************************************************************************************/

void Legend()
{
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
    Log(LOG_LEVEL_VERBOSE, "PREFIX LEGEND:");
    Log(LOG_LEVEL_VERBOSE, " V: variable or parameter new definition in scope");
    Log(LOG_LEVEL_VERBOSE, " C: class/context new definition ");
    Log(LOG_LEVEL_VERBOSE, " B: bundle start/end execution marker");
    Log(LOG_LEVEL_VERBOSE, " P: promise execution output ");
    Log(LOG_LEVEL_VERBOSE, " A: accounting output ");
    Log(LOG_LEVEL_VERBOSE, " T: time measurement for stated object (promise or bundle)");
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
}

/****************************************************************************************/

void Banner(const char *s)
{
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
    Log(LOG_LEVEL_VERBOSE, " %s ", s);
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");

}

/****************************************************************************************/

void BundleBanner(const Bundle *bp, const Rlist *params)
{
    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");

    if (params)
    {
        Writer *w = StringWriter();
        RlistWrite(w, params);
        Log(LOG_LEVEL_VERBOSE, "B: BEGIN bundle %s(%s)", bp->name, StringWriterData(w));
        WriterClose(w);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "B: BEGIN bundle %s", bp->name);
    }

    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
}

/****************************************************************************************/

void EndBundleBanner(const Bundle *bp)
{
    if (bp == NULL)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
    Log(LOG_LEVEL_VERBOSE, "B: END bundle %s", bp->name);
    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
}
