#include "cf3.defs.h"
#include "atexit.h"

typedef struct AtExitList
{
    AtExitFn fn;
    struct AtExitList *next;
} AtExitList;

static pthread_once_t register_atexit_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t atexit_functions_mutex = PTHREAD_MUTEX_INITIALIZER;
static AtExitList *atexit_functions;

/* To be called only by Windows service implementation */

void CallAtExitFunctions(void)
{
    pthread_mutex_lock(&atexit_functions_mutex);

    AtExitList *p = atexit_functions;
    while (p)
    {
        AtExitList *cur = p;
        (cur->fn)();
        p = cur->next;
        free(cur);
    }

    atexit_functions = NULL;

    pthread_mutex_unlock(&atexit_functions_mutex);
}

static void RegisterAtExitHandler(void)
{
    atexit(&CallAtExitFunctions);
}

void RegisterAtExitFunction(AtExitFn fn)
{
    pthread_once(&register_atexit_once, &RegisterAtExitHandler);

    pthread_mutex_lock(&atexit_functions_mutex);

    AtExitList *p = xmalloc(sizeof(AtExitList));
    p->fn = fn;
    p->next = atexit_functions;

    atexit_functions = p;

    pthread_mutex_unlock(&atexit_functions_mutex);
}
