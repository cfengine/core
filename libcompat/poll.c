/*
 * Emulate poll() using select().
 *
 * poll() was introduced in 1997. Some OS are missing this function (Windows
 * XP) and others have buggy implementation, e.g. Mac OS X until 10.4.
 *
 * See http://daniel.haxx.se/docs/poll-vs-select.html
 */


int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    /* According to POSIX, FD_* macros have undefined behaviour when
     * fd > FD_SETSIZE, since fd_set is Usually a bitfield of just
     * 1024 bits, which means that bad things happen when we have more
     * file descriptors open because of connection caching.

     * However on all OSes tested the macros work fine if the buffer
     * is allocated manually, and select() is bound only by limits set
     * in the kernel.

     * So this union is a hack to allocate the correct amount of
     * space, while still passing an fd_set type to select(). */
    const size_t fdset_size = MAX(sizeof(fd_set),
                                  ConnectionInfoSocket(conn->conn_info) / CHAR_BIT + 1);
    union {
        fd_set fdset;
        char buf[fdset_size];
    } myset;

    memset(&myset, 0, sizeof(myset));
    FD_ZERO(&myset.fdset);
    FD_SET(ConnectionInfoSocket(conn->conn_info), &myset.fdset);

    /* now wait for connect, but no more than tvp.sec */
    res = select(ConnectionInfoSocket(conn->conn_info) + 1, NULL, &myset.fdset, NULL, tvp);
}
