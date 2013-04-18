#ifndef CFENGINE_SERVER_TRANSFORM_H
#define CFENGINE_SERVER_TRANSFORM_H

#include "cf3.defs.h"
#include "server.h"

void Summarize(void);
void KeepPromises(EvalContext *ctx, Policy *policy, GenericAgentConfig *config);

#endif
