/*
  Copyright 2023 Northern.tech AS

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

#include <stdio.h>

#include <logging.h>
#include <buffer.h>
#include <json.h>
#include <pipes.h>
#include <alloc.h>
#include <file_lib.h>
#include <string_lib.h>
#include <known_dirs.h>
#include <ip_address.h>

#include <cf-execd-runagent.h>


/**
 * Handle request to run cf-runagent.
 *
 * @note Expected to be called from a forked child process (blocks and forks).
 */
void HandleRunagentRequest(int conn_fd, const char *local_run_command)
{
    FILE *conn_file = fdopen(conn_fd, "r+");
    if (conn_file == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to create file stream from the connection file descriptor: %s",
            GetErrorStr());

        /* SIGH: There's no dprintf() on exotics. */
        char error_buf[256];
        size_t length = snprintf(error_buf, sizeof(error_buf),
                              "{\"error\": \"Failed to create file stream from the connection file descriptor: %s\"}",
                              GetErrorStr());
        assert(length < sizeof(error_buf));
        NDEBUG_UNUSED size_t written = write(conn_fd, error_buf, MIN(sizeof(error_buf), (size_t) length));
        assert(written == MIN(sizeof(error_buf), (size_t) length));
        close(conn_fd);
        return;
    }

    char *request = NULL;
    size_t line_size = 0;
    ssize_t n_read = getline(&request, &line_size, conn_file);
    if (n_read <= 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to read the runagent request: %s",
            (n_read < 0) ? GetErrorStr() : "no data");
        fprintf(conn_file, "{\"error\": \"Failed to read the runagent request: %s\"}",
                (n_read < 0) ? GetErrorStr() : "no data");
        free(request);
        fclose(conn_file);
        return;
    }

    /* Strip the trailing newline (if any). */
    if (request[n_read - 1] == '\n')
    {
        request[n_read - 1] = '\0';
    }

    /* TODO: '--raw' for just getting output from cf-runagent directly? */
    const char *tool = NULL;
    char *command;

    if (StringEqual(request, "localhost") || StringIsLocalHostIP(request))
    {
        tool = "cf-agent";
        command = xstrdup(local_run_command);
    }
    else
    {
        tool = "cf-runagent";
        xasprintf(&command, "%s%ccf-runagent -b -H %s", GetBinDir(), FILE_SEPARATOR, request);
    }
    free(request);

    FILE *run_agent_output = cf_popen(command, "r", true);
    free(command);

    if (run_agent_output == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to start %s and connect to its output", tool);
        fprintf(conn_file, "{\"error\": \"Failed to start %s and connect to its output\"}", tool);
        fclose(conn_file);
        return;
    }

    Buffer *collected_output = BufferNewWithCapacity(1024);
    char output_buffer[CF_BUFSIZE];
    size_t read_bytes = fread(output_buffer, 1, sizeof(output_buffer), run_agent_output);
    while (read_bytes > 0)
    {
        BufferAppend(collected_output, output_buffer, read_bytes);
        read_bytes = fread(output_buffer, 1, sizeof(output_buffer), run_agent_output);
    }
    if (!feof(run_agent_output))
    {
        Log(LOG_LEVEL_ERR, "Failed to read output from %s", tool);
        fprintf(conn_file, "{\"error\": \"Failed to read output from %s\"}", tool);
        cf_pclose(run_agent_output);
        fclose(conn_file);
        return;
    }
    BufferAppendChar(collected_output, '\0');

    int run_agent_ret = cf_pclose(run_agent_output);
    if (run_agent_ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to wait for the %s process to terminate", tool);
        fprintf(conn_file, "{\"error\": \"Failed to wait for the %s process to terminate\"}", tool);
        fclose(conn_file);
        return;
    }

    Writer *json_writer = FileWriter(conn_file);
    NDEBUG_UNUSED size_t written = WriterWrite(json_writer, "{\n\"output\" : \"");
    assert(written > 0);
    JsonEncodeStringWriter(BufferData(collected_output), json_writer);
    written = WriterWrite(json_writer, "\",\n");
    assert(written > 0);
    written = WriterWriteF(json_writer, "\"exit_code\": %d\n", run_agent_ret);
    assert(written > 0);
    written = WriterWrite(json_writer, "}\n");
    assert(written > 0);

    BufferDestroy(collected_output);
    fclose(conn_file);
    return;
}
