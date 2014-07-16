#include <test.h>

#include <unistd.h>
#include <string.h>
#include <key.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

/*
 * Initialization
 */
static int initialized = 0;
static RSA *rsa = NULL;
void test_setup()
{
    rsa = RSA_new();
    if (rsa)
    {
        BIGNUM *bn = NULL;
        bn = BN_new();
        if (!bn)
        {
            RSA_free(rsa);
            initialized = 0;
            return;
        }
        BN_set_word(bn, 3);
        RSA_generate_key_ex(rsa, 1024, bn, NULL);
        BN_free(bn);
    }
    initialized = 1;
}

void test_teardown()
{
    rsa = NULL;
    initialized = 0;
}
#define ASSERT_IF_NOT_INITIALIZED \
    assert_int_equal(1, initialized)
/*
 * Tests
 */
static void test_key_basic(void)
{
    test_setup();
    ASSERT_IF_NOT_INITIALIZED;
    Key *key = NULL;
    assert_true(key == NULL);
    key = KeyNew(rsa, HASH_METHOD_MD5);
    assert_true(key != NULL);
    assert_int_equal(HASH_METHOD_MD5, KeyHashMethod(key));
    assert_true(rsa == KeyRSA(key));
    unsigned int length = 0;
    assert_true(KeyBinaryHash(key, &length) != NULL);
    assert_int_equal(CF_MD5_LEN, length);
    assert_true(KeyPrintableHash(key) != NULL);
    /* Negative cases */
    assert_true(NULL == KeyNew(NULL, HASH_METHOD_MD5));
    assert_true(NULL == KeyNew(rsa, HASH_METHOD_NONE));
    assert_true(NULL == KeyNew(NULL, HASH_METHOD_NONE));
    /* Finish */
    KeyDestroy(&key);
    assert_true(key == NULL);
    test_teardown();
}

static void test_key_hash(void)
{
    test_setup();
    ASSERT_IF_NOT_INITIALIZED;
    Key *key = NULL;
    assert_true(key == NULL);
    key = KeyNew(rsa, HASH_METHOD_MD5);
    assert_true(key != NULL);
    assert_int_equal(HASH_METHOD_MD5, KeyHashMethod(key));
    /* We now examine the first four bytes of the hash, to check the printable bit */
    const char *md5_hash = KeyPrintableHash(key);
    assert_true((md5_hash[0] == 'M') && (md5_hash[1] == 'D') && (md5_hash[2] == '5') && (md5_hash[3] == '='));
    /* When we change the hashing algorithm, a new hash is automatically generated. */
    assert_int_equal(0, KeySetHashMethod(key, HASH_METHOD_SHA256));
    const char *sha256_hash = KeyPrintableHash(key);
    assert_true((sha256_hash[0] == 'S') && (sha256_hash[1] == 'H') && (sha256_hash[2] == 'A') && (sha256_hash[3] == '='));
    KeyDestroy(&key);
    test_teardown();
}

/*
 * Main routine
 * Notice the calls to both setup and teardown.
 */
int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_key_basic),
        unit_test(test_key_hash)
    };
    OpenSSL_add_all_digests();
    int result = run_tests(tests);
    return result;
}
