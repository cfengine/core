/*
  Copyright 2020 Northern.tech AS

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

#ifndef CFENGINE_PROTOCOL_H
#define CFENGINE_PROTOCOL_H

#include <cfnet.h>
#include <sequence.h>
#include <protocol_version.h> // ProtocolVersion

/**
 * Receives a directory listing from a remote host.
 *
 * The server will use "/var/cfengine" as working directory if absolute path
 * is not specified. The server sends the directory entries as a string
 * separated by NUL-bytes, and ending with the magic string CFD_TERMINATOR.
 *
 * The function shall fail if connection is not established, or if the server
 * gives a bad response (denoted by a message preceded by "BAD").
 *
 * @param [in] conn  The connection to use
 * @param [in] path  Path on remote host
 * @return A sequence of filenames in the requested directory on success, NULL
 *         on failure.
 *
 * Example (for printing each directory entry):
 * @code
 *     AgentConnection *conn = ServerConnection("127.0.0.1", "666", ...);
 *     Seq *dir = ProtocolOpenDir(conn, "masterfiles");
 *     for (int i = 0; i < SeqLength(dir); i++)
 *     {
 *         char *entry = SeqAt(i);
 *         printf("%s\n", entry);
 *     }
 * @endcode
 *
 * In the protocol, this will look like this on the server side:
 *     Received: OPENDIR masterfiles
 *     Translated to: OPENDIR /var/cfengine/masterfiles
 *     Sends string:
 *     ".\0..\0cfe_internal\0cf_promises_release_id\0...
 *      ...templates\0update.cf\0" CFD_TERMINATOR
 */
Seq *ProtocolOpenDir(AgentConnection *conn, const char *path);

/**
 * Receives a file from a remote host.
 *
 * The server will use "/var/cfengine" as working directory if absolute path
 * is not specified. The client will send a request that looks like this:
 *      `GET <buf_size> <remote_path>`
 *
 * `buf_size` is the local buffer size: how much of the file to receive in
 * each transaction. It should be aligned to block size (which is usually
 * 4096), but currently the protocol only allows _exactly_ 4095 bytes per
 * transaction.
 *
 * The function shall fail if connection is not established, or if the server
 * gives a bad response (denoted by a message preceded by "BAD").
 *
 * @param [in] conn         The connection to use
 * @param [in] remote_path  Path on remote host
 * @param [in] local_path   Path of received file
 * @param [in] file_size    Size of file to get
 * @param [in] perms        Permissions of local file
 * @return True if file was successfully transferred, false otherwise
 *
 * Example (for printing each directory entry):
 * @code
 *     AgentConnection *conn = ServerConnection("127.0.0.1", "666", ...);
 *     bool got_file = ProtocolGet(conn, "masterfiles/update.cf",
 *                                 "update.cf", CF_MSGSIZE, 0644);
 *     if (got_file)
 *     {
 *        struct stat sb;
 *        stat("update.cf", &sb);
 *        printf("This file is %ld big!\n", sb.st_size);
 *     }
 * @endcode
 *
 * In the protocol, this will look like this on the server side:
 *     Received: GET masterfiles/update.cf
 *     Translated to: GET /var/cfengine/masterfiles/update.cf
 */
bool ProtocolGet(AgentConnection *conn, const char *remote_path,
                 const char *local_path, const uint32_t file_size, int perms);


/**
 * Receives a file from a remote host, see documentation for #ProtocolGet
 *
 * This funtion will first stat the remote path before attempting to receive
 * it.
 */
bool ProtocolStatGet(AgentConnection *conn, const char *remote_path,
                     const char *local_path, int perms);

/**
 * Receives statistics about a remote file.
 *
 * This is a cacheless version of #cf_remote_stat from stat_cache.c. This
 * only supports sending with the latest cfnet protocol.
 *
 * When the `STAT` request is sent, it is sent together with the current time
 * since the Epoch given by the `time` syscall denoted by `SYNCH <tloc>`. If
 * the server is set to deny bad clocks (which is default), it will reject
 * `STAT` requests from hosts where the clocks differ too much.
 *
 * When the server accepts the `STAT` request, it will send each field of the
 * `Stat` struct from `stat_cache.h` as numbers delimited by spaces in a
 * single string. Since the `Stat` struct is not cached, its fields are
 * transferred to the \p stat_buf parameter.
 *
 * Example
 * @code
 *      AgentConnection *conn = ServerConnection("127.0.0.1", "666", ...);
 *      struct stat stat_buf;
 *      ProtocolStat(conn, "masterfiles/update.cf", &stat_buf);
 *      assert((stat_buf.st_mode & S_IFMT) == S_IFREG);
 * @endcode
 *
 * This is how the above example looks on the server side:
 *      Received: SYNCH 12356789 STAT masterfiles/update.cf
 *      Translated to: STAT /var/cfengine/masterfiles/update.cf
 *      Sends string:
 *      "OK: 0 33188 0 ..." etc.
 *
 * @param [in]  conn         The connection to use
 * @param [in]  remote_path  Path on remote host
 * @param [out] stat_buf     Where to store statistics
 * @return true on success, false on failure.
 */
bool ProtocolStat(AgentConnection *conn, const char *remote_path,
                  struct stat *stat_buf);

#endif
