#include <test.h>

#include <cf3.defs.h>
#include <dbm_api.h>
#include <misc_lib.h>                                          /* xsnprintf */


char CFWORKDIR[CF_BUFSIZE];

void tests_setup(void)
{
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/db_test.XXXXXX");
    mkdtemp(CFWORKDIR);
}

void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

struct arg_struct {
    int base;
};


/*****************************************************************
* launch 5 threads
*   fct(i)
*     one by one insert
*     batch insert
*     one by one update
*     batch update
*     delete one by one
*     batch delete
* 
* join
* check
* 0 - 1999
*   first 100
*   last 1900
*   first 500, if %20, update +1
*   last 1500, if %20, update +1
*   first 500, if %50, delete
*   last 1500, if %50, delete
*   
* 2000- 3999 
* 4000- 5999 
* 6000- 7999 
* 8000- 9999 
*****************************************************************/
static void *fct2(void *arguments)
{
    struct arg_struct *args = (struct arg_struct *)arguments;
    int base = (int)((Seq *)args->base);

    CF_DB *db;
    char key[256];
    char val[256];
    OpenDB(&db, dbid_classes);

    for(int i = base*2000; i<base*2000+100; i++) {
        xsnprintf(key, sizeof(key), "foo%d", i);
        xsnprintf(val, sizeof(val), "bar%d", i);
        WriteDB(db, key, val, strlen(val) + 1);
    }
    for(int i = base*2000; i<base*2000+100; i++) {
        xsnprintf(key, sizeof(key), "foo%d", i);
        xsnprintf(val, sizeof(val), "bar%d", i + 1);

        if ( (i % 2) == 0)
        {
            WriteDB(db, key, val, strlen(val) + 1);
        }
    }
    for(int i = base*2000; i<base*2000+100; i++) {
        if ( (i % 5) == 0)
        {
            xsnprintf(key, sizeof(key), "foo%d", i);
            DeleteDB(db, key);
        }
    }

    xsnprintf(key, sizeof(key), "foo%d", base*2000+90);
    assert_int_equal(HasKeyDB(db, key, strlen(key)+1), false);
    xsnprintf(key, sizeof(key), "foo%d", base*2000+88);
    assert_int_equal(HasKeyDB(db, key, strlen(key)+1), true);
    xsnprintf(key, sizeof(key), "foo%d", base*2000+89);
    assert_int_equal(HasKeyDB(db, key, strlen(key)+1), true); 

    CloseDB(db);
    return NULL;
}

void test_db_concurrent(void)
{
    pthread_t tid[10];
    struct arg_struct args[10];
    int i;
    for (i=0; i < 10; i++)
    {
        args[i].base = i; 
        pthread_create(&tid[i], NULL, (void *) fct2, (void *)&args[i]);
    }
    for (i=0; i < 10; i++)
    {
        pthread_join(tid[i], NULL);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_db_concurrent),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    tests_teardown();
    return ret;
}

/* STUBS */

void FatalError(ARG_UNUSED char *s, ...)
{
    fail();
    exit(42);
}

