/*
   Copyright (C) CFEngine AS

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

#include <pipes.h>
#include <rlist.h>
#include <buffer.h>
#include <signals.h>
#include <string_lib.h>

bool PipeTypeIsOk(const char *type)
{
    if (type[0] != 'r' && type[0] != 'w')
    {
        return false;
    }
    else if (type[1] != 't' && type[1] != '+')
    {
        if (type[1] == '\0')
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (type[2] == '\0' || type[2] == 't')
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*******************************************************************/
/* Pipe read/write interface, originally in package modules        */
/*******************************************************************/

int PipeIsReadWriteReady(const IOData *io, int timeout_sec)
{
    fd_set  rset;
    FD_ZERO(&rset);
    FD_SET(io->read_fd, &rset);

    struct timeval tv = {
        .tv_sec = timeout_sec,
        .tv_usec = 0,
    };

    //TODO: For Windows we will need different method and select might not
    //      work with file descriptors.
    int ret = select(io->read_fd + 1, &rset, NULL, NULL, &tv);

    if (ret < 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Failed checking for data. (select: %s)",
            GetErrorStr());
        return -1;
    }
    else if (FD_ISSET(io->read_fd, &rset))
    {
        return io->read_fd;
    }

    /* We have reached timeout */
    if (ret == 0)
    {
        Log(LOG_LEVEL_DEBUG, "Timeout reading from application pipe.");
        return 0;
    }

    Log(LOG_LEVEL_VERBOSE,
        "Unknown outcome (ret > 0 but our only fd is not set).");

    return -1;
}

Rlist *PipeReadData(const IOData *io, int pipe_timeout_secs, int pipe_termination_check_secs)
{
    char buff[CF_BUFSIZE] = {0};

    Buffer *data = BufferNew();
    if (!data)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Unable to allocate buffer for handling pipe responses.");
        return NULL;
    }

    int timeout_seconds_left = pipe_timeout_secs;

    while (!IsPendingTermination() && timeout_seconds_left > 0)
    {
        int fd = PipeIsReadWriteReady(io, pipe_termination_check_secs);

        if (fd < 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Error reading data from application pipe");
            return NULL;
        }
        else if (fd == io->read_fd)
        {
            ssize_t res = read(fd, buff, sizeof(buff) - 1);
            if (res == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    Log(LOG_LEVEL_ERR,
                        "Unable to read output from application pipe: %s",
                        GetErrorStr());
                    BufferDestroy(data);
                    return NULL;
                }
            }
            else if (res == 0) /* reached EOF */
            {
                break;
            }
            Log(LOG_LEVEL_DEBUG, "Data read from application pipe: %zu [%s]",
                res, buff);

            BufferAppendString(data, buff);
            memset(buff, 0, sizeof(buff));
        }
        else if (fd == 0) /* timeout */
        {
            timeout_seconds_left -= pipe_termination_check_secs;
            continue;
        }
    }

    char *read_string = BufferClose(data);
    Rlist *response_lines = RlistFromSplitString(read_string, '\n');
    free(read_string);

    return response_lines;
}

int PipeWrite(IOData *io, const char *data)
{
    /* If there is nothing to write close writing end of pipe. */
    if (data == NULL || strlen(data) == 0)
    {
        if (io->write_fd >= 0)
        {
            cf_pclose_full_duplex_side(io->write_fd);
            io->write_fd = -1;
        }
        return 0;
    }

    ssize_t wrt = write(io->write_fd, data, strlen(data));

    /* Make sure to close write_fd after sending all data. */
    if (io->write_fd >= 0)
    {
        cf_pclose_full_duplex_side(io->write_fd);
        io->write_fd = -1;
    }
    return wrt;
}

int PipeWriteData(const char *base_cmd, const char *args, const char *data)
{
    assert(base_cmd);
    assert(args);

    char *command = StringFormat("%s %s", base_cmd, args);
    IOData io = cf_popen_full_duplex(command, false, true);
    free(command);

    if (io.write_fd == -1 || io.read_fd == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Error occurred while opening pipes for "
            "communication with application '%s'.", base_cmd);
        return -1;
    }

    Log(LOG_LEVEL_DEBUG, "Opened fds %d and %d for command '%s'.",
        io.read_fd, io.write_fd, args);

    int res = 0;
    if (PipeWrite(&io, data) != strlen(data))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Was not able to send whole data to application '%s'.",
            base_cmd);
        res = -1;
    }

    /* If script returns non 0 status */
    int close = cf_pclose_full_duplex(&io);
    if (close != EXIT_SUCCESS)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Application '%s' returned with non zero return code: %d",
            base_cmd, close);
        res = -1;
    }
    return res;
}

/* In some cases the response is expected to be not filled out. Some requests
   will have response filled only in case of errors. */
int PipeReadWriteData(const char *base_cmd, const char *args, const char *request,
                             Rlist **response, int pipe_timeout_secs, int pipe_termination_check_secs)
{
    assert(base_cmd);
    assert(args);

    char *command = StringFormat("%s %s", base_cmd, args);
    IOData io = cf_popen_full_duplex(command, false, true);

    if (io.write_fd == -1 || io.read_fd == -1)
    {
        Log(LOG_LEVEL_INFO, "Some error occurred while communicating with %s", command);
        free(command);
        return -1;
    }

    Log(LOG_LEVEL_DEBUG, "Opened fds %d and %d for command '%s'.",
        io.read_fd, io.write_fd, command);

    if (PipeWrite(&io, request) != strlen(request))
    {
        Log(LOG_LEVEL_VERBOSE, "Couldn't send whole data to application '%s'.",
            base_cmd);
        free(command);
        return -1;
    }

    /* We can have some error message here. */
    Rlist *res = PipeReadData(&io, pipe_timeout_secs, pipe_termination_check_secs);

    /* If script returns non 0 status */
    int close = cf_pclose_full_duplex(&io);
    if (close != EXIT_SUCCESS)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Command '%s' returned with non zero return code: %d",
            command, close);
        free(command);
        RlistDestroy(res);
        return -1;
    }

    free(command);
    *response = res;
    return 0;
}
