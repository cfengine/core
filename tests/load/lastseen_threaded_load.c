#include <cf3.defs.h>
#include <lastseen.h>
#include <dbm_api.h>
#include <process_lib.h>                               /* GracefulTerminate */
#include <mutex.h>                                     /* ThreadLock */

#include <libgen.h>                                             /* basename */


#define NSEC 10                            /* how long to run each loop */
#define NHOSTS 5000                        /* how many hosts to store in db */
#define MAX_NUM_THREADS 10000
#define MAX_NUM_FORKS   10000


/* TODO all these counters should be guarded by mutex since they are written
 * from child threads and read from the main one. It's only a test, so
 * @ediosyncratic please spare me. */
time_t START_TIME;
char CFWORKDIR[CF_BUFSIZE];
unsigned long      lastsaw_COUNTER[MAX_NUM_THREADS];
unsigned long     keycount_COUNTER[MAX_NUM_THREADS];
unsigned long scanlastseen_COUNTER[MAX_NUM_THREADS];
int CHILDREN_OUTPUTS[MAX_NUM_FORKS];
volatile bool DONE;

/* Counter and wait condition to see if test properly finished. */
unsigned long FINISHED_THREADS = 0;
unsigned long TOTAL_NUM_THREADS;
pthread_mutex_t end_mtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_cond_t end_cond = PTHREAD_COND_INITIALIZER;


void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp);


static bool PurgeCurrentLastSeen()
{
    CF_DB *db_conn = NULL;
    CF_DBC *db_cursor = NULL;
    char *key = NULL;
    void *value = NULL;
    int ksize = 0, vsize = 0;

    if (!OpenDB(&db_conn, dbid_lastseen))
    {
        Log(LOG_LEVEL_ERR, "Unable to open lastseen db");
        return false;
    }

    if (!NewDBCursor(db_conn, &db_cursor))
    {
        Log(LOG_LEVEL_ERR, "Unable to scan lastseen db");
        CloseDB(db_conn);
        return false;
    }

    while (NextDB(db_cursor, &key, &ksize, &value, &vsize))
    {
        /* Only read the 'quality of connection' entries */
        if (key[0] != 'q')
        {
            continue;
        }

        time_t then = 0;

        if (value != NULL)
        {
            if (sizeof(KeyHostSeen) < vsize)
            {
                Log(LOG_LEVEL_ERR, "Invalid entry in lastseen database.");
                continue;
            }

            KeyHostSeen entry = { 0 };
            memcpy(&entry, value, vsize);

            then = entry.lastseen;
        }

        if (then - START_TIME > NHOSTS)
        {
            DBCursorDeleteEntry(db_cursor);
            Log(LOG_LEVEL_DEBUG, "Deleting expired entry for %s", key);
            continue;
        }
    }
    DeleteDBCursor(db_cursor);
    CloseDB(db_conn);

    return true;
}

static bool callback(const char *hostkey ARG_UNUSED,
                     const char *address ARG_UNUSED,
                     bool incoming ARG_UNUSED,
                     const KeyHostSeen *quality ARG_UNUSED,
                     void *ctx ARG_UNUSED)
{
    return true;
}


/* ============== WORKER THREADS ================ */

static void thread_exit_clean()
{
    /* Signal that we finished. */
    ThreadLock(&end_mtx);
    FINISHED_THREADS++;
    if (FINISHED_THREADS > TOTAL_NUM_THREADS)
    {
        pthread_cond_signal(&end_cond);
    }
    ThreadUnlock(&end_mtx);
}

void *lastsaw_worker_thread(void *arg)
{
    int thread_id = (intptr_t) arg;

    size_t i = 0;
    while (!DONE)
    {
        char hostkey[50];
        snprintf(hostkey, sizeof(hostkey), "SHA-%040zx", i);
        char ip[50];
        snprintf(ip, sizeof(ip), "250.%03zu.%03zu.%03zu",
                 i / (256*256), (i / 256) % 256, i % 256);

        UpdateLastSawHost(hostkey, ip,
                          ((i % 2 == 0) ?
                           LAST_SEEN_ROLE_ACCEPT :
                           LAST_SEEN_ROLE_CONNECT),
                          START_TIME + i);

        i = (i + 1) % NHOSTS;
        lastsaw_COUNTER[thread_id]++;
    }

    thread_exit_clean();
    return NULL;
}

void *keycount_worker_thread(void *arg)
{
    int id = (intptr_t) arg;

    while (!DONE)
    {
        LastSeenHostKeyCount();
        keycount_COUNTER[id]++;
    }

    thread_exit_clean();
    return NULL;
}

void *scanlastseen_worker_thread(void *arg)
{
    int id = (intptr_t) arg;

    while (!DONE)
    {
        ScanLastSeenQuality(callback, NULL);
        scanlastseen_COUNTER[id]++;
    }

    thread_exit_clean();
    return NULL;
}

/* ============== END OF WORKER THREADS ================ */


/* ============== CHILD PROCESS ======================== */

unsigned long child_COUNTER;
int PIPE_FD[2];
FILE *PARENT_INPUT;
bool child_START = false;

void print_progress_sighandler(int signum ARG_UNUSED)
{
    fprintf(PARENT_INPUT, "%5lu", child_COUNTER);
    putc('\0', PARENT_INPUT);
    fflush(PARENT_INPUT);

    child_COUNTER = 0;
}

/* First time the signal is received, it interrupts the sleep() syscall and
 * sets child_START to true, which causes db crunching to start. Second time
 * child_START is set to false and we exit the child process. */
void startstop_handler(int signum ARG_UNUSED)
{
    child_START = !child_START;
}

void worker_process()
{
    struct sigaction new_handler;
    int ret;

    /* 1a. Register SIGUSR1 handler so that we start/finish the test */
    new_handler = (struct sigaction) { .sa_handler = startstop_handler };
    sigemptyset(&new_handler.sa_mask);
    ret = sigaction(SIGUSR1, &new_handler, NULL);
    if (ret != 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /* 1b. Register SIGUSR2 handler so that we report progress when pinged */
    new_handler = (struct sigaction) { .sa_handler = print_progress_sighandler };
    sigemptyset(&new_handler.sa_mask);
    ret = sigaction(SIGUSR2, &new_handler, NULL);
    if (ret != 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /* 2. Wait for signal */
    unsigned retu = sleep(100);
    if (retu == 0)
    {
        fprintf(stderr,
                "Child was never signaled to start, exiting!\n");
        exit(EXIT_FAILURE);
    }

    /* 3. DO THE WORK until SIGUSR1 comes */
    while (child_START)
    {
        if (child_COUNTER % 10 == 0)
        {
            LastSeenHostKeyCount();
        }
        else if (child_COUNTER % 10 == 1)
        {
            ScanLastSeenQuality(callback, NULL);
        }
        else if (child_COUNTER % 10 == 2)
        {
            PurgeCurrentLastSeen();
        }
        else
        {
            char hostkey[50];
            snprintf(hostkey, sizeof(hostkey), "SHA-%040lx", child_COUNTER);
            char ip[50];
            snprintf(ip, sizeof(ip), "250.%03lu.%03lu.%03lu",
                      child_COUNTER / (256*256),
                     (child_COUNTER / 256) % 256,
                      child_COUNTER % 256);

            UpdateLastSawHost(hostkey, ip,
                              ((child_COUNTER % 2 == 0) ?
                               LAST_SEEN_ROLE_ACCEPT :
                               LAST_SEEN_ROLE_CONNECT),
                              START_TIME + child_COUNTER);
        }

        child_COUNTER++;
    }
}

/* ============== END OF CHILD PROCESS ================= */


void spawn_worker_threads(void *(*worker_routine) (void *),
                          int num_threads, const char *description)
{
    pthread_t tid[num_threads];

    printf("Spawning %s worker threads: ", description);
    for (int i = 0; i < num_threads; i++)
    {
        int ret = pthread_create(&tid[i], NULL,
                                 worker_routine, (void *)(intptr_t) i);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_create(%d): %s",
                    i, GetErrorStrFromCode(ret));
            exit(EXIT_FAILURE);
        }

        printf("%i ", i+1);
    }
    printf("done!\n");
}

void print_progress(int lastsaw_num_threads, int keycount_num_threads,
                    int scanlastseen_num_threads, int num_children)
{
    for (int j = 0; j < lastsaw_num_threads; j++)
    {
        printf("%5lu", lastsaw_COUNTER[j]);
        lastsaw_COUNTER[j] = 0;
    }
    if (keycount_num_threads > 0)
    {
        fputs(" | ", stdout);
    }
    for (int j = 0; j < keycount_num_threads; j++)
    {
        printf("%5lu", keycount_COUNTER[j]);
        keycount_COUNTER[j] = 0;
    }
    if (scanlastseen_num_threads > 0)
    {
        fputs(" | ", stdout);
    }
    for (int j = 0; j < scanlastseen_num_threads; j++)
    {
        printf("%5lu", scanlastseen_COUNTER[j]);
        scanlastseen_COUNTER[j] = 0;
    }
    if (num_children > 0)
    {
        fputs(" | Children:", stdout);
    }
    for (int j = 0; j < num_children; j++)
    {
        char child_report[32] = {0};
        int ret = read(CHILDREN_OUTPUTS[j],
                       child_report, sizeof(child_report) - 1);
        if (ret <= 0)
        {
            fprintf(stderr,
                    "Couldn't read from child %d, it probably died!\n", j);
            exit(EXIT_FAILURE);
        }
        printf("%5s", child_report);
    }
}

void parse_args(int argc, char *argv[],
                int *lastsaw_num_threads, int *keycount_num_threads,
                int *scanlastseen_num_threads)
{
    *lastsaw_num_threads      = 0;
    *keycount_num_threads     = 0;
    *scanlastseen_num_threads = 0;

    if (argc >= 2)
    {
        sscanf(argv[1], "%d", lastsaw_num_threads);
    }
    if (argc >= 3)
    {
        sscanf(argv[2], "%d", keycount_num_threads);
    }
    if (argc >= 4)
    {
        sscanf(argv[3], "%d", scanlastseen_num_threads);
    }

    if (*lastsaw_num_threads  <= 0 || *lastsaw_num_threads  > MAX_NUM_THREADS)
    {
        printf("\
Usage:\n\
	%s LASTSAW_NUM_THREADS [KEYCOUNT_NUM_THREADS [SCAN_NUM_THREADS]]\n\
\n\
This program creates a parent process stressing lastseen. After several seconds\n\
creating the threads you defined, it also spawns a separate process doing things\n\
to the same database.\n\n",
               basename(argv[0]));

        exit(EXIT_FAILURE);
    }
}


int main(int argc, char *argv[])
{
    int ret;
    int lastsaw_num_threads, keycount_num_threads, scanlastseen_num_threads;
    const int total_num_children = 1;

    LogSetGlobalLevel(LOG_LEVEL_DEBUG);

    parse_args(argc, argv,
               &lastsaw_num_threads, &keycount_num_threads,
               &scanlastseen_num_threads);
    TOTAL_NUM_THREADS =
        lastsaw_num_threads + keycount_num_threads + scanlastseen_num_threads;


    snprintf(CFWORKDIR, sizeof(CFWORKDIR),
             "/tmp/lastseen_threaded_load.XXXXXX");
    char *retp = mkdtemp(CFWORKDIR);
    if (retp == NULL)
    {
        perror("mkdtemp");
        exit(EXIT_FAILURE);
    }
    printf("Work directory: %s\n", CFWORKDIR);

    START_TIME = time(NULL);


    /* === SPAWN A CHILD PROCESS FOR LATER === */

    ret = pipe(PIPE_FD);
    if (ret != 0)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t child = fork();
    if (child == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (child == 0)
    {
        /* We only write to the pipe. */
        close(PIPE_FD[0]);
        /* Connect pipe to a FILE to talk to parent via fprintf(). */
        PARENT_INPUT = fdopen(PIPE_FD[1], "w");
        if (PARENT_INPUT == NULL)
        {
            perror("child fdopen");
            exit(EXIT_FAILURE);
        }

        worker_process();
        exit(EXIT_SUCCESS);
    }

    /* We only read from the pipe. */
    close(PIPE_FD[1]);
    CHILDREN_OUTPUTS[0] = PIPE_FD[0];

    printf("Operations per second:\n\n");

    /* === CREATE WORKER THREADS === */

    spawn_worker_threads(lastsaw_worker_thread, lastsaw_num_threads,
                         "UpdateLastSawHost()");

    /* === PRINT PROGRESS FOR NSEC SECONDS === */

    for (int i = 0; i < NSEC; i++)
    {
        sleep(1);
        print_progress(lastsaw_num_threads, 0, 0, 0);
        putc('\n', stdout);
    }

    /* === CREATE CURSOR COUNTING WORKERS === */

    if (keycount_num_threads > 0)
    {
        spawn_worker_threads(keycount_worker_thread, keycount_num_threads,
                             "LastSeenHostKeyCount()");

        /* === PRINT PROGRESS FOR NSEC SECONDS === */
        for (int i = 0; i < NSEC; i++)
        {
            sleep(1);
            print_progress(lastsaw_num_threads, keycount_num_threads, 0, 0);
            putc('\n', stdout);
        }
    }

    /* === CREATE CURSOR READING WORKERS === */

    if (scanlastseen_num_threads > 0)
    {
        spawn_worker_threads(scanlastseen_worker_thread, scanlastseen_num_threads,
                             "ScanLastSeenQuality()");

        /* === PRINT PROGRESS FOR NSEC SECONDS === */
        for (int i = 0; i < NSEC; i++)
        {
            sleep(1);
            print_progress(lastsaw_num_threads, keycount_num_threads,
                           scanlastseen_num_threads, 0);
            putc('\n', stdout);
        }
    }

    /* === START CHILD PROCESS WORK === */

    printf("Forking child doing mix of operations\n");
    kill(child, SIGUSR1);

    /* === PRINT PROGRESS FOR NSEC SECONDS === */

    for (int i = 0; i < NSEC; i++)
    {
        sleep(1);
        kill(child, SIGUSR2);                /* receive progress from child */
        print_progress(lastsaw_num_threads, keycount_num_threads,
                       scanlastseen_num_threads, 1);
        putc('\n', stdout);
    }

    /* === TEST FINISHED === */

    DONE = true;
    kill(child, SIGUSR1);

    /* === WAIT AT MOST 30 SECONDS FOR EVERYBODY TO FINISH === */

    printf("Waiting at most 30s for all threads to finish...\n");

    unsigned long finished_children = 0;
    time_t wait_starttime = time(NULL);
    time_t seconds_waited = 0;
    ThreadLock(&end_mtx);
    while (!(    FINISHED_THREADS == TOTAL_NUM_THREADS
             && finished_children == total_num_children)
           && seconds_waited < 30)
    {
        struct timespec ts = { .tv_sec = 1 };
        if (FINISHED_THREADS < TOTAL_NUM_THREADS)
        {
            pthread_cond_timedwait(&end_cond, &end_mtx, &ts);
        }
        else
        {
            sleep(1);
        }

        /* Has any child process died? */
        while (waitpid(-1, NULL, WNOHANG) > 0)
        {
            finished_children++;
        }

        seconds_waited = time(NULL) - wait_starttime;
    }
    ThreadUnlock(&end_mtx);

    /* === CLEAN UP TODO register these with atexit() === */

    int retval = EXIT_SUCCESS;
    if (finished_children != total_num_children)
    {
        fprintf(stderr,
                "Forked child seems to be still alive, killing it!\n");
        GracefulTerminate(child, PROCESS_START_TIME_UNKNOWN);
        wait(NULL);
        retval = EXIT_FAILURE;
    }

    if (FINISHED_THREADS != TOTAL_NUM_THREADS)
    {
        fprintf(stderr, "Only %lu of %lu threads actually finished!\n",
                FINISHED_THREADS, TOTAL_NUM_THREADS);
        retval = EXIT_FAILURE;
    }

    if (retval == EXIT_SUCCESS)
    {
        printf("DONE!\n\n");
    }

    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);

    return retval;
}
