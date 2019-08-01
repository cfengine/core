#ifndef __BACKUP_H__
#define __BACKUP_H__

#include <sequence.h>

int backup_files(Seq *filenames);
int backup_main(int argc, const char *const *argv);

#endif
