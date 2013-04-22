#ifndef CFENGINE_TOKYO_CHECK_H
#define CFENGINE_TOKYO_CHECK_H

#ifdef HAVE_CONFIG_H
#include  <config.h>
#endif

#ifdef TCDB
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <tcutil.h>
#include <tchdb.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "sglib.h"
#include "logging.h"
#include "cf3.defs.h"
#endif

int CheckTokyoDBCoherence( char *path );

#endif
