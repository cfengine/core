#ifndef CFENGINE_FEATURE_H
#define CFENGINE_FEATURE_H


int KnownFeature(const char *feature);

void CreateHardClassesFromFeatures(EvalContext *ctx, char *tags);

#endif
