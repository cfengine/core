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

#include <exec_tools.h>

#include <files_names.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <generic_agent.h> // CloseLog

/********************************************************************/

bool GetExecOutput(const char *command, char **buffer, size_t *buffer_size, ShellType shell)
/* Buffer initially contains whole exec string */
{
    FILE *pp;

    if (shell == SHELL_TYPE_USE)
    {
        pp = cf_popen_sh(command, "rt");
    }
    else if (shell == SHELL_TYPE_POWERSHELL)
    {
#ifdef __MINGW32__
        pp = cf_popen_powershell(command, "rt");
#else // !__MINGW32__
        Log(LOG_LEVEL_ERR, "Powershell is only supported on Windows");
        return false;
#endif // __MINGW32__
    }
    else
    {
        pp = cf_popen(command, "rt", true);
    }

    if (pp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe to command '%s'. (cf_popen: %s)", command, GetErrorStr());
        return false;
    }

    size_t offset = 0;
    size_t line_size = CF_EXPANDSIZE;
    size_t attempted_size = 0;
    char *line = xcalloc(1, line_size);

    while (*buffer_size < CF_MAXSIZE)
    {
        ssize_t res = CfReadLine(&line, &line_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                Log(LOG_LEVEL_ERR, "Unable to read output of command '%s'. (fread: %s)", command, GetErrorStr());
                cf_pclose(pp);
                free(line);
                return false;
            }
            else
            {
                break;
            }
        }

        if ((attempted_size = snprintf(*buffer + offset, *buffer_size - offset, "%s\n", line)) >= *buffer_size - offset)
        {
            *buffer_size += (attempted_size > CF_EXPANDSIZE ? attempted_size : CF_EXPANDSIZE);
            *buffer = xrealloc(*buffer, *buffer_size);
            snprintf(*buffer + offset, *buffer_size - offset, "%s\n", line);
        }

        offset += strlen(line) + 1;
    }

    if (offset > 0)
    {
        if (Chop(*buffer, *buffer_size) == -1)
        {
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }
    }

    Log(LOG_LEVEL_DEBUG, "GetExecOutput got '%s'", *buffer);

    cf_pclose(pp);
    free(line);
    return true;
}

/**********************************************************************/

void ActAsDaemon()
{
    int fd;

#ifdef HAVE_SETSID
    setsid();
#endif

    CloseNetwork();
    CloseLog();

    fflush(NULL);
    fd = open(NULLFILE, O_RDWR, 0);

    if (fd != -1)
    {
        if (dup2(fd, STDIN_FILENO) == -1)
        {
            Log(LOG_LEVEL_ERR, "Could not dup. (dup2: %s)", GetErrorStr());
        }

        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            Log(LOG_LEVEL_ERR, "Could not dup. (dup2: %s)", GetErrorStr());
        }

        dup2(fd, STDERR_FILENO);

        if (fd > STDERR_FILENO)
        {
            close(fd);
        }
    }

    if (chdir("/"))
    {
        UnexpectedError("Failed to chdir into '/'");
    }
}

/**********************************************************************/

#define INITIAL_ARGS 8

char **ArgSplitCommand(const char *comm)
{
    const char *s = comm;

    int argc = 0;
    int argslen = INITIAL_ARGS;
    char **args = xmalloc(argslen * sizeof(char *));

    while (*s != '\0')
    {
        const char *end;
        char *arg;

        if (isspace((int)*s))        /* Skip whitespace */
        {
            s++;
            continue;
        }

        switch (*s)
        {
        case '"':              /* Look for matching quote */
        case '\'':
        case '`':
        {
            char delim = *s++;  /* Skip first delimeter */

            end = strchr(s, delim);
            break;
        }
        default:               /* Look for whitespace */
            end = strpbrk(s, " \f\n\r\t\v");
            break;
        }

        if (end == NULL)        /* Delimeter was not found, remaining string is the argument */
        {
            arg = xstrdup(s);
            s += strlen(arg);
        }
        else
        {
            arg = xstrndup(s, end - s);
            s = end;
            if ((*s == '"') || (*s == '\'') || (*s == '`'))   /* Skip second delimeter */
                s++;
        }

        /* Argument */

        if (argc == argslen)
        {
            argslen *= 2;
            args = xrealloc(args, argslen * sizeof(char *));
        }

        args[argc++] = arg;
    }

/* Trailing NULL */

    if (argc == argslen)
    {
        argslen += 1;
        args = xrealloc(args, argslen * sizeof(char *));
    }
    args[argc++] = NULL;

    return args;
}

/**********************************************************************/

void ArgFree(char **args)
{
    char **arg = args;

    for (; *arg; ++arg)
    {
        free(*arg);
    }
    free(args);
}
