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

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *SPAWN_PROCESS = NULL;
char *SPAWN_PROCESS_ON_SIGNAL = NULL;
int REFUSE_TO_DIE = 0;
char **NEXT_PROCESS_ARGV = NULL;
int NEXT_PROCESS_ARGC = 0;

char *PIDFILE = NULL;

void spawn_process(const char *program)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("Could not fork\n");
        exit(1);
    }
    else if (pid == 0)
    {
        const char * args[NEXT_PROCESS_ARGC + 2]; // One for program and one for NULL.
        args[0] = program;
        for (int c = 1; c <= NEXT_PROCESS_ARGC; c++)
        {
            args[c] = NEXT_PROCESS_ARGV[c-1];
        }
        args[NEXT_PROCESS_ARGC + 1] = NULL;
        execv(program, (char * const *)args);

        printf("Could not execute %s\n", program);
        exit(1);
    }
}

void signal_handler(int signal)
{
    (void)signal;
    // No-op. All the handling is in the main loop
}

void process_signal(void)
{
    if (SPAWN_PROCESS_ON_SIGNAL)
    {
        // Insert artificial delay so that a match for the agent right after
        // attempting to kill the daemon will not work. Trying to make it as
        // difficult as possible for the killing script! :-)
        sleep(1);

        spawn_process(SPAWN_PROCESS_ON_SIGNAL);
    }

    if (!REFUSE_TO_DIE)
    {
        if (PIDFILE)
        {
            unlink(PIDFILE);
        }
        exit(0);
    }
}

int main(int argc, char **argv)
{
    sigset_t mask;
    sigemptyset(&mask);
    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = &signal_handler;
    sig.sa_mask = mask;
    const int signals[] = { SIGTERM, SIGQUIT, SIGINT, SIGHUP, 0 };

    for (int c = 0; signals[c]; c++)
    {
        if (sigaction(signals[c], &sig, NULL) != 0)
        {
            printf("Unable to set signal handlers\n");
            return 1;
        }
    }

    for (int c = 1; c < argc; c++)
    {
        if (strcmp(argv[c], "--spawn-process") == 0)
        {
            if (++c + 1 >= argc)
            {
                printf("%s requires two arguments\n", argv[c]);
                return 1;
            }
            // The reason for splitting the argument into two parts is to avoid
            // a false match on a process just because the it has an argument
            // containing the string we are looking for.
            SPAWN_PROCESS = malloc(strlen(argv[c]) + strlen(argv[c+1]) + 2);
            sprintf(SPAWN_PROCESS, "%s/%s", argv[c], argv[c+1]);

            c++;
        }
        else if (strcmp(argv[c], "--spawn-process-on-signal") == 0)
        {
            if (++c + 1 >= argc)
            {
                printf("%s requires two arguments\n", argv[c]);
                return 1;
            }
            // See comment for SPAWN_PROCESS.
            SPAWN_PROCESS_ON_SIGNAL = malloc(strlen(argv[c]) + strlen(argv[c+1]) + 2);
            sprintf(SPAWN_PROCESS_ON_SIGNAL, "%s/%s", argv[c], argv[c+1]);

            c++;
        }
        else if (strcmp(argv[c], "--refuse-to-die") == 0)
        {
            REFUSE_TO_DIE = 1;
        }
        else if (strcmp(argv[c], "--pass-to-next-process") == 0)
        {
            // Stops argument processing and passes all remaining arguments to
            // the spawned process instead.
            NEXT_PROCESS_ARGV = &argv[++c];
            NEXT_PROCESS_ARGC = argc - c;
            break;
        }
        else
        {
            printf("Unknown argument: %s\n", argv[c]);
            return 1;
        }
    }

    const char *piddir = getenv("CFTEST_PREFIX");
    if (piddir)
    {
        const char *file = strrchr(argv[0], '/');
        file++;
        PIDFILE = malloc(strlen(piddir) + strlen(file) + 6);
        sprintf(PIDFILE, "%s/%s.pid", piddir, file);
    }

    pid_t child = fork();
    if (child < 0)
    {
        printf("Could not fork\n");
        exit(1);
    }
    else if (child > 0)
    {
        // Daemonize.
        if (PIDFILE)
        {
            FILE *fptr = fopen(PIDFILE, "w");
            if (!fptr)
            {
                printf("Could not open file %s\n", PIDFILE);
                exit(1);
            }
            fprintf(fptr, "%d\n", (int)child);
            fclose(fptr);
        }

        return 0;
    }

    if (SPAWN_PROCESS)
    {
        spawn_process(SPAWN_PROCESS);
    }

    // 60 seconds of consecutive sleep will end the program, just so that we do
    // in fact terminate if we are left over.
    while (sleep(60) != 0)
    {
        // If we didn't sleep the whole time, then it must be a signal.
        process_signal();
    }

    return 1;
}
