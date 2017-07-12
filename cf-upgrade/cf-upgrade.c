/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <platform.h>

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <update.h>
#include <alloc-mini.h>
#include <command_line.h>
#include <configuration.h>
#include <log.h>

#define CF_UPGRADE_VERSION          "1.0.0"
void usage()
{
    puts("Usage: cf-upgrade [-c copy] <-b backup tool> <-s backup path> "
         "[-f CFEngine Folder] <-i command + arguments>");
    puts("Usage: cf-upgrade -h: help");
    puts("Usage: cf-upgrade -v: version");
    puts("More detailed help can be found in the accompanying README.md file.");
}

int main(int argc, char **argv)
{
    int result = 0;
    Configuration *configuration = NULL;

    logInit();
    log_entry(LogVerbose, "Starting %s", argv[0]);

    result = parse(argc, argv, &configuration);
    if (result < 0)
    {
        usage();
        return 1;
    }
    if (ConfigurationVersion(configuration))
    {
        char version[] = CF_UPGRADE_VERSION;
        printf("cf-upgrade %s\n", version);
        return 0;
    }
    if (ConfigurationHelp(configuration))
    {
        usage();
        return 0;
    }

    result = RunUpdate(configuration);

    log_entry(LogVerbose, "Finished %s", argv[0]);
    logFinish();

    return (result == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
