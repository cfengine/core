#include "cf3.defs.h"
#include "dbm_api.h"
#include "test.h"
#include "lastseen.h"

#include <setjmp.h>
#include <cmockery.h>

char CFWORKDIR[CF_BUFSIZE];

void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp);

static void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/lastseen_test.XXXXXX");
    mkdtemp(CFWORKDIR);
}

static void setup(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'/*", CFWORKDIR);
    system(cmd);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

static void test_newentry(void **context)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 666);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    KeyHostSeen q;
    assert_int_equal(ReadDB(db, "qiSHA-12345", &q, sizeof(q)), true);

    assert_int_equal(q.lastseen, 666);
    assert_double_close(q.Q.q, 0.0);
    assert_double_close(q.Q.dq, 0.0);
    assert_double_close(q.Q.expect, 0.0);
    assert_double_close(q.Q.var, 0.0);

    assert_int_equal(ReadDB(db, "qoSHA-12345", &q, sizeof(q)), false);

    char address[CF_BUFSIZE];
    assert_int_equal(ReadDB(db, "kSHA-12345", address, sizeof(address)), true);
    assert_string_equal(address, "127.0.0.64");

    char hostkey[CF_BUFSIZE];
    assert_int_equal(ReadDB(db, "a127.0.0.64", hostkey, sizeof(hostkey)), true);
    assert_string_equal(hostkey, "SHA-12345");

    CloseDB(db);
}

static void test_update(void **context)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);
    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 1110);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    KeyHostSeen q;
    assert_int_equal(ReadDB(db, "qiSHA-12345", &q, sizeof(q)), true);

    assert_int_equal(q.lastseen, 1110);
    assert_double_close(q.Q.q, 555.0);
    assert_double_close(q.Q.dq, 555.0);
    assert_double_close(q.Q.expect, 222.0);
    assert_double_close(q.Q.var, 123210.0);

    CloseDB(db);
}

static void test_reverse_missing(void **context)
{
    setup();

    /* Check that resolution return false */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey("127.0.0.64", result), false);
}

static void test_reverse_conflict(void **context)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    assert_int_equal(WriteDB(db, "a127.0.0.64", "SHA-98765", strlen("SHA-98765") + 1), true);

    /* Check that resolution return false */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey("127.0.0.64", result), false);

    /* Check that entry is removed */
    assert_int_equal(HasKeyDB(db, "a127.0.0.64", strlen("a127.0.0.64") + 1), false);

    CloseDB(db);
}

static void test_reverse_missing_forward(void **context)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    assert_int_equal(DeleteDB(db, "kSHA-12345"), true);

    /* Check that resolution return false */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey("127.0.0.64", result), false);

    /* Check that entry is removed */
    assert_int_equal(HasKeyDB(db, "a127.0.0.64", strlen("a127.0.0.64") + 1), false);

    CloseDB(db);
}

static void test_remove(void **context)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);
    UpdateLastSawHost("SHA-12345", "127.0.0.64", false, 556);

    RemoveHostFromLastSeen("SHA-12345");

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    assert_int_equal(HasKeyDB(db, "qiSHA-12345", strlen("qiSHA-12345") + 1), false);
    assert_int_equal(HasKeyDB(db, "qoSHA-12345", strlen("qoSHA-12345") + 1), false);
    assert_int_equal(HasKeyDB(db, "kSHA-12345", strlen("kSHA-12345") + 1), false);
    assert_int_equal(HasKeyDB(db, "a127.0.0.64", strlen("a127.0.0.64") + 1), false);

    CloseDB(db);
}

int main()
{
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_newentry),
            unit_test(test_update),
            unit_test(test_reverse_missing),
            unit_test(test_reverse_conflict),
            unit_test(test_reverse_missing_forward),
            unit_test(test_remove),
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

