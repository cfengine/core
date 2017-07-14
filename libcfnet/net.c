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

#include <platform.h>
#include <cfnet.h>                                            /* CF_BUFSIZE */
#include <net.h>
#include <classic.h>
#include <tls_generic.h>
#include <connection_info.h>
#include <logging.h>
#include <misc_lib.h>


/* TODO remove libpromises dependency. */
extern char BINDINTERFACE[];                  /* cf3globals.c, cf3.extern.h */


/**
 * @param len is the number of bytes to send, or 0 if buffer is a
 *        '\0'-terminated string so strlen(buffer) can be used.
 * @return -1 in case of error or connection closed
 *         (also currently returns 0 for success but don't count on it)
 * @NOTE #buffer can't be of zero length, our protocol
 *       does not allow empty transactions!
 * @NOTE (len <= CF_BUFSIZE - CF_INBAND_OFFSET)
 *
 * @TODO Currently only transactions up to CF_BUFSIZE-CF_INBAND_OFFSET are
 *       allowed to be sent. This function should be changed to allow up to
 *       CF_BUFSIZE-1 (since '\0' is not sent, but the receiver needs space to
 *       append it). So transaction length will be at most 4095!
 */
int SendTransaction(ConnectionInfo *conn_info,
                    const char *buffer, int len, char status)
{
    assert(status == CF_MORE || status == CF_DONE);

    char work[CF_BUFSIZE] = { 0 };
    int ret;

    if (len == 0)
    {
        len = strlen(buffer);
    }

    /* Not allowed to send zero-payload packets */
    assert(len > 0);

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

    switch(conn_info->protocol)
    {

    case CF_PROTOCOL_CLASSIC:
        ret = SendSocketStream(conn_info->sd, work,
                               len + CF_INBAND_OFFSET);
        break;

    case CF_PROTOCOL_TLS:
        ret = TLSSend(conn_info->ssl, work, len + CF_INBAND_OFFSET);
        if (ret <= 0)
        {
            ret = -1;
        }
        break;

    default:
        UnexpectedError("SendTransaction: ProtocolVersion %d!",
                        conn_info->protocol);
        ret = -1;
    }

    if (ret == -1)
    {
        /* We are experiencing problems with sending data to server.
         * This might lead to packages being not delivered in correct
         * order and unexpected issues like directories being replaced
         * with files.
         * In order to make sure that file transfer is reliable we have to
         * close connection to avoid broken packages being received. */
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
        return -1;
    }
    else
    {
        /* SSL_MODE_AUTO_RETRY guarantees no partial writes. */
        assert(ret == len + CF_INBAND_OFFSET);

        return 0;
    }
}

/*************************************************************************/

/**
 *  Receive a transaction packet of at most CF_BUFSIZE-1 bytes, and
 *  NULL-terminate it.
 *
 *  @param #buffer must be of size at least CF_BUFSIZE.
 *
 *  @return -1 in case of closed socket, other error or timeout.
 *              The connection MAY NOT BE FINALISED!
 *          >0 the number of bytes read, transaction was successfully received.
 *
 *  @TODO shutdown() the connection in all cases were this function returns -1,
 *        in order to protect against future garbage reads.
 */
int ReceiveTransaction(ConnectionInfo *conn_info, char *buffer, int *more)
{
    char proto[CF_INBAND_OFFSET + 1] = { 0 };
    int ret;

    /* Get control channel. */
    switch(conn_info->protocol)
    {
    case CF_PROTOCOL_CLASSIC:
        ret = RecvSocketStream(conn_info->sd, proto, CF_INBAND_OFFSET);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSRecv(conn_info->ssl, proto, CF_INBAND_OFFSET);
        break;
    default:
        UnexpectedError("ReceiveTransaction: ProtocolVersion %d!",
                        conn_info->protocol);
        ret = -1;
    }

    /* If error occurred or recv() timeout or if connection was gracefully
     * closed. Connection has been finalised. */
    if (ret <= 0)
    {
        /* We are experiencing problems with receiving data from server.
         * This might lead to packages being not delivered in correct
         * order and unexpected issues like directories being replaced
         * with files.
         * In order to make sure that file transfer is reliable we have to
         * close connection to avoid broken packages being received. */
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
        return -1;
    }
    else if (ret != CF_INBAND_OFFSET)
    {
        /* If we received less bytes than expected. Might happen
         * with TLSRecv(). */
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: bogus short header (%d bytes: '%s')",
            ret, proto);
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
        return -1;
    }

    LogRaw(LOG_LEVEL_DEBUG, "ReceiveTransaction header: ", proto, ret);

    char status = 'x';
    int len = 0;

    ret = sscanf(proto, "%c %d", &status, &len);
    if (ret != 2)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: bogus header: %s", proto);
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
        return -1;
    }

    if (status != CF_MORE && status != CF_DONE)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: bogus header (more='%c')", status);
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
        return -1;
    }
    if (len > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: packet too long (len=%d)", len);
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
        return -1;
    }
    else if (len <= 0)
    {
        /* Zero-length packets are disallowed, because
         * ReceiveTransaction() == 0 currently means connection closed. */
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: packet too short (len=%d)", len);
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
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
    switch(conn_info->protocol)
    {
    case CF_PROTOCOL_CLASSIC:
        ret = RecvSocketStream(conn_info->sd, buffer, len);
        break;
    case CF_PROTOCOL_TLS:
        ret = TLSRecv(conn_info->ssl, buffer, len);
        break;
    default:
        UnexpectedError("ReceiveTransaction: ProtocolVersion %d!",
                        conn_info->protocol);
        ret = -1;
    }

    /* Connection gracefully closed (ret==0) or connection error (ret==-1) or
     * just partial receive of bytestream.*/
    if (ret != len)
    {
        /*
         * Should never happen except with TLS, given that we are using
         * SSL_MODE_AUTO_RETRY and that transaction payload < CF_BUFSIZE < TLS
         * record size, it can currently only happen if the other side does
         * TLSSend(wrong_number) for the transaction.
         *
         * TODO IMPORTANT terminate TLS session in that case.
         */
        Log(LOG_LEVEL_ERR,
            "Partial transaction read %d != %d bytes!",
            ret, len);
        conn_info->status = CONNECTIONINFO_STATUS_BROKEN;
        return -1;
    }

    LogRaw(LOG_LEVEL_DEBUG, "ReceiveTransaction data: ", buffer, ret);

    return ret;
}

/* BWlimit global variables

  Throttling happens for all network interfaces, all traffic being sent for
  any connection of this process (cf-agent or cf-serverd).
  We need a lock, to avoid concurrent writes to "bwlimit_next".
  Then, "bwlimit_next" is the absolute time (as of clock_gettime() ) that we
  are clear to send, after. It is incremented with the delay for every packet
  scheduled for sending. Thus, integer arithmetic will make sure we wait for
  the correct amount of time, in total.
 */

#ifndef _WIN32
static pthread_mutex_t bwlimit_lock = PTHREAD_MUTEX_INITIALIZER;
static struct timespec bwlimit_next = {0, 0L};
#endif

uint32_t bwlimit_kbytes = 0; /* desired limit, in kB/s */


/** Throttle traffic, if next packet happens too soon after the previous one
 * 
 *  This function is global, across all network operations (and interfaces, perhaps)
 *  @param tosend Length of current packet being sent out (in bytes)
 */

#ifdef CLOCK_MONOTONIC
# define PREFERRED_CLOCK CLOCK_MONOTONIC
#else
/* Some OS-es don't have monotonic clock, but we can still use the
 * next available one */
# define PREFERRED_CLOCK CLOCK_REALTIME
#endif

void EnforceBwLimit(int tosend)
{
    if (!bwlimit_kbytes)
    {
        /* early return, before any expensive syscalls */
        return;
    }

#ifdef _WIN32
    Log(LOG_LEVEL_WARNING, "Bandwidth limiting with \"bwlimit\" is not supported on Windows.");
    (void)tosend; // Avoid "unused" warning.
    return;
#else

    const uint32_t u_10e6 = 1000000L;
    const uint32_t u_10e9 = 1000000000L;
    struct timespec clock_now = {0, 0L};

    if (pthread_mutex_lock(&bwlimit_lock) == 0)
    {
        clock_gettime(PREFERRED_CLOCK, &clock_now);

        if ((bwlimit_next.tv_sec < clock_now.tv_sec) ||
            ( (bwlimit_next.tv_sec == clock_now.tv_sec) &&
              (bwlimit_next.tv_nsec < clock_now.tv_nsec) ) )
        {
            /* penalty has expired, we can immediately send data. But reset the timestamp */
            bwlimit_next = clock_now;
            clock_now.tv_sec = 0;
            clock_now.tv_nsec = 0L;
        }
        else
        {
            clock_now.tv_sec = bwlimit_next.tv_sec - clock_now.tv_sec;
            clock_now.tv_nsec = bwlimit_next.tv_nsec - clock_now.tv_nsec;
            if (clock_now.tv_nsec < 0L)
            {
                clock_now.tv_sec --;
                clock_now.tv_nsec += u_10e9;
            }
        }

        uint64_t delay = ((uint64_t) tosend * u_10e6) / bwlimit_kbytes; /* in ns */

        bwlimit_next.tv_sec += (delay / u_10e9);
        bwlimit_next.tv_nsec += (long) (delay % u_10e9);
        if (bwlimit_next.tv_nsec >= u_10e9)
        {
            bwlimit_next.tv_sec++;
            bwlimit_next.tv_nsec -= u_10e9;
        }

        if (bwlimit_next.tv_sec > 20)
        {
            /* Upper limit of 20sec for penalty. This will avoid huge wait if
             * our clock has jumped >minutes back in time. Still, assuming that
             * most of our packets are <= 2048 bytes, the lower bwlimit is bound
             * to 102.4 Bytes/sec. With 65k packets (rare) is 3.7kBytes/sec in
             * that extreme case.
             * With more clients hitting a single server, this lower bound is
             * multiplied by num of clients, eg. 102.4kBytes/sec for 1000 reqs.
             * simultaneously.
             */
            bwlimit_next.tv_sec = 20;
        }
        pthread_mutex_unlock(&bwlimit_lock);
    }

    /* Even if we push our data every few bytes to the network interface,
      the software+hardware buffers will queue it and send it in bursts,
      anyway. It is more likely that we will waste CPU sys-time calling
      nanosleep() for such short delays.
      So, sleep only if we have >1ms penalty
    */
    if (clock_now.tv_sec > 0 || ( (clock_now.tv_sec == 0) && (clock_now.tv_nsec >= u_10e6))  )
    {
        nanosleep(&clock_now, NULL);
    }
#endif // !_WIN32
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
    struct addrinfo *response = NULL, *ap;
    bool connected = false;
    int sd = -1;

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
        if (response != NULL)
        {
            freeaddrinfo(response);
        }
        return -1;
    }

    for (ap = response; !connected && ap != NULL; ap = ap->ai_next)
    {
        /* Convert address to string. */
        getnameinfo(ap->ai_addr, ap->ai_addrlen,
                    txtaddr, txtaddr_size,
                    NULL, 0, NI_NUMERICHOST);
        Log(LOG_LEVEL_VERBOSE,
            "Connecting to host %s, port %s as address %s",
            host, port, txtaddr);

        sd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
        if (sd == -1)
        {
            Log(LOG_LEVEL_ERR, "Couldn't open a socket to '%s' (socket: %s)",
                txtaddr, GetErrorStr());
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

                struct addrinfo *response2 = NULL, *ap2;
                int ret2 = getaddrinfo(BINDINTERFACE, NULL, &query2, &response2);
                if (ret2 != 0)
                {
                    Log(LOG_LEVEL_ERR,
                        "Unable to lookup interface '%s' to bind. (getaddrinfo: %s)",
                        BINDINTERFACE, gai_strerror(ret2));

                    if (response2 != NULL)
                    {
                        freeaddrinfo(response2);
                    }
                    assert(response);   /* first getaddrinfo was successful */
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
                assert(response2);     /* second getaddrinfo was successful */
                freeaddrinfo(response2);
            }

            connected = TryConnect(sd, connect_timeout * 1000,
                                   ap->ai_addr, ap->ai_addrlen);
            if (!connected)
            {
                Log(LOG_LEVEL_VERBOSE, "Unable to connect to address %s (%s)",
                    txtaddr, GetErrorStr());
                cf_closesocket(sd);
                sd = -1;
            }
        }
    }

    assert(response != NULL);           /* first getaddrinfo was successful */
    freeaddrinfo(response);

    if (connected)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Connected to host %s address %s port %s (socket descriptor %d)",
            host, txtaddr, port, sd);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,
            "Unable to connect to host %s port %s (socket descriptor %d)",
            host, port, sd);
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
    assert(sd != -1);
    assert(sa != NULL);

    if (sd >= FD_SETSIZE)
    {
        Log(LOG_LEVEL_ERR,
            "Open connections exceed FD_SETSIZE limit (%d >= %d)",
            sd, FD_SETSIZE);
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

    /* Connection succeeded, return to blocking mode. */
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
        Log(LOG_LEVEL_VERBOSE,
            "Failed to set socket timeout to %lu milliseconds.", ms);
        return -1;
    }

    return 0;
}
