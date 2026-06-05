/*
  Copyright 2026 Northern.tech AS

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

#ifndef PATCH_STREAM_H
#define PATCH_STREAM_H

/**
 * @file patch_stream.h
 *
 *  +---------+                       +---------+
 *  | client  |                       | server  |
 *  +----+----+                       +----+----+
 *       |                                 |
 *       | Send("GETPATCH <hash>") ------->| patch = Create(hash)
 *       |                                 |
 *       | patch = Recv() <----------------| Send(patch)
 *       |                                 |
 *       v                                 v
 *
 * 1. Client (the hub) requests a leech2 patch containing the changes since
 *    the last known block hash
 * 2. Server creates a patch from its leech2 block chain
 * 3. Server sends the patch to the client
 *
 * The patch is an opaque byte buffer to this API; creating it (server side)
 * and applying it (client side) is up to the caller.
 */

#include <tls_generic.h>
#include <stdbool.h>

/**
 * @brief The leech2 genesis block hash (40 zeros)
 *
 * Requesting a patch since genesis yields a full state patch.
 */
#define PATCH_STREAM_GENESIS_HASH "0000000000000000000000000000000000000000"

/**
 * @brief Maximum size (in bytes) of a patch accepted by PatchStreamFetch()
 *
 * Bounds how much memory we'll buffer for a single patch, so that a malicious
 * or buggy peer cannot stream chunks forever and exhaust our memory.
 */
#define PATCH_STREAM_MAX_SIZE (256 * 1024 * 1024)

/**
 * @brief Reply with unspecified server refusal
 *
 * E.g., use this function when access is denied or when the server is unable
 * to serve patches (e.g., leech2 is not available). We don't distinguish
 * between these reasons for security purposes.
 *
 * @param conn The SSL connection object
 * @return true on success, otherwise false
 */
bool PatchStreamRefuse(SSL *conn);

/**
 * @brief Serve a patch using the stream API
 *
 * @param conn The SSL connection object
 * @param data The patch buffer
 * @param len The length of the patch buffer
 * @return true on success, otherwise false
 */
bool PatchStreamServe(SSL *conn, const void *data, size_t len);

/**
 * @brief Fetch a patch using the stream API
 *
 * @param conn The SSL connection object
 * @param data Is set to the received patch buffer (caller takes ownership
 *             and must free it with free()). Is set to NULL if and only if
 *             len is set to 0 (i.e. no payload bytes were received);
 *             free(NULL) is a no-op, so the caller can free() it regardless.
 * @param len Is set to the length of the received patch buffer
 * @return true on success, otherwise false (including when the patch would
 *         exceed PATCH_STREAM_MAX_SIZE)
 */
bool PatchStreamFetch(SSL *conn, char **data, size_t *len);

#endif // PATCH_STREAM_H
