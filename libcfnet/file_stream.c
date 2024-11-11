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
#include <stdint.h>
#include <stdarg.h>

/*********************************************************/
/* Network protocol                                      */
/*********************************************************/

/**
 * @brief Simple network protocol on top of SSL/TCP. Used for client-server
 * communication during file stream.
 *
 * @details Header format:
 *   +----------+----------+----------+----------+
 *   | SDU Len. | Reserved | EOF Flag | ERR Flag |
 *   +----------+----------+----------+----------+
 *   | 12 bits  | 2 bits   | 1 bit    | 1 bit    |
 *   +----------+----------+----------+----------+
 *
 * The header consists of 16 bits and the fields are defined as follows:
 * SDU Length           Length of the SDU (i.e. payload) encapsulated within
 *                      this datagram.
 * Reserved             2 bits reserved for future use.
 * End-of-File flag     Signals whether or not the receiver should expect to
 *                      receive more datagrams.
 * Error flag           Signals that the transmission must be canceled due to
 *                      unexpected error.
 *
 * @note If the End-of-File flag is set, there may still be data to process in
 *       in the payload. If the Error flag is set, there may be an error
 *       message in the payload.
 */
#define HEADER_SIZE 2

/**
 * @note The TLS Generic API requires that the message length is less than
 *       CF_BUFSIZE. Furthermore, the protocol can only handle up to 4095
 *       Bytes, because it's the largest unsigned integer you can represent
 *       with 12 bits (2^12 - 1 = 4095).
 */
#define MESSAGE_SIZE MIN(CF_BUFSIZE - 1, 4095)

/**
 * @brief Send a message using the file stream protocol
 * @warning You probably want to use SendMessage() or SendError() instead
 *
 * @param conn The SSL connection object
 * @param msg The message to send
 * @param len The length of the message to send (must be less or equal to
 *            MESSAGE_SIZE Bytes)
 * @param eof Set to true if this is the last message in a transaction,
 *            otherwise false
 * @param err Set to true if transaction must be canceled (e.g., due to an
 *            unexpected error), otherwise false
 * @note If the err parameter is set to true, the expected return value is
 *       still true.
 * @return true on success, otherwise false
 */
static bool __SendMessage(
    SSL *conn, const char *msg, size_t len, bool eof, bool err)
{
    assert(conn != NULL);
    assert(msg != NULL || len == 0);
    assert(len <= MESSAGE_SIZE);

    /* Set message length */
    assert(sizeof(len) >= 3); /* It's probably guaranteed, but let's make sure
                               * to avoid potentially nasty surprises */
    uint16_t header = len << 4;

    /* Set Error flag */
    if (err)
    {
        header |= (1 << 0);
    }

    /* Set End-of-File flag */
    if (eof)
    {
        header |= (1 << 1);
    }

    /* Send header */
    header = htons(header);
    int ret = TLSSend(conn, (char *) &header, HEADER_SIZE);
    if (ret != HEADER_SIZE)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to send message header during file stream: "
            "Expected to send %d bytes, but sent %d bytes",
            ret,
            HEADER_SIZE);
        return false;
    }

    if (len > 0)
    {
        /* Send payload */
        ret = TLSSend(conn, msg, len);
        if (ret != (int) len)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to send message payload during file stream: "
                "Expected to send %d bytes, but sent %zu bytes",
                ret,
                len);
            return false;
        }
    }

    return true;
}

/**
 * @brief Send a message using the file stream protocol
 *
 * @param conn The SSL connection object
 * @param msg The message to send
 * @param len The length of the message to send (must be less or equal to
 *            MESSAGE_SIZE Bytes)
 * @param eof Set to true if this is the last message in a transaction,
 *            otherwise false
 * @return true on success, otherwise false
 */
static inline bool SendMessage(
    SSL *conn, const char *msg, size_t len, bool eof)
{
    assert(conn != NULL);
    assert(msg != NULL || len == 0);

    return __SendMessage(conn, msg, len, eof, false);
}

/**
 * @brief Receive a message using the file stream protocol
 *
 * @param conn The SSL connection object
 * @param msg The message receive buffer (must be MESSAGE_SIZE Bytes large)
 * @param len The length of the reveived message
 * @param eof Is set to true if this was the last message in the transaction
 * @return true on success, otherwise false
 *
 * @note RecvMessage fails if the communication is broken or if we received an
 *       error from the remote host. In both cases, we should not try to flush
 *       the stream.
 */
static bool RecvMessage(SSL *conn, char *msg, size_t *len, bool *eof)
{
    assert(conn != NULL);
    assert(msg != NULL);
    assert(len != NULL);
    assert(eof != NULL);

    /* TLSRecv() expects a buffer this size */
    char recv_buffer[CF_BUFSIZE];

    /* Receive header */
    int ret = TLSRecv(conn, recv_buffer, HEADER_SIZE);
    if (ret != HEADER_SIZE)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to receive message header during file stream: "
            "Expected to receive %d bytes, but received %d bytes",
            ret,
            HEADER_SIZE);
        return false;
    }

    /* Why not receive the bytes directly into header in the TLSRecv()?
     * Because it actually writes a NUL-Byte after the requested bytes which
     * would cause memory violations. */
    uint16_t header;
    memcpy(&header, recv_buffer, HEADER_SIZE);
    header = ntohs(header);

    /* Extract Error flag */
    bool err = header & (1 << 0);

    /* Extract End-of-File flag */
    *eof = header & (1 << 1);

    /* Extract message length */
    assert(sizeof(*len) >= 2); /* It's probably guaranteed, but let's make
                                * sure to avoid potentially nasty surprises */
    *len = header >> 4;

    /* Read payload */
    if (*len > 0)
    {
        /* The TLSRecv() function's doc string says that the returned value
         * may be less than the requested length if the other side completed a
         * send with less bytes. I take it that this means that there is no
         * short reads/recvs. Futhermore, TLSSend() says that its return value
         * is always equal to the requested length as long as TLS is setup
         * correctly. I take it that the same is true for TLSRecv(). Hence, we
         * will interpret a shorter read than what we expect as an error. */
        ret = TLSRecv(conn, recv_buffer, *len);
        if (ret != *len)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to receive message payload during file stream: "
                "Expected to receive %d bytes, but received %zu bytes",
                ret,
                *len);
            return false;
        }
        memcpy(msg, recv_buffer, *len);

        if (err)
        {
            /* If the error flag is set, then the payload contains an error
             * message in the form of a NUL-byte terminated string. */
            Log(LOG_LEVEL_ERR, "Remote file stream error: %s", msg);
        }
    }

    return !err;
}

/**
 * @brief Flush the file stream
 *
 * It's used to prevent the remote host from blocking while sending the
 * remaining data after we have experienced an unexpected error and need to
 * abort the file stream. Once the stream has been successfully flushed, the
 * remote host will be ready to receive our error message.
 *
 * @param conn The SSL connection object
 * @return true on success, otherwise false
 */
static bool FlushStream(SSL *conn)
{
    assert(conn != NULL);

    char msg[MESSAGE_SIZE];
    size_t len;
    bool eof;
    while (RecvMessage(conn, msg, &len, &eof))
    {
        if (eof)
        {
            return true;
        }
    }

    Log(LOG_LEVEL_ERR, "Remote file stream error: %s", msg);
    return false;
}

/**
 * @brief Send an error message using the file stream protocol
 *
 * @param conn The SSL connection object
 * @param flush Whether or not to flush the stream (see FlushStream())
 * @param fmt The format string
 * @param ... The format string arguments
 * @return true on success, otherwise false
 */
static bool SendError(SSL *conn, bool flush, const char *fmt, ...)
    FUNC_ATTR_PRINTF(3, 4);

static bool SendError(SSL *conn, bool flush, const char *fmt, ...)
{
    assert(conn != NULL);
    assert(fmt != NULL);

    va_list ap;
    char msg[MESSAGE_SIZE];

    va_start(ap, fmt);
    int len = vsnprintf(msg, MESSAGE_SIZE, fmt, ap);
    va_end(ap);

    assert(len >= 0); /* Let's make sure we detect this in debug builds */
    if (len < 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to format error message during file stream");
        len = 0; /* We still want to send the header */
    }
    else if (len >= MESSAGE_SIZE)
    {
        Log(LOG_LEVEL_WARNING,
            "Error message truncated during file stream: "
            "Message is %d bytes, but maximum message size is %d bytes",
            len,
            MESSAGE_SIZE);
        /* Add dots to indicate message truncation. We don't need the
         * terminating NULL-byte in the buffer. Furthermore, TLSRecv() will
         * append one, upon receiving the message */
        msg[MESSAGE_SIZE - 1] = '.';
        msg[MESSAGE_SIZE - 2] = '.';
        msg[MESSAGE_SIZE - 3] = '.';
        len = MESSAGE_SIZE;
    }

    if (flush)
    {
        FlushStream(conn);
    }

    return __SendMessage(conn, msg, (size_t) len, false, true);
}

/*********************************************************/
/* Server specific                                       */
/*********************************************************/

#define ERROR_MSG_UNSPECIFIED_SERVER_REFUSAL "Unspecified server refusal"
#define ERROR_MSG_INTERNAL_SERVER_ERROR "Internal server error"

bool FileStreamRefuse(SSL *conn)
{
    return SendError(conn, false, ERROR_MSG_UNSPECIFIED_SERVER_REFUSAL);
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
    char in_buf[MESSAGE_SIZE * 2];

    /* Start a job for loading a signature into memory */
    rs_job_t *job = rs_loadsig_begin(sig);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for loading signature");
        SendError(conn, true, ERROR_MSG_INTERNAL_SERVER_ERROR);
        return false;
    }

    /* Setup buffers for the job */
    rs_buffers_t bufs = {0};

    rs_result res;
    do
    {
        /* Fill input buffers */
        if (bufs.eof_in == 0)
        {
            if (bufs.avail_in > MESSAGE_SIZE)
            {
                /* The job requires more data, but we cannot fit another
                 * message into the input buffer */
                Log(LOG_LEVEL_ERR,
                    "Insufficient buffer capacity to receive file stream signature: "
                    "%zu of %zu bytes available, but %d bytes is required to fit another message",
                    sizeof(in_buf) - bufs.avail_in,
                    sizeof(in_buf),
                    MESSAGE_SIZE);
                SendError(conn, true, ERROR_MSG_INTERNAL_SERVER_ERROR);

                rs_job_free(job);
                return false;
            }

            if (bufs.avail_in > 0)
            {
                /* Move leftover tail data to the front of the buffer */
                memmove(in_buf, bufs.next_in, bufs.avail_in);
            }

            size_t n_bytes;
            bool eof;
            if (!RecvMessage(conn, in_buf + bufs.avail_in, &n_bytes, &eof))
            {
                /* Error is already logged */
                rs_job_free(job);
                return false;
            }

            bufs.eof_in = eof ? 1 : 0;
            bufs.next_in = in_buf;
            bufs.avail_in += n_bytes;
        }

        /* Iterate job */
        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for loading signature: %s",
                rs_strerror(res));
            SendError(conn, bufs.eof_in == 0, ERROR_MSG_INTERNAL_SERVER_ERROR);
            rs_job_free(job);
            return false;
        }
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
    char in_buf[MESSAGE_SIZE], out_buf[MESSAGE_SIZE];

    /* Open source file */
    FILE *file = safe_fopen(filename, "rb");
    if (file == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open the source file '%s' for computing delta during file stream: %s",
            filename,
            GetErrorStr());
        SendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);
        return false;
    }

    /* Build hash table */
    rs_result res = rs_build_hash_table(sig);
    if (res != RS_DONE)
    {
        Log(LOG_LEVEL_ERR, "Failed to build hash table: %s", rs_strerror(res));
        SendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);
        fclose(file);
        return false;
    }

    /* Start generating delta */
    rs_job_t *job = rs_delta_begin(sig);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for generating delta");
        SendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);
        fclose(file);
        return false;
    }

    /* Setup buffers for the job */
    rs_buffers_t bufs = {0};
    bufs.next_out = out_buf;
    bufs.avail_out = MESSAGE_SIZE; /* We cannot send more using the protocol */

    do
    {
        /* Fill input buffers */
        if (bufs.eof_in == 0)
        {
            if (bufs.avail_in >= sizeof(in_buf))
            {
                /* The job requires more data, but the input buffer is full */
                Log(LOG_LEVEL_ERR,
                    "Insufficient buffer capacity to compute delta: "
                    "%zu of %zu bytes available",
                    sizeof(in_buf) - bufs.avail_in,
                    sizeof(in_buf));
                SendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);

                fclose(file);
                rs_job_free(job);
                return false;
            }

            if (bufs.avail_in > 0)
            {
                /* Move leftover tail data to the front of the buffer */
                memmove(in_buf, bufs.next_in, bufs.avail_in);
            }

            size_t n_bytes = fread(
                in_buf + bufs.avail_in,
                1 /* Byte */,
                sizeof(in_buf) - bufs.avail_in,
                file);
            if (n_bytes == 0)
            {
                if (ferror(file))
                {
                    Log(LOG_LEVEL_ERR,
                        "Failed to read the source file '%s' during file stream: %s",
                        filename,
                        GetErrorStr());
                    SendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);

                    fclose(file);
                    rs_job_free(job);
                    return false;
                }

                /* End-of-File reached */
                bufs.eof_in = feof(file);
                assert(bufs.eof_in != 0);
            }

            bufs.next_in = in_buf;
            bufs.avail_in += n_bytes;
        }

        /* Iterate job */
        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for generating delta: %s",
                rs_strerror(res));
            SendError(conn, false, ERROR_MSG_INTERNAL_SERVER_ERROR);

            fclose(file);
            rs_job_free(job);
            return false;
        }

        /* Drain output buffer, if there is data */
        size_t present = bufs.next_out - out_buf;
        if (present > 0)
        {
            assert(present <= MESSAGE_SIZE);
            if (!SendMessage(conn, out_buf, present, res == RS_DONE))
            {
                fclose(file);
                rs_job_free(job);
                return false;
            }

            bufs.next_out = out_buf;
            bufs.avail_out = MESSAGE_SIZE;
        }
        else if (res == RS_DONE)
        {
            /* Send End-of-File */
            if (!SendMessage(conn, NULL, 0, 1))
            {
                fclose(file);
                rs_job_free(job);
                return false;
            }
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

#define ERROR_MSG_INTERNAL_CLIENT_ERROR "Internal client error"


/**
 * @brief Get the size of a file
 *
 * @param file The file pointer
 * @return the file size or -1 on error
 * @note -1 on error is quite handy, because rs_sig_args() interprets it as
 *       unknown file size
 */
static rs_long_t GetFileSize(FILE *file)
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
 * @return true on success, otherwise false
 */
static bool SendSignature(SSL *conn, const char *filename)
{
    assert(conn != NULL);
    assert(filename != NULL);

    /* In this case, the input buffer does not need to be twice the message
     * size, because we can control how much we read into it */
    char in_buf[MESSAGE_SIZE], out_buf[MESSAGE_SIZE];

    /* Open basis file */
    FILE *file = safe_fopen(filename, "rb");
    if (file == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open the basis file '%s' for computing delta during file stream: %s",
            filename,
            GetErrorStr());
        SendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);
        return false;
    }

    /* Get file size */
    rs_long_t fsize = GetFileSize(file);

    /* Get recommended arguments */
    rs_magic_number sig_magic = 0;
    size_t block_len = 0, strong_len = 0;
    rs_result res = rs_sig_args(fsize, &sig_magic, &block_len, &strong_len);
    if (res != RS_DONE)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to get recommended signature arguments: %s",
            rs_strerror(res));
        SendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);
        fclose(file);
        return false;
    }

    /* Start generating signature */
    rs_job_t *job = rs_sig_begin(block_len, strong_len, sig_magic);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for generating signature");
        SendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);
        fclose(file);
        return false;
    }

    /* Setup buffers */
    rs_buffers_t bufs = {0};
    bufs.next_out = out_buf;
    bufs.avail_out = MESSAGE_SIZE; /* We cannot send more using the protocol */

    do
    {
        if (bufs.eof_in == 0)
        {
            if (bufs.avail_in >= sizeof(in_buf))
            {
                /* The job requires more data, but the input buffer is full */
                Log(LOG_LEVEL_ERR,
                    "Insufficient buffer capacity to compute delta: "
                    "%zu of %zu bytes available",
                    sizeof(in_buf) - bufs.avail_in,
                    sizeof(in_buf));
                SendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);

                fclose(file);
                rs_job_free(job);
                return false;
            }

            if (bufs.avail_in > 0)
            {
                /* Move leftover tail data to the front of the buffer */
                memmove(in_buf, bufs.next_in, bufs.avail_in);
            }

            /* Fill input buffer */
            size_t n_bytes = fread(
                in_buf + bufs.avail_in,
                1 /* Byte */,
                sizeof(in_buf) - bufs.avail_in,
                file);
            if (n_bytes == 0)
            {
                if (ferror(file))
                {
                    Log(LOG_LEVEL_ERR,
                        "Failed to read the basis file '%s' during file stream: %s",
                        filename,
                        GetErrorStr());
                    SendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);

                    fclose(file);
                    rs_job_free(job);
                    return false;
                }

                /* End-of-File reached */
                bufs.eof_in = feof(file);
                assert(bufs.eof_in != 0);
            }

            bufs.next_in = in_buf;
            bufs.avail_in += n_bytes;
        }

        /* Iterate job */
        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for generating signature: %s",
                rs_strerror(res));
            SendError(conn, false, ERROR_MSG_INTERNAL_CLIENT_ERROR);

            fclose(file);
            rs_job_free(job);
            return false;
        }

        /* Drain output buffer, if there is data */
        size_t present = bufs.next_out - out_buf;
        if (present > 0)
        {
            assert(present <= MESSAGE_SIZE);
            if (!SendMessage(conn, out_buf, present, res == RS_DONE))
            {
                fclose(file);
                rs_job_free(job);
                return false;
            }

            bufs.next_out = out_buf;
            bufs.avail_out = MESSAGE_SIZE;
        }
        else if (res == RS_DONE)
        {
            /* Send End-of-File */
            if (!SendMessage(conn, NULL, 0, 1))
            {
                fclose(file);
                rs_job_free(job);
                return false;
            }
        }
    } while (res != RS_DONE);

    fclose(file);
    rs_job_free(job);

    return true;
}

/**
 * @brief Receive delta and apply patch to the outdated copy of the file
 *
 * @param conn The SSL connection object
 * @param basis The name of basis file
 * @param dest The name of destination file
 * @param perms The desired file permissions of the destination file
 * @return true on success, otherwise false
 */
static bool RecvDelta(
    SSL *conn, const char *basis, const char *dest, mode_t perms)
{
    assert(conn != NULL);
    assert(basis != NULL);
    assert(dest != NULL);

    /* The input buffer has to be twice the message size, so that it can fit a
     * new message, as well as some tail data from the last job iteration */
    char in_buf[MESSAGE_SIZE * 2], out_buf[MESSAGE_SIZE];

    /* Open/create the destination file */
    FILE *new = safe_fopen_create_perms(dest, "wb", perms);
    if (new == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open/create destination file '%s': %s",
            dest,
            GetErrorStr());

        /* At this point the server will not be expecting any more messages
         * from the client as far as the File Stream API is concerned. Hence,
         * we don't have to send error message. Instead we just flush the
         * stream. */
        FlushStream(conn);
        return false;
    }

    /* Open the basis file */
    FILE *old = safe_fopen(basis, "rb");
    if (old == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to open basis file '%s': %s",
            basis,
            GetErrorStr());
        FlushStream(conn);
        return false;
    }

    /* Start a job for patching destination file */
    rs_job_t *job = rs_patch_begin(rs_file_copy_cb, old);
    if (job == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to begin job for patching");
        FlushStream(conn);
        return false;
    }

    /* Setup buffers for the job */
    rs_buffers_t bufs = {0};
    bufs.next_out = out_buf;
    bufs.avail_out = sizeof(out_buf);

    rs_result res;
    do
    {
        /* Fill input buffers */
        if (bufs.eof_in == 0)
        {
            if (bufs.avail_in > MESSAGE_SIZE)
            {
                /* The job requires more data, but we cannot fit another
                 * message into the input buffer */
                Log(LOG_LEVEL_ERR,
                    "Insufficient buffer capacity to receive file stream delta: "
                    "%zu of %zu bytes available, but %d bytes is required to fit another message",
                    sizeof(in_buf) - bufs.avail_in,
                    sizeof(in_buf),
                    MESSAGE_SIZE);
                FlushStream(conn);

                fclose(new);
                fclose(old);
                rs_job_free(job);
                return false;
            }

            if (bufs.avail_in > 0)
            {
                /* Move leftover tail data to the front of the buffer */
                memmove(in_buf, bufs.next_in, bufs.avail_in);
            }

            size_t n_bytes;
            bool eof;
            if (!RecvMessage(conn, in_buf + bufs.avail_in, &n_bytes, &eof))
            {
                /* Error is already logged */
                fclose(new);
                fclose(old);
                rs_job_free(job);
                return false;
            }

            bufs.eof_in = eof ? 1 : 0;
            bufs.next_in = in_buf;
            bufs.avail_in += n_bytes;
        }

        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to iterate job for patching: %s",
                rs_strerror(res));
            if (bufs.eof_in == 0)
            {
                FlushStream(conn);
            }

            fclose(new);
            fclose(old);
            rs_job_free(job);
            return false;
        }

        /* Drain output buffer, if there is data */
        size_t present = bufs.next_out - out_buf;
        if (present > 0)
        {
            size_t n_bytes = fwrite(out_buf, 1 /* Byte */, present, new);
            if (n_bytes == 0)
            {
                Log(LOG_LEVEL_ERR,
                    "Failed to write to destination file '%s' during file stream: %s",
                    dest,
                    GetErrorStr());
                if (bufs.eof_in == 0)
                {
                    FlushStream(conn);
                }

                fclose(new);
                fclose(old);
                rs_job_free(job);
                return false;
            }

            bufs.next_out = out_buf;
            bufs.avail_out = sizeof(out_buf);
        }
    } while (res != RS_DONE);

    fclose(new);
    fclose(old);
    rs_job_free(job);

    return true;
}

bool FileStreamFetch(
    SSL *conn, const char *basis, const char *dest, mode_t perms)
{
    assert(conn != NULL);
    assert(basis != NULL);
    assert(dest != NULL);

    /* Let's make sure the basis file exists */
    FILE *file = safe_fopen_create_perms(basis, "wb", perms);
    if (file != NULL)
    {
        fclose(file);
    }

    Log(LOG_LEVEL_VERBOSE,
        "Computing- & sending signature of file '%s'...",
        basis);
    if (!SendSignature(conn, basis))
    {
        /* Error is already logged */
        return false;
    }

    Log(LOG_LEVEL_VERBOSE,
        "Receiving delta & applying patch to file '%s'...",
        dest);
    if (!RecvDelta(conn, basis, dest, perms))
    {
        /* Error is already logged */
        return false;
    }

    return true;
}
