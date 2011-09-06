#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>

#include "cf3.defs.h"
#include "cf3.extern.h"

/*
 * FIXME: dependencies we want to cut later
 */
int DEBUG=0;
int D1=0;
int D2=0;

int FuzzySetMatch(char *s1, char *s2)
{
return 0;
}

void DeleteScope(char *name)
{
}

void NewScope(char *name)
{
}

void ForceScalar(char *lval, char *rval)
{
}

bool IsExcluded(const char *exception)
{
return false;
}

void PromiseRef(enum cfreport level, struct Promise *pp)
{
}

enum insert_match String2InsertMatch(char *s)
{
return 0;
}

/*
 * End of deps
 */

int main()
{
}
