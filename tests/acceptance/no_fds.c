/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <logging.h>
#include <platform.h>

static void usage(void)
{
    printf("Runs the given command without inheriting any file descriptors.\n"
           "Usage: no_fds [options]\n"
           "Options:\n"
           "      --no-std\n"
           "        Even stdin/stdout/stderr will not be inherited.\n"
           "  -h, --help\n"
           "        This help screen.\n");
}

static void close_on_exec(int fd)
{
#ifdef _WIN32
    SetHandleInformation((HANDLE)_get_osfhandle(fd), HANDLE_FLAG_INHERIT, 0);
#else
    long flags = fcntl(fd, F_GETFD);
    flags |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, flags);
#endif
}

int main(int argc, char **argv)
{
    bool std = true;
    int startarg = 1;

    for (int arg = 1; arg < argc; arg++)
    {
        if (strcmp(argv[arg], "--no-std") == 0)
        {
            std = false;
            startarg++;
        }
        else if (strcmp(argv[arg], "-h") == 0 || strcmp(argv[arg], "--help") == 0)
        {
            usage();
            return 0;
        }
        else if (argv[arg][0] == '-')
        {
            fprintf(stderr, "Unknown option: %s\n", argv[arg]);
            usage();
            return 1;
        }
        else
        {
            break;
        }
    }

    closefrom(3);

    if (!std)
    {
        for (int fd = 0; fd < 3; fd++)
        {
            close_on_exec(fd);
        }
    }

    char *new_args[argc - startarg + 1];
    for (int i = startarg; i < argc; i++)
    {
        new_args[i - startarg] = argv[i];
    }
    new_args[argc - startarg] = NULL;

    execvp(new_args[0], new_args);

    fprintf(stderr, "Could not execute command '%s': %s\n", new_args[0], GetErrorStr());

    return 1;
}
