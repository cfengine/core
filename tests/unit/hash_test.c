#include <test.h>

#include <unistd.h>
#include <string.h>
#include <hash.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bn.h>


/*
 * To test the hash implementation we need three things:
 * - A string
 * - A file
 * - A RSA key
 * We run one test for each, using two algorithms, MD5 and SHA256.
 */
static int initialized = 0;
static char message[] = "This is a message";
static int message_length = 0;
static char file[] = "/tmp/hashXXXXXX";
static int fd = -1;
static RSA *rsa = NULL;
void tests_setup()
{
    int result = 0;
    fd = mkstemp(file);
    if (fd < 0)
    {
        initialized = 0;
        return;
    }
    message_length = strlen(message);
    result = write(fd, message, message_length);
    if (result < 0)
    {
        close (fd);
        unlink (file);
        initialized = 0;
        return;
    }
    rsa = RSA_new();
    if (rsa)
    {
        BIGNUM *bn = NULL;
        bn = BN_new();
        if (!bn)
        {
            close (fd);
            unlink (file);
            RSA_free(rsa);
            initialized = 0;
            return;
        }
        BN_set_word(bn, 3);
        RSA_generate_key_ex(rsa, 1024, bn, NULL);
        BN_free(bn);
    }
    OpenSSL_add_all_digests();
    initialized = 1;
}

void tests_teardown()
{
    if (fd >= 0)
    {
        close (fd);
        unlink (file);
    }
    if (rsa)
    {
        RSA_free(rsa);
    }
    initialized = 0;
}
#define ASSERT_IF_NOT_INITIALIZED \
    assert_int_equal(1, initialized)
/*
 * Tests
 * Each test does the same but from different sources, this is:
 * 1. Create a new hash structure using MD5
 * 2. Check the length of the generated hash.
 * 3. Check the printable version (check that is not NULL).
 * 4. Destroy the hash structure.
 * 5. Create a new hash structure using SHA256.
 * 6. Check the length of the generated hash.
 * 7. Check the printable version (check that is not NULL).
 * 8. Destroy the hash structure.
 */
static void test_HashString(void)
{
    ASSERT_IF_NOT_INITIALIZED;
    Hash *hash = NULL;
    unsigned int length = 0;
    assert_true(hash == NULL);
    hash = HashNew(message, message_length, HASH_METHOD_MD5);
    assert_true(hash != NULL);
    assert_int_equal(HASH_METHOD_MD5, HashType(hash));
    assert_int_equal(CF_MD5_LEN, HashLength(hash));
    assert_true(HashData(hash, &length) != NULL);
    assert_int_equal(length, CF_MD5_LEN);
    assert_true(HashPrintable(hash) != NULL);
    const char *md5_hash = HashPrintable(hash);
    assert_true((md5_hash[0] == 'M') && (md5_hash[1] == 'D') && (md5_hash[2] == '5') && (md5_hash[3] == '='));
    HashDestroy(&hash);
    assert_true(hash == NULL);
    hash = HashNew(message, message_length, HASH_METHOD_SHA256);
    assert_true(hash != NULL);
    assert_int_equal(HASH_METHOD_SHA256, HashType(hash));
    assert_int_equal(CF_SHA256_LEN, HashLength(hash));
    assert_true(HashData(hash, &length) != NULL);
    assert_int_equal(length, CF_SHA256_LEN);
    assert_true(HashPrintable(hash) != NULL);
    const char *sha256_hash = HashPrintable(hash);
    assert_true((sha256_hash[0] == 'S') && (sha256_hash[1] == 'H') && (sha256_hash[2] == 'A') && (sha256_hash[3] == '='));
    HashDestroy(&hash);
    /* Negative cases */
    assert_true(HashNew(NULL, message_length, HASH_METHOD_MD5) == NULL);
    assert_true(HashNew(message, 0, HASH_METHOD_MD5) == NULL);
    assert_true(HashNew(message, message_length, HASH_METHOD_NONE) == NULL);
    assert_true(HashNew(message, 0, HASH_METHOD_NONE) == NULL);
    assert_true(HashNew(NULL, message_length, HASH_METHOD_NONE) == NULL);
}

static void test_HashDescriptor(void)
{
    ASSERT_IF_NOT_INITIALIZED;
    Hash *hash = NULL;
    unsigned int length = 0;
    assert_true(hash == NULL);
    hash = HashNewFromDescriptor(fd, HASH_METHOD_MD5);
    assert_true(hash != NULL);
    assert_int_equal(HASH_METHOD_MD5, HashType(hash));
    assert_int_equal(CF_MD5_LEN, HashLength(hash));
    assert_true(HashData(hash, &length) != NULL);
    assert_int_equal(length, CF_MD5_LEN);
    assert_true(HashPrintable(hash) != NULL);
    HashDestroy(&hash);
    assert_true(hash == NULL);
    hash = HashNewFromDescriptor(fd, HASH_METHOD_SHA256);
    assert_true(hash != NULL);
    assert_int_equal(HASH_METHOD_SHA256, HashType(hash));
    assert_int_equal(CF_SHA256_LEN, HashLength(hash));
    assert_true(HashData(hash, &length) != NULL);
    assert_int_equal(length, CF_SHA256_LEN);
    assert_true(HashPrintable(hash) != NULL);
    HashDestroy(&hash);
    /* Negative cases */
    assert_true(HashNewFromDescriptor(-1, HASH_METHOD_MD5) == NULL);
    assert_true(HashNewFromDescriptor(fd, HASH_METHOD_NONE) == NULL);
    assert_true(HashNewFromDescriptor(-1, HASH_METHOD_NONE) == NULL);
}

static void test_HashKey(void)
{
    ASSERT_IF_NOT_INITIALIZED;
    Hash *hash = NULL;
    unsigned int length = 0;
    assert_true(hash == NULL);
    hash = HashNewFromKey(rsa, HASH_METHOD_MD5);
    assert_true(hash != NULL);
    assert_int_equal(HASH_METHOD_MD5, HashType(hash));
    assert_int_equal(CF_MD5_LEN, HashLength(hash));
    assert_true(HashData(hash, &length) != NULL);
    assert_int_equal(length, CF_MD5_LEN);
    assert_true(HashPrintable(hash) != NULL);
    HashDestroy(&hash);
    assert_true(hash == NULL);
    hash = HashNewFromKey(rsa, HASH_METHOD_SHA256);
    assert_true(hash != NULL);
    assert_int_equal(HASH_METHOD_SHA256, HashType(hash));
    assert_int_equal(CF_SHA256_LEN, HashLength(hash));
    assert_true(HashData(hash, &length) != NULL);
    assert_int_equal(length, CF_SHA256_LEN);
    assert_true(HashPrintable(hash) != NULL);
    HashDestroy(&hash);
    /* Negative cases */
    assert_true(HashNewFromKey(NULL, HASH_METHOD_MD5) == NULL);
    assert_true(HashNewFromKey(rsa, HASH_METHOD_NONE) == NULL);
    assert_true(HashNewFromKey(NULL, HASH_METHOD_NONE) == NULL);
}

static void test_HashCopy(void)
{
    ASSERT_IF_NOT_INITIALIZED;
    Hash *hash = NULL;
    Hash *copy = NULL;
    unsigned int length = 0;
    assert_true(hash == NULL);
    hash = HashNew(message, message_length, HASH_METHOD_MD5);
    assert_true(hash != NULL);
    assert_int_equal(HASH_METHOD_MD5, HashType(hash));
    assert_int_equal(CF_MD5_LEN, HashLength(hash));
    assert_true(HashData(hash, &length) != NULL);
    assert_int_equal(length, CF_MD5_LEN);
    assert_true(HashPrintable(hash) != NULL);
    assert_int_equal(0, HashCopy(hash, &copy));
    assert_int_equal(HASH_METHOD_MD5, HashType(copy));
    assert_int_equal(CF_MD5_LEN, HashLength(copy));
    assert_true(HashData(copy, &length) != NULL);
    assert_int_equal(length, CF_MD5_LEN);
    assert_true(HashPrintable(copy) != NULL);
    assert_string_equal(HashPrintable(hash), HashPrintable(copy));
    HashDestroy(&copy);
    assert_true(copy == NULL);
    /* Negative cases */
    assert_int_equal(-1, HashCopy(NULL, &copy));
    assert_int_equal(-1, HashCopy(hash, NULL));
    assert_int_equal(-1, HashCopy(NULL, NULL));
    /* Finish */
    HashDestroy(&hash);
    assert_true(hash == NULL);
}

/*
 * Main routine
 * Notice the calls to both setup and teardown.
 */
int main()
{
    PRINT_TEST_BANNER();
    tests_setup();
    const UnitTest tests[] =
    {
        unit_test(test_HashString),
        unit_test(test_HashDescriptor),
        unit_test(test_HashKey),
        unit_test(test_HashCopy)
    };
    int result = run_tests(tests);
    tests_teardown();
    return result;
}

