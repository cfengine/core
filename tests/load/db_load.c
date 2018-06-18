#include <stdlib.h>
#include <sys/stat.h>
#include <cf3.defs.h>
#include <known_dirs.h>
#include <cleanup.h>

#include <dbm_api.h>


#define MAX_THREADS 10000
#define DB_ID dbid_classes

#define STATUS_SUCCESS 0
#define STATUS_FAILED_OPEN 1
#define STATUS_FAILED_CLOSE 2
#define STATUS_ERROR 3


#define READWRITEKEY 123123123
#define READWRITEDATA1 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define READWRITEDATA2 "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"

#define RECORD_COUNT_JUNK 7000
#define RECORD_COUNT_READWRITE 1  // only one read/write key above
#define RECORD_COUNT_TOTAL (RECORD_COUNT_JUNK + RECORD_COUNT_READWRITE)
#define VALUE_OFFSET1 10000
#define VALUE_OFFSET2 100000

char CFWORKDIR[CF_BUFSIZE];

static void WriteReadWriteData(CF_DB *db);
static bool ReadWriteDataIsValid(char *data);
static void DBWriteTestData(CF_DB *db);
static void TestReadWriteData(CF_DB *db);
static void TestCursorIteration(CF_DB *db);

static void tests_setup(void)
{
    static char env[] = /* Needs to be static for putenv() */
        "CFENGINE_TEST_OVERRIDE_WORKDIR=/tmp/db_load.XXXXXX";

    char *workdir = strchr(env, '=') + 1; /* start of the path */
    assert(workdir - 1 && workdir[0] == '/');

    mkdtemp(workdir);
    strlcpy(CFWORKDIR, workdir, CF_BUFSIZE);
    putenv(env);
    mkdir(GetStateDir(), (S_IRWXU | S_IRWXG | S_IRWXO));
}

static void *contend(ARG_UNUSED void *param)
{
    CF_DB *db;

    if (!OpenDB(&db, DB_ID))
    {
        return (void *)STATUS_FAILED_OPEN;
    }

    DBWriteTestData(db);

    TestReadWriteData(db);
    TestCursorIteration(db);

    CloseDB(db);

    return (void *)STATUS_SUCCESS;
}

static void TestReadWriteData(CF_DB *db)
{
    WriteReadWriteData(db);

    int iterations = rand() % 1000000;

    for(int i = 0; i < iterations; i++)
    {
        // sleep gets complicated in threads...
    }

    static const int key = READWRITEKEY;

    char readData[sizeof(READWRITEDATA1)];

    if(!ReadComplexKeyDB(db, (const char *)&key, sizeof(key), readData, sizeof(readData)))
    {
        printf("Error read\n");
    }

    if(!ReadWriteDataIsValid(readData))
    {
        printf("corrupt data: \"%s\"\n", readData);
    }
}

static bool CoinFlip(void)
{
    return rand() % 2 == 0;
}

static void WriteReadWriteData(CF_DB *db)
{
    const char *const data = CoinFlip() ? READWRITEDATA1 : READWRITEDATA2;
    static const int key = READWRITEKEY;

    if(!WriteComplexKeyDB(db, (const char *)&key, sizeof(key), data, sizeof(READWRITEDATA1)))
    {
        printf("Error write!\n");
        pthread_exit((void*)STATUS_ERROR);
    }
}

static bool ReadWriteDataIsValid(char *data)
{
    return (strcmp(data, READWRITEDATA1) == 0 ||
            strcmp(data, READWRITEDATA2) == 0);
}

static void TestCursorIteration(CF_DB *db)
{
    CF_DBC *dbc;

    if(!NewDBCursor(db, &dbc))
    {
        fprintf(stderr, "Test: could not create cursor");
        pthread_exit((void*)STATUS_ERROR);
        exit(EXIT_FAILURE);
    }

    char *key;
    void *value;
    int key_sz, value_sz;

    int count = 0;
    while(NextDB(dbc, &key, &key_sz, &value, &value_sz))
    {
        int key_num = *(int *)key;
        int value_num = *(int *)value;

        if(key_num >= 0 && key_num < RECORD_COUNT_JUNK)
        {
            if((key_num + VALUE_OFFSET1 != value_num) && (key_num + VALUE_OFFSET2 != value_num))
            {
                printf("Error: key,value %d,%d are inconsistent\n", key_num, value_num);
            }
        }
        else if(key_num == READWRITEKEY)
        {
            if(!ReadWriteDataIsValid(value))
            {
                printf("Error: ReadWrite data is invalid\n");
            }
        }
        else
        {
            printf("Error: invalid key \"%s\"", key);
        }

        count++;
    }

    if(count != RECORD_COUNT_TOTAL)
    {
        printf("Error: During iteration count was %d (expected %d)\n", count, RECORD_COUNT_TOTAL);
    }

    if(!DeleteDBCursor(dbc))
    {
        fprintf(stderr, "Test: could not delete cursor");
        exit(EXIT_FAILURE);
    }

}


int WriteReturnValues(int retvals[MAX_THREADS], pthread_t tids[MAX_THREADS], int numthreads)
{
    int failures = 0;

    for(int i = 0; i < numthreads; i++)
    {
        uintptr_t status;
        pthread_join(tids[i], (void **)&status);
        retvals[i] = status;

        if(status != STATUS_SUCCESS)
        {
            failures++;
        }
    }

    return failures;
}

static void Cleanup(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: db_load <num_threads>\n");
        exit(EXIT_FAILURE);
    }

    /* To clean up after databases are closed */
    RegisterCleanupFunction(&Cleanup);

    tests_setup();

    int numthreads = atoi(argv[1]);

    assert(numthreads < MAX_THREADS);

    srand(time(NULL));

    pthread_t tids[MAX_THREADS];
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 65536);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for (int i = 0; i < numthreads; ++i)
    {
        int ret = pthread_create(&(tids[i]), &attr, &contend, NULL);

        if (ret != 0)
        {
            fprintf(stderr, "Unable to create thread: %s\n", strerror(ret));
        }
    }

    pthread_attr_destroy(&attr);

    int retvals[MAX_THREADS];

    int failures = WriteReturnValues(retvals, tids, numthreads);

    DoCleanupAndExit(failures);
}


static void DBWriteTestData(CF_DB *db)
{
    for(int i = 0; i < RECORD_COUNT_JUNK; i++)
    {
        bool flip = CoinFlip();
        int value_num = i + (flip ? VALUE_OFFSET1 : VALUE_OFFSET2);

        if (!WriteComplexKeyDB(db, (const char *)&i, sizeof(i), &value_num, sizeof(value_num)))
        {
            Log(LOG_LEVEL_ERR, "Unable to write data to database");
            pthread_exit((void*)STATUS_ERROR);
        }
    }

    WriteReadWriteData(db);
}

/* Stub out */

void FatalError(ARG_UNUSED const EvalContext *ctx, char *fmt, ...)
{
    if (fmt)
    {
        va_list ap;
        char buf[CF_BUFSIZE] = "";

        va_start(ap, fmt);
        vsnprintf(buf, CF_BUFSIZE - 1, fmt, ap);
        va_end(ap);
        Log(LOG_LEVEL_ERR, "Fatal CFEngine error: %s", buf);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Fatal CFEngine error (no description)");
    }

    exit(EXIT_FAILURE);
}


pthread_mutex_t test_lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

pthread_mutex_t *cft_dbhandle;

const char *const DAY_TEXT[] = {};
const char *const MONTH_TEXT[] = {};
