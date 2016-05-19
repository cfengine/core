#include <test.h>

#include <cf3.defs.h>
#include <crypto.h>

#define PLAINTEXT "123456789012345678901234567890123"
#define KEY "1234567890123456789012345678901234567890123456789012345678901234"  /* at least 512 bits long (to be sure) */

// use Blowfish (64-bit block size) for now
#define CIPHER_TYPE_CFENGINE 'c'
#define CIPHER_BLOCK_SIZE_BYTES 8
static const char CIPHERTEXT_PRECOMPUTED[] = 
{ 
    0x99, 0xfd, 0x86, 0x9c, 0x17, 0xb9, 0xe4, 0x98,
    0xab, 0x01, 0x17, 0x5a, 0x4a, 0xcf, 0xfc, 0x1f, 
    0xd4, 0xc5, 0xa3, 0xab, 0xf0, 0x1c, 0xa7, 0x39, 
    0xf1, 0xf4, 0x09, 0xe4, 0xac, 0xb6, 0x44, 0xbb, 
    0x47, 0xdd, 0xe6, 0xc4, 0x0e, 0x4a, 0x16, 0xf0 
};

static int ComputeCiphertextLen(int plaintext_len, int cipher_block_size_bytes)
{
    int last_block_offset = plaintext_len % cipher_block_size_bytes;
    int padding = cipher_block_size_bytes - last_block_offset;

    return (plaintext_len + padding);
}

static void test_cipher_init(void)
{
    unsigned char key[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    unsigned char iv[] = {1,2,3,4,5,6,7,8};
    EVP_CIPHER_CTX ctx;

    EVP_CIPHER_CTX_init(&ctx);
    EVP_EncryptInit_ex(&ctx, EVP_bf_cbc(), NULL, key, iv);
    EVP_CIPHER_CTX_cleanup(&ctx);
}

static void test_symmetric_encrypt(void)
{
    char ciphertext[CF_BUFSIZE];
    int plaintext_len = strlen(PLAINTEXT) + 1;
    
    int ciphertext_len = EncryptString(ciphertext, sizeof(ciphertext),
                                       PLAINTEXT, plaintext_len,
                                       CIPHER_TYPE_CFENGINE, KEY);

    assert_int_equal(ciphertext_len, ComputeCiphertextLen(plaintext_len, CIPHER_BLOCK_SIZE_BYTES));

    assert_memory_equal(ciphertext, CIPHERTEXT_PRECOMPUTED, ciphertext_len);
}

static void test_symmetric_decrypt(void)
{
    char *ciphertext = (char *)CIPHERTEXT_PRECOMPUTED;
    int ciphertext_len = sizeof(CIPHERTEXT_PRECOMPUTED);
    
    char plaintext_out[CF_BUFSIZE];
    
    int plaintext_len = DecryptString(CIPHER_TYPE_CFENGINE, ciphertext, plaintext_out, KEY, ciphertext_len);

    assert_int_equal(plaintext_len, strlen(PLAINTEXT) + 1);

    assert_string_equal(plaintext_out, PLAINTEXT);
}

static void test_cipher_block_size(void)
{
    assert_int_equal(CipherBlockSizeBytes(EVP_bf_cbc()), 8);
    assert_int_equal(CipherBlockSizeBytes(CfengineCipher('c')), 8);

    assert_int_equal(CipherBlockSizeBytes(EVP_aes_256_cbc()), 16);
    assert_int_equal(CipherBlockSizeBytes(CfengineCipher('N')), 16);
}

static void test_cipher_text_size_max(void)
{
    assert_int_equal(CipherTextSizeMax(EVP_aes_256_cbc(), 1), 32);
    assert_int_equal(CipherTextSizeMax(CfengineCipher('N'), 1), 32);

    assert_int_equal(CipherTextSizeMax(EVP_aes_256_cbc(), CF_BUFSIZE), 4127);
    assert_int_equal(CipherTextSizeMax(CfengineCipher('N'), CF_BUFSIZE), 4127);

    assert_int_equal(CipherTextSizeMax(EVP_bf_cbc(), 1), 16);
    assert_int_equal(CipherTextSizeMax(CfengineCipher('c'), 1), 16);

    assert_int_equal(CipherTextSizeMax(EVP_bf_cbc(), CF_BUFSIZE), 4111);
    assert_int_equal(CipherTextSizeMax(CfengineCipher('c'), CF_BUFSIZE), 4111);
}

int main()
{
    PRINT_TEST_BANNER();
    CryptoInitialize();

    const UnitTest tests[] =
    {
        unit_test(test_cipher_init),
        unit_test(test_symmetric_encrypt),
        unit_test(test_symmetric_decrypt),
        unit_test(test_cipher_block_size),
        unit_test(test_cipher_text_size_max),
    };
    
    return run_tests(tests);
}
