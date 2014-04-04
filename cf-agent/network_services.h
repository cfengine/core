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
int QueryOSPFServiceState(EvalContext *ctx, CommonOSPF *ospfp);
int QueryOSPFInterfaceState(EvalContext *ctx, const Attributes *a, const Promise *pp, LinkStateOSPF *ospfp);
CommonOSPF *NewOSPFState(void);
void DeleteOSPFState(CommonOSPF *state);
void KeepOSPFLinkServiceControlPromises(CommonOSPF *policy, CommonOSPF *state);
