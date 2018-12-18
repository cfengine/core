#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <sequence.h>

// These functions should be moved to libutils once cf-check is
// implemented and backported.

bool copy_file(const char *src, const char *dst);
const char *filename_part(const char *path);
bool copy_file_to_folder(const char *src, const char *dst_dir);
Seq *ls(const char *dir, const char *extension);
char *join_paths_alloc(const char *dir, const char *leaf);
Seq *argv_to_seq(int argc, char **argv);
Seq *default_lmdb_files();
Seq *argv_to_lmdb_files(int argc, char **argv);

#endif
