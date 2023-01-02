/*
  Copyright 2023 Northern.tech AS

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

#include <retcode.h>

#include <printsize.h>
#include <actuator.h>
#include <rlist.h>
#include <string_lib.h>

bool VerifyCommandRetcode(EvalContext *ctx, int retcode, const Attributes *a, const Promise *pp, PromiseResult *result)
{
    assert(a != NULL);
    assert(pp != NULL);
    bool result_retcode = true;

    if (a->classes.retcode_kept ||
        a->classes.retcode_repaired ||
        a->classes.retcode_failed)
    {
        int matched = false;
        char retcodeStr[PRINTSIZE(retcode)];
        xsnprintf(retcodeStr, sizeof(retcodeStr), "%d", retcode);

        LogLevel info_or_verbose = LOG_LEVEL_INFO;

        // inform constraint is only for commands promises,
        // a->inform is actually false for other promise types, so
        // checking the promise type here is important:
        if (StringEqual("commands", PromiseGetPromiseType(pp)) && (!a->inform))
        {
            // for commands promises which don't make changes to the system,
            // you can use this to make the log messages verbose:
            // inform => "false";
            info_or_verbose = LOG_LEVEL_VERBOSE;
        }

        if (RlistKeyIn(a->classes.retcode_kept, retcodeStr))
        {
            cfPS(ctx, info_or_verbose, PROMISE_RESULT_NOOP, pp, a,
                 "Command related to promiser '%s' returned code '%d' defined as promise kept", pp->promiser,
                 retcode);
            matched = true;
        }

        if (RlistKeyIn(a->classes.retcode_repaired, retcodeStr))
        {

            cfPS(ctx, info_or_verbose, PROMISE_RESULT_CHANGE, pp, a,
                 "Command related to promiser '%s' returned code '%d' defined as promise repaired", pp->promiser,
                 retcode);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            matched = true;
        }

        if (RlistKeyIn(a->classes.retcode_failed, retcodeStr))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Command related to promiser '%s' returned code '%d' defined as promise failed", pp->promiser,
                 retcode);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            result_retcode = false;
            matched = true;
        }

        if (!matched)
        {
            cfPS(ctx, info_or_verbose, PROMISE_RESULT_FAIL, pp, a,
                 "Command related to promiser '%s' returned code '%d' not defined as promise kept, not kept or repaired; setting to failed",
                 pp->promiser, retcode);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            result_retcode = false;
        }

    }
    else // default: 0 is success, != 0 is failure
    {
        if (retcode == 0)
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Finished command related to promiser '%s' -- succeeded",
                 pp->promiser);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Finished command related to promiser '%s' -- an error occurred, returned %d", pp->promiser,
                 retcode);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            result_retcode = false;
        }
    }

    return result_retcode;
}
