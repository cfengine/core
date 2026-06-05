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

#include <platform.h>

#include "file_stream.h"

#include <librsync.h>
#include <definitions.h>
#include <logging.h>
#include <file_lib.h>
#include <stream_protocol.h>
#include <stdint.h>
#include <stdarg.h>

/*********************************************************/
/* Common                                                */
/*********************************************************/

/**
 * @brief Move leftover tail data in the input buffer to the front before next
 *        job iteration.
 *
 * After a job iteration:
 *  - the 'next_in' attribute will point to the byte after the last one
 *    consumed from 'in_buf'.
 *  - the 'avail_in' attribute will contain the number of remaining/unconsumed
 *    bytes in 'in_buf'.
 *
 * @param bufs The RS buffers
 * @param in_buf The input buffer
 */
static void MoveLeftoversToFrontOfInputBuffer(rs_buffers_t *bufs, char *in_buf)
{
    assert(bufs != NULL);
    assert(in_buf != NULL);
    assert(bufs->next_in >= in_buf);

    if (bufs->avail_in > 0)
    {
        memmove(in_buf, bufs->next_in, bufs->avail_in);
    }
    bufs->next_in = in_buf;
}

/**
 * @brief Fill input buffer with messages received from remote host
 *
 * @param bufs RS buffers
 * @param in_buf Input buffer
 * @param conn SSL connection
 * @return false in case of failure
 */
static bool FillInputBufferFromHost(
    rs_buffers_t *bufs, char *in_buf, SSL *conn)
{
    assert(bufs != NULL);
    assert(in_buf != NULL);
    assert(conn != NULL);

    MoveLeftoversToFrontOfInputBuffer(bufs, in_buf);

    if (bufs->eof_in != 0)
    {
        /* No more data to fill the buffer with */
        return true;
    }

    if (bufs->avail_in > PROTOCOL_MESSAGE_SIZE)
    {
        /* We don't have space for another message */
        return true;
    }

    size_t msg_len;
    bool eof;
    if (!ProtocolRecvMessage(
            conn, bufs->next_in + bufs->avail_in, &msg_len, &eof))
    {
        return false;
    }

    bufs->eof_in = eof ? 1 : 0;
    bufs->avail_in += msg_len;
    return true;
}

/**
 * @brief Fill input buffer with contents from file
 *
 * @param bufs RS buffers
 * @param in_buf Input buffer
 * @param file The file
 * @return false in case of failure
 */
static bool FillInputBufferFromFile(
    rs_buffers_t *bufs, char *in_buf, FILE *file)
{
    assert(bufs != NULL);
    assert(in_buf != NULL);
    assert(file != NULL);

    MoveLeftoversToFrontOfInputBuffer(bufs, in_buf);

    if (bufs->eof_in != 0)
    {
        /* No more data to fill the buffer with */
        return true;
    }

    assert(bufs->avail_in <= PROTOCOL_MESSAGE_SIZE);
    const size_t remaining = PROTOCOL_MESSAGE_SIZE - bufs->avail_in;
    if (remaining == 0)
    {
        /* There is no more space in buffer */
        return true;
    }

    size_t num_bytes_read =
        fread(bufs->next_in + bufs->avail_in, 1, remaining, file);
    if ((num_bytes_read == 0) && ferror(file))
    {
        /* Failed to read */
        return false;
    }

    bufs->eof_in = feof(file);
    bufs->avail_in += num_bytes_read;
    return true;
}

/**
 * @brief Send contents of output buffer to remote host
 *
 * @param bufs RS buffers
 * @param out_buf Output buffer
 * @param is_done Whether to set End-of-File flag
 * @param conn SSL connection
 * @return false in case of failure
 */
static bool DrainOutputBufferToHost(
    rs_buffers_t *bufs, char *out_buf, bool is_done, SSL *conn)
{
    assert(bufs != NULL);
    assert(out_buf != NULL);
    assert(conn != NULL);

    const size_t num_bytes = bufs->next_out - out_buf;
    assert(num_bytes <= PROTOCOL_MESSAGE_SIZE);
    if ((num_bytes == 0) && !is_done)
    {
        /* There is nothing to send (avoid sending empty messages) */
        return true;
    }

    if (!ProtocolSendMessage(conn, out_buf, num_bytes, is_done))
    {
        /* Failed to send message (error is already logged) */
        return false;
    }

    bufs->next_out = out_buf;
    bufs->avail_out = PROTOCOL_MESSAGE_SIZE;
    return true;
}

/**
 * @brief Write contents of output buffer to sparse file
 *
 * @param bufs RS buffers
 * @param out_buf The output buffer
 * @param fd The file descriptor
 * @param last_write_made_hole Output parameter to tell whether last write
 *                             made a hole in the sparse file
 * @return false in case of failure
 */
static bool DrainOutputBufferToFile(
    rs_buffers_t *bufs, char *out_buf, int fd, bool *last_write_made_hole)
{
    assert(bufs != NULL);
    assert(out_buf != NULL);
    assert(last_write_made_hole != NULL);

    /* Drain output buffer, if there is data */
    size_t num_bytes = bufs->next_out - out_buf;
    assert(num_bytes <= PROTOCOL_MESSAGE_SIZE);
    if (num_bytes == 0)
    {
        /* There is nothing to write */
        return true;
    }

    if (!FileSparseWrite(fd, out_buf, num_bytes, last_write_made_hole))
    {
        /* Error is already logged */
        return false;
    }

    bufs->next_out = out_buf;
    bufs->avail_out = PROTOCOL_MESSAGE_SIZE;
    return true;
}

/*********************************************************/
/* Server specific                                       */
/*********************************************************/

bool FileStreamRefuse(SSL *conn)
{
    return ProtocolSendError(
        conn, false, ERROR_MSG_UNSPECIFIED_SERVER_REFUSAL);
}

/**
 * @brief Receive and load signature into memory
 *
 * @param conn The SSL connection object
 * @param sig The signature of the outdated file
 * @return true on success, otherwise false
 */
static bool RecvSignature(SSL *conn, rs_signature_t **sig)
{
    assert(conn != NULL);
    assert(sig != NULL);

    /* The input buffer has to be twice the message size, so that it can fit a
     * new message, as well as some tail data from the last job iteration */
    char in_buf[PROTOCOL_MESSAGE_SIZE * 2];

    /* Start a job for loading a signature into memory */
    rs_job_t *job = rs_loadsig_begin(sig);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for loading signature");
        ProtocolSendError(conn, true, ERROR_MSG_INTERNAL_SERVER_ERROR);
        return false;
    }

    /* Setup buffers for the job */
    rs_buffers_t bufs = {0};
    bufs.next_in = in_buf;

    rs_result res;
    do
    {
        if (!FillInputBufferFromHost(&bufs, in_buf, conn))
        {
            /* Error is already logged */
            rs_job_free(job);
            return false;
        }

        /* Iterate job */
        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for loading signature: %s",
                rs_strerror(res));
            ProtocolSendError(
                conn, bufs.eof_in == 0, ERROR_MSG_INTERNAL_SERVER_ERROR);
            rs_job_free(job);
            return false;
        }

        /* The job takes care of draining the output buffer */
    } while (res != RS_DONE);

    rs_job_free(job);

    return true;
}

/**
 * @brief Compute and send delta based on the source file and the signature of
 *        the basis file
 *
 * @param conn The SSL connection object
 * @param sig The signature of the basis file
 * @param filename The name of the source file
 * @return true on success, otherwise false
 */
static bool SendDelta(SSL *conn, rs_signature_t *sig, const char *filename)
{
    assert(conn != NULL);
    assert(sig != NULL);
    assert(filename != NULL);

    /* In this case, the input buffer does not need to be twice the message
     * size, because we can control how much we read into it */
    char in_buf[PROTOCOL_MESSAGE_SIZE], out_buf[PROTOCOL_MESSAGE_SIZE];

    /* Open source file */
    FILE *file = safe_fopen(filename, "rb");
    if (file == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open the source file '%s' for computing delta during file stream: %s",
            filename,
            GetErrorStr());
        ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);
        return false;
    }

    /* Build hash table */
    rs_result res = rs_build_hash_table(sig);
    if (res != RS_DONE)
    {
        Log(LOG_LEVEL_ERR, "Failed to build hash table: %s", rs_strerror(res));
        ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);
        fclose(file);
        return false;
    }

    /* Start generating delta */
    rs_job_t *job = rs_delta_begin(sig);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for generating delta");
        ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);
        fclose(file);
        return false;
    }

    /* Setup buffers for the job */
    rs_buffers_t bufs = {0};
    bufs.next_in = in_buf;
    bufs.next_out = out_buf;
    bufs.avail_out =
        PROTOCOL_MESSAGE_SIZE; /* We cannot send more using the protocol */

    do
    {
        if (!FillInputBufferFromFile(&bufs, in_buf, file))
        {
            Log(LOG_LEVEL_ERR,
                "Failed to read the source file '%s' during file stream: %s",
                filename,
                GetErrorStr());
            ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);

            fclose(file);
            rs_job_free(job);
            return false;
        }

        /* Iterate job */
        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for generating delta: %s",
                rs_strerror(res));
            ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);

            fclose(file);
            rs_job_free(job);
            return false;
        }

        if (!DrainOutputBufferToHost(&bufs, out_buf, (res == RS_DONE), conn))
        {
            /* Error is already logged in ProtocolSendMessage() */
            fclose(file);
            rs_job_free(job);
            return false;
        }
    } while (res != RS_DONE);

    fclose(file);
    rs_job_free(job);

    return true;
}

bool FileStreamServe(SSL *conn, const char *filename)
{
    assert(conn != NULL);
    assert(filename != NULL);

    Log(LOG_LEVEL_VERBOSE,
        "Receiving- & loading signature into memory for file '%s'...",
        filename);
    rs_signature_t *sig;
    if (!RecvSignature(conn, &sig))
    {
        /* Error is already logged */
        return false;
    }

    Log(LOG_LEVEL_VERBOSE,
        "Computing- & sending delta for file '%s'...",
        filename);
    if (!SendDelta(conn, sig, filename))
    {
        /* Error is already logged */
        rs_free_sumset(sig);
        return false;
    }

    rs_free_sumset(sig);
    return true;
}

/*********************************************************/
/* Client specific                                       */
/*********************************************************/

/**
 * @brief Get the size of a file
 *
 * @param file The file pointer
 * @return the file size or -1 on error
 * @note -1 on error is quite handy, because rs_sig_args() interprets it as
 *       unknown file size
 */
static rs_long_t GetSizeOfFile(FILE *file)
{
    /* librsync has rs_file_size() as a utility/convenience function which
     * basically does the exact same thing. However, it is not available in
     * versions prior to librsync-2.1.0 which caused problems for some of the
     * older platforms we support. Hence, we provide our own implementation in
     * order to have some backwards compatibility in terms of librsync
     * versions provided by package managers. */

    int fd = fileno(file);
    if (fd == -1)
    {
        return -1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        return -1;
    }

    return (rs_long_t) (S_ISREG(sb.st_mode) ? sb.st_size : -1);
}

/**
 * @brief Compute and send a signature of the basis file
 *
 * @param conn The SSL connection object
 * @param filename The name of the basis file
 * @param print_stats Whether or not to print performance statistics
 * @return true on success, otherwise false
 */
static bool SendSignature(SSL *conn, const char *filename, bool print_stats)
{
    assert(conn != NULL);
    assert(filename != NULL);

    /* Variables used for performance statistics */
    size_t bytes_in = 0;
    size_t bytes_out = 0;

    /* In this case, the input buffer does not need to be twice the message
     * size, because we can control how much we read into it */
    char in_buf[PROTOCOL_MESSAGE_SIZE], out_buf[PROTOCOL_MESSAGE_SIZE];

    /* Open basis file */
    FILE *file = safe_fopen(filename, "rb");
    if (file == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open the basis file '%s' for computing delta during file stream: %s",
            filename,
            GetErrorStr());
        ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);
        return false;
    }

    /* Get file size */
    rs_long_t fsize = GetSizeOfFile(file);

    /* Get recommended arguments */
    rs_magic_number sig_magic = 0;
    size_t block_len = 0, strong_len = 0;
    rs_result res = rs_sig_args(fsize, &sig_magic, &block_len, &strong_len);
    if (res != RS_DONE)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to get recommended signature arguments: %s",
            rs_strerror(res));
        ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);
        fclose(file);
        return false;
    }

    /* Start generating signature */
    rs_job_t *job = rs_sig_begin(block_len, strong_len, sig_magic);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for generating signature");
        ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);
        fclose(file);
        return false;
    }

    /* Setup buffers */
    rs_buffers_t bufs = {0};
    bufs.next_in = in_buf;
    bufs.next_out = out_buf;
    bufs.avail_out =
        PROTOCOL_MESSAGE_SIZE; /* We cannot send more using the protocol */

    do
    {
        if (!FillInputBufferFromFile(&bufs, in_buf, file))
        {
            Log(LOG_LEVEL_ERR,
                "Failed to read the basis file '%s' during file stream: %s",
                filename,
                GetErrorStr());
            ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);

            fclose(file);
            rs_job_free(job);
            return false;
        }

        /* Count bytes read for logging stats */
        bytes_in += bufs.avail_in;

        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for generating signature: %s",
                rs_strerror(res));
            ProtocolSendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);

            fclose(file);
            rs_job_free(job);
            return false;
        }

        /* Count bytes sent for logging stats */
        bytes_out += (bufs.next_out - out_buf);

        if (!DrainOutputBufferToHost(&bufs, out_buf, (res == RS_DONE), conn))
        {
            /* Error is already logged in ProtocolSendMessage() */
            fclose(file);
            rs_job_free(job);
            return false;
        }
    } while (res != RS_DONE);

    fclose(file);
    rs_job_free(job);

    const char *msg =
        "Send signature statistics:\n"
        "  %zu bytes in (read from '%s')\n"
        "  %zu bytes out (sent to server)\n";
    Log(LOG_LEVEL_DEBUG, msg, bytes_in, filename, bytes_out);
    if (print_stats)
    {
        fprintf(stderr, msg, bytes_in, filename, bytes_out);
    }

    return true;
}

/**
 * @brief Receive delta and apply patch to the outdated copy of the file
 *
 * @param conn The SSL connection object
 * @param basis The name of basis file
 * @param dest The name of destination file
 * @param perms The desired file permissions of the destination file
 * @param print_stats Whether or not to print performance statistics
 * @return true on success, otherwise false
 */
static bool RecvDelta(
    SSL *conn,
    const char *basis,
    const char *dest,
    mode_t perms,
    bool print_stats)
{
    assert(conn != NULL);
    assert(basis != NULL);
    assert(dest != NULL);

    /* Variables used for performance statistics */
    size_t bytes_in = 0;
    size_t bytes_out = 0;

    /* The input buffer has to be twice the message size, so that it can fit a
     * new message, as well as some tail data from the last job iteration */
    char in_buf[PROTOCOL_MESSAGE_SIZE * 2], out_buf[PROTOCOL_MESSAGE_SIZE];

    /* Open/create the destination file */
    unlink(dest);
    int new_fd = safe_open_create_perms(
        dest, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY, perms);
    if (new_fd == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open/create destination file '%s': %s",
            dest,
            GetErrorStr());

        /* At this point the server will not be expecting any more messages
         * from the client as far as the File Stream API is concerned. Hence,
         * we don't have to send error message. Instead we just flush the
         * stream. */
        ProtocolFlushStream(conn);
        return false;
    }

    /* Open the basis file */
    FILE *old_file = safe_fopen(basis, "rb");
    if (old_file == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open basis file '%s': %s",
            basis,
            GetErrorStr());
        ProtocolFlushStream(conn);
        close(new_fd);
        unlink(dest);
        return false;
    }

    /* Start a job for patching destination file */
    rs_job_t *job = rs_patch_begin(rs_file_copy_cb, old_file);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for patching");
        ProtocolFlushStream(conn);
        close(new_fd);
        fclose(old_file);
        unlink(dest);
        return false;
    }

    /* Setup buffers for the job */
    rs_buffers_t bufs = {0};
    bufs.next_in = in_buf;
    bufs.next_out = out_buf;
    bufs.avail_out = PROTOCOL_MESSAGE_SIZE;

    /* Sparse file specific */
    bool last_write_made_hole = false;

    rs_result res;
    do
    {
        if (!FillInputBufferFromHost(&bufs, in_buf, conn))
        {
            /* Error is already logged in ProtocolRecvMessage() */
            close(new_fd);
            fclose(old_file);
            rs_job_free(job);
            unlink(dest);
            return false;
        }

        /* Count bytes received for logging stats */
        bytes_in += bufs.avail_in;

        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for patching: %s",
                rs_strerror(res));
            if (bufs.eof_in == 0)
            {
                ProtocolFlushStream(conn);
            }

            close(new_fd);
            fclose(old_file);
            rs_job_free(job);
            unlink(dest);
            return false;
        }

        /* Count bytes written for logging stats */
        bytes_out += (bufs.next_out - out_buf);

        /* Drain output buffer, if there is data */
        if (!DrainOutputBufferToFile(
                &bufs, out_buf, new_fd, &last_write_made_hole))
        {
            /* Error is already logged */
            close(new_fd);
            fclose(old_file);
            rs_job_free(job);
            unlink(dest);
            return false;
        }
    } while (res != RS_DONE);

    fclose(old_file);
    rs_job_free(job);

    if (!FileSparseClose(new_fd, dest, false, bytes_out, last_write_made_hole))
    {
        /* Error is already logged */
        unlink(dest);
        return false;
    }

    const char *msg =
        "Receive delta statistics:\n"
        "  %zu bytes in (received from server)\n"
        "  %zu bytes out (written to '%s')\n";
    Log(LOG_LEVEL_DEBUG, msg, bytes_in, bytes_out, dest);
    if (print_stats)
    {
        fprintf(stderr, msg, bytes_in, bytes_out, dest);
    }

    return true;
}

bool FileStreamFetch(
    SSL *conn,
    const char *basis,
    const char *dest,
    mode_t perms,
    bool print_stats)
{
    assert(conn != NULL);
    assert(basis != NULL);
    assert(dest != NULL);

    /* Let's make sure the basis file exists, but don't truncate it */
    FILE *file = safe_fopen_create_perms(basis, "ab+", perms);
    if (file != NULL)
    {
        fclose(file);
    }

    Log(LOG_LEVEL_VERBOSE,
        "Computing- & sending signature of file '%s'...",
        basis);
    if (!SendSignature(conn, basis, print_stats))
    {
        /* Error is already logged */
        return false;
    }

    Log(LOG_LEVEL_VERBOSE,
        "Receiving delta & applying patch to file '%s'...",
        dest);
    if (!RecvDelta(conn, basis, dest, perms, print_stats))
    {
        /* Error is already logged */
        return false;
    }

    return true;
}
