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

#include <verify_users.h>
#include <attributes.h>
#include <eval_context.h>
#include <ornaments.h>
#include <locks.h>
#include <promises.h>
#include <string_lib.h>
#include <misc_lib.h>

static int UserSanityCheck(Attributes a, const Promise *pp);

PromiseResult VerifyUsersPromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    Attributes a = GetUserAttributes(ctx, pp);

    if (!UserSanityCheck(a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    PromiseBanner(ctx, pp);

    snprintf(lockname, CF_BUFSIZE - 1, "user-%s-%d", pp->promiser, a.users.policy);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    VerifyOneUsersPromise(pp->promiser, a.users, &result, a.transaction.action, ctx, &a, pp);

    switch (result) {
    case PROMISE_RESULT_NOOP:
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "User promise kept");
        break;
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_INTERRUPTED:
    case PROMISE_RESULT_WARN:
        cfPS(ctx, LOG_LEVEL_INFO, result, pp, a, "User promise not kept");
        break;
    case PROMISE_RESULT_CHANGE:
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "User promise repaired");
        break;
    default:
        ProgrammingError("Unknown promise result");
        break;
    }


    YieldCurrentLock(thislock);
    return result;
}

/** Pre-check of promise contents **/

static int UserSanityCheck(Attributes a, const Promise *pp)
{
    User *u = &a.users;
    switch (u->policy)
    {
    case USER_STATE_PRESENT:
    case USER_STATE_ABSENT:
    case USER_STATE_LOCKED:
        break;
    default:
        Log(LOG_LEVEL_ERR, "No policy specified for 'users' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((SafeStringLength(u->password) == 0 && u->password_format != PASSWORD_FORMAT_NONE)
        || (SafeStringLength(u->password) != 0 && u->password_format == PASSWORD_FORMAT_NONE))
    {
        Log(LOG_LEVEL_ERR, "Both 'data' and 'format' must be specified in password body for 'users' promise '%s'", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    return true;
}
