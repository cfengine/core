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

#include <execd-config.h>

#include <generic_agent.h>
#include <eval_context.h>
#include <conversion.h>
#include <string_lib.h>

static double GetSplay(void)
{
    char splay[CF_BUFSIZE];
    snprintf(splay, CF_BUFSIZE, "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());
    return ((double) StringHash(splay, 0, CF_HASHTABLESIZE)) / CF_HASHTABLESIZE;
}

ExecdConfig *ExecdConfigNew(const EvalContext *ctx, const Policy *policy)
{
    ExecdConfig *execd_config = xcalloc(1, sizeof(ExecdConfig));

    execd_config->schedule = StringSetNew();
    StringSetAdd(execd_config->schedule, xstrdup("Min00"));
    StringSetAdd(execd_config->schedule, xstrdup("Min05"));
    StringSetAdd(execd_config->schedule, xstrdup("Min10"));
    StringSetAdd(execd_config->schedule, xstrdup("Min15"));
    StringSetAdd(execd_config->schedule, xstrdup("Min20"));
    StringSetAdd(execd_config->schedule, xstrdup("Min25"));
    StringSetAdd(execd_config->schedule, xstrdup("Min30"));
    StringSetAdd(execd_config->schedule, xstrdup("Min35"));
    StringSetAdd(execd_config->schedule, xstrdup("Min40"));
    StringSetAdd(execd_config->schedule, xstrdup("Min45"));
    StringSetAdd(execd_config->schedule, xstrdup("Min50"));
    StringSetAdd(execd_config->schedule, xstrdup("Min55"));
    execd_config->splay_time = 0;
    execd_config->log_facility = xstrdup("LOG_USER");

    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_EXECUTOR);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes))
            {
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_executor");
            DataType t;
            const void *value = EvalContextVariableGet(ctx, ref, &t);
            VarRefDestroy(ref);

            if (t == CF_DATA_TYPE_NONE)
            {
                ProgrammingError("Unknown attribute '%s' in control body,"
                                 " should have already been stopped by the parser",
                                 cp->lval);
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_EXECUTORFACILITY].lval) == 0)
            {
                free(execd_config->log_facility);
                execd_config->log_facility = xstrdup(value);
                Log(LOG_LEVEL_DEBUG, "executorfacility '%s'", execd_config->log_facility);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SPLAYTIME].lval) == 0)
            {
                int time = IntFromString(value);
                execd_config->splay_time = (int) (time * SECONDS_PER_MINUTE * GetSplay());
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SCHEDULE].lval) == 0)
            {
                Log(LOG_LEVEL_DEBUG, "Loading user-defined schedule...");
                StringSetClear(execd_config->schedule);

                for (const Rlist *rp = value; rp; rp = rp->next)
                {
                    StringSetAdd(execd_config->schedule, xstrdup(RlistScalarValue(rp)));
                    Log(LOG_LEVEL_DEBUG, "Adding '%s'", RlistScalarValue(rp));
                }
            }
        }
    }

    return execd_config;
}

void ExecdConfigDestroy(ExecdConfig *execd_config)
{
    if (execd_config)
    {
        free(execd_config->log_facility);
        StringSetDestroy(execd_config->schedule);
    }
    free(execd_config);
}
