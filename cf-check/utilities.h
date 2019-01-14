#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <sequence.h>

// These functions should be moved to libutils once cf-check is
// implemented and backported.

Seq *argv_to_seq(int argc, char **argv);
Seq *default_lmdb_files();
Seq *argv_to_lmdb_files(int argc, char **argv);

#endif
