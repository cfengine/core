#ifndef CFENGINE_ATEXIT_H
#define CFENGINE_ATEXIT_H

typedef void (*AtExitFn)(void);

void RegisterAtExitFunction(AtExitFn fn);

#endif
