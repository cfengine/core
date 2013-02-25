#ifndef CFENGINE_PATH_LIB_H
#define CFENGINE_PATH_LIB_H

#include "platform.h"

typedef struct Path_ Path;

Path *PathNew(void);
void PathDestroy(Path *path);

Path *PathFromString(const char *path);
char *PathToString(const Path *path);

bool PathIsAbsolute(const Path *path);

#endif
