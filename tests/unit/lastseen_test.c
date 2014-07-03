#include <test.h>

#include <cf3.defs.h>
#include <dbm_api.h>
#include <lastseen.h>
#include <item_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */


char CFWORKDIR[CF_BUFSIZE];

void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp);

static void tests_setup(void)
{
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/lastseen_test.XXXXXX");
    mkdtemp(CFWORKDIR);
}

static void setup(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'/*", CFWORKDIR);
    system(cmd);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

static void test_newentry(void)
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

static void test_update(void)
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

static void test_reverse_missing(void)
{
    setup();

    /* Check that resolution return false */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey(result, sizeof(result), "127.0.0.64"), false);
}

static void test_reverse_conflict(void)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    assert_int_equal(WriteDB(db, "a127.0.0.64", "SHA-98765", strlen("SHA-98765") + 1), true);

    /* Check that resolution return false */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey(result, sizeof(result), "127.0.0.64"), false);

    /* Check that entry is removed */
    assert_int_equal(HasKeyDB(db, "a127.0.0.64", strlen("a127.0.0.64") + 1), false);

    CloseDB(db);
}

static void test_reverse_missing_forward(void)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    assert_int_equal(DeleteDB(db, "kSHA-12345"), true);

    /* Check that resolution return false */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey(result, sizeof(result), "127.0.0.64"), false);

    /* Check that entry is removed */
    assert_int_equal(HasKeyDB(db, "a127.0.0.64", strlen("a127.0.0.64") + 1), false);

    CloseDB(db);
}

static void test_remove(void)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);
    UpdateLastSawHost("SHA-12345", "127.0.0.64", false, 556);

    //RemoveHostFromLastSeen("SHA-12345");
    DeleteDigestFromLastSeen("SHA-12345", NULL);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    assert_int_equal(HasKeyDB(db, "qiSHA-12345", strlen("qiSHA-12345") + 1), false);
    assert_int_equal(HasKeyDB(db, "qoSHA-12345", strlen("qoSHA-12345") + 1), false);
    assert_int_equal(HasKeyDB(db, "kSHA-12345", strlen("kSHA-12345") + 1), false);
    assert_int_equal(HasKeyDB(db, "a127.0.0.64", strlen("a127.0.0.64") + 1), false);

    CloseDB(db);
}

static void test_remove_ip(void)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);
    UpdateLastSawHost("SHA-12345", "127.0.0.64", false, 556);

    char digest[CF_BUFSIZE];
    DeleteIpFromLastSeen("127.0.0.64", digest);

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
            unit_test(test_remove_ip),
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

HashMethod CF_DEFAULT_DIGEST;
pthread_mutex_t *cft_output;
char VIPADDRESS[CF_MAX_IP_LEN];
RSA *PUBKEY;
bool MINUSF;

char *MapAddress(ARG_UNUSED char *addr)
{
    fail();
}

char *HashPrintSafe(ARG_UNUSED char *dst, ARG_UNUSED size_t dst_size,
                    ARG_UNUSED const unsigned char *digest,
                    ARG_UNUSED HashMethod type, ARG_UNUSED bool use_prefix)
{
    fail();
}

void HashPubKey(ARG_UNUSED RSA *key,
                ARG_UNUSED unsigned char digest[EVP_MAX_MD_SIZE + 1],
                ARG_UNUSED HashMethod type)
{
    fail();
}

