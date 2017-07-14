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
#ifndef CFENGINE_PASSOPENFILE_H
#define CFENGINE_PASSOPENFILE_H

/* Sharing file descriptors between processes.
 *
 * This presumes a local communication socket (archetypically a Unix
 * Domain Socket, opened with domain AF_UNIX - also known as AF_LOCAL)
 * between two processes on a single machine.  One process has an open
 * file descriptor (and possibly some accompanying data), to be passed
 * to the other process.  The former Put()s, the latter Get()s to
 * implement this.  See ../tests/unit/passopenfile_test.c for a test
 * that illustrates usage.
 *
 * The accompanying text is optional: if NULL is passed to Put(),
 * Get() shall receive NULL; in contrast, any non-NULL pointer (even
 * to an empty string) shall result in Get() allocating memory in
 * which to receive a copy (even if it's just allocating one byte in
 * which to store a '\0').  Texts over 1023 bytes shall be truncated.
 *
 * Details of how the local socket is established are left to the
 * callers; one end shall need to bind() a listen()ing socket so that
 * the other can connect() one end and the first can accept() the
 * other end.  Callers are responsible for ensuring the local sockets
 * are ready for use, e.g. by calling select().
 *
 * This allows a process that has accept()ed a connection to hand off
 * its socket (and the information accept() gave it about the other
 * end) to another process for handling; or allows one process to
 * initiate a connection and then hand it off to another to work with
 * it.  We use this where there is one process that naturally should
 * do certain jobs but some other process to which initiating the
 * connection, or accept()ing it, is more practical.
 *
 * Passing local file descriptors can also be used for privilege
 * separation, with a privileged process sending open descriptors to a
 * worker process that lacks privileges to open the files but handles
 * untrusted data so can't be trusted to perform access control
 * reliably.  However (see following) this only works on Unix.
 *
 * On MinGW (i.e. MS-Win), the present implementation can only pass
 * sockets, not local file descriptors.  It also may block, calling
 * select(), in its improvised protocol for exchanging needed
 * information over the local sockets.
 *
 * The essential activity here is the same as GNUlib's passfd, albeit
 * with a slightly different API (most notably, providing for
 * transmission of a text message accompanying the descriptor).
 */

#include <platform.h>
/* Send a file descriptor to another process.
 *
 * @param uds The local communications socket.
 * @param descriptor Descriptor for the open file to transfer.
 * @param text NULL or a '\0'-terminated string to transmit.
 * @return True on successful transmission.
 */
extern bool PassOpenFile_Put(int uds, int descriptor, const char *text);

/* Receive a file descriptor from another process.
 *
 * On success, the caller is responsible for close()ing the descriptor
 * and free()ing the text (if any).  If text == NULL is passed, any
 * text transmitted with the descriptor is (read, if necessary, and)
 * discarded; otherwise, on failure, *text should be treated as
 * uninitialised (even if it was initialised before).
 *
 * @param uds The local communications socket.
 * @param text Pointer to where to record the returned string.
 * @return The received descriptor or, on failure, -1.
 */
extern int PassOpenFile_Get(int uds, char **text);

#endif /* CFENGINE_PASSOPENFILE_H */
