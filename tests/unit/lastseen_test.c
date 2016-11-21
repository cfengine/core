#include <test.h>

#include <cf3.defs.h>
#include <dbm_api.h>
#include <lastseen.h>
#include <item_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */
#include <known_dirs.h>


char CFWORKDIR[CF_BUFSIZE];

void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp);

/* For abbreviation of tests. */
#define IP1 "127.0.0.121"
#define IP2 "127.0.0.122"
#define IP3 "127.0.0.123"
#define KEY1 "SHA=key1"
#define KEY2 "SHA=key2"
#define KEY3 "SHA=key3"
#define ACC LAST_SEEN_ROLE_ACCEPT
#define DBHasStr(dbh, s)    HasKeyDB(dbh, s, strlen(s)+1)
#define DBPutStr(dbh, k, s) WriteDB(dbh, k, s, strlen(s)+1)
char tmpbuf[CF_BUFSIZE];
#define DBGetStr(dbh, s)    (ReadDB(dbh, s, tmpbuf, sizeof(tmpbuf)) ? tmpbuf : NULL)


static void tests_setup(void)
{
    static char env[] = /* Needs to be static for putenv() */
        "CFENGINE_TEST_OVERRIDE_WORKDIR=/tmp/lastseen_test.XXXXXX";

    char *workdir = strchr(env, '=') + 1; /* start of the path */
    assert(workdir - 1 && workdir[0] == '/');

    mkdtemp(workdir);
    strlcpy(CFWORKDIR, workdir, CF_BUFSIZE);
    putenv(env);
    mkdir(GetStateDir(), (S_IRWXU | S_IRWXG | S_IRWXO));
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", GetStateDir());
    system(cmd);
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", GetWorkDir());
    system(cmd);
}


static void setup(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'/*", GetStateDir());
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

    /* Overwrite reverse entry with different one. */
    DBHandle *db;
    OpenDB(&db, dbid_lastseen);
    assert_int_equal(WriteDB(db, "a127.0.0.64", "SHA-98765", strlen("SHA-98765") + 1), true);

    /* Check that resolution returns the last forced entry and is not bothered
     * by the inconsistency (despite outputing a warning). */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey(result, sizeof(result), "127.0.0.64"), true);
    assert_string_equal(result, "SHA-98765");

    /* Both entries still exist despite inconsistency. */
    assert_int_equal(DBHasStr(db, "a127.0.0.64"), true);
    assert_int_equal(DBHasStr(db, "kSHA-12345"), true);
    /* And the inconsistency (missing entry) is not auto-fixed... :-( */
    assert_int_equal(DBHasStr(db, "kSHA-98765"), false);

    CloseDB(db);
}

static void test_reverse_missing_forward(void)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);

    DBHandle *db;
    OpenDB(&db, dbid_lastseen);

    assert_int_equal(DeleteDB(db, "kSHA-12345"), true);

    /* Check that resolution returns true despite the missing entry (a warning
     * should be printed though). */
    char result[CF_BUFSIZE];
    assert_int_equal(Address2Hostkey(result, sizeof(result), "127.0.0.64"), true);

    /* Entry still exists despite inconsistency. */
    assert_int_equal(DBHasStr(db, "a127.0.0.64"), true);
    /* And the inconsistency was not auto-fixed, entry is still missing. :-( */
    assert_int_equal(DBHasStr(db, "kSHA-12345"), false);

    CloseDB(db);
}

static void test_remove(void)
{
    setup();

    UpdateLastSawHost("SHA-12345", "127.0.0.64", true, 555);
    UpdateLastSawHost("SHA-12345", "127.0.0.64", false, 556);

    //RemoveHostFromLastSeen("SHA-12345");
    DeleteDigestFromLastSeen("SHA-12345", NULL, 0);

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


/* These tests can't be multi-threaded anyway. */
static DBHandle *DBH;

static void begin()
{
    bool b = OpenDB(&DBH, dbid_lastseen);
    assert_int_equal(b, true);

    //*state = db;
}
static void end()
{
    CloseDB(DBH);
    DBH = NULL;

    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, sizeof(cmd), "rm -rf '%s'/*", GetStateDir());
    system(cmd);
}


/**
 * ============== DB CONSISTENCY TESTS ===============
 *
 * @WARNING TO CODER: think twice before you change this test. Changing it to
 *          accommodate your needs means that you change what "consistent"
 *          lastseen database is. Lastseen consistency was transcribed as this
 *          test after a lot of late-hours pondering.
 *
 *          Are you sure you want to continue? (y/n)
 */


/**
 * ===== CONSISTENT CASES =====
 *
 *
 * 1. Everything is as expected here.
 *
 * aIP1  -> KEY1
 * kKEY1 -> IP1
 * aIP2  -> KEY2
 * kKEY2 -> IP2
 *
 * consistent_1a: Fill lastseen DB using the lastseen.h API.
 * consistent_1b: Same, but fill lastseen DB directly by use of dbm_api.h API.
 *
 *
 * 2. A host connecting from IP1, then connects from IP2.
 *
 * aIP1  -> KEY1
 * aIP2  -> KEY1
 * kKEY1 -> IP2
 *
 * consistent_2a: lastseen.h API.
 * consistent_2b: dbm_api.h API.
 *
 *
 * 3. The host at IP1 gets reinstalled and changes key from KEY1 to KEY2.
 *
 * aIP1  -> KEY2
 * kKEY1 -> aIP1
 * kKEY2 -> aIP1
 *
 * consistent_3a: lastseen.h API.
 * consistent_3b: dbm_api.h API.
 *
 *
 * 4. Many hosts can use the same key - a mess, but consistent
 *    (usecase by Bas van der Vlies in scientific clusters).
 *
 * aIP1  -> KEY1
 * aIP2  -> KEY1
 * aIP3  -> KEY1
 * kKEY1 -> aIP1
 *
 *
 * 5. Host connects from IP1 with KEY1, then changes address to IP2 keeps the
 *    same key, later changes key to KEY2.
 *
 * aIP1  -> KEY1
 * aIP2  -> KEY2
 * kKEY1 -> aIP2
 * kKEY2 -> aIP2
 *
 *
 * 6. Similar to previous but can't occur unless somebody restores an old key
 *    on a host. Still the db should be considered consistent.
 *
 * aIP1  -> KEY1
 * aIP2  -> KEY1
 * kKEY1 -> aIP2
 * kKEY2 -> aIP2
 *
 *
 * 7. Similarly messed-up but not inconsistent state.
 *
 * aIP1  -> KEY1
 * aIP2  -> KEY1
 * kKEY1 -> aIP2
 * kKEY2 -> aIP1
 *
 *
 */

/*  TODO assert the two DBs from "a" and "b" tests are exactly the same! */

static void test_consistent_1a()
{
    LastSaw1(IP1, KEY1, ACC);
    LastSaw1(IP2, KEY2, ACC);

    assert_string_equal(DBGetStr(DBH, "a"IP1), KEY1);
    assert_string_equal(DBGetStr(DBH, "a"IP2), KEY2);
    assert_string_equal(DBGetStr(DBH, "k"KEY1), IP1);
    assert_string_equal(DBGetStr(DBH, "k"KEY2), IP2);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_1b()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP2, KEY2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY2, IP2), true);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_2a()
{
    LastSaw1(IP1, KEY1, ACC);
    LastSaw1(IP2, KEY1, ACC);

    assert_string_equal(DBGetStr(DBH, "a"IP1), KEY1);
    assert_string_equal(DBGetStr(DBH, "a"IP2), KEY1);
    assert_string_equal(DBGetStr(DBH, "k"KEY1), IP2);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_2b()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP2, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP2), true);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_3a()
{
    LastSaw1(IP1, KEY1, ACC);
    LastSaw1(IP1, KEY2, ACC);

    assert_string_equal(DBGetStr(DBH, "a"IP1), KEY2);
    assert_string_equal(DBGetStr(DBH, "k"KEY1), IP1);
    assert_string_equal(DBGetStr(DBH, "k"KEY2), IP1);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_3b()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP1), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY2, IP1), true);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_4a()
{
    LastSaw1(IP1, KEY1, ACC);
    LastSaw1(IP2, KEY1, ACC);
    LastSaw1(IP3, KEY1, ACC);

    assert_string_equal(DBGetStr(DBH, "a"IP1), KEY1);
    assert_string_equal(DBGetStr(DBH, "a"IP2), KEY1);
    assert_string_equal(DBGetStr(DBH, "a"IP3), KEY1);
    assert_string_equal(DBGetStr(DBH, "k"KEY1), IP3);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_4b()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP2, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP3, KEY1), true);
    /* Just a bit different than what the lastseen API created in the 4a case,
     * but still consistent. */
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP1), true);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_5a()
{
    LastSaw1(IP1, KEY1, ACC);
    LastSaw1(IP2, KEY1, ACC);
    LastSaw1(IP2, KEY2, ACC);

    assert_string_equal(DBGetStr(DBH, "a"IP1), KEY1);
    assert_string_equal(DBGetStr(DBH, "a"IP2), KEY2);
    assert_string_equal(DBGetStr(DBH, "k"KEY1), IP2);
    assert_string_equal(DBGetStr(DBH, "k"KEY2), IP2);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_5b()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP2, KEY2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY2, IP2), true);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_6a()
{
    LastSaw1(IP1, KEY1, ACC);                    /* initial bootstrap */
    LastSaw1(IP2, KEY1, ACC);                    /* move to new IP */
    LastSaw1(IP2, KEY2, ACC);                    /* issue new key */
    LastSaw1(IP2, KEY1, ACC);                    /* restore old key */

    assert_string_equal(DBGetStr(DBH, "a"IP1), KEY1);
    assert_string_equal(DBGetStr(DBH, "a"IP2), KEY1);
    assert_string_equal(DBGetStr(DBH, "k"KEY1), IP2);
    assert_string_equal(DBGetStr(DBH, "k"KEY2), IP2);

    assert_int_equal(IsLastSeenCoherent(), true);
}
static void test_consistent_6b()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP2, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY2, IP2), true);

    assert_int_equal(IsLastSeenCoherent(), true);
}
/**
 * @NOTE I haven't been able to get the consistent_7 case with regular
 *       dbm_api.h calls. Maybe it should be considered inconsistent state of
 *       the db?
 */
/*
static void test_consistent_7a()
{
    LastSaw1(IP2, KEY1, ACC);
    LastSaw1(IP1, KEY2, ACC);
    LastSaw1(IP2, KEY1, ACC);
    LastSaw1(IP1, KEY1, ACC);

    assert_string_equal(DBGetStr(DBH, "a"IP1), KEY1);
    assert_string_equal(DBGetStr(DBH, "a"IP2), KEY1);
    assert_string_equal(DBGetStr(DBH, "k"KEY1), IP2);
    assert_string_equal(DBGetStr(DBH, "k"KEY2), IP1);

    assert_int_equal(IsLastSeenCoherent(), true);
}
*/
static void test_consistent_7b()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP2, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY2, IP1), true);

    assert_int_equal(IsLastSeenCoherent(), true);
}


/**
 * ===== INCONSISTENT CASES =====
 * Should never happen if our software is bug-free!
 *
 *
 * 1. KEY2 appears as a value but not "kKEY2" as a key.
 *
 * aIP1  -> KEY2
 *
 *
 * 2. Same case, a bit more complex example.
 *
 * aIP1  -> KEY1
 * aIP2  -> KEY2
 * aKEY1 -> IP1
 *
 *
 * 3. IP2 appears as a value but not "aIP2" as a key.
 *
 * kKEY1 -> IP2
 *
 *
 * 4. Same case, a bit more complex example.
 *
 * aIP1  -> KEY1
 * kKEY1 -> aIP1
 * kKEY2 -> aIP2
 *
 *
 * 5. The two previous cases at the same time!
 *
 * kKEY1 -> IP2
 * aIP1  -> KEY2
 *
 *
 * 6. Same, a bit more complex example.
 *
 * aIP1  -> KEY1
 * aIP3  -> KEY2
 * kKEY1 -> IP2
 * kKEY3 -> IP2
 *
 */

static void test_inconsistent_1()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY2), true);

    assert_int_equal(IsLastSeenCoherent(), false);
}
static void test_inconsistent_2()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP2, KEY2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP1), true);

    assert_int_equal(IsLastSeenCoherent(), false);
}
static void test_inconsistent_3()
{
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP2), true);

    assert_int_equal(IsLastSeenCoherent(), false);
}
static void test_inconsistent_4()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP1), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY2, IP2), true);

    assert_int_equal(IsLastSeenCoherent(), false);
}
static void test_inconsistent_5()
{
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP2), true);
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY2), true);

    assert_int_equal(IsLastSeenCoherent(), false);
}
static void test_inconsistent_6()
{
    assert_int_equal(DBPutStr(DBH, "a"IP1, KEY1), true);
    assert_int_equal(DBPutStr(DBH, "a"IP3, KEY2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY1, IP2), true);
    assert_int_equal(DBPutStr(DBH, "k"KEY3, IP2), true);

    assert_int_equal(IsLastSeenCoherent(), false);
}



/* TODO run lastseen consistency checks after every cf-serverd *acceptance*
 *      test, deployment test, and stress test! */



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

            unit_test_setup_teardown(test_consistent_1a, begin, end),
            unit_test_setup_teardown(test_consistent_1b, begin, end),
            unit_test_setup_teardown(test_consistent_2a, begin, end),
            unit_test_setup_teardown(test_consistent_2b, begin, end),
            unit_test_setup_teardown(test_consistent_3a, begin, end),
            unit_test_setup_teardown(test_consistent_3b, begin, end),
            unit_test_setup_teardown(test_consistent_4a, begin, end),
            unit_test_setup_teardown(test_consistent_4b, begin, end),
            unit_test_setup_teardown(test_consistent_5a, begin, end),
            unit_test_setup_teardown(test_consistent_5b, begin, end),
            unit_test_setup_teardown(test_consistent_6a, begin, end),
            unit_test_setup_teardown(test_consistent_6b, begin, end),
//            unit_test_setup_teardown(test_consistent_7a, begin, end),
            unit_test_setup_teardown(test_consistent_7b, begin, end),
            unit_test_setup_teardown(test_inconsistent_1, begin, end),
            unit_test_setup_teardown(test_inconsistent_2, begin, end),
            unit_test_setup_teardown(test_inconsistent_3, begin, end),
            unit_test_setup_teardown(test_inconsistent_4, begin, end),
            unit_test_setup_teardown(test_inconsistent_5, begin, end),
            unit_test_setup_teardown(test_inconsistent_6, begin, end),
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

