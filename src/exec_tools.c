/* 

   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "cf3.defs.h"

#include "files_names.h"
#include "files_interfaces.h"
#include "cfstream.h"
#include "pipes.h"

/********************************************************************/

int GetExecOutput(char *command, char *buffer, int useshell)
/* Buffer initially contains whole exec string */
{
    int offset = 0;
    char line[CF_EXPANDSIZE], *sp;
    FILE *pp;
    int flatten_newlines = false;

    CfDebug("GetExecOutput(%s,%s) - use shell = %d\n", command, buffer, useshell);

    if (useshell)
    {
        pp = cf_popen_sh(command, "r");
    }
    else
    {
        pp = cf_popen(command, "r");
    }

    if (pp == NULL)
    {
        CfOut(cf_error, "cf_popen", "Couldn't open pipe to command %s\n", command);
        return false;
    }

    memset(buffer, 0, CF_EXPANDSIZE);

    while (!feof(pp))
    {
        if (ferror(pp))         /* abortable */
        {
            fflush(pp);
            break;
        }

        CfReadLine(line, CF_EXPANDSIZE, pp);

        if (ferror(pp))         /* abortable */
        {
            fflush(pp);
            break;
        }

        if (flatten_newlines)
        {
            for (sp = line; *sp != '\0'; sp++)
            {
                if (*sp == '\n')
                {
                    *sp = ' ';
                }
            }
        }

        if (strlen(line) + offset > CF_EXPANDSIZE - 10)
        {
            CfOut(cf_error, "", "Buffer exceeded %d bytes in exec %s\n", CF_EXPANDSIZE, command);
            break;
        }

        if (flatten_newlines)
        {
            snprintf(buffer + offset, CF_EXPANDSIZE, "%s ", line);
        }
        else
        {
            snprintf(buffer + offset, CF_EXPANDSIZE, "%s\n", line);
        }

        offset += strlen(line) + 1;
    }

    if (offset > 0)
    {
        Chop(buffer);
    }

    CfDebug("GetExecOutput got: [%s]\n", buffer);

    cf_pclose(pp);
    return true;
}

/**********************************************************************/

void ActAsDaemon(int preserve)
{
    int fd, maxfd;

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
            CfOut(cf_error, "dup2", "Could not dup");
        }

        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            CfOut(cf_error, "dup2", "Could not dup");
        }

        dup2(fd, STDERR_FILENO);

        if (fd > STDERR_FILENO)
        {
            close(fd);
        }
    }

    chdir("/");

#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
# ifdef _POXIX_OPEN_MAX
    maxfd = _POSIX_OPEN_MAX;
# else
    maxfd = 1024;
# endif
#endif

    for (fd = STDERR_FILENO + 1; fd < maxfd; ++fd)
    {
        if (fd != preserve)
        {
            close(fd);
        }
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
