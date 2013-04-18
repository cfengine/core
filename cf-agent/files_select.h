#ifndef CFENGINE_FILES_SELECT_H
#define CFENGINE_FILES_SELECT_H

#include "cf3.defs.h"

int SelectLeaf(char *path, struct stat *sb, FileSelect fs);

#endif
