/*
   Copyright 2017 Northern.tech AS

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
#include <passopenfile.h> /* Starts with <platform.h> */

#include <logging.h>
#include <alloc.h>

/* Local tuning parameter: should anything else know about it ? */
#define MAX_MESSAGE_SIZE  1024 /* strlen()+1 limit on text transmitted */

#ifdef __MINGW32__
#include <printsize.h>
static const char PID_FMT[] = "PID: %lu\n";
#define PID_MSG_SIZE (PRINTSIZE(unsigned long) + 8)
static const char ACK_MSG[] = "ACK\n";
typedef uint32_t MSG_LEN_T;

/* TODO: replace explicit waiting with use of blocking UDS. */
/* Package a call to select(), to avoid duplicating boilerplate code. */
static bool wait_for(const int uds, bool write, bool *ready)
{
    struct timeval tv;
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(uds, &fds);
    tv.tv_sec = 1; /* Wait for up to a second */
    tv.tv_usec = 0;

    int ret;
    if (write)
    {
        ret = select(uds + 1, NULL, &fds, NULL, &tv);
    }
    else
    {
        ret = select(uds + 1, &fds, NULL, NULL, &tv);
    }

    if (ret < 0)
    {
        return false;
    }
    *ready = FD_ISSET(uds, &fds);
    return true;
}

bool PassOpenFile_Put(int uds, int descriptor, const char *text)
{
    /* From the WSADuplicateSocket() doc [0]: "A process can call
     * closesocket on a duplicated socket and the descriptor will
     * become deallocated. The underlying socket, however, will remain
     * open until closesocket is called by the last remaining
     * descriptor."  So our caller can cf_closesocket(descriptor), as
     * long as they do it after the recipient calls WSASocket(); we
     * include an ACK at the end of this protocol to ensure that.
     *
     * [0] https://msdn.microsoft.com/en-us/library/windows/desktop/ms741565(v=vs.85).aspx
     */
    WSAPROTOCOL_INFO blob;
    /* Receive pid from peer */
    char buffer[PID_MSG_SIZE + 1];
    ssize_t got = recv(uds, buffer, PID_MSG_SIZE, 0);
    if (got < 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to read PID to which to pass descriptor");
        return false;
    }
    buffer[got] = '\0';
    unsigned long pid;
    if (sscanf(buffer, PID_FMT, &pid) != 1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to parse peer PID from: %s", buffer);
        return false;
    }
    else if (WSADuplicateSocket(descriptor, pid, &blob) != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to generate socket transmission data blob");
        return false;
    }

    bool ready;
    if (!wait_for(uds, true, &ready))
    {
        Log(LOG_LEVEL_ERR,
            "Can't pass socket to peer (select: %s)",
            GetErrorStr());
        return false;
    }
    else if (!ready)
    {
        Log(LOG_LEVEL_ERR,
            "Can't pass socket (peer not ready)");
        return false;
    }
    else
    {
        /* Transmit blob and text over UDS: */
        ssize_t had = send(uds, (const void*)&blob, sizeof(blob), 0);
        /* Casts needed because MinGW thinks send(,const char*,,) :-( */
        if (had == sizeof(blob))
        {
            MSG_LEN_T size = text ? strlen(text) + 1 : 0;
            had = send(uds, (const void*)&size, sizeof(size), 0);
            if (had == sizeof(size))
            {
                if (text)
                {
                    had = send(uds, text, size, 0);
                }
            }
            else
            {
                had = -1;
            }
        }
        else
        {
            had = -1;
        }
        if (had < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Failed to send socket-blob and accompanying text to peer");
            return false;
        }
    }

    /* Wait for ACK so we don't closesocket() before recipient has opened it */
    if (!wait_for(uds, false, &ready))
    {
        Log(LOG_LEVEL_ERR,
            "Can't get ACK from descriptor recipient (select: %s)",
            GetErrorStr());
    }
    else if (!ready)
    {
        Log(LOG_LEVEL_ERR,
            "Can't get ACK from descriptor recipient (peer not ready)");
    }
    else
    {
        char answer[sizeof(ACK_MSG) + 1]; /* +1 for the '\0' below */
        ssize_t got = recv(uds, answer, sizeof(ACK_MSG), 0);
        if (got > 0)
        {
            answer[got] = '\0'; /* In case unexpected message isn't terminated */
            if (strcmp(answer, ACK_MSG) != 0)
            {
                Log(LOG_LEVEL_WARNING,
                    "ACK message wasn't as expected: '%s' != '%s' (ignoring)",
                    ACK_MSG, answer);
            }
            return true;
        }
    }
    return false;
}

int PassOpenFile_Get(int uds, char **text)
{
    SOCKET descriptor = SOCKET_ERROR;

    /* Deliver pid to peer over uds */
    char msg[PID_MSG_SIZE];
    assert(sizeof(PID_FMT) - 3 + PRINTSIZE(unsigned long) <= PID_MSG_SIZE);
    unsigned long pid = GetCurrentProcessId();
    int len = snprintf(msg, sizeof(msg), PID_FMT, pid);
    assert(len > 0 && len < PID_MSG_SIZE);
    send(uds, msg, len + 1, 0);

    bool ready;
    if (!wait_for(uds, false, &ready))
    {
        Log(LOG_LEVEL_ERR,
            "Can't receive descriptor (select: %s)",
            GetErrorStr());
        return -1;
    }
    else if (!ready)
    {
        Log(LOG_LEVEL_VERBOSE, "No descriptor received.");
        return -1;
    }
    else
    {
        WSAPROTOCOL_INFO blob;

        /* Receive blob and text over UDS: */
        ssize_t got = recv(uds, (const void*)&blob, sizeof(blob), 0);
        /* Casts needed because MinGW thinks recv(,const char*,,) :-( */
        if (got == sizeof(blob))
        {
            descriptor = WSASocket(FROM_PROTOCOL_INFO,
                                   FROM_PROTOCOL_INFO,
                                   FROM_PROTOCOL_INFO,
                                   /* Args after the blob are ignored. */
                                   &blob, 0, WSA_FLAG_OVERLAPPED);
            if (descriptor == SOCKET_ERROR)
            {
                return -1;
            }
            /* else we *must* either use or closesocket() this descriptor. */

            if (text)
            {
                *text = NULL;
            }
            MSG_LEN_T size; /* Including space for final '\0' byte: */
            got = recv(uds, (const void*)&size, sizeof(size), 0);
            if (got == sizeof(size))
            {
                if (size)
                {
                    char *buffer = malloc(size);
                    if (*text)
                    {
                        got = recv(uds, buffer, size, 0);
                        if (got != size)
                        {
                            Log(LOG_LEVEL_ERR,
                                "Failed to receive whole text accompanying descriptor");
                        }
                    }
                    else
                    {
                        got = 0; /* != size */
                        Log(LOG_LEVEL_ERR,
                            "Failed to allocate buffer for text accompanying descriptor");
                    }

                    if (text && got == size)
                    {
                        *text = buffer;
                    }
                    else
                    {
                        free(buffer);
                    }
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "Failed to read size of text accompanying descriptor");
                size = 0;
            }

            if (text && size && !*text)
            {
                closesocket(descriptor);
                return -1;
            }
        }
        else
        {
            Log(LOG_LEVEL_ERR,
                "Failed to receive whole descriptor blob");
            return -1;
        }
    }

    /* Send ACK now that we've opened our end of the socket. */
    if (!wait_for(uds, true, &ready))
    {
        Log(LOG_LEVEL_ERR,
            "Can't ACK received descriptor (select: %s)",
            GetErrorStr());
    }
    else if (!ready)
    {
        Log(LOG_LEVEL_VERBOSE,
            "No descriptor supplier to ACK to, aborting receipt.");
    }
    else
    {
        send(uds, ACK_MSG, sizeof(ACK_MSG), 0);

        return descriptor;
    }

    closesocket(descriptor);
    return -1;
}

#else /* Unix: */

#ifdef HAVE_MSGHDR_MSG_CONTROL
/* This is the modern interface.  It should be present in most modern
 * Unix systems (conforming to the Single UNIX Specification).
 *
 * Transferred file descriptors "behave as though they have been
 * created with dup(2)" according to Linux's unix(7); which means the
 * sender can close() its end without prejudice to the recipient's use
 * of its copy. */
#define INTERFACE_STYLE "SUS"

# ifndef CMSG_SPACE
/* Solaris 9 contains support for .msg_control but lacks these macros
 * (here copied from Linux's <socket.h> - thankfully they're platform
 * agnostic) that we use to manipulate its header: */
#define CMSG_ALIGN(len) (((len) + sizeof (size_t) - 1) \
			 & (size_t) ~(sizeof (size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN (len) \
			 + CMSG_ALIGN (sizeof (struct cmsghdr)))
#define CMSG_LEN(len)   (CMSG_ALIGN (sizeof (struct cmsghdr)) + (len))
# endif

#elif defined(HAVE_MSGHDR_ACCRIGHTS)
/* This is the old (BSD) interface.  No Linux should use this, it's
 * only known to be present in older BSD-based Unixes. */
#define INTERFACE_STYLE "BSD"
#else
#error "No support for connection sharing on this platform :-("
#endif

#ifndef MSG_WAITALL /* Linux >= 2.2 */
#define MSG_WAITALL 0
#endif
static const char NULL_MSG[] = "\0NULL";

bool PassOpenFile_Put(int uds, int descriptor, const char *text)
{
    struct msghdr message;
    struct iovec vector;
    size_t msglen = text ? strlen(text) + 1 : sizeof(NULL_MSG);
    assert(MAX_MESSAGE_SIZE >= msglen);
    Log(LOG_LEVEL_VERBOSE,
        "Connected to peer, passing descriptor %d with %s %s",
        descriptor,
        text ? "text:" : "no",
        text ? text : "text");

    /* Prepare the message
     *
     * We need to send at least one byte of content, in an iovec, so
     * that a message actually gets received at the other end.  This
     * means that we want to send *at least something* even when our
     * text is NULL.  Fortunately, what we send isn't constrained by
     * C-string '\0'-termination, so we can send a message that
     * follows a '\0' byte with non-'\0' content (the NULL_MSG above)
     * which is definitely distinct from what we send for any non-NULL
     * text - non-empty has non-'\0' first byte; empty has size 1;
     * NULL also has '\0' as first byte but has size 6.  The recipient
     * isn't actually told the size sent, though; so has to pre-fill
     * the receiving buffer with something that can't be mistaken for
     * our NULL_MSG, so as to safely compare.
     *
     * We are confident that the .iov_base on the sending end is only
     * ever read, so casting away its constness here is not a problem.
     *
     * The manual page of sendmsg(2) says .msg_name is used to specify
     * the target address of a datagram; its documentation of failure
     * modes says you can get EISCONN if:
     * "The connection-mode socket was connected already but a
     *  recipient was specified.  (Now either this error is returned,
     *  or the recipient specification is ignored.)"
     * So, in order to keep world peace we need to set the name to NULL.
     * While we're at it, we clear all fields (using memset, so we don't
     * trip over variation between platforms in what fields exist).
     */
    memset(&message, 0, sizeof(message));
    memset(&vector, 0, sizeof(vector));
    vector.iov_base = (void*) (text ? text : NULL_MSG); /* const_cast */
    vector.iov_len = msglen;
    message.msg_iov = &vector;
    message.msg_iovlen = 1;

#ifdef HAVE_MSGHDR_MSG_CONTROL
    char control_message_data[CMSG_SPACE(sizeof(descriptor))];
    struct cmsghdr *control_message = NULL;
    message.msg_control = control_message_data;
    message.msg_controllen = sizeof(control_message_data);
    /* Conjecture: setting .msg_controllen influences CMSG_FIRSTHDR in
     * ways we need; but notice that we over-write it below. */
    control_message = CMSG_FIRSTHDR(&message);
    control_message->cmsg_level = SOL_SOCKET;
    control_message->cmsg_type = SCM_RIGHTS;
    control_message->cmsg_len = CMSG_LEN(sizeof(descriptor));
    *(int*)CMSG_DATA(control_message) = descriptor;
    message.msg_controllen = control_message->cmsg_len;
#elif HAVE_MSGHDR_ACCRIGHTS
    message.msg_accrights  = (char *)&descriptor;
    message.msg_accrightslen = sizeof(descriptor);
#endif

    /* Send message: */
    if (sendmsg(uds, &message, 0) < 0)
    {
        Log(LOG_LEVEL_ERR,
            "Can't pass descriptor to peer (sendmsg: %s)",
            GetErrorStr());
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Descriptor %d sent", descriptor);
        return true;
    }

    return false;
}

int PassOpenFile_Get(int uds, char **text)
{
    struct msghdr message;
    struct iovec vector;
    char buffer[MAX_MESSAGE_SIZE] = "PassOpenFile: failed to transmit any message";
    assert(strcmp(buffer + 1, NULL_MSG + 1) != 0);
    int received_descriptor = -1;

    Log(LOG_LEVEL_DEBUG, "Receiving descriptor via " INTERFACE_STYLE
        " interface (UDS descriptor %d)", uds);

    memset(&message, 0, sizeof(message));
    memset(&vector, 0, sizeof(vector));
    memset(buffer, 0, sizeof(buffer));
    vector.iov_base = buffer;
    vector.iov_len = sizeof(buffer); /* This is *not* updated by recvmsg ! */
    /* Prepare the message.  See Put()'s comment on the topic. */
    message.msg_iov = &vector;
    message.msg_iovlen = 1;

#ifdef HAVE_MSGHDR_MSG_CONTROL
    char control_message_data[CMSG_SPACE(sizeof(received_descriptor))];
    message.msg_control = control_message_data;
    message.msg_controllen = sizeof(control_message_data);
#elif HAVE_MSGHDR_ACCRIGHTS
    message.msg_accrights    = (char *)&received_descriptor;
    message.msg_accrightslen = sizeof(received_descriptor);
#endif

    /* Receive message: */
    if (recvmsg(uds, &message, MSG_WAITALL) < 0)
    {
        Log(LOG_LEVEL_ERR,
            "Can't receive descriptor (recvmsg: %s)",
            GetErrorStr());
        return -1;
    }

    /* Recover the file descriptor */
#ifdef HAVE_MSGHDR_MSG_CONTROL
    struct cmsghdr *control_message = CMSG_FIRSTHDR(&message);
    if (control_message == NULL)
    {
        Log(LOG_LEVEL_ERR, "Received no message.");
        return -1;
    }
    else if (control_message->cmsg_type != SCM_RIGHTS)
    {
        Log(LOG_LEVEL_ERR,
            "Received message does not deliver a descriptor.");
        return -1;
    }
    assert((char *)control_message + control_message->cmsg_len ==
           /* i.e. we only have *one* descriptor here; otherwise, we
            * need to close() all but the first. */
           (char *)CMSG_DATA(control_message) + sizeof(int));
    received_descriptor = *(int*)CMSG_DATA(control_message);
#elif HAVE_MSGHDR_ACCRIGHTS
    if (message.msg_accrightslen <= 0)
    {
        Log(LOG_LEVEL_ERR, "Received no data for descriptor.");
        return -1;
    }
#endif

    if (received_descriptor < 0)
    {
        Log(LOG_LEVEL_ERR, "Received invalid descriptor.");
        return -1;
    }
    /* Else, have a duty to close received_descriptor, one way or another. */

    /* Recover the message and report success: */
    if (buffer[0] || strcmp(buffer + 1, NULL_MSG + 1))
    {
        if (text)
        {
            *text = xstrndup(buffer, sizeof(buffer));
        }
        Log(LOG_LEVEL_VERBOSE,
            "Received descriptor %d with text '%s'",
            received_descriptor, buffer);
    }
    else
    {
        if (text)
        {
            *text = NULL;
        }
        Log(LOG_LEVEL_VERBOSE,
            "Received descriptor %d with no text",
            received_descriptor);
    }

    return received_descriptor;
}

#endif /* __MINGW32__ */
