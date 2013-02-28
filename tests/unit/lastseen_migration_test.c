#include "cf3.defs.h"
#include "dbm_api.h"
#include "test.h"
#include "lastseen.h"

#include <setjmp.h>
#include <cmockery.h>

typedef struct
{
    char address[128];
    double q;
    double expect;
    double var;
} KeyHostSeen0;

char CFWORKDIR[CF_BUFSIZE];

void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/lastseen_migration_test.XXXXXX");
    mkdtemp(CFWORKDIR);
}

/*
 * Provides empty lastseen DB
 */
static DBHandle *setup(bool clean)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'/*", CFWORKDIR);
    system(cmd);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    if (clean)
    {
        /* There is no way to disable hook in OpenDB yet, so just undo
         * everything */

        DBCursor *cursor;
        if (!NewDBCursor(db, &cursor))
        {
            return NULL;
        }

        char *key;
        void *value;
        int ksize, vsize;

        while (NextDB(db, cursor, &key, &ksize, &value, &vsize))
        {
            DBCursorDeleteEntry(cursor);
        }

        if (!DeleteDBCursor(db, cursor))
        {
            return NULL;
        }
    }

    return db;
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

static void test_no_migration(void **context)
{
    DBHandle *db = setup(true);

    CloseDB(db);

    /* Migration on empty DB should produce single "version" key */

    assert_int_equal(OpenDB(&db, dbid_lastseen), true);

    DBCursor *cursor;
    assert_int_equal(NewDBCursor(db, &cursor), true);

    char *key;
    void *value;
    int ksize, vsize;

    while (NextDB(db, cursor, &key, &ksize, &value, &vsize))
    {
        assert_int_equal(ksize, strlen("version") + 1);
        assert_string_equal(key, "version");
        assert_int_equal(vsize, 2);
        assert_string_equal(value, "1");
    }

    assert_int_equal(DeleteDBCursor(db, cursor), true);

    CloseDB(db);
}

static void test_up_to_date(void **context)
{
    /* Test that upgrade is not performed if there is already a version
     * marker */

    DBHandle *db = setup(false);
    const char *value = "+";
    assert_int_equal(WriteDB(db, "+++", value, 2), true);
    CloseDB(db);

    /* Test that manually inserted key which matches the format of old-style
     * keys is still present next time database is open, which is an indicator
     * of database not being upgraded */

    assert_int_equal(OpenDB(&db, dbid_lastseen), true);

    char read_value[CF_BUFSIZE];

    assert_int_equal(ReadDB(db, "+++", &read_value, sizeof(read_value)), true);
    assert_string_equal(read_value, "+");

    CloseDB(db);
}

#define KEYHASH \
    "SHA=f7b335bef201230c7bf573b8dedf299fa745efe71e34a9002369248ff8519089"
#define KEYHASH_KEY "k" KEYHASH

#define KEYHASH_IN "-" KEYHASH
#define QUALITY_IN "qi" KEYHASH

#define KEYHASH_OUT "+" KEYHASH
#define QUALITY_OUT "qo" KEYHASH

void test_migrate_single(const char *expected_old_key,
                         const char *expected_quality_key)
{
    /* Test migration of single entry */

    DBHandle *db = setup(true);
    KeyHostSeen0 khs0 = {
        .q = 666777.0,
        .expect = 12345.0,
        .var = 6543210.0,
    };
    strcpy(khs0.address, "1.2.3.4");
    assert_int_equal(WriteDB(db, expected_old_key, &khs0, sizeof(khs0)), true);
    CloseDB(db);

    assert_int_equal(OpenDB(&db, dbid_lastseen), true);

    /* Old entry migrated */
    assert_int_equal(HasKeyDB(db, expected_old_key,
                              strlen(expected_old_key) + 1), false);

    /* Version marker */
    assert_int_equal(HasKeyDB(db, "version", strlen("version") + 1), true);

    /* Incoming connection quality */
    KeyHostSeen khs;

    assert_int_equal(ReadDB(db, expected_quality_key, &khs, sizeof(khs)), true);

    assert_int_equal(khs.lastseen, 666777);
    assert_double_close(khs.Q.q, 12345.0);
    assert_double_close(khs.Q.expect, 12345.0);
    assert_double_close(khs.Q.var, 6543210.0);

    /* Address mapping */
    char address[CF_BUFSIZE];

    assert_int_equal(ReadDB(db, KEYHASH_KEY, address, sizeof(address)), true);
    assert_string_equal(address, "1.2.3.4");

    /* Reverse mapping */

    char keyhash[CF_BUFSIZE];

    assert_int_equal(ReadDB(db, "a1.2.3.4", keyhash, sizeof(keyhash)), true);
    assert_string_equal(keyhash, KEYHASH);

    CloseDB(db);
}

void test_migrate_incoming(void **context)
{
    test_migrate_single(KEYHASH_IN, QUALITY_IN);
}

void test_migrate_outgoing(void **context)
{
    test_migrate_single(KEYHASH_OUT, QUALITY_OUT);
}

void test_ignore_wrong_sized(void **context)
{
    /* Test that malformed values are discarded */

    DBHandle *db = setup(true);
    const char *value = "+";
    assert_int_equal(WriteDB(db, "+++", value, 2), true);
    CloseDB(db);

    assert_int_equal(OpenDB(&db, dbid_lastseen), true);

    assert_int_equal(HasKeyDB(db, "+++", 4), false);
    assert_int_equal(HasKeyDB(db, "k++", 4), false);
    assert_int_equal(HasKeyDB(db, "qi++", 5), false);
    assert_int_equal(HasKeyDB(db, "qo++", 5), false);
    assert_int_equal(HasKeyDB(db, "a+", 3), false);

    CloseDB(db);
}

int main()
{
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_no_migration),
            unit_test(test_up_to_date),
            unit_test(test_migrate_incoming),
            unit_test(test_migrate_outgoing),
            unit_test(test_ignore_wrong_sized),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    tests_teardown();

    return ret;
}

/* STUBS */

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    fail();
    exit(42);
}

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    fprintf(stderr, "CFOUT<%d>: ", level);
    if (errstr)
    {
        fprintf(stderr, " %s: %s ", errstr, strerror(errno));
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

HashMethod CF_DEFAULT_DIGEST;
const char *DAY_TEXT[] = {};
const char *MONTH_TEXT[] = {};
const char *SHIFT_TEXT[] = {};
pthread_mutex_t *cft_output;
char VIPADDRESS[18];
RSA *PUBKEY;
int DEBUG;
bool MINUSF;

char *MapAddress(char *addr)
{
    fail();
}

char *HashPrint(HashMethod type, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    fail();
}

int ThreadLock(pthread_mutex_t *name)
{
    fail();
}

int ThreadUnlock(pthread_mutex_t *name)
{
    fail();
}

void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    fail();
}
