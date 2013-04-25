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

#include "ornaments.h"
#include "logging.h"
#include "rlist.h"

void PromiseBanner(const Promise *pp)
{
    char handle[CF_MAXVARSIZE];
    const char *sp;

    if ((sp = PromiseGetHandle(pp)) || (sp = PromiseID(pp)))
    {
        strncpy(handle, sp, CF_MAXVARSIZE - 1);
    }
    else
    {
        strcpy(handle, "(enterprise only)");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "    .........................................................\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s>     Promise's handle: %s\n", VPREFIX, handle);
        printf("%s>     Promise made by: \"%s\"", VPREFIX, pp->promiser);
    }

    if (pp->promisee.item)
    {
        if (VERBOSE)
        {
            printf("\n%s>     Promise made to (stakeholders): ", VPREFIX);
            RvalShow(stdout, pp->promisee);
        }
    }

    if (VERBOSE)
    {
        printf("\n");
    }

    if (pp->comment)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "    Comment:  %s\n", pp->comment);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "    .........................................................\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

void BannerSubBundle(const Bundle *bp, const Rlist *params)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s>       BUNDLE %s", VPREFIX, bp->name);
    }

    if (params && (VERBOSE || DEBUG))
    {
        printf("(");
        RlistShow(stdout, params);
        printf(" )\n");
    }
    else
    {
        if (VERBOSE || DEBUG)
            printf("\n");
    }
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

void Banner(const char *s)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "***********************************************************\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " %s ", s);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "***********************************************************\n");
}

void BannerPromiseType(const char *bundlename, const char *type, int pass)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   =========================================================\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   %s in bundle %s (%d)\n", type, bundlename, pass);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "   =========================================================\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

void BannerSubPromiseType(const EvalContext *ctx, const char *bundlename, const char *type)
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

void BannerBundle(Bundle *bp, Rlist *params)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "*****************************************************************\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s> BUNDLE %s", VPREFIX, bp->name);
    }

    if (params && (VERBOSE || DEBUG))
    {
        printf("(");
        RlistShow(stdout, params);
        printf(" )\n");
    }
    else
    {
        if (VERBOSE || DEBUG)
            printf("\n");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "*****************************************************************\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");

}
