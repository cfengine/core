#ifndef __DIAGNOSE_H__
#define __DIAGNOSE_H__

#include <sequence.h>

size_t diagnose_files(Seq *filenames, Seq **corrupt);
int diagnose_main(int argc, const char *const *argv);

#endif
