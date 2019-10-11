#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <sequence.h>

// These functions should be moved to libutils once cf-check is
// implemented and backported.

Seq *argv_to_seq(int argc, const char *const *argv);
Seq *default_lmdb_files();

Seq *argv_to_lmdb_files(int count, const char *const *files, size_t offset);
bool matches_option(
    const char *const supplied,
    const char *const longopt,
    const char *const shortopt);

#endif
