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

#include "retcode.h"
#include "rlist.h"
#include "logging.h"

int VerifyCommandRetcode(EvalContext *ctx, int retcode, int fallback, Attributes a, Promise *pp)
{
    char retcodeStr[128] = { 0 };
    int result = true;
    int matched = false;

    if ((a.classes.retcode_kept) || (a.classes.retcode_repaired) || (a.classes.retcode_failed))
    {

        snprintf(retcodeStr, sizeof(retcodeStr), "%d", retcode);

        if (RlistKeyIn(a.classes.retcode_kept, retcodeStr))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_NOOP, "", pp, a,
                 "-> Command related to promiser \"%s\" returned code defined as promise kept (%d)", pp->promiser,
                 retcode);
            result = true;
            matched = true;
        }

        if (RlistKeyIn(a.classes.retcode_repaired, retcodeStr))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a,
                 "-> Command related to promiser \"%s\" returned code defined as promise repaired (%d)", pp->promiser,
                 retcode);
            result = true;
            matched = true;
        }

        if (RlistKeyIn(a.classes.retcode_failed, retcodeStr))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_FAIL, "", pp, a,
                 "!! Command related to promiser \"%s\" returned code defined as promise failed (%d)", pp->promiser,
                 retcode);
            result = false;
            matched = true;
        }

        if (!matched)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  "Command related to promiser \"%s\" returned code %d -- did not match any failed, repaired or kept lists",
                  pp->promiser, retcode);
        }

    }
    else if (fallback)          // default: 0 is success, != 0 is failure
    {
        if (retcode == 0)
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, " -> Finished command related to promiser \"%s\" -- succeeded",
                 pp->promiser);
            result = true;
        }
        else
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_FAIL, "", pp, a,
                 " !! Finished command related to promiser \"%s\" -- an error occurred (returned %d)", pp->promiser,
                 retcode);
            result = false;
        }
    }

    return result;
}
