/*
   Copyright 2018 Northern.tech AS

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

#include <exec_tools.h>

#include <files_names.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <file_lib.h>
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
    if (setsid() == (pid_t) -1)
    {
        Log(LOG_LEVEL_WARNING,
            "Failed to become a session leader while daemonising (setsid: %s)",
            GetErrorStr());
    }
#endif

    CloseNetwork();
    CloseLog();

    fflush(NULL);

    /* Close descriptors 0,1,2 and reopen them with /dev/null. */

    fd = open(NULLFILE, O_RDWR, 0);
    if (fd == -1)
    {
        Log(LOG_LEVEL_WARNING, "Could not open '%s', "
            "stdin/stdout/stderr are still open (open: %s)",
            NULLFILE, GetErrorStr());
    }
    else
    {
        if (dup2(fd, STDIN_FILENO) == -1)
        {
            Log(LOG_LEVEL_WARNING,
                "Could not close stdin while daemonising (dup2: %s)",
                GetErrorStr());
        }

        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            Log(LOG_LEVEL_WARNING,
                "Could not close stdout while daemonising (dup2: %s)",
                GetErrorStr());
        }

        if (dup2(fd, STDERR_FILENO) == -1)
        {
            Log(LOG_LEVEL_WARNING,
                "Could not close stderr while daemonising (dup2: %s)",
                GetErrorStr());
        }

        if (fd > STDERR_FILENO)
        {
            close(fd);
        }
    }

    if (chdir("/"))
    {
        Log(LOG_LEVEL_WARNING,
            "Failed to chdir into '/' directory while daemonising (chdir: %s)",
            GetErrorStr());
    }
}

/**********************************************************************/

/**
 * Split the command string like "/bin/echo -n Hi!" in two parts -- the
 * executable ("/bin/echo") and the arguments for the executable ("-n Hi!").
 *
 * @param[in]  comm  the whole command to split
 * @param[out] exec  pointer to **a newly allocated** string with the executable
 * @param[out] args  pointer to **a newly allocated** string with the args
 *
 * @note Whitespace between the executable and the arguments is skipped.
 */
void ArgGetExecutableAndArgs(const char *comm, char **exec, char **args)
{
    const char *s = comm;
    while (*s != '\0')
    {
        const char *end = NULL;

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
            char delim = *(s++);  /* Skip first delimeter */

            end = strchr(s, delim);
            break;
        }
        default:               /* Look for whitespace */
            end = strpbrk(s, " \f\n\r\t\v");
            break;
        }

        if (end == NULL)        /* Delimeter was not found, remaining string is the executable */
        {
            *exec = xstrdup(s);
            *args = NULL;
            return;
        }
        else
        {
            assert(end > s);
            const size_t length = end - s;
            *exec = xstrndup(s, length);

            const char *args_start = end;
            if (*(args_start + 1) != '\0')
            {
                args_start++; /* Skip second delimeter */
                args_start += strspn(args_start, " \f\n\r\t\v"); /* Skip whitespace */
                *args = xstrdup(args_start);
            }
            else
            {
                *args = NULL;
            }
            return;
        }
    }

    /* was not able to parse/split the command */
    *exec = NULL;
    *args = NULL;
    return;
}

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
    if (args != NULL)
    {
        for (char **arg = args; *arg; ++arg)
        {
            free(*arg);
        }
        free(args);
    }
}
