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

/*
 * A test source file with alternative implementations of some functions. Those
 * that are affected are generally ifdef'ed out in the original source file by
 * TEST_CF_EXECD macro.
 */

#include <platform.h>
#include <cf-execd-runner.h>

static void *FeedSmtpDirectives(void *data)
{
    int socket = *(int *)data;

#define COND_SEND(X) if (send(socket, X, strlen(X), 0) == -1) \
    { \
        Log(LOG_LEVEL_ERR, "Failed to write SMTP directive in %s:%i", __FILE__, __LINE__); \
        return NULL; \
    } \
    else \
    { \
        fwrite(X, strlen(X), 1, stdout); \
    }

#define COND_RECV(X) if ((rcvd = recv(socket, X, sizeof(X), 0)) == -1) \
    { \
        Log(LOG_LEVEL_ERR, "Failed to read SMTP response in %s:%i", __FILE__, __LINE__); \
        return NULL; \
    } \
    else \
    { \
        fwrite(X, rcvd, 1, stdout); \
    }

    char recvbuf[CF_BUFSIZE];
    int rcvd;
    COND_SEND("220 test.com\r\n");
    COND_RECV(recvbuf);
    COND_SEND("250 Hello test.com, pleased to meet you\r\n");
    COND_RECV(recvbuf);
    COND_SEND("250 from@test.com... Sender ok\r\n");
    COND_RECV(recvbuf);
    COND_SEND("250 to@test.com... Recipient ok\r\n");
    COND_RECV(recvbuf);
    COND_SEND("354 Enter mail, end with \".\" on a line by itself\r\n");
    /* The end of DATA marker may be split across several recv() calls, so
     * track the last bytes of the stream rather than each chunk alone. */
    char last[5] = { '\0', '\0', '\0', '\r', '\n' };
    bool done = false;
    while (!done)
    {
        COND_RECV(recvbuf);
        if (rcvd == 0)
        {
            Log(LOG_LEVEL_ERR, "Connection closed before end of DATA in %s:%i", __FILE__, __LINE__);
            return NULL;
        }
        /* It's a sliding window:
         * For each received byte we left shift the buffer and append the new
         * byte to the end. The client waits for our reply after sending the
         * End-of-Data marker, so the marker can only appear at the end of
         * the received data. */
        for (int i = 0; i < rcvd; i++)
        {
            memmove(last, last + 1, sizeof(last) - 1);
            last[sizeof(last) - 1] = recvbuf[i];
        }
        done = (memcmp(last, "\r\n.\r\n", sizeof(last)) == 0);
    }
    COND_SEND("250 Message accepted for delivery\r\n");
    COND_RECV(recvbuf);
    COND_SEND("221 test.com closing connection\r\n");

#undef COND_SEND
#undef COND_RECV

    cf_closesocket(socket);
    free(data);

    return NULL;
}

int ConnectToSmtpSocket(ARG_UNUSED const ExecConfig *config)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
    {
        return -1;
    }

    int *thread_socket = xmalloc(sizeof(int));
    *thread_socket = sockets[1];

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    int ret = pthread_create(&thread, &attr, &FeedSmtpDirectives, thread_socket);
    pthread_attr_destroy(&attr);

    if (ret != 0)
    {
        free(thread_socket);
        cf_closesocket(sockets[0]);
        cf_closesocket(sockets[1]);
        return -1;
    }

    return sockets[0];
}
