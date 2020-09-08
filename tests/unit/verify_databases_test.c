#include <test.h>

#include <verify_databases.c> // Include .c file to test static functions

void test_ValidateSQLTableName(void)
{
    char db[CF_MAXVARSIZE];
    char table[CF_MAXVARSIZE];

    // Valid database table paths:
    assert_true(ValidateSQLTableName("cfsettings.users", db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "cfsettings");
    assert_string_equal(table, "users");

    assert_true(ValidateSQLTableName("a.b", db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "a");
    assert_string_equal(table, "b");

    assert_true(ValidateSQLTableName(".b", db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "");
    assert_string_equal(table, "b");

    assert_true(ValidateSQLTableName("a.", db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "a");
    assert_string_equal(table, "");


    // Invalid paths:

    char path[CF_MAXVARSIZE]; // ValidateSQLTableName will write to it :O

    strcpy(path, "nosep");
    db[0] = table[0] = '\0';
    assert_false(ValidateSQLTableName(path, db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "");
    assert_string_equal(table, "");

    strcpy(path, "cfsettings\\users/users");
    db[0] = table[0] = '\0';
    assert_false(ValidateSQLTableName(path, db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "");
    assert_string_equal(table, "");

    strcpy(path, "a.b/c");
    db[0] = table[0] = '\0';
    assert_false(ValidateSQLTableName(path, db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "");
    assert_string_equal(table, "");

    // Seems like they should work, but they are invalid because of bugs:
    db[0] = table[0] = '\0';
    strcpy(path, "a/b");
    assert_false(ValidateSQLTableName(path, db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "");
    assert_string_equal(table, "");

    db[0] = table[0] = '\0';
    strcpy(path, "a\\b");
    assert_false(ValidateSQLTableName(path, db, sizeof(db), table, sizeof(table)));
    assert_string_equal(db, "");
    assert_string_equal(table, "");
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_ValidateSQLTableName),
    };

    return run_tests(tests);
}
