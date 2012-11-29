#ifndef CFENGINE_ARGS_H
#define CFENGINE_ARGS_H

#include "cf3.defs.h"

#include "rlist.h"

int MapBodyArgs(const char *scopeid, Rlist *give, const Rlist *take);
Rlist *NewExpArgs(const FnCall *fp, const Promise *pp);
void ArgTemplate(FnCall *fp, const FnCallArg *argtemplate, Rlist *finalargs);
void DeleteExpArgs(Rlist *args);

#endif
