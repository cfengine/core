/*
  Copyright 2024 Northern.tech AS

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

#ifndef FILE_STREAM_H
#define FILE_STREAM_H

/**
 * @file file_stream.h
 *
 *  +---------+                  +---------+
 *  | client  |                  | server  |
 *  +----+----+                  +----+----+
 *       |                            |
 *       | sig = Compute(basis)       |
 *       |                            |
 *       | Send(sig) ---------------->| sig = Recv()
 *       |                            |
 *       |                            | sig = BuildHashTable(sig)
 *       |                            |
 *       |                            | delta = Compute(sig, src)
 *       |                            |
 *       | delta = Recv() <-----------| Send(delta)
 *       |                            |
 *       | dest = Patch(delta, basis) |
 *       |                            |
 *       v                            v
 *
 * 1. Client generates a signature of the basis file (i.e., the "outdated"
 *    file)
 * 2. Client sends signature to server
 * 4. Server builds a hash table from the signature
 * 5. Server generates delta from signature and the source file (i.e., the
 *    "up-to-date" file)
 * 6. Server sends delta to client
 * 7. Client applies delta on contents of the basis file in order to create
 *    the destination file
 *
 */

#include <tls_generic.h>
#include <stdbool.h>
#include <sys/types.h> /* mode_t */


/**
 * @brief Reply with unspecified server refusal
 *
 * E.g., use this function when the resource does not exist or access is
 * denied. We don't disinguish between these two for security reasons.
 *
 * @param conn The SSL connection object
 * @return true on success, otherwise false
 */
bool FileStreamRefuse(SSL *conn);

/**
 * @brief Serve a file using the stream API
 *
 * @param conn The SSL connection object
 * @param filename The name of the source file
 * @return true on success, otherwise false
 *
 * @note If the source file is a symlink, this function serves the contents of
 *       the symlink target.
 */
bool FileStreamServe(SSL *conn, const char *filename);

/**
 * @brief Fetch a file using the stream API
 *
 * @param conn The SSL connection object
 * @param basis The name of the basis file
 * @param dest The name of the destination file
 * @param perms The desired permissions of the destination file
 * @param print_stats Print performance statistics
 * @return true on success, otherwise false
 *
 * @note If the destination file is a symlink, this function fetches the
 *       contents into the symlink target.
 */
bool FileStreamFetch(
    SSL *conn,
    const char *basis,
    const char *dest,
    mode_t perms,
    bool print_stats);

#endif // FILE_STREAM_H
