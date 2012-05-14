#include "policy.h"

#include <assert.h>

Policy *PolicyNew(void)
{
    Policy *policy = xcalloc(1, sizeof(Policy));
    return policy;
}

void PolicyDestroy(Policy *policy)
{
    if (policy)
    {
        DeleteBundles(policy->bundles);
        DeleteBodies(policy->bodies);
        free(policy);
    }
}

Policy *PolicyFromPromise(const Promise *promise)
{
    assert(promise);

    SubType *subtype = promise->parent_subtype;
    assert(subtype);

    Bundle *bundle = subtype->parent_bundle;
    assert(bundle);

    return bundle->parent_policy;
}
