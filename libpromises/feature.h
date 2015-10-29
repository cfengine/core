#ifndef CFENGINE_SYNTAX_H
#define CFENGINE_SYNTAX_H


int KnownFeature(const char *feature);

void CreateHardClassesFromFeatures(EvalContext *ctx, char *tags);

#endif

