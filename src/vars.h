#ifndef CFENGINE_VARS_H
#define CFENGINE_VARS_H

#include "cf3.defs.h"

void LoadSystemConstants(void);
void ForceScalar(char *lval, char *rval);
void NewScalar(const char *scope, const char *lval, const char *rval, enum cfdatatype dt);
void DeleteScalar(const char *scope, const char *lval);
void NewList(const char *scope, const char *lval, void *rval, enum cfdatatype dt);

#endif
