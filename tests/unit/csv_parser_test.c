#include <test.h>
#include <sequence.h>
#include <alloc.h>
#include <csv_parser.h>

static void test_new_csv_reader_basic()
{
    Seq *list = NULL;
    char *lines[4] = {
        "aaaa,bbb,ccccc",
        "\"aaaa\",\"bbb\",\"ccccc\"",
        "\"aaaa\",bbb,\"ccccc\"",
        "aaaa,\"bbb\",ccccc"
    };


    for (int i=0; i<4; i++)
    {
        list = SeqParseCsvString(lines[i]);
        assert_int_equal(list->length, 3);
        assert_string_equal(list->data[2], "ccccc");
        if (list != NULL)
        {
            SeqDestroy(list);
        } else {
            assert_true(false);
        }
    }
}

static void test_new_csv_reader()
{
    Seq *list = NULL;
    char line[]="  Type    ,\"walo1\",  \"walo2\"   ,  \"wal,o \"\" 3\",  \"  ab,cd  \", walo solo ,, \"walo\"";


    list = SeqParseCsvString(line);

    assert_int_equal(list->length, 8);
    assert_string_equal(list->data[3], "wal,o \" 3");
    assert_string_equal(list->data[6], "");
    assert_string_equal(list->data[7], "walo");
    if (list != NULL)
    {
        SeqDestroy(list);
    }
    else
    {
        assert_true(false);
    }
}

static void test_new_csv_reader_lfln()
{
    Seq *list = NULL;
    char line[]="  Type    ,\"walo1\",  \"walo2\"   ,  \"wa\r\nl,o\n \"\"\r 3\",  \"  ab,cd  \", walo solo ,, \"walo\" ";


    list = SeqParseCsvString(line);

    assert_int_equal(list->length, 8);
    assert_string_equal(list->data[3], "wa\r\nl,o\n \"\r 3");
    if (list != NULL)
    {
        SeqDestroy(list);
    }
    else
    {
        assert_true(false);
    }
}

static void test_new_csv_reader_lfln_at_end()
{
    Seq *list = NULL;
    char line[]="  Type    ,\"walo1\",  \"walo2\"   ,  \"wal\r\n,o \"\" 3\",  \"  ab,cd  \", walo solo , , \"walo\"       \r\n           ";


    list = SeqParseCsvString(line);

    assert_int_equal(list->length, 8);
    assert_string_equal(list->data[3], "wal\r\n,o \" 3");
    assert_string_equal(list->data[6], " ");
    assert_string_equal(list->data[7], "walo");
    if (list != NULL)
    {
        SeqDestroy(list);
    }
    else
    {
        assert_true(false);
    }
}

static void test_new_csv_reader_lfln_at_end2()
{
    Seq *list = NULL;
    char line[]="  Type    ,\"walo1\",  \"walo2\"   ,  \"wal\r\n,o \"\" 3\",  \"  ab,cd  \", walo solo , ,walo\r\n";

    list = SeqParseCsvString(line);

    assert_int_equal(list->length, 8);
    assert_string_equal(list->data[7], "walo");
    if (list != NULL)
    {
        SeqDestroy(list);
    }
    else
    {
        assert_true(false);
    }
}

static void test_new_csv_reader_lfln_at_end3()
{
    Seq *list = NULL;
    char line[]="  Type    ,\"walo1\",  \"walo2\"   ,  \"wal\r\n,o \"\" 3\",  \"  ab,cd  \", walo solo , , \r\n";


    list = SeqParseCsvString(line);

    assert_int_equal(list->length, 8);
    assert_string_equal(list->data[7], " ");
    if (list != NULL)
    {
        SeqDestroy(list);
    }
    else
    {
        assert_true(false);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_new_csv_reader_basic),
        unit_test(test_new_csv_reader),
        unit_test(test_new_csv_reader_lfln),
        unit_test(test_new_csv_reader_lfln_at_end),
        unit_test(test_new_csv_reader_lfln_at_end2),
        unit_test(test_new_csv_reader_lfln_at_end3),
    };

    return run_tests(tests);
}

