#include <test.h>

#include <string.h>
#include <cfversion.h>
#include <cfversion.c>
#include <buffer.h>

static void test_creation_destruction(void)
{
    char right_version[] = "1.2.3";
    char wrong_version[] = "a.0.1";
    char right_unix_version[] = "1.2.3-4";
    char wrong_unix_version_0[] = "3-5.1-1";
    char wrong_unix_version_1[] = "3.5-1-1";
    char wrong_unix_version_2[] = "3.5.1.1";
    char right_windows_version[] = "1.2.3.4-5";
    char wrong_windows_version_0[] = "3-5.1.0-1";
    char wrong_windows_version_1[] = "3.5-1.0-1";
    char wrong_windows_version_2[] = "3.5.1-0-1";
    char wrong_windows_version_3[] = "3.5.1.0.1";
    char large_version_0[] = "256.1.2.3-4";
    char large_version_1[] = "1.256.3-4";
    char large_version_2[] = "1.2.256.3-4";
    char large_version_3[] = "1.2.3.256-4";

    Version *version = NULL;
    version = VersionNew();
    assert_true (version != NULL);
    assert_int_equal(version->major, 0);
    assert_int_equal(version->minor, 0);
    assert_int_equal(version->patch, 0);
    assert_int_equal(version->extra, 0);
    assert_int_equal(version->build, 0);
    VersionDestroy(&version);
    assert_true (version == NULL);

    version = VersionNewFromCharP(right_version, strlen(right_version));
    assert_true(version != NULL);
    assert_int_equal(version->major, 1);
    assert_int_equal(version->minor, 2);
    assert_int_equal(version->patch, 3);
    assert_int_equal(version->extra, 0);
    assert_int_equal(version->build, 0);
    VersionDestroy(&version);
    assert_true (version == NULL);

    version = VersionNewFromCharP(right_unix_version, strlen(right_unix_version));
    assert_true(version != NULL);
    assert_int_equal(version->major, 1);
    assert_int_equal(version->minor, 2);
    assert_int_equal(version->patch, 3);
    assert_int_equal(version->extra, 0);
    assert_int_equal(version->build, 4);
    VersionDestroy(&version);
    assert_true (version == NULL);

    version = VersionNewFromCharP(right_windows_version, strlen(right_windows_version));
    assert_true(version != NULL);
    assert_int_equal(version->major, 1);
    assert_int_equal(version->minor, 2);
    assert_int_equal(version->patch, 3);
    assert_int_equal(version->extra, 4);
    assert_int_equal(version->build, 5);
    VersionDestroy(&version);
    assert_true (version == NULL);

    version = VersionNewFromCharP(wrong_version, strlen(wrong_version));
    assert_true(version == NULL);
    version = VersionNewFromCharP(wrong_unix_version_0, strlen(wrong_unix_version_0));
    assert_true(version == NULL);
    version = VersionNewFromCharP(wrong_unix_version_1, strlen(wrong_unix_version_1));
    assert_true(version == NULL);
    version = VersionNewFromCharP(wrong_unix_version_2, strlen(wrong_unix_version_2));
    assert_true(version == NULL);
    version = VersionNewFromCharP(wrong_windows_version_0, strlen(wrong_windows_version_0));
    assert_true(version == NULL);
    version = VersionNewFromCharP(wrong_windows_version_1, strlen(wrong_windows_version_1));
    assert_true(version == NULL);
    version = VersionNewFromCharP(wrong_windows_version_2, strlen(wrong_windows_version_2));
    assert_true(version == NULL);
    version = VersionNewFromCharP(wrong_windows_version_3, strlen(wrong_windows_version_3));
    assert_true(version == NULL);
    version = VersionNewFromCharP(large_version_0, strlen(large_version_0));
    assert_true(version == NULL);
    version = VersionNewFromCharP(large_version_1, strlen(large_version_1));
    assert_true(version == NULL);
    version = VersionNewFromCharP(large_version_2, strlen(large_version_2));
    assert_true(version == NULL);
    version = VersionNewFromCharP(large_version_3, strlen(large_version_3));
    assert_true(version == NULL);

    /* Creation from a buffer uses the same path as the creation from a char p, we just test the two main cases */
    Buffer *buffer = NULL;
    buffer = BufferNewFrom(right_version, strlen(right_version));
    assert_true (buffer != NULL);
    version = VersionNewFrom(buffer);
    assert_true(version != NULL);
    assert_int_equal(version->major, 1);
    assert_int_equal(version->minor, 2);
    assert_int_equal(version->patch, 3);
    assert_int_equal(version->extra, 0);
    assert_int_equal(version->build, 0);
    VersionDestroy(&version);
    assert_true (version == NULL);
    BufferDestroy(buffer);

    buffer = BufferNewFrom(wrong_version, strlen(wrong_version));
    assert_true (buffer != NULL);
    version = VersionNewFrom(buffer);
    assert_true(version == NULL);
    BufferDestroy(buffer);
}

static void test_comparison(void)
{
    char lowest_version[] = "0.0.0.0-0";
    char normal_version[] = "1.2.3.0-4";
    char normal_version_with_extra[] = "1.2.3.9-4";
    char highest_version[] = "255.255.255.0-255";

    Version *a = NULL;
    Version *b = NULL;

    a = VersionNewFromCharP(lowest_version, strlen(lowest_version));
    assert_true(a != NULL);
    b = VersionNewFromCharP(normal_version, strlen(normal_version));
    assert_true(b != NULL);
    assert_true(VersionCompare(a, b) < 0);
    assert_true(VersionCompare(b, a) > 0);
    assert_true(VersionCompare(a, b) != 0);

    VersionDestroy(&a);
    assert_true(a == NULL);
    a = VersionNewFromCharP(normal_version_with_extra, strlen(normal_version_with_extra));
    assert_true(a != NULL);
    assert_int_equal(VersionCompare(a,b), 0);

    VersionDestroy(&b);
    assert_true(b == NULL);
    b = VersionNewFromCharP(highest_version, strlen(highest_version));
    assert_true(VersionCompare(a,b) < 0);

    VersionDestroy(&a);
    assert_true(a == NULL);
    VersionDestroy(&b);
    assert_true(b == NULL);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_creation_destruction)
        , unit_test(test_comparison)
    };

    return run_tests(tests);
}

