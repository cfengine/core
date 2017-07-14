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
#include <actuator.h>

#include <misc_lib.h>

PromiseResult PromiseResultUpdate(PromiseResult prior, PromiseResult evidence)
{
    switch (prior)
    {
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_INTERRUPTED:
    case PROMISE_RESULT_TIMEOUT:
        return prior;

    case PROMISE_RESULT_WARN:
        switch (evidence)
        {
        case PROMISE_RESULT_DENIED:
        case PROMISE_RESULT_FAIL:
        case PROMISE_RESULT_INTERRUPTED:
        case PROMISE_RESULT_TIMEOUT:
        case PROMISE_RESULT_WARN:
            return evidence;

        case PROMISE_RESULT_CHANGE:
        case PROMISE_RESULT_NOOP:
        case PROMISE_RESULT_SKIPPED:
            return prior;
        }

    case PROMISE_RESULT_SKIPPED:
        return evidence;

    case PROMISE_RESULT_NOOP:
        switch (evidence)
        {
        case PROMISE_RESULT_SKIPPED:
            return prior;

        default:
            return evidence;
        }

    case PROMISE_RESULT_CHANGE:
        switch (evidence)
        {
        case PROMISE_RESULT_DENIED:
        case PROMISE_RESULT_FAIL:
        case PROMISE_RESULT_INTERRUPTED:
        case PROMISE_RESULT_TIMEOUT:
        case PROMISE_RESULT_WARN:
            return evidence;

        case PROMISE_RESULT_CHANGE:
        case PROMISE_RESULT_NOOP:
        case PROMISE_RESULT_SKIPPED:
            return prior;
        }
    }

    ProgrammingError("Never reach");
}

bool PromiseResultIsOK(PromiseResult result)
{
    return (result == PROMISE_RESULT_CHANGE) || (result == PROMISE_RESULT_NOOP);
}
