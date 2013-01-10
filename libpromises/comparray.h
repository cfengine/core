#ifndef CFENGINE_COMPARRAY_H
#define CFENGINE_COMPARRAY_H

#include "cf3.defs.h"

int FixCompressedArrayValue(int i, char *value, CompressedArray **start);
void DeleteCompressedArray(CompressedArray *start);
int CompressedArrayElementExists(CompressedArray *start, int key);
char *CompressedArrayValue(CompressedArray *start, int key);

#endif
