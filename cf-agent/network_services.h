/*****************************************************************************/
/*                                                                           */
/* File: network_services.h                                                  */
/*                                                                           */
/* Created: Tue Apr  1 14:08:20 2014                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

void InitializeOSPF(const Policy *policy, EvalContext *ctx);
bool HaveOSPFService(EvalContext *ctx);
