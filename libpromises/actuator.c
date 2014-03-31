#include <actuator.h>

#include <misc_lib.h>

PromiseResult PromiseResultUpdate(PromiseResult prior, PromiseResult evidence)
{
    switch (prior)
    {
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_INTERRUPTED:
    case PROMISE_RESULT_TIMEOUT:
        return prior;

    case PROMISE_RESULT_WARN:
        switch (evidence)
        {
        case PROMISE_RESULT_DENIED:
        case PROMISE_RESULT_FAIL:
        case PROMISE_RESULT_INTERRUPTED:
        case PROMISE_RESULT_TIMEOUT:
        case PROMISE_RESULT_WARN:
            return evidence;

        case PROMISE_RESULT_CHANGE:
        case PROMISE_RESULT_NOOP:
        case PROMISE_RESULT_SKIPPED:
            return prior;
        }

    case PROMISE_RESULT_SKIPPED:
        return evidence;

    case PROMISE_RESULT_NOOP:
        switch (evidence)
        {
        case PROMISE_RESULT_SKIPPED:
            return prior;

        default:
            return evidence;
        }

    case PROMISE_RESULT_CHANGE:
        switch (evidence)
        {
        case PROMISE_RESULT_DENIED:
        case PROMISE_RESULT_FAIL:
        case PROMISE_RESULT_INTERRUPTED:
        case PROMISE_RESULT_TIMEOUT:
        case PROMISE_RESULT_WARN:
            return evidence;

        case PROMISE_RESULT_CHANGE:
        case PROMISE_RESULT_NOOP:
        case PROMISE_RESULT_SKIPPED:
            return prior;
        }
    }

    ProgrammingError("Never reach");
}

bool PromiseResultIsOK(PromiseResult result)
{
    return (result == PROMISE_RESULT_CHANGE) || (result == PROMISE_RESULT_NOOP);
}
