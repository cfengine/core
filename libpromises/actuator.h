#ifndef CFENGINE_ACTUATOR_H
#define CFENGINE_ACTUATOR_H

#include <cf3.defs.h>

typedef void PromiseActuator(EvalContext *ctx, Promise *pp, void *param);

PromiseResult PromiseResultUpdate(PromiseResult prior, PromiseResult evidence);
bool PromiseResultIsOK(PromiseResult result);

#endif
