#ifndef __REPAIR_H__
#define __REPAIR_H__

#define REPAIR_FILE_EXTENSION ".copy"

int repair_main(int argc, const char *const *argv);
int repair_lmdb_default();
int repair_lmdb_file(const char *file);

#endif
