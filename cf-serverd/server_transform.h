#ifndef CFENGINE_SERVER_TRANSFORM_H
#define CFENGINE_SERVER_TRANSFORM_H

#include "cf3.defs.h"
#include "server.h"

Auth *GetAuthPath(char *path, Auth *list);
void Summarize(void);

#endif
