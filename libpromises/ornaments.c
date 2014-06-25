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

void PromiseBanner(const Promise *pp)
{
    if (!LEGACY_OUTPUT)
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
        strcpy(handle, "(enterprise only)");
    }

    Log(LOG_LEVEL_VERBOSE, "    .........................................................");
    Log(LOG_LEVEL_VERBOSE, "     Promise's handle: '%s'", handle);
    Log(LOG_LEVEL_VERBOSE, "     Promise made by: '%s'", pp->promiser);

    if (pp->comment)
    {
        Log(LOG_LEVEL_VERBOSE, "\n");
        Log(LOG_LEVEL_VERBOSE, "    Comment:  %s", pp->comment);
    }

    Log(LOG_LEVEL_VERBOSE, "    .........................................................");
    Log(LOG_LEVEL_VERBOSE, "\n");
}

void BannerSubBundle(const Bundle *bp, const Rlist *params)
{
    if (!LEGACY_OUTPUT)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *");
    Log(LOG_LEVEL_VERBOSE, "       BUNDLE %s", bp->name);

    if (params)
    {
        Writer *w = StringWriter();
        RlistWrite(w, params);
        Log(LOG_LEVEL_VERBOSE, "(%s)", StringWriterData(w));
        WriterClose(w);
    }
    Log(LOG_LEVEL_VERBOSE, "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *");
}

void Banner(const char *s)
{
    if (!LEGACY_OUTPUT)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "***********************************************************");
    Log(LOG_LEVEL_VERBOSE, " %s ", s);
    Log(LOG_LEVEL_VERBOSE, "***********************************************************");
}

void BannerPromiseType(const char *bundlename, const char *type, int pass)
{
    if (!LEGACY_OUTPUT)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "\n");
    Log(LOG_LEVEL_VERBOSE, "   =========================================================");
    Log(LOG_LEVEL_VERBOSE, "   %s in bundle %s (%d)", type, bundlename, pass);
    Log(LOG_LEVEL_VERBOSE, "   =========================================================");
    Log(LOG_LEVEL_VERBOSE, "\n");
}

void BannerSubPromiseType(const EvalContext *ctx, const char *bundlename, const char *type)
{
    if (!LEGACY_OUTPUT)
    {
        return;
    }

    if (strcmp(type, "processes") == 0)
    {
        {
            Log(LOG_LEVEL_VERBOSE, "     ??? Local class context: ");

            ClassTableIterator *iter = EvalContextClassTableIteratorNewLocal(ctx);
            Class *cls = NULL;
            while ((cls = ClassTableIteratorNext(iter)))
            {
                Log(LOG_LEVEL_VERBOSE, "       %s", cls->name);
            }
            ClassTableIteratorDestroy(iter);

            Log(LOG_LEVEL_VERBOSE, "\n");
        }
    }

    Log(LOG_LEVEL_VERBOSE, "\n");
    Log(LOG_LEVEL_VERBOSE, "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = ");
    Log(LOG_LEVEL_VERBOSE, "      %s in bundle %s", type, bundlename);
    Log(LOG_LEVEL_VERBOSE, "      = = = = = = = = = = = = = = = = = = = = = = = = = = = = ");
    Log(LOG_LEVEL_VERBOSE, "\n");
}

void BannerBundle(const Bundle *bp, const Rlist *params)
{
    if (!LEGACY_OUTPUT)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "*****************************************************************");
    Log(LOG_LEVEL_VERBOSE, "BUNDLE %s", bp->name);

    if (params)
    {
        Writer *w = StringWriter();
        RlistWrite(w, params);
        Log(LOG_LEVEL_VERBOSE, "(%s)", StringWriterData(w));
        WriterClose(w);
    }

    Log(LOG_LEVEL_VERBOSE, "*****************************************************************");
}
