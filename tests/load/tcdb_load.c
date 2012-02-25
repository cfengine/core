#include "cf3.defs.h"
#include "cf3.extern.h"

#define DB_FILE "db.db"
#define ATTEMPTS 1000

void *contend(void *param)
{
    CF_DB *db;
    int i;

    for (i = 0; i < ATTEMPTS; ++i)
    {
        if (!OpenDB(DB_FILE, &db))
        {
            exit(42);
        }
        else
        {
            CloseDB(db);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 65536);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int numthreads = atoi(argv[1]);

    int i;

    for (i = 0; i < numthreads; ++i)
    {
        pthread_t tid;

        int ret = pthread_create(&tid, &attr, &contend, NULL);

        if (ret != 0)
        {
            fprintf(stderr, "Unable to create thread: %s\n", strerror(ret));
        }
    }

    pthread_attr_destroy(&attr);
    pthread_exit(NULL);
}

/* Stub out */

void CfOut(enum cfreport level, const char *function, const char *fmt, ...)
{
    exit(42);
}

void FatalError(char *fmt, ...)
{
    exit(42);
}

int DEBUG;

#if defined(HAVE_PTHREAD)
int ThreadLock(pthread_mutex_t *t)
{
    return 1;
}

int ThreadUnlock(pthread_mutex_t *t)
{
    return 1;
}

pthread_mutex_t *cft_dbhandle;
#endif
