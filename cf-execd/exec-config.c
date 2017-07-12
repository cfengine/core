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

#include <exec-config.h>

#include <alloc.h>
#include <string_lib.h>
#include <writer.h>

#include <rlist.h>
#include <eval_context.h>
#include <conversion.h>
#include <generic_agent.h> // TODO: fix
#include <item_lib.h>


static char *GetIpAddresses(const EvalContext *ctx)
{
    Writer *ipbuf = StringWriter();

    for (Item *iptr = EvalContextGetIpAddresses(ctx); iptr != NULL; iptr = iptr->next)
    {
        WriterWrite(ipbuf, iptr->name);
        if (iptr->next != NULL)
        {
            WriterWriteChar(ipbuf, ' ');
        }
    }

    return StringWriterClose(ipbuf);
}

static void RegexFree(void *ptr)
{
    pcre_free(ptr);
}

static void MailFilterFill(const char *str,
                           Seq **output, Seq **output_regex,
                           const char *filter_type)
{
    const char *errorstr;
    int erroffset;
    pcre *rx = pcre_compile(str,
                            PCRE_MULTILINE | PCRE_DOTALL | PCRE_ANCHORED,
                            &errorstr, &erroffset, NULL);
    if (!rx)
    {
        Log(LOG_LEVEL_ERR,
            "Invalid regular expression in mailfilter_%s: "
            "pcre_compile() '%s' in expression '%s' (offset: %d). "
            "Ignoring expression.", filter_type,
            errorstr, str, erroffset);
    }
    else
    {
        SeqAppend(*output, xstrdup(str));
        SeqAppend(*output_regex, rx);
    }
}

static void RlistMailFilterFill(const Rlist *input,
                                Seq **output, Seq **output_regex,
                                const char *filter_type)
{
    int len = RlistLen(input);

    *output = SeqNew(len, &free);
    *output_regex = SeqNew(len, &RegexFree);

    const Rlist *ptr = input;
    for (int i = 0; i < len; i++)
    {
        const char *str = ptr->val.item;
        MailFilterFill(str, output, output_regex, filter_type);

        ptr = ptr->next;
    }
}

static void SeqMailFilterFill(const Seq *input,
                           Seq **output, Seq **output_regex,
                           const char *filter_type)
{
    int len = SeqLength(input);

    *output = SeqNew(len, &free);
    *output_regex = SeqNew(len, &RegexFree);

    for (int i = 0; i < len; i++)
    {
        MailFilterFill(SeqAt(input, i), output, output_regex, filter_type);
    }
}

ExecConfig *ExecConfigNew(bool scheduled_run, const EvalContext *ctx, const Policy *policy)
{
    ExecConfig *exec_config = xcalloc(1, sizeof(ExecConfig));

    exec_config->scheduled_run = scheduled_run;
    exec_config->exec_command = xstrdup("");
    exec_config->agent_expireafter = 2 * 60;                   /* two hours */

    exec_config->mail_server = xstrdup("");
    exec_config->mail_from_address = xstrdup("");
    exec_config->mail_to_address = xstrdup("");
    exec_config->mail_subject = xstrdup("");
    exec_config->mail_max_lines = 30;
    exec_config->mailfilter_include = SeqNew(0, &free);
    exec_config->mailfilter_include_regex = SeqNew(0, &RegexFree);
    exec_config->mailfilter_exclude = SeqNew(0, &free);
    exec_config->mailfilter_exclude_regex = SeqNew(0, &RegexFree);

    exec_config->fq_name = xstrdup(VFQNAME);
    exec_config->ip_address = xstrdup(VIPADDRESS);
    exec_config->ip_addresses = GetIpAddresses(ctx);

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

            if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILFROM].lval) == 0)
            {
                free(exec_config->mail_from_address);
                exec_config->mail_from_address = xstrdup(value);
                Log(LOG_LEVEL_DEBUG, "mailfrom '%s'", exec_config->mail_from_address);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILTO].lval) == 0)
            {
                free(exec_config->mail_to_address);
                exec_config->mail_to_address = xstrdup(value);
                Log(LOG_LEVEL_DEBUG, "mailto '%s'", exec_config->mail_to_address);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILSUBJECT].lval) == 0)
            {
                free(exec_config->mail_subject);
                exec_config->mail_subject = xstrdup(value);
                Log(LOG_LEVEL_DEBUG, "mailsubject '%s'", exec_config->mail_subject);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_SMTPSERVER].lval) == 0)
            {
                free(exec_config->mail_server);
                exec_config->mail_server = xstrdup(value);
                Log(LOG_LEVEL_DEBUG, "smtpserver '%s'", exec_config->mail_server);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_EXECCOMMAND].lval) == 0)
            {
                free(exec_config->exec_command);
                exec_config->exec_command = xstrdup(value);
                Log(LOG_LEVEL_DEBUG, "exec_command '%s'", exec_config->exec_command);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_AGENT_EXPIREAFTER].lval) == 0)
            {
                exec_config->agent_expireafter = IntFromString(value);
                Log(LOG_LEVEL_DEBUG, "agent_expireafter %d", exec_config->agent_expireafter);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILMAXLINES].lval) == 0)
            {
                exec_config->mail_max_lines = IntFromString(value);
                Log(LOG_LEVEL_DEBUG, "maxlines %d", exec_config->mail_max_lines);
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILFILTER_INCLUDE].lval) == 0)
            {
                SeqDestroy(exec_config->mailfilter_include);
                SeqDestroy(exec_config->mailfilter_include_regex);
                RlistMailFilterFill(value, &exec_config->mailfilter_include,
                                    &exec_config->mailfilter_include_regex, "include");
            }
            else if (strcmp(cp->lval, CFEX_CONTROLBODY[EXEC_CONTROL_MAILFILTER_EXCLUDE].lval) == 0)
            {
                SeqDestroy(exec_config->mailfilter_exclude);
                SeqDestroy(exec_config->mailfilter_exclude_regex);
                RlistMailFilterFill(value, &exec_config->mailfilter_exclude,
                                    &exec_config->mailfilter_exclude_regex, "exclude");
            }
        }
    }

    return exec_config;
}

ExecConfig *ExecConfigCopy(const ExecConfig *config)
{
    ExecConfig *copy = xcalloc(1, sizeof(ExecConfig));

    copy->scheduled_run = config->scheduled_run;
    copy->exec_command = xstrdup(config->exec_command);
    copy->agent_expireafter = config->agent_expireafter;
    copy->mail_server = xstrdup(config->mail_server);
    copy->mail_from_address = xstrdup(config->mail_from_address);
    copy->mail_to_address = xstrdup(config->mail_to_address);
    copy->mail_subject = xstrdup(config->mail_subject);
    copy->mail_max_lines = config->mail_max_lines;
    SeqMailFilterFill(config->mailfilter_include, &copy->mailfilter_include,
                      &copy->mailfilter_include_regex, "include");
    SeqMailFilterFill(config->mailfilter_exclude, &copy->mailfilter_exclude,
                      &copy->mailfilter_exclude_regex, "exclude");
    copy->fq_name = xstrdup(config->fq_name);
    copy->ip_address = xstrdup(config->ip_address);
    copy->ip_addresses = xstrdup(config->ip_addresses);

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
        free(exec_config->mail_subject);
        SeqDestroy(exec_config->mailfilter_include);
        SeqDestroy(exec_config->mailfilter_exclude);
        SeqDestroy(exec_config->mailfilter_include_regex);
        SeqDestroy(exec_config->mailfilter_exclude_regex);
        free(exec_config->fq_name);
        free(exec_config->ip_address);
        free(exec_config->ip_addresses);

        free(exec_config);
    }
}
