#ifndef CFENGINE_EVAL_H
#define CFENGINE_EVAL_H

#include <actuator.h>

/**
 * @brief Evaluate a bundle with an actuator function
 *
 * @param ctx Evaluation context
 * @param bp Bundle to evaluate
 * @param args Arguments to bundle
 * @param inherit_previous Whether to inherit variables from a caller bundle (e.g. for edit_line bundles)
 * @param pass At which pass this bundles is being evaluated (for logging)
 * @param actuator Which actuator function should be applied to each evaluated promise iteration
 * @param actuator_param Arbitrary user data argument given to the actuator function
 * @param promise_type_sequence Ordering of promise types to evaluate in the bundle. NULL-terminated.
 *
 * @return True if the bundle was evaluated successfully, false if it was subject to bundle abortion
 */
bool EvalBundle(EvalContext *ctx, const Bundle *bp, const Rlist *args, bool inherit_previous, size_t pass,
                PromiseActuator *actuator, void *actuator_param, const char *const *promise_type_sequence);

#endif
