#ifndef __UTILITIES_H__
#define __UTILITIES_H__

// These functions should be moved to libutils once cf-check is
// implemented and backported.

bool copy_file(const char *src, const char *dst);
const char *filename_part(const char *path);
bool copy_file_to_folder(const char *src, const char *dst_dir);

#endif
