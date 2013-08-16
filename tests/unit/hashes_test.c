#include <test.h>

#include <crypto.h>
#include <hashes.h>
#include <alloc.h>

char *MakeTempFileWithContent(const char *content)
{
    char *path = tempnam(NULL, "cfengine_test");
    FILE *fp = fopen(path, "w");

    assert_true(fp);
    assert_true(fputs(content, fp) >= 0);

    assert_int_equal(0, fclose(fp));
    return xstrdup(path);
}

static void test_file_checksum(void)
{
    {
        char *file1 = MakeTempFileWithContent("snookie");
        char *file2 = MakeTempFileWithContent("jwow");

        unsigned char digest1[EVP_MAX_MD_SIZE + 1] = { 0 };
        int len1 = FileChecksum(file1, digest1);
        unlink(file1);
        free(file1);

        unsigned char digest2[EVP_MAX_MD_SIZE + 1] = { 0 };
        FileChecksum(file2, digest2);
        unlink(file2);
        free(file2);

        bool equal = true;
        for (int i = 0; i < len1; i++)
        {
            if (digest1[i] != digest2[i])
            {
                equal = false;
            }
        }
        assert_false(equal);
    }

    {
        char *file1 = MakeTempFileWithContent("snookie");
        char *file2 = MakeTempFileWithContent("snookie");

        unsigned char digest1[EVP_MAX_MD_SIZE + 1] = { 0 };
        int len1 = FileChecksum(file1, digest1);
        unlink(file1);
        free(file1);

        unsigned char digest2[EVP_MAX_MD_SIZE + 1] = { 0 };
        FileChecksum(file2, digest2);
        unlink(file2);
        free(file2);

        bool equal = true;
        for (int i = 0; i < len1; i++)
        {
            if (digest1[i] != digest2[i])
            {
                equal = false;
            }
        }
        assert_true(equal);
    }
}

int main()
{
    PRINT_TEST_BANNER();

    CryptoInitialize();

    const UnitTest tests[] =
    {
        unit_test(test_file_checksum)
    };

    return run_tests(tests);
}
