/*
   Copyright 2017 Northern.tech AS

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

#include <verify_measurements.h>

#include <promises.h>
#include <files_names.h>
#include <attributes.h>
#include <policy.h>
#include <cf-monitord-enterprise-stubs.h>
#include <eval_context.h>
#include <ornaments.h>

static bool CheckMeasureSanity(Measurement m, const Promise *pp);

/*****************************************************************************/

PromiseResult VerifyMeasurementPromise(EvalContext *ctx, double *measurement, const Promise *pp)
{
    PromiseBanner(ctx, pp);

    Attributes a = GetMeasurementAttributes(ctx, pp);

    if (!CheckMeasureSanity(a.measure, pp))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Measurement promise is not valid");
        return PROMISE_RESULT_INTERRUPTED;
    }

    return VerifyMeasurement(ctx, measurement, a, pp);
}

/*****************************************************************************/

static bool CheckMeasureSanity(Measurement m, const Promise *pp)
{
    int retval = true;

    if (!IsAbsPath(pp->promiser))
    {
        Log(LOG_LEVEL_ERR, "The promiser '%s' of a measurement was not an absolute path",
            pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        retval = false;
    }

    if (m.data_type == CF_DATA_TYPE_NONE)
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
            case CF_DATA_TYPE_COUNTER:
            case CF_DATA_TYPE_STRING:
            case CF_DATA_TYPE_INT:
            case CF_DATA_TYPE_REAL:
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
