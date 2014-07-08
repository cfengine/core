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

#include <ornaments.h>
#include <rlist.h>
#include <logging.h>

/****************************************************************************************/

void PromiseBanner(EvalContext *ctx, const Promise *pp)
{
    if (MACHINE_OUTPUT)
    {
        if (pp->comment)
        {
            Log(LOG_LEVEL_VERBOSE, "Comment '%s'", pp->comment);
        }
        return;
    }

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
    Log(LOG_LEVEL_VERBOSE, "P: BEGIN promise of type \"%s\" (pass %d)", pp->parent_promise_type->name, EvalContextGetPass(ctx));
    Log(LOG_LEVEL_VERBOSE, "P:    Promiser/affected object: '%s'", pp->promiser);

    Rlist *params = NULL;

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

    LoggingContext *lctx = GetCurrentThreadContext();
    Log(LOG_LEVEL_VERBOSE, "P:    Container path : '%s'", lctx->pctx->log_hook(lctx->pctx, EvalContextGetPass(ctx), ""));


    if (strlen(handle) > 0)
    {
        Log(LOG_LEVEL_VERBOSE, "P:    Promise's handle: '%s'", handle);
    }

    if (pp->comment)
    {
        Log(LOG_LEVEL_VERBOSE, "P:\n");
        Log(LOG_LEVEL_VERBOSE, "P:    Comment:  %s", pp->comment);
    }

    Log(LOG_LEVEL_VERBOSE, "P: .........................................................");
    Log(LOG_LEVEL_VERBOSE, "\n");
}

/****************************************************************************************/

void Banner(const char *s)
{
    if (MACHINE_OUTPUT)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "------------------------------------------------------------------------");
    Log(LOG_LEVEL_VERBOSE, " CFE %s ", s);
    Log(LOG_LEVEL_VERBOSE, "------------------------------------------------------------------------");

}

/****************************************************************************************/

void BundleBanner(const Bundle *bp, const Rlist *params)
{
    if (MACHINE_OUTPUT)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");

    if (params)
    {
        Writer *w = StringWriter();
        RlistWrite(w, params);
        Log(LOG_LEVEL_VERBOSE, "B: BEGIN BUNDLE %s(%s)", bp->name, StringWriterData(w));
        WriterClose(w);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "B: BEGIN BUNDLE %s", bp->name);
    }

    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
}

/****************************************************************************************/

void EndBundleBanner(const Bundle *bp)
{
    if (MACHINE_OUTPUT)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
    Log(LOG_LEVEL_VERBOSE, "B: END BUNDLE %s", bp->name);
    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
}
