
#include <platform.h>
#include <server_code.h>

#include <cf3.extern.h>                 // BINDINTERFACE
#include <printsize.h>                  // PRINTSIZE
#include <systype.h>                    // CLASSTEXT
#include <signals.h>                    // GetSignalPipe
#include <cleanup.h>                    // DoCleanupAndExit
#include <ctype.h>                      // isdigit

#if HAVE_SYSTEMD_SD_DAEMON_H
#include <systemd/sd-daemon.h>          // sd_listen_fds
#endif

/* Wait up to a minute for an in-coming connection.
 *
 * @param sd The listening socket or -1.
 * @param tm_sec timeout in seconds
 * @retval > 0 In-coming connection.
 * @retval 0 No in-coming connection.
 * @retval -1 Error (other than interrupt).
 * @retval < -1 Interrupted while waiting.
 */
int WaitForIncoming(int sd, time_t tm_sec)
{
    Log(LOG_LEVEL_DEBUG, "Waiting at incoming select...");
    struct timeval timeout = { .tv_sec = tm_sec };
    int signal_pipe = GetSignalPipe();
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(signal_pipe, &rset);

    /* sd might be -1 if "listen" attribute in body server control is set
     * to off (enterprise feature for call-collected clients). */
    if (sd != -1)
    {
        FD_SET(sd, &rset);
    }

    int result = select(MAX(sd, signal_pipe) + 1,
                        &rset, NULL, NULL, &timeout);
    if (result == -1)
    {
        return (errno == EINTR) ? -2 : -1;
    }
    assert(result >= 0);

    /* Empty the signal pipe, it is there to only detect missed
     * signals in-between checking IsPendingTermination() and calling
     * select(). */
    unsigned char buf;
    while (recv(signal_pipe, &buf, 1, 0) > 0)
    {
        /* skip */
    }

    /* We have an incoming connection if select() marked sd as ready: */
    if (sd != -1 && result > 0 && FD_ISSET(sd, &rset))
    {
        return 1;
    }
    return 0;
}

/**
 * Orders 'struct addrinfo *' linked list in a descending order based on the
 * ai_family, prefering IPV6.
 */
static void OrderAddrInfoResponsesByAIfamily(struct addrinfo **first_response)
{
    /* Below is just a trivial implementation of the Bubble-sort algorithm to
     * sort the linked list. */
    struct addrinfo *first = *first_response;
    bool change = true;
    while (change)
    {
        change = false;
        struct addrinfo *item = first;
        struct addrinfo *prev = NULL;
        while (item->ai_next != NULL)
        {
            /* AF_INET6 should be greater than AF_INET, but let's not rely on
             * that and use a direct comparison. */
            if ((item->ai_family == AF_INET) && (item->ai_next->ai_family == AF_INET6))
            {
                struct addrinfo *orig_next = item->ai_next;
                item->ai_next = orig_next->ai_next;
                orig_next->ai_next = item;
                if (prev != NULL)
                {
                    prev->ai_next = orig_next;
                }
                else
                {
                    first = orig_next;
                }
                prev = orig_next;
                change = true;
            }
            else
            {
                item = item->ai_next;
            }
        }
    }
    *first_response = first;
}

/**
 * @param bind_address address to bind to or %NULL to use the default BINDINTERFACE
 */
static int OpenReceiverChannel(char *bind_address)
{
    struct addrinfo *response = NULL, *ap;
    struct addrinfo query = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    if (bind_address == NULL)
    {
        bind_address = BINDINTERFACE;
    }

    /* Listen to INADDR(6)_ANY if BINDINTERFACE unset. */
    char *ptr = NULL;
    if (bind_address[0] != '\0')
    {
        ptr = bind_address;

        /* Just a quick check if the string looks like an IPv4 address to see if
         * we should use AI_NUMERICHOST or not. IPv6 addresses could use it too,
         * but they are not so easy to recognize and the proper check would be
         * more expensive than the optimization. */
        bool is_numeric_host = true;
        for (char *c = ptr; is_numeric_host && (*c != '\0'); c++)
        {
            is_numeric_host = ((*c == '.') || isdigit(*c));
        }
        if (is_numeric_host)
        {
            query.ai_flags |= AI_NUMERICHOST;
        }
    }

    /* Resolve listening interface. */
    int gres = getaddrinfo(ptr, CFENGINE_PORT_STR, &query, &response);
    if (gres != 0)
    {
        Log(LOG_LEVEL_ERR, "DNS/service lookup failure. (getaddrinfo: %s)",
            gai_strerror(gres));
        if (response)
        {
            freeaddrinfo(response);
        }
        return -1;
    }

    /* Make sure IPV6 addresses/interfaces are preferred over IPV4 ones (binding
     * to the IPV6 address makes connections to the IPV4 equivalent work too, in
     * particular for INADDR6_ANY (::) and INADDR_ANY (0.0.0.0/0). */
    OrderAddrInfoResponsesByAIfamily(&response);

    int sd = -1;
    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        sd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
        if (sd == -1)
        {
            if (ap->ai_family == AF_INET)
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to create socket for binding to an IPV4 interface");
            }
            else if (ap->ai_family == AF_INET6)
            {
                Log(LOG_LEVEL_VERBOSE, "Failed to create socket for binding to an IPV6 interface");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Failed to create socket for binding to an interface of ai_family %d",
                    ap->ai_family);
            }
            continue;
        }

       #ifdef IPV6_V6ONLY
        /* Properly implemented getaddrinfo(AI_PASSIVE) should return the IPV6
           loopback address first. Some platforms (notably Windows) don't
           listen to both address families when binding to it and need this
           flag. Some other platforms won't even honour this flag
           (openbsd). */
        if (bind_address[0] == '\0' && ap->ai_family == AF_INET6)
        {
            int no = 0;
            if (setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY,
                           &no, sizeof(no)) == -1)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Failed to clear IPv6-only flag on listening socket"
                    " (setsockopt: %s)",
                    GetErrorStr());
            }
        }
        #endif

        int yes = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
                       &yes, sizeof(yes)) == -1)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Socket option SO_REUSEADDR was not accepted. (setsockopt: %s)",
                GetErrorStr());
        }

        struct linger cflinger = {
            .l_onoff = 1,
            .l_linger = 60
        };
        if (setsockopt(sd, SOL_SOCKET, SO_LINGER,
                       &cflinger, sizeof(cflinger)) == -1)
        {
            Log(LOG_LEVEL_INFO,
                "Socket option SO_LINGER was not accepted. (setsockopt: %s)",
                GetErrorStr());
        }

        if (bind(sd, ap->ai_addr, ap->ai_addrlen) != -1)
        {
            if (WouldLog(LOG_LEVEL_DEBUG))
            {
                /* Convert IP address to string, no DNS lookup performed. */
                char txtaddr[CF_MAX_IP_LEN] = "";
                getnameinfo(ap->ai_addr, ap->ai_addrlen,
                            txtaddr, sizeof(txtaddr),
                            NULL, 0, NI_NUMERICHOST);
                Log(LOG_LEVEL_DEBUG, "Bound to address '%s' on '%s' = %d", txtaddr,
                    CLASSTEXT[VSYSTEMHARDCLASS], VSYSTEMHARDCLASS);
            }
            break;
        }
        Log(LOG_LEVEL_ERR, "Could not bind server address. (bind: %s)", GetErrorStr());
        cf_closesocket(sd);
        sd = -1;
    }

    if (sd == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Failed to bind to all attempted addresses (bind specification: '%s'",
            bind_address);
    }

    assert(response != NULL);               /* getaddrinfo() was successful */
    freeaddrinfo(response);
    return sd;
}

/**
 * @param queue_size   length of the queue for pending connections
 * @param bind_address address to bind to or %NULL to use the default BINDINTERFACE
 */
int InitServer(size_t queue_size, char *bind_address)
{
#if HAVE_SYSTEMD_SD_DAEMON_H
    int n = sd_listen_fds(0);
    if (n > 1)
    {
        Log(LOG_LEVEL_ERR, "Too many file descriptors received from systemd");
    }
    else if (n == 1)
    {
        // we can check here that we have a socket with sd_is_socket_inet(3)
        // but why should we limit ourselves
        return SD_LISTEN_FDS_START;
    }
    else
#endif // HAVE_SYSTEMD_SD_DAEMON_H
    {
        int sd = OpenReceiverChannel(bind_address);

        if (sd == -1)
        {
            /* More detailed info is logged in case of error in
             * OpenReceiverChannel() */
            Log(LOG_LEVEL_ERR, "Unable to start server");
        }
        else if (listen(sd, queue_size) == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed to listen on the '%s' address (listen: %s)",
                bind_address, GetErrorStr());
            cf_closesocket(sd);
        }
        else
        {
            return sd;
        }
    }

    DoCleanupAndExit(EXIT_FAILURE);
}
