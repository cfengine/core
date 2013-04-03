#ifndef CFENGINE_EXEC_CONFIG_H
#define CFENGINE_EXEC_CONFIG_H

#include "cf3.defs.h"
#include "set.h"

typedef struct
{
    bool scheduled_run;
    char *exec_command;
    int agent_expireafter;

    char *mail_server;
    char *mail_from_address;
    char *mail_to_address;
    int mail_max_lines;

    StringSet *schedule;
    int splay_time;
    char *log_facility;

    /*
     * Host information.
     * Might change during policy reload, so copy is retained in each worker.
     */
    char *fq_name;
    char *ip_address;
} ExecConfig;

ExecConfig *ExecConfigNewDefault(bool scheduled_run, const char *fq_name, const char *ip_address);
ExecConfig *ExecConfigCopy(const ExecConfig *exec_config);
void ExecConfigDestroy(ExecConfig *exec_config);

void ExecConfigUpdate(const EvalContext *ctx, const Policy *policy, ExecConfig *exec_config);

#endif
