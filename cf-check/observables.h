#ifndef CF_CHECK_TS_KEY_READ_H
#define CF_CHECK_TS_KEY_READ_H

#include <sequence.h>

// copy of libpromises/cf3.defs.h, TODO refactor
#define CF_OBSERVABLES 300

char **GetObservableNames(const char *ts_key_path);

#endif
