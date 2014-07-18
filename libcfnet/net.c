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

#include <platform.h>
#include <cfnet.h>                                            /* CF_BUFSIZE */
#include <cf3.defs.h>
#include <cf3.extern.h>                                    /* BINDINTERFACE */
#include <net.h>
#include <classic.h>
#include <tls_generic.h>
#include <connection_info.h>
#include <logging.h>
#include <misc_lib.h>



/**
 * @param len is the number of bytes to send, or 0 if buffer is a
 *        '\0'-terminated string so strlen(buffer) can used.
 */
int SendTransaction(const ConnectionInfo *conn_info,
                    const char *buffer, int len, char status)
{
    assert(status == CF_MORE || status == CF_DONE);

    char work[CF_BUFSIZE] = { 0 };
    int ret;

    if (len == 0)
    {
        len = strlen(buffer);
    }

    if (len > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR, "SendTransaction: len (%d) > %d - %d",
            len, CF_BUFSIZE, CF_INBAND_OFFSET);
        return -1;
    }

    snprintf(work, CF_INBAND_OFFSET, "%c %d", status, len);

    memcpy(work + CF_INBAND_OFFSET, buffer, len);

    Log(LOG_LEVEL_DEBUG, "SendTransaction header: %s", work);
    LogRaw(LOG_LEVEL_DEBUG, "SendTransaction data: ",
           work + CF_INBAND_OFFSET, len);

    switch(ConnectionInfoProtocolVersion(conn_info))
    {
    case CF_PROTOCOL_CLASSIC:
        ret = SendSocketStream(ConnectionInfoSocket(conn_info), work,
                               len + CF_INBAND_OFFSET);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSSend(ConnectionInfoSSL(conn_info), work, len + CF_INBAND_OFFSET);
        break;
    default:
        UnexpectedError("SendTransaction: ProtocolVersion %d!",
                        ConnectionInfoProtocolVersion(conn_info));
        ret = -1;
    }

    if (ret == -1)
        return -1;
    else
        return 0;
}

/*************************************************************************/

/**
 *  @return 0 in case of socket closed, -1 in case of other error, or
 *          >0 the number of bytes read.
 */
int ReceiveTransaction(const ConnectionInfo *conn_info, char *buffer, int *more)
{
    char proto[CF_INBAND_OFFSET + 1] = { 0 };
    char status = 'x';
    unsigned int len = 0;
    int ret;

    /* Get control channel. */
    switch(ConnectionInfoProtocolVersion(conn_info))
    {
    case CF_PROTOCOL_CLASSIC:
        ret = RecvSocketStream(ConnectionInfoSocket(conn_info), proto, CF_INBAND_OFFSET);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSRecv(ConnectionInfoSSL(conn_info), proto, CF_INBAND_OFFSET);
        break;
    default:
        UnexpectedError("ReceiveTransaction: ProtocolVersion %d!",
                        ConnectionInfoProtocolVersion(conn_info));
        ret = -1;
    }
    if (ret == -1 || ret == 0)
        return ret;

    LogRaw(LOG_LEVEL_DEBUG, "ReceiveTransaction header: ", proto, ret);

    ret = sscanf(proto, "%c %u", &status, &len);
    if (ret != 2)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: Bad packet -- bogus header: %s", proto);
        return -1;
    }
    if (len > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: Bad packet -- too long (len=%d)", len);
        return -1;
    }
    if (status != CF_MORE && status != CF_DONE)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: Bad packet -- bogus header (more='%c')",
            status);
        return -1;
    }

    if (more != NULL)
    {
        switch (status)
        {
        case CF_MORE:
                *more = true;
                break;
        case CF_DONE:
                *more = false;
                break;
        default:
            ProgrammingError("Unreachable, "
                             "bogus headers have already been checked!");
        }
    }

    /* Get data. */
    switch(ConnectionInfoProtocolVersion(conn_info))
    {
    case CF_PROTOCOL_CLASSIC:
        ret = RecvSocketStream(ConnectionInfoSocket(conn_info), buffer, len);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSRecv(ConnectionInfoSSL(conn_info), buffer, len);
        break;
    default:
        UnexpectedError("ReceiveTransaction: ProtocolVersion %d!",
                        ConnectionInfoProtocolVersion(conn_info));
        ret = -1;
    }

    LogRaw(LOG_LEVEL_DEBUG, "ReceiveTransaction data: ", buffer, ret);

    return ret;
}

/*************************************************************************/


/**
   Tries to connect() to server #host, returns the socket descriptor and the
   IP address that succeeded in #txtaddr.

   @param #connect_timeout how long to wait for connect(), zero blocks forever
   @param #txtaddr If connected successfully return the IP connected in
                   textual representation
   @return Connected socket descriptor or -1 in case of failure.
*/
int SocketConnect(const char *host, const char *port,
                  unsigned int connect_timeout, bool force_ipv4,
                  char *txtaddr, size_t txtaddr_size)
{
    struct addrinfo *response, *ap;
    struct addrinfo *response2, *ap2;
    int sd = -1, connected = false;

    struct addrinfo query = {
        .ai_family = force_ipv4 ? AF_INET : AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    int ret = getaddrinfo(host, port, &query, &response);
    if (ret != 0)
    {
        Log(LOG_LEVEL_INFO,
              "Unable to find host '%s' service '%s' (%s)",
              host, port, gai_strerror(ret));
        return -1;
    }

    ap = response;
    while (ap != NULL && !connected)
    {
        /* Convert address to string. */
        getnameinfo(ap->ai_addr, ap->ai_addrlen,
                    txtaddr, txtaddr_size,
                    NULL, 0, NI_NUMERICHOST);
        Log(LOG_LEVEL_VERBOSE,
            "Connecting to host %s (address %s), port %s",
            host, txtaddr, port);

        sd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
        if (sd == -1)
        {
            Log(LOG_LEVEL_ERR, "Couldn't open a socket. (socket: %s)",
                GetErrorStr());
        }
        else
        {
            /* Bind socket to specific interface, if requested. */
            if (BINDINTERFACE[0] != '\0')
            {
                struct addrinfo query2 = {
                    .ai_family = force_ipv4 ? AF_INET : AF_UNSPEC,
                    .ai_socktype = SOCK_STREAM,
                    /* returned address is for bind() */
                    .ai_flags = AI_PASSIVE
                };

                int ret2 = getaddrinfo(BINDINTERFACE, NULL, &query2, &response2);
                if (ret2 != 0)
                {
                    Log(LOG_LEVEL_ERR,
                        "Unable to lookup interface '%s' to bind. (getaddrinfo: %s)",
                        BINDINTERFACE, gai_strerror(ret2));

                    freeaddrinfo(response2);
                    freeaddrinfo(response);
                    cf_closesocket(sd);
                    return -1;
                }

                for (ap2 = response2; ap2 != NULL; ap2 = ap2->ai_next)
                {
                    if (bind(sd, ap2->ai_addr, ap2->ai_addrlen) == 0)
                    {
                        break;
                    }
                }
                if (ap2 == NULL)
                {
                    Log(LOG_LEVEL_ERR,
                        "Unable to bind to interface '%s'. (bind: %s)",
                        BINDINTERFACE, GetErrorStr());
                }
                freeaddrinfo(response2);
            }

            connected = TryConnect(sd, connect_timeout * 1000,
                                   ap->ai_addr, ap->ai_addrlen);
            ap = ap->ai_next;
        }
    }

    if (response != NULL)
    {
        freeaddrinfo(response);
    }

    if (!connected)
    {
        Log(LOG_LEVEL_VERBOSE, "Unable to connect to host %s (%s)",
            host, GetErrorStr());

        if (sd != -1)
        {
            cf_closesocket(sd);
            sd = -1;
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,
            "Connected to host %s address %s port %s",
            host, txtaddr, port);
    }

    return sd;
}


#if !defined(__MINGW32__)

#if defined(__hpux) && defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
// HP-UX GCC type-pun warning on FD_SET() macro:
// While the "fd_set" type is defined in /usr/include/sys/_fd_macros.h as a
// struct of an array of "long" values in accordance with the XPG4 standard's
// requirements, the macros for the FD operations "pretend it is an array of
// int32_t's so the binary layout is the same for both Narrow and Wide
// processes," as described in _fd_macros.h. In the FD_SET, FD_CLR, and
// FD_ISSET macros at line 101, the result is cast to an "__fd_mask *" type,
// which is defined as int32_t at _fd_macros.h:82.
//
// This conflict between the "long fds_bits[]" array in the XPG4-compliant
// fd_set structure, and the cast to an int32_t - not long - pointer in the
// macros, causes a type-pun warning if -Wstrict-aliasing is enabled.
// The warning is merely a side effect of HP-UX working as designed,
// so it can be ignored.
#endif

/**
 * Tries to connect for #timeout_ms milliseconds. On success sets the recv()
 * timeout to #timeout_ms as well.
 *
 * @param #timeout_ms How long to wait for connect(), if zero wait forever.
 * @return true on success, false otherwise.
 **/
bool TryConnect(int sd, unsigned long timeout_ms,
                const struct sockaddr *sa, socklen_t sa_len)
{
    assert(sa != NULL);

    if (sd >= FD_SETSIZE)
    {
        Log(LOG_LEVEL_ERR, "Open connections exceed FD_SETSIZE limit of %d",
            FD_SETSIZE);
        return false;
    }

    /* set non-blocking socket */
    int arg = fcntl(sd, F_GETFL, NULL);
    int ret = fcntl(sd, F_SETFL, arg | O_NONBLOCK);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to set socket to non-blocking mode (fcntl: %s)",
            GetErrorStr());
    }

    ret = connect(sd, sa, sa_len);
    if (ret == -1)
    {
        if (errno != EINPROGRESS)
        {
            Log(LOG_LEVEL_INFO, "Failed to connect to server (connect: %s)",
                GetErrorStr());
            return false;
        }

        int errcode;
        socklen_t opt_len = sizeof(errcode);
        fd_set myset;
        FD_ZERO(&myset);
        FD_SET(sd, &myset);

        Log(LOG_LEVEL_VERBOSE, "Waiting to connect...");

        struct timeval tv, *tvp;
        if (timeout_ms > 0)
        {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            tvp = &tv;
        }
        else
        {
            tvp = NULL;                                /* wait indefinitely */
        }

        ret = select(sd + 1, NULL, &myset, NULL, tvp);
        if (ret == 0)
        {
            Log(LOG_LEVEL_INFO, "Timeout connecting to server");
            return false;
        }
        if (ret == -1)
        {
            if (errno == EINTR)
            {
                Log(LOG_LEVEL_ERR,
                    "Socket connect was interrupted by signal");
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "Failure while connecting (select: %s)",
                    GetErrorStr());
            }
            return false;
        }

        ret = getsockopt(sd, SOL_SOCKET, SO_ERROR,
                              (void *) &errcode, &opt_len);
        if (ret == -1)
        {
            Log(LOG_LEVEL_ERR,
                "Could not check connection status (getsockopt: %s)",
                GetErrorStr());
            return false;
        }

        if (errcode != 0)
        {
            Log(LOG_LEVEL_INFO, "Failed to connect to server: %s",
                GetErrorStrFromCode(errcode));
            return false;
        }
    }

    /* Connection suceeded, return to blocking mode. */
    ret = fcntl(sd, F_SETFL, arg);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to set socket back to blocking mode (fcntl: %s)",
            GetErrorStr());
    }

    if (timeout_ms > 0)
    {
        SetReceiveTimeout(sd, timeout_ms);
    }

    return true;
}

#if defined(__hpux) && defined(__GNUC__)
#pragma GCC diagnostic warning "-Wstrict-aliasing"
#endif

#endif /* !defined(__MINGW32__) */



/**
 * Set timeout for recv(), in milliseconds.
 * @param ms must be > 0.
 */
int SetReceiveTimeout(int fd, unsigned long ms)
{
    assert(ms > 0);

    Log(LOG_LEVEL_VERBOSE, "Setting socket timeout to %lu seconds.", ms/1000);

/* On windows SO_RCVTIMEO is set by a DWORD indicating the timeout in
 * milliseconds, on UNIX it's a struct timeval. */

#if !defined(__MINGW32__)
    struct timeval tv = {
        .tv_sec = ms / 1000,
        .tv_usec = (ms % 1000) * 1000
    };
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#else
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &ms, sizeof(ms));
#endif

    if (ret != 0)
    {
        Log(LOG_LEVEL_INFO,
            "Failed to set socket timeout to %lu milliseconds.", ms);
        return -1;
    }

    return 0;
}
