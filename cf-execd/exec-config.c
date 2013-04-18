#include "exec-config.h"

#include "alloc.h"
#include "hashes.h"

#include "rlist.h"
#include "env_context.h"
#include "conversion.h"
#include "logging.h"
#include "generic_agent.h" // TODO: fix

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

static void ExecConfigResetDefault(ExecConfig *exec_config)
{
    free(exec_config->log_facility);
    exec_config->log_facility = xstrdup("LOG_USER");

    free(exec_config->exec_command);
    exec_config->exec_command = xstrdup("");

    free(exec_config->mail_server);
    exec_config->mail_server = xstrdup("");

    free(exec_config->mail_from_address);
    exec_config->mail_from_address = xstrdup("");

    free(exec_config->mail_to_address);
    exec_config->mail_to_address = xstrdup("");

    exec_config->mail_max_lines = 30;
    exec_config->agent_expireafter = 10800;
    exec_config->splay_time = 0;

    StringSetClear(exec_config->schedule);
    StringSetAdd(exec_config->schedule, xstrdup("Min00"));
    StringSetAdd(exec_config->schedule, xstrdup("Min05"));
    StringSetAdd(exec_config->schedule, xstrdup("Min10"));
    StringSetAdd(exec_config->schedule, xstrdup("Min15"));
    StringSetAdd(exec_config->schedule, xstrdup("Min20"));
    StringSetAdd(exec_config->schedule, xstrdup("Min25"));
    StringSetAdd(exec_config->schedule, xstrdup("Min30"));
    StringSetAdd(exec_config->schedule, xstrdup("Min35"));
    StringSetAdd(exec_config->schedule, xstrdup("Min40"));
    StringSetAdd(exec_config->schedule, xstrdup("Min45"));
    StringSetAdd(exec_config->schedule, xstrdup("Min50"));
    StringSetAdd(exec_config->schedule, xstrdup("Min55"));
}

ExecConfig *ExecConfigNewDefault(bool scheduled_run, const char *fq_name, const char *ip_address)
{
    ExecConfig *exec_config = xcalloc(1, sizeof(ExecConfig));

    exec_config->scheduled_run = scheduled_run;
    exec_config->fq_name = xstrdup(fq_name);
    exec_config->ip_address = xstrdup(ip_address);
    exec_config->schedule = StringSetNew();

    ExecConfigResetDefault(exec_config);

    return exec_config;
}

ExecConfig *ExecConfigCopy(const ExecConfig *config)
{
    ExecConfig *copy = xcalloc(1, sizeof(ExecConfig));

    copy->scheduled_run = config->scheduled_run;
    copy->exec_command = xstrdup(config->exec_command);
    copy->mail_server = xstrdup(config->mail_server);
    copy->mail_from_address = xstrdup(config->mail_from_address);
    copy->mail_to_address = xstrdup(config->mail_to_address);
    copy->fq_name = xstrdup(config->fq_name);
    copy->ip_address = xstrdup(config->ip_address);
    copy->mail_max_lines = config->mail_max_lines;
    copy->agent_expireafter = config->agent_expireafter;

    return copy;
}

void ExecConfigDestroy(ExecConfig *exec_config)
{
    if (exec_config)
    {
        free(exec_config->exec_command);
        free(exec_config->mail_server);
        free(exec_config->mail_from_address);
        free(exec_config->mail_to_address);
        free(exec_config->log_facility);
        free(exec_config->fq_name);
        free(exec_config->ip_address);

        StringSetDestroy(exec_config->schedule);

        free(exec_config);
    }
}

static double GetSplay(void)
{
    char splay[CF_BUFSIZE];
    snprintf(splay, CF_BUFSIZE, "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());
    return ((double) OatHash(splay, CF_HASHTABLESIZE)) / CF_HASHTABLESIZE;
}

void ExecConfigUpdate(const EvalContext *ctx, const Policy *policy, ExecConfig *exec_config)
{
    ExecConfigResetDefault(exec_config);

    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_EXECUTOR);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes, NULL))
            {
                continue;
            }

            Rval retval;
            if (!EvalContextVariableGet(ctx, (VarRef) { NULL, "control_executor", cp->lval }, &retval, NULL))
            {
                // TODO: should've been checked before this point. change to programming error
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unknown lval %s in exec control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILFROM].lval) == 0)
            {
                free(exec_config->mail_from_address);
                exec_config->mail_from_address = xstrdup(retval.item);
                CfDebug("mailfrom = %s\n", exec_config->mail_from_address);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILTO].lval) == 0)
            {
                free(exec_config->mail_to_address);
                exec_config->mail_to_address = xstrdup(retval.item);
                CfDebug("mailto = %s\n", exec_config->mail_to_address);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SMTPSERVER].lval) == 0)
            {
                free(exec_config->mail_server);
                exec_config->mail_server = xstrdup(retval.item);
                CfDebug("smtpserver = %s\n", exec_config->mail_server);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_EXECCOMMAND].lval) == 0)
            {
                free(exec_config->exec_command);
                exec_config->exec_command = xstrdup(retval.item);
                CfDebug("exec_command = %s\n", exec_config->exec_command);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_AGENT_EXPIREAFTER].lval) == 0)
            {
                exec_config->agent_expireafter = IntFromString(retval.item);
                CfDebug("agent_expireafter = %d\n", exec_config->agent_expireafter);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_EXECUTORFACILITY].lval) == 0)
            {
                exec_config->log_facility = xstrdup(retval.item);
                CfDebug("executorfacility = %s\n", exec_config->log_facility);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILMAXLINES].lval) == 0)
            {
                exec_config->mail_max_lines = IntFromString(retval.item);
                CfDebug("maxlines = %d\n", exec_config->mail_max_lines);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SPLAYTIME].lval) == 0)
            {
                int time = IntFromString(RvalScalarValue(retval));
                exec_config->splay_time = (int) (time * SECONDS_PER_MINUTE * GetSplay());
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SCHEDULE].lval) == 0)
            {
                CfDebug("Loading user-defined schedule...\n");
                StringSetClear(exec_config->schedule);

                for (const Rlist *rp = retval.item; rp; rp = rp->next)
                {
                    StringSetAdd(exec_config->schedule, xstrdup(RlistScalarValue(rp)));
                    CfDebug("Adding %s\n", RlistScalarValue(rp));
                }
            }
        }
    }
}

