#ifndef __REPAIR_H__
#define __REPAIR_H__

#define REPAIR_FILE_EXTENSION ".copy"

int repair_main(int argc, const char *const *argv);
int repair_lmdb_default(bool force);
int repair_lmdb_file(const char *file, int fd_tstamp);

#endif
