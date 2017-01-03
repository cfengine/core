#include <test.h>
#include <passopenfile.h>

#include <file_lib.h>
#include <prototypes3.h>

#ifndef __MINGW32__
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#endif /* !__MINGW32__ */
#include <unistd.h>

/* Whether to re-use Unix Domain Sockets
 *
 * Strictly, the API is only advertised to be fit for sending *one*
 * descriptor over each UDS; but there's no reason it shouldn't work
 * for more - and stressing it is good.  Enabling REUSE_UDS thus
 * semi-abuses the API to stress it a bit more.
#define REUSE_UDS
 */

/* Globals used by the tests. */

static int SPAWNED_PID = -1; /* child used as other process in each test */
static char DIALUP[PATH_MAX] = ""; /* synchronisation file for listen()/connect() */
static bool DIALEDUP = false;

/* TODO - not really testing this API, but testing we can use it
 * securely: test that the listener can secure its socket to only be
 * connect()ed to by the same user; likewise by one in its group.
 * Probably put that test in some other file ! */


/* Ensure no stray child is running. */
static void wait_for_child(bool impatient)
{
    if (SPAWNED_PID == -1)                      /* it's BAD to run kill(-1) */
    {
        return;
    }

    while(true)
    {
        if (impatient)
        {
            errno = 0;
            int ret = kill(SPAWNED_PID, SIGKILL);
            if (ret == -1 && errno == ESRCH)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Child process to be killed does not exist (PID %jd)",
                    (intmax_t) SPAWNED_PID);

                break;                            /* process does not exist */
            }

            Log(LOG_LEVEL_NOTICE,
                "Killed previous child process (PID %jd)",
                (intmax_t) SPAWNED_PID);
        }

        if (waitpid(SPAWNED_PID, NULL, 0) > 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Child process is dead and has been reaped (PID %jd)",
                (intmax_t) SPAWNED_PID);

            break;                               /* zombie reaped properly  */
        }
    }

    SPAWNED_PID = -1;
}

static bool wait_for_io(int whom, bool write)
{
    struct timeval tv;
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(whom, &fds);
    tv.tv_sec = 10; /* 10s timeout for select() */
    tv.tv_usec = 0;

    int ret;
    if (write)
    {
        ret = select(whom + 1, NULL, &fds, NULL, &tv);
    }
    else
    {
        ret = select(whom + 1, &fds, NULL, NULL, &tv);
    }

    if (ret < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed select: %s", GetErrorStr());
    }
    else if (!FD_ISSET(whom, &fds))
    {
        Log(LOG_LEVEL_ERR, "Timed out select() for descriptor %d", whom);
    }

    return ret > 0 && FD_ISSET(whom, &fds);
}

/* Whoever connect()s needs to wait for the other to bind() and listen(): */

static bool wait_for_dialup(bool write)
{
    assert(DIALUP[0] != '\0');

    for (int i = 5; i-- > 0;)
    {
        if (access(DIALUP, write ? W_OK : R_OK) == 0) /* bind() has happened */
        {
            sleep(1); /* To let listen() happen, too. */
            return true;
        }
        sleep(1);
    }
    Log(LOG_LEVEL_ERR,
        "Failed to access %s as synchronisation file: %s",
        DIALUP, GetErrorStr());

    return false;
}

/* Used, via atexit(), to ensure we delete the file if we create it at the
 * exit of all processes, either parent or children. */

static void clear_listener(void)
{
    if (DIALEDUP)
    {
        assert(DIALUP[0] != '\0');
        Log(LOG_LEVEL_VERBOSE, "Cleaning up UDS file");
        unlink(DIALUP);
        DIALEDUP = false;
    }
}

static void clear_previous_test(void)
{
    /* KILL possible child. */
    wait_for_child(true);

    /* Clear possible UDS file created by this process (parent). */
    if (DIALEDUP)
    {
        clear_listener();
    }

    /* What if child got KILLed and never cleaned its UDS file? */
    struct stat statbuf;
    if (stat(DIALUP, &statbuf) != -1)
    {
        Log(LOG_LEVEL_NOTICE, "UDS file from previous test still exists, "
            "maybe child did not clean up? Removing and continuing...");
        unlink(DIALUP);
    }
}

/* Choose a file for the Unix Domain Socket, by which to connect() to the
 * bind()er. */

static bool choose_dialup_UDS_file(void)
{
    if (DIALUP[0] == '\0')
    {
        /* Using insecure tempnam().
         *
         * There really isn't any sensible alternative.  What we need
         * is a file *name* for use in the address we bind() and
         * connect() to.  A file descriptor (from mkstemp) or FILE*
         * (from tmpfile) does us no good.  The former would at least
         * give us a unique filename, but we'd be unlink()ing the file
         * it creates in order to actually bind() to it, and we'd have
         * the exact same race condition as tempnam between when we
         * bind() and when we connect().  This is a deficiency of the
         * UDS APIs that we just have to live with.  In production, we
         * need to make sure we use a UDS based on a file somewhere we
         * secure suitably (e.g. in a directory owned by root and
         * inaccessible to anyone else).  The "cfpof" stands for
         * CF(Engine) Pass Open File, in case you wondered.
         */
        char *using = tempnam("/tmp", "cfpof");
        if (using == NULL)
        {
            Log(LOG_LEVEL_ERR, "Failed tempnam: %s", GetErrorStr());
        }
        else
        {
            assert(using[0]); /* non-empty */
            strlcpy(DIALUP, using, sizeof(DIALUP));
            free(using);
            Log(LOG_LEVEL_VERBOSE, "Synchronising UDS via %s", DIALUP);
        }
    }
    return DIALUP[0] != '\0';
}

/* Set up listen()ing socket; used by one process in each test. */

static int setup_listener(void)
{
    /* Create "server" socket, bind() it, listen() on it, return it. */
    assert(DIALUP[0] != '\0');
    {
        /* The Unix Domain Socket file was cleaned up in the previous test. */
        struct stat statbuf;
        bool UDS_exists = (stat(DIALUP, &statbuf) != -1);
        assert_false(UDS_exists);
    }

    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server >= 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Opened %d to bind to and listen on.", server);
        if (server < FD_SETSIZE) /* else problem for FD_SET when select()ing */
        {
            struct sockaddr_un address = { 0 };
            assert(strlen(DIALUP) < sizeof(address.sun_path));
            address.sun_family = AF_UNIX;
            strlcpy(address.sun_path, DIALUP, sizeof(address.sun_path));

            Log(LOG_LEVEL_VERBOSE, "Attempting to bind(%d, ...)", server);
            if (bind(server, (struct sockaddr *)&address, sizeof(address)) == 0)
            {
                /* That's created the file DIALUP, that we now have a
                 * duty to tidy away. */
                DIALEDUP = true;

                Log(LOG_LEVEL_VERBOSE, "Calling listen(%d, 1)", server);
                if (listen(server, 2) == 0)
                {
                    /* Success */
                    return server;
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "Failed listen: %s", GetErrorStr());
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Failed bind: %s", GetErrorStr());
            }
        }
        cf_closesocket(server);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed socket: %s", GetErrorStr());
    }
    return -1;
}

/* Set up Unix Domain Sockets.
 *
 * One of parent and child uses setup_accept(); the other uses
 * setup_connect().  Each gets a UDS back.  One wants to write to the
 * result, the other wants to read from it.  These two two-way choices
 * give us four tests. */

static int setup_accept(int server, bool write)
{
    /* When listening socket is ready, accept(); wait for the result
     * to be ready to read/write. */
    assert(server >= 0);
    Log(LOG_LEVEL_VERBOSE, "Calling accept(%d, NULL, NULL)", server);
    if (wait_for_io(server, false))
    {
        int uds = accept(server, NULL, NULL);
        if (uds == -1)
        {
            Log(LOG_LEVEL_ERR, "Failed accept: %s", GetErrorStr());
        }
        else if (uds < FD_SETSIZE && /* else problem for FD_SET when select()ing */
                 wait_for_io(uds, write))
        {
            Log(LOG_LEVEL_VERBOSE, "Ready to use accept()ed UDS %d", uds);

            return uds; /* Success */
        }
        else
        {
            Log(LOG_LEVEL_ERR,
                "Unable to %s on accept()ed Unix Domain Socket",
                write ? "write" : "read");
            cf_closesocket(uds);
        }
    }
    return -1;
}

static int setup_connect(bool write)
{
    /* Create socket, connect() to the listening socket, wait until
     * ready to read/write */
    int uds = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds >= 0)
    {
        if (uds < FD_SETSIZE && /* else problem for FD_SET when select()ing */
            wait_for_dialup(write))
        {
            struct sockaddr_un address;
            assert(strlen(DIALUP) < sizeof(address.sun_path));
            address.sun_family = AF_UNIX;
            strlcpy(address.sun_path, DIALUP, sizeof(address.sun_path));

            if (connect(uds, (struct sockaddr *)&address, sizeof(address)) == 0 &&
                wait_for_io(uds, write))
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Ready to use connect()ed UDS %d", uds);

                return uds; /* Success */
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "Failed connect (or select): %s", GetErrorStr());
            }
        }
        cf_closesocket(uds);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed socket: %s", GetErrorStr());
    }
    return -1;
}

/* Wrapper to set up a Unix Domain Socket.
 *
 * Variants on the child/parent processes are given a listening socket
 * on which to accept() or -1 to tell them to connect().  This
 * function mediates that choice for them.  It probably gets inlined. */
static int setup_uds(int server, bool write)
{
    return server < 0 ? setup_connect(write) : setup_accept(server, write);
}

/* Reverse of setup_pipe: */

static void close_pipe(int pair[2])
{
    int idx = 2;
    while (idx-- > 0)
    {
        if (pair[idx] >= 0)
        {
            cf_closesocket(pair[idx]);
            pair[idx] = -1;
        }
    }
}

/* Socket pair used by the conversation between processes over the
 * exchanged descriptors. */

static bool setup_pipe(int pipe[2])
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe) == 0)
    {
        /* No need to make them non-blocking ! */
        return true;
    }
    Log(LOG_LEVEL_ERR, "Failed socketpair: %s", GetErrorStr());
    return false;
}

/* Check we can use transferred sockets.
 *
 * For the parent-child dialogs, we only try one direction.  Each end
 * actually has both ends of the pipe, but trying to communicate both
 * ways gets mixed up with the communication we're doing with the
 * given ends. */
const char FALLBACK[] = "\0Fallback Message";

/* Parent's half: checks everything. */
static bool check_hail(int sock, const char *message)
{
    const char *msg = message ? message : (FALLBACK + 1);
    const size_t msglen = message ? strlen(message) : sizeof(FALLBACK);
    const char noise[] = "The quick brown fox jumps over the lazy dog";
    char buffer[80];
    assert(msglen && msglen < sizeof(buffer));
    strlcpy(buffer, noise, sizeof(buffer));

    errno = 0;
    ssize_t sent;
    if (!wait_for_io(sock, true))
    {
        /* wait_for_io already Log()ged. */

    }
    else if (0 > (sent = send(sock, "hello", 6, 0)))
    {
        Log(LOG_LEVEL_ERR,
            "Parent failed to send 'hello': %s",
            GetErrorStr());
    }
    else if (sent != 6)
    {
        Log(LOG_LEVEL_ERR,
            "Parent sent wrong length (%zd != 5) for 'hello'",
            sent);
    }
    else if (!wait_for_io(sock, false))
    {
        /* wait_for_io already Log()ged. */

    }
    else if (0 > (sent = recv(sock, buffer, sizeof(buffer), 0)))
    {
        Log(LOG_LEVEL_ERR,
            "Parent failed to receive '%s': %s",
            msg, GetErrorStr());
    }
    else if (sent != msglen)
    {
        buffer[MIN(sent, sizeof(buffer) - 1)] = '\0';
        Log(LOG_LEVEL_ERR,
            "Parent received wrong length (%zd != %zd) for '%s' != %s",
            sent, msglen,
            (message || buffer[0]) ? buffer : (buffer + 1),
            msg);
    }
    else if (strcmp(buffer + msglen, noise + msglen) != 0)
    {
        buffer[sizeof(buffer) - 1] = '\0';
        Log(LOG_LEVEL_ERR, "Parent recv() trampled buffer: %s", buffer);
    }
    else
    {
        buffer[msglen] = '\0';

        if (message ? strcmp(buffer, message) == 0 :
            (buffer[0] == '\0' &&
             strcmp(buffer + 1, FALLBACK + 1) == 0))
        {
            return true;
        }
        Log(LOG_LEVEL_ERR,
            "Parent received wrong text '%s' != '%s'",
            (message || buffer[0]) ? buffer : (buffer + 1),
            msg);
    }

    return false;
}

/* Dummy conversational partner for check_hail (used in child). */
static void child_hail(int sock, const char *message)
{
    const size_t msglen = message ? strlen(message) : sizeof(FALLBACK);
    char buffer[80];
    if (wait_for_io(sock, false) &&
        recv(sock, buffer, sizeof(buffer), 0) == 6 &&
        strcmp(buffer, "hello") == 0 &&
        wait_for_io(sock, true))
    {
        send(sock, message ? message : FALLBACK, msglen, 0);
    }
}

/* Similar but for a parent talking to self after child died.
 *
 * For this one, we test two-way communications, since we're inside a
 * single process, where that should all work fine.  OTOH, we don't
 * bother with the buffer-overrun checks and other complications
 * addressed by check_hail(). */

static bool self_hail(int pair[2])
{
    /* We send these with sizeof(), so include the '\0' endings.
     * They should thus be received with those endings at buffer[sent - 1].
     */
    const char exclaim[] = "Dead! Dead!",
        lament[] = "And never called me mother";
    char buffer[80];
    ssize_t sent;
    if (!wait_for_io(pair[0], true))
    {
        /* wait_for_io already Log()ged. */

    }
    else if (0 > (sent = send(pair[0], exclaim, sizeof(exclaim), 0)))
    {
        Log(LOG_LEVEL_ERR, "Failed to send exclamation (send: %s)",
            GetErrorStr());
    }
    else if (sent != sizeof(exclaim))
    {
        Log(LOG_LEVEL_ERR, "Sent wrong-sized exclamation: %zd != %zu",
            sent, sizeof(exclaim));
    }
    else if (!wait_for_io(pair[1], false))
    {
        /* wait_for_io already Log()ged. */

    }
    else if (0 > (sent = recv(pair[1], buffer, sizeof(buffer), 0)))
    {
        Log(LOG_LEVEL_ERR, "Failed to receive exclamation (recv: %s)",
            GetErrorStr());
    }
    else if (sent != sizeof(exclaim) || buffer[sent - 1])
    {
        buffer[MIN(sent, sizeof(buffer) - 1)] = '\0';
        Log(LOG_LEVEL_ERR, "Received wrong-sized exclamation (%zd != %zu): %s",
            sent, sizeof(exclaim), buffer);
    }
    else if (strcmp(buffer, exclaim) != 0)
    {
        buffer[MIN(sent, sizeof(buffer) - 1)] = '\0';
        Log(LOG_LEVEL_ERR, "Mismatch in exclamation: '%s' != '%s'",
            buffer, exclaim);
    }
    else if (!wait_for_io(pair[1], true))
    {
        /* wait_for_io already Log()ged. */

    }
    else if (0 > (sent = send(pair[1], lament, sizeof(lament), 0)))
    {
        Log(LOG_LEVEL_ERR, "Failed to send lament (send: %s)",
            GetErrorStr());
    }
    else if (sent != sizeof(lament))
    {
        Log(LOG_LEVEL_ERR, "Sent wrong-sized lament: %zd != %zu",
            sent, sizeof(lament));
    }
    else if (!wait_for_io(pair[0], false))
    {
        /* wait_for_io already Log()ged. */

    }
    else if (0 > (sent = recv(pair[0], buffer, sizeof(buffer), 0)))
    {
        Log(LOG_LEVEL_ERR, "Failed to receive lament (recv: %s)",
            GetErrorStr());
    }
    else if (sent != sizeof(lament))
    {
        buffer[MIN(sent, sizeof(buffer) - 1)] = '\0';
        Log(LOG_LEVEL_ERR, "Received wrong-sized lament (%zd != %zu): %s",
            sent, sizeof(lament), buffer);
    }
    else if (strcmp(buffer, lament) != 0)
    {
        Log(LOG_LEVEL_ERR, "Mismatch in lament: '%s' != '%s'",
            buffer, lament);
    }
    else
    {
        return true;
    }
    return false;
}

/* Variants on the child/parent process.
 *
 * In each case the sending process creates a socket pair (setup_pipe)
 * and passes both ends to the other process; after that, they have
 * the conversation above (see *_hail) using these sockets.
 *
 * For the two-process tests, three things are tested (each setting a
 * bool variable for the parent): sending a descriptor attending to
 * any accompanying message (word), sending a descriptor ignoring
 * message (none) and using one each of the shared descriptors to
 * communicate between processes (chat).  For the test by a parent
 * after its child has died, only success of the conversation is
 * recorded to pass back to the parent.
 *
 * Naturally, the details (particularly synchronisation) are trickier
 * than that.  See REUSE_UDS, above, for one complication.
 *
 * We have one parent_*()/child_*() pair for the parent as sending
 * process with the child receiving, one pair the other way round and
 * one for the parent that outlives its child.  In each test, one of
 * parent and child is passed a listening socket on which to accept()
 * UDS connections; the other is passed -1 and connect()s to it.  The
 * former has a duty to close the listening socket.  For each of the
 * resulting combinations, the two-process tests then have one test in
 * which the non-ignored message is NULL (*_silent) and another in
 * which it has some content (*_message). */

static void child_for_outlive(int server)
{
    /* Parent takes, so the child must send. */
    int pipe[2];
    if (setup_pipe(pipe))
    {
        int uds = setup_uds(server, true);
        if (uds >= 0)
        {
            PassOpenFile_Put(uds, pipe[1], NULL);
#ifndef REUSE_UDS
            cf_closesocket(uds);
        }
        uds = setup_uds(server, true);
        if (uds >= 0)
        {
#endif /* REUSE_UDS */
#ifdef REUSE_UDS
            wait_for_io(uds, true) &&
#endif
                PassOpenFile_Put(uds, pipe[0], NULL);
            cf_closesocket(uds);
        }
        close_pipe(pipe);
    }

    if (server != -1)
    {
        cf_closesocket(server);
    }
}

static bool parent_outlive(int server, bool *chat)
{
    int pair[2] = { -1, -1 };
    bool result = true;
    *chat = false;

    int uds = setup_uds(server, false);
#ifdef REUSE_UDS
    if (server != -1)
    {
        cf_closesocket(server);
    }
#endif
    if (uds < 0)
    {
        result = false;
    }
    else
    {
        char *text = NULL;
        pair[1] = PassOpenFile_Get(uds, &text);
        free(text);
        text = NULL;
#ifndef REUSE_UDS
        cf_closesocket(uds);
    }
    uds = setup_uds(server, false);
    if (server != -1)
    {
        cf_closesocket(server);
    }

    if (uds < 0)
    {
        result = false;
    }
    else
    {
#endif /* REUSE_UDS */
        pair[0] =
#ifdef REUSE_UDS
            !wait_for_io(uds, false) ? -1 :
#endif
            PassOpenFile_Get(uds, NULL);

        cf_closesocket(uds);
        if (pair[0] < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Parent failed to receive descriptor (ignoring text)");
        }
        else if (pair[1] >= 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Parent received both descriptors");
            wait_for_child(false); /* Don't kill, be patient. */
            *chat = self_hail(pair);
        }
    }

    close_pipe(pair);
    return result;
}

/* Child sends descriptors to parent.
 *
 * In this case, verifying we get the right message can be done
 * directly by the parent on receiving the message. */

static void child_for_take(int server, const char *message)
{
    /* Of course, for the parent to take, the child must send. */
    int pipe[2];
    if (setup_pipe(pipe))
    {
        bool sent = false;
        int uds = setup_uds(server, true);
        if (uds >= 0)
        {
            sent = PassOpenFile_Put(uds, pipe[1], message);
#ifndef REUSE_UDS
            cf_closesocket(uds);
        }
        uds = setup_uds(server, true);
        if (uds >= 0)
        {
#endif /* REUSE_UDS */
            sent =
#ifdef REUSE_UDS
                wait_for_io(uds, true) &&
#endif
                PassOpenFile_Put(uds, pipe[0], message) && sent;
            cf_closesocket(uds);
        }
        if (sent)
        {
            Log(LOG_LEVEL_VERBOSE, "Child delivered both descriptors");
            child_hail(pipe[0], message);
        }
        close_pipe(pipe);
    }

    if (server != -1)
    {
        cf_closesocket(server);
    }
}

static bool parent_take(int server, const char *message,
                        bool *word, bool *none, bool *chat)
{
    int pair[2] = { -1, -1 };
    bool result = true;
    *word = *none = *chat = false;

    int uds = setup_uds(server, false);
#ifdef REUSE_UDS
    if (server != -1)
    {
        cf_closesocket(server);
    }
#endif
    if (uds < 0)
    {
        result = false;
    }
    else
    {
        char *text = NULL;
        pair[1] = PassOpenFile_Get(uds, &text);
        *word = pair[1] >= 0 && (message ? text && strcmp(text, message) == 0 : !text);
        free(text);
        text = NULL;
#ifndef REUSE_UDS
        cf_closesocket(uds);
    }
    uds = setup_uds(server, false);
    if (server != -1)
    {
        cf_closesocket(server);
    }

    if (uds < 0)
    {
        result = false;
    }
    else
    {
#endif /* REUSE_UDS */
        pair[0] =
#ifdef REUSE_UDS
            !wait_for_io(uds, false) ? -1 :
#endif
            PassOpenFile_Get(uds, NULL);

        cf_closesocket(uds);
        if (pair[0] < 0)
        {
            Log(LOG_LEVEL_ERR, "Parent failed to receive descriptor (ignoring text)");
        }
        else
        {
            *none = true;
            if (pair[1] >= 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Parent received both descriptors");
                *chat = check_hail(pair[1], message);
            }
        }
    }

    close_pipe(pair);
    return result;
}

/* Parent sends to child.
 *
 * Verifying the message is transmitted correctly depends on the child
 * sending the message back as part of the conversation after we
 * exchange descriptors. */

static void child_for_send(int server)
{
    /* Of course, for the parent to send, the child must take. */
    int pair[2] = { -1, -1 };

    int uds = setup_uds(server, false);
#ifdef REUSE_UDS
    if (server != -1)
    {
        cf_closesocket(server);
    }
#endif
    if (uds >= 0)
    {
        pair[1] = PassOpenFile_Get(uds, NULL);
        if (pair[1] < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Child failed to receive descriptor (ignoring text)");
        }
#ifndef REUSE_UDS
        cf_closesocket(uds);
    }

    uds = setup_uds(server, false);
    if (server >= 0)
    {
        cf_closesocket(server);
    }

    if (uds >= 0)
    {
#endif /* REUSE_UDS */
        char *text = NULL;
        pair[0] =
#ifdef REUSE_UDS
            !wait_for_io(uds, false) ? -1 :
#endif
            PassOpenFile_Get(uds, &text);
        cf_closesocket(uds);
        if (pair[0] < 0)
        {
            Log(LOG_LEVEL_ERR,
                "Child failed to receive descriptor");
        }
        else if (pair[1] >= 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Child received both descriptors (%s %s)",
                text ? "text:" : "no",
                text ? text : "text");

            child_hail(pair[1], text);
        }
        free(text);
    }
    close_pipe(pair);
}

static bool parent_send(int server, const char *message,
                        bool *word, bool *none, bool *chat)
{
    /* We have a duty to close(server) if not -1 */
    bool result = false;
    int pipe[2];
    *word = *none = *chat = false;

    if (setup_pipe(pipe))
    {
        result = true;
        *word = *none = *chat = false;
        int uds = setup_uds(server, true);
        if (uds < 0)
        {
            result = false;
        }
        else
        {
            *none = PassOpenFile_Put(uds, pipe[1], message);
#ifndef REUSE_UDS
            cf_closesocket(uds);
        }
        uds = setup_uds(server, true);
        if (uds < 0)
        {
            result = false;
        }
        else
        {
#endif /* REUSE_UDS */
            *word =
#ifdef REUSE_UDS
                wait_for_io(uds, true) &&
#endif
                PassOpenFile_Put(uds, pipe[0], message);
            cf_closesocket(uds);
        }

        if (*word && *none)
        {
            Log(LOG_LEVEL_VERBOSE, "Parent delivered both descriptors");
            *chat = check_hail(pipe[0], message);
        }
        close_pipe(pipe);
    }

    if (server != -1)
    {
        cf_closesocket(server);
    }

    return result;
}

/* The actual tests, built on those tools.
 *
 * Classified according to whether parent is listen()ing or
 * connect()ing, with the child doing the other, and whether parent is
 * sending or taking descriptors from child.  The parent is
 * responsible for all testing, although the child helps out via the
 * chatter on exchanged sockets. */

static void test_take_listen_message(void)
{
    clear_previous_test();
    const char message[] = "verbiage";
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        child_for_take(-1, message);
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Parent failed to set up listener");
        }
        else
        {
            bool word, none, chat;
            if (parent_take(server, message, &word, &none, &chat))
            {
                assert_true(word && "can receive fd with a message");
                assert_true(none && "can receive fd ignoring message");
                assert_true(chat && "can use the received fds");

                if (waitpid(new_pid, NULL, 1) > 0)
                {
                    SPAWNED_PID = -1;
                }
                return;
            }
            Log(LOG_LEVEL_ERR, "Failed to set up UDS");
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_take_connect_message(void)
{
    clear_previous_test();
    const char message[] = "waffle";
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Child failed to set up listener");
        }
        else
        {
            child_for_take(server, message);
        }
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        bool word, none, chat;
        if (parent_take(-1, message, &word, &none, &chat))
        {
            assert_true(word && "can receive fd with a message");
            assert_true(none && "can receive fd ignoring message");
            assert_true(chat && "can use the received fds");

            if (waitpid(new_pid, NULL, 1) > 0)
            {
                SPAWNED_PID = -1;
            }
            return;
        }
        Log(LOG_LEVEL_ERR, "Failed to set up parent");
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_send_listen_message(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        child_for_send(-1);
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Parent failed to set up listener");
        }
        else
        {
            bool word, none, chat;
            if (parent_send(server, "mumble", &word, &none, &chat))
            {
                assert_true(word && "can transmit fd with a message");
                assert_true(none && "can transmit fd ignoring message");
                assert_true(chat && "can use the transmitted fds");

                if (waitpid(new_pid, NULL, 1) > 0)
                {
                    SPAWNED_PID = -1;
                }
                return;
            }
            Log(LOG_LEVEL_ERR, "Failed to set up parent");
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_send_connect_message(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Child failed to set up listener");
        }
        else
        {
            child_for_send(server);
        }
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        bool word, none, chat;
        if (parent_send(-1, "mutter", &word, &none, &chat))
        {
            assert_true(word && "can transmit fd with a message");
            assert_true(none && "can transmit fd ignoring message");
            assert_true(chat && "can use the transmitted fds");

            if (waitpid(new_pid, NULL, 1) > 0)
            {
                SPAWNED_PID = -1;
            }
            return;
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_take_listen_silent(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        child_for_take(-1, NULL);
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Parent failed to set up listener");
        }
        else
        {
            bool word, none, chat;
            if (parent_take(server, NULL, &word, &none, &chat))
            {
                assert_true(word && "can receive fd with no message");
                assert_true(none && "can receive fd ignoring no message");
                assert_true(chat && "can use the received fds");

                if (waitpid(new_pid, NULL, 1) > 0)
                {
                    SPAWNED_PID = -1;
                }
                return;
            }
            Log(LOG_LEVEL_ERR, "Failed to set up UDS");
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_take_connect_silent(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Child failed to set up listener");
        }
        else
        {
            child_for_take(server, NULL);
        }
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        bool word, none, chat;
        if (parent_take(-1, NULL, &word, &none, &chat))
        {
            assert_true(word && "can receive fd with no message");
            assert_true(none && "can receive fd ignoring no message");
            assert_true(chat && "can use the received fds");

            if (waitpid(new_pid, NULL, 1) > 0)
            {
                SPAWNED_PID = -1;
            }
            return;
        }
        Log(LOG_LEVEL_ERR, "Failed to set up parent");
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_send_listen_silent(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        child_for_send(-1);
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Parent failed to set up listener");
        }
        else
        {
            bool word, none, chat;
            if (parent_send(server, NULL, &word, &none, &chat))
            {
                assert_true(word && "can transmit fd with no message");
                assert_true(none && "can transmit fd ignoring no message");
                assert_true(chat && "can use the transmitted fds");

                if (waitpid(new_pid, NULL, 1) > 0)
                {
                    SPAWNED_PID = -1;
                }
                return;
            }
            Log(LOG_LEVEL_ERR, "Failed to set up parent");
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_send_connect_silent(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Child failed to set up listener");
        }
        else
        {
            child_for_send(server);
        }
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        bool word, none, chat;
        if (parent_send(-1, NULL, &word, &none, &chat))
        {
            assert_true(word && "can transmit fd with no message");
            assert_true(none && "can transmit fd ignoring no message");
            assert_true(chat && "can use the transmitted fds");

            if (waitpid(new_pid, NULL, 1) > 0)
            {
                SPAWNED_PID = -1;
            }
            return;
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_connect_outlive(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        child_for_outlive(-1);
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        bool chat;
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Parent failed to set up listener");
        }
        else if (parent_outlive(server, &chat))
        {
            assert_true(chat && "can use received fds after sender exits");
            return;
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

static void test_listen_outlive(void)
{
    clear_previous_test();
    pid_t new_pid = fork();
    if (new_pid == 0)
    {
        /* child */
        int server = setup_listener();
        if (server < 0)
        {
            Log(LOG_LEVEL_ERR, "Child failed to set up listener");
        }
        else
        {
            child_for_outlive(server);
        }
        exit(0);
    }
    else if (new_pid > 0)
    {
        SPAWNED_PID = new_pid;

        /* parent */
        bool chat;
        if (parent_outlive(-1, &chat))
        {
            assert_true(chat && "can use received fds after sender exits");
            return;
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Failed fork: %s", GetErrorStr());
    }
    assert_true(false);
}

/* The driver */

int main(void)
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_take_listen_message),
        unit_test(test_take_connect_message),
        unit_test(test_send_listen_message),
        unit_test(test_send_connect_message),
        unit_test(test_take_listen_silent),
        unit_test(test_take_connect_silent),
        unit_test(test_send_listen_silent),
        unit_test(test_send_connect_silent),
        unit_test(test_connect_outlive),
        unit_test(test_listen_outlive)
    };

#if 0 /* 1: toggle as needed. */
    LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
#endif

    /* Needed to clean up the UDS created from the children, when they exit(),
       for each test that the child calls setup_listener. Also cleans the last
       socket (created by this parent process) when this testsuite exit. */
    atexit(clear_listener);

    int retval;
    if (choose_dialup_UDS_file() == true)
    {
        retval = run_tests(tests);
    }
    else
    {
        retval = EXIT_FAILURE;
    }

    /* Make sure no child is left behind. */
    if (SPAWNED_PID >= 0)
    {
        kill(SPAWNED_PID, SIGKILL);
    }

    return retval;
}
