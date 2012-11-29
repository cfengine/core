#include "cf3.defs.h"

#include "dbm_api.h"
#include "cfstream.h"

#include <assert.h>

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


int DEBUG = false;  // wether or not to get output from CfDebug()
char CFWORKDIR[CF_BUFSIZE];

static bool CoinFlip(void);
static void WriteReadWriteData(CF_DB *db);
static bool ReadWriteDataIsValid(char *data);
static void DBWriteTestData(CF_DB *db);
static void TestReadWriteData(CF_DB *db);
static void TestCursorIteration(CF_DB *db);

void *contend(void *param)
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

    //char key[64];
    //snprintf(key, sizeof(key), "%050d", READWRITEKEY);

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

static void WriteReadWriteData(CF_DB *db)
{
    bool flip = CoinFlip();

    char *data;
    if(flip)
    {
        data = READWRITEDATA1;
    }
    else
    {
        data = READWRITEDATA2;
    }

    //char key[64];
    //snprintf(key, sizeof(key), "%050d", READWRITEKEY);

    static const int key = READWRITEKEY;

    if(!WriteComplexKeyDB(db, (const char *)&key, sizeof(key), data, sizeof(READWRITEDATA1)))
    {
        printf("Error write!\n");
        pthread_exit((void*)STATUS_ERROR);
    }
}

static bool CoinFlip(void)
{
    bool flip = (rand() % 2 == 0) ? true : false;
    return flip;
}

static bool ReadWriteDataIsValid(char *data)
{
    if(strcmp(data, READWRITEDATA1) == 0 || strcmp(data, READWRITEDATA2) == 0)
    {
        return true;
    }

    return false;
}

static void TestCursorIteration(CF_DB *db)
{
    CF_DBC *dbc;

    if(!NewDBCursor(db, &dbc))
    {
        FatalError("Test: could not create cursor");
        pthread_exit((void*)STATUS_ERROR);
    }

    char *key;
    void *value;
    int key_sz, value_sz;

    int count = 0;
    while(NextDB(db, dbc, &key, &key_sz, &value, &value_sz))
    {
        int key_num = *(int *)key;
        int value_num = *(int *)value;

        //int key_num = atoi(key);
        //int value_num = atoi(value);

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

    if(!DeleteDBCursor(db, dbc))
    {
        FatalError("Test: could not delete cursor");
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
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: db_load <num_threads>\n");
        exit(1);
    }

    /* To clean up after databases are closed */
    atexit(&Cleanup);

    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/db_load.XXXXXX");
    mkdtemp(CFWORKDIR);

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

    exit(failures);
}


static void DBWriteTestData(CF_DB *db)
{
    //char key[64];
    //char value[128];

    for(int i = 0; i < RECORD_COUNT_JUNK; i++)
    {
        bool flip = CoinFlip();
        int value_num;

        if(flip)
        {
            value_num = i + VALUE_OFFSET1;
        }
        else
        {
            value_num = i + VALUE_OFFSET2;
        }

        //snprintf(key, sizeof(key), "%050d", i);
        //snprintf(value, sizeof(value), "%0100d", value_num);

        if (!WriteComplexKeyDB(db, (const char *)&i, sizeof(i), &value_num, sizeof(value_num)))
        {
            CfOut(cf_error, "", "Unable to write data to database");
            pthread_exit((void*)STATUS_ERROR);
        }
    }

    WriteReadWriteData(db);
}

/* Stub out */

void CfOut(enum cfreport level, const char *function, const char *fmt, ...)
{
    va_list ap;
    char buf[CF_BUFSIZE] = "";

    va_start(ap, fmt);
    vsnprintf(buf, CF_BUFSIZE - 1, fmt, ap);
    va_end(ap);
    printf("CfOut: %s\n", buf);
}

void FatalError(char *fmt, ...)
{
    if (fmt)
    {
        va_list ap;
        char buf[CF_BUFSIZE] = "";

        va_start(ap, fmt);
        vsnprintf(buf, CF_BUFSIZE - 1, fmt, ap);
        va_end(ap);
        CfOut(cf_error, "", "Fatal CFEngine error: %s", buf);
    }
    else
    {
        CfOut(cf_error, "", "Fatal CFEngine error (no description)");
    }

    exit(1);
}


#if defined(HAVE_PTHREAD)

pthread_mutex_t test_lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

int ThreadLock(pthread_mutex_t *t)
{
    int result = pthread_mutex_lock(&test_lock);

    if (result != 0)
    {
        FatalError("Could not lock mutex");
    }

    return true;
}

int ThreadUnlock(pthread_mutex_t *t)
{
    int result = pthread_mutex_unlock(&test_lock);

    if (result != 0)
    {
        FatalError("Could not unlock mutex");
    }

    return true;
}

pthread_mutex_t *cft_dbhandle;
#endif

const char *DAY_TEXT[] = {};
const char *MONTH_TEXT[] = {};
