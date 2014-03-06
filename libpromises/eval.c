#include <eval.h>

#include <expand.h>
#include <ornaments.h>

bool EvalBundle(EvalContext *ctx, const Bundle *bp, const Rlist *args, bool inherit_previous, size_t pass,
                PromiseActuator *actuator, void *actuator_param, const char *const *promise_type_sequence)
{
    assert(ctx);
    assert(bp);
    assert(actuator);
    assert(promise_type_sequence);

    BannerBundle(bp, args);

    EvalContextStackPushBundleFrame(ctx, bp, args, inherit_previous);
    for (size_t i = 0; promise_type_sequence[i]; i++)
    {
        const PromiseType *pt = BundleGetPromiseType(bp, promise_type_sequence[i]);
        if (!pt || SeqLength(pt->promises) == 0)
        {
            continue;
        }

        BannerPromiseType(bp->name, pt->name, pass);

        EvalContextStackPushPromiseTypeFrame(ctx, pt);
        for (size_t ppi = 0; ppi < SeqLength(pt->promises); ppi++)
        {
            Promise *pp = SeqAt(pt->promises, ppi);
            ExpandPromise(ctx, pp, actuator, actuator_param);
            if (Abort(ctx))
            {
                EvalContextStackPopFrame(ctx);
                EvalContextStackPopFrame(ctx);
                return false;
            }
        }
        EvalContextStackPopFrame(ctx);
    }
    EvalContextStackPopFrame(ctx);

    return true;
}
