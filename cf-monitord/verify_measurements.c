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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "verify_measurements.h"

#include "promises.h"
#include "files_names.h"
#include "attributes.h"
#include "policy.h"
#include "cf-monitord-enterprise-stubs.h"
#include "env_context.h"
#include "ornaments.h"

#ifdef HAVE_NOVA
#include "history.h"
#endif

static bool CheckMeasureSanity(Measurement m, Promise *pp);

/*****************************************************************************/

void VerifyMeasurementPromise(EvalContext *ctx, double *this, Promise *pp)
{
    Attributes a = { {0} };

    if (EvalContextPromiseIsDone(ctx, pp))
    {
        if (pp->comment)
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping static observation '%s' (comment: %s), already done", pp->promiser, pp->comment);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping static observation '%s', already done", pp->promiser);
        }

        return;
    }

    PromiseBanner(pp);

    a = GetMeasurementAttributes(ctx, pp);

    if (!CheckMeasureSanity(a.measure, pp))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Measurement promise is not valid");
        return;
    }

    VerifyMeasurement(ctx, this, a, pp);
}

/*****************************************************************************/

static bool CheckMeasureSanity(Measurement m, Promise *pp)
{
    int retval = true;

    if (!IsAbsPath(pp->promiser))
    {
        Log(LOG_LEVEL_ERR, "The promiser '%s' of a measurement was not an absolute path",
             pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        retval = false;
    }

    if (m.data_type == DATA_TYPE_NONE)
    {
        Log(LOG_LEVEL_ERR, "The promiser '%s' did not specify a data type", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        retval = false;
    }
    else
    {
        if ((m.history_type) && (strcmp(m.history_type, "weekly") == 0))
        {
            switch (m.data_type)
            {
            case DATA_TYPE_COUNTER:
            case DATA_TYPE_STRING:
            case DATA_TYPE_INT:
            case DATA_TYPE_REAL:
                break;

            default:
                Log(LOG_LEVEL_ERR, "The promiser '%s' cannot have history type weekly as it is not a number", pp->promiser);
                PromiseRef(LOG_LEVEL_ERR, pp);
                retval = false;
                break;
            }
        }
    }

    if ((m.select_line_matching) && (m.select_line_number != CF_NOINT))
    {
        Log(LOG_LEVEL_ERR, "The promiser '%s' cannot select both a line by pattern and by number", pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        retval = false;
    }

    if (!m.extraction_regex)
    {
        Log(LOG_LEVEL_VERBOSE, "No extraction regex, so assuming whole line is the value");
    }
    else
    {
        if ((!strchr(m.extraction_regex, '(')) && (!strchr(m.extraction_regex, ')')))
        {
            Log(LOG_LEVEL_ERR, "The extraction_regex must contain a single backreference for the extraction");
            retval = false;
        }
    }

    return retval;
}
