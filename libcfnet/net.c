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

#include <net.h>
#include <classic.h>
#include <tls_generic.h>
#include <connection_info.h>
#include <logging.h>
#include <misc_lib.h>

/*************************************************************************/

/**
 * @param len is the number of bytes to send, or 0 if buffer is a
 *        '\0'-terminated string so strlen(buffer) can used.
 */
int SendTransaction(const ConnectionInfo *conn_info, const char *buffer, int len, char status)
{
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
    Log(LOG_LEVEL_DEBUG, "SendTransaction header:'%s'", work);
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
            "ReceiveTransaction: Bad packet -- bogus header '%s'", proto);
        return -1;
    }

    if (len > CF_BUFSIZE - CF_INBAND_OFFSET)
    {
        Log(LOG_LEVEL_ERR,
            "ReceiveTransaction: Bad packet -- too long (len=%d)", len);
        return -1;
    }

    if (more != NULL)
    {
        if (status == 'm')
            *more = true;
        else
            *more = false;
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

/* BWlimit global variables

  Throttling happens for all network interfaces, all traffic being sent for
  any connection of this process (cf-agent or cf-serverd).
  We need a lock, to avoid concurrent writes to "bwlimit_next".
  Then, "bwlimit_next" is the absolute time (as of clock_gettime() ) that we
  are clear to send, after. It is incremented with the delay for every packet
  scheduled for sending. Thus, integer arithmetic will make sure we wait for
  the correct amount of time, in total.
 */

static pthread_mutex_t bwlimit_lock = PTHREAD_MUTEX_INITIALIZER;
static struct timespec bwlimit_next = {0, 0L};
uint32_t bwlimit_kbytes = 0; /* desired limit, in kB/s */ 


/** Throttle traffic, if next packet happens too soon after the previous one
 * 
 *  This function is global, accross all network operations (and interfaces, perhaps)
 *  @param tosend Length of current packet being sent out (in bytes)
 */

void EnforceBwLimit(int tosend)
{
    const uint32_t u_10e6 = 1000000L;
    const uint32_t u_10e9 = 1000000000L;
    struct timespec clock_now;

    if (!bwlimit_kbytes)
    {
        /* early return, before any expensive syscalls */
        return;
    }

    if (pthread_mutex_lock(&bwlimit_lock) == 0)
    {
        clock_gettime(CLOCK_MONOTONIC, &clock_now);

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

}


/*************************************************************************/

/*
  NB: recv() timeout interpretation differs under Windows: setting tv_sec to
  50 (and tv_usec to 0) results in a timeout of 0.5 seconds on Windows, but
  50 seconds on Linux.
*/

#ifdef __linux__

int SetReceiveTimeout(int fd, const struct timeval *tv)
{
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)tv, sizeof(struct timeval)))
    {
        return -1;
    }

    return 0;
}

#else

int SetReceiveTimeout(ARG_UNUSED int fd, ARG_UNUSED const struct timeval *tv)
{
    return 0;
}

#endif
