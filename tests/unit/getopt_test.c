#include <test.h>

#include <misc_lib.h>
#include <alloc.h>
#include <string_lib.h>
#include <getopt.h>
#include <sequence.h>

/* Test that we have a working version of getopt functionality, either
 * system-provided or our own in libcfecompat. */

static Seq *CheckOpts(int argc, char **argv)
{
    Seq *seq = SeqNew(3, free);

    const struct option OPTIONS[] =
    {
        {"no-arg", no_argument, 0, 'n'},
        {"opt-arg", optional_argument, 0, 'o'},
        {"req-arg", required_argument, 0, 'r'},
        {NULL, 0, 0, '\0'}
    };

    /* Reset optind to 1 in order to restart scanning of argv
     *  - getopt(3) – Linux Manual
     */
    optind = 1; 

    int c;
    while ((c = getopt_long(argc, argv, "no::r:", OPTIONS, NULL)) != -1)
    {
        switch (c)
        {
        case 'n': /* no argument */
            SeqAppend(seq, xstrdup("none"));
            break;

        case 'o': /* optional argument */
            if (OPTIONAL_ARGUMENT_IS_PRESENT)
            {
                SeqAppend(seq, xstrdup(optarg));
            }
            else
            {
                SeqAppend(seq, xstrdup("default"));
            }
            break;

        case 'r': /* required argument */
            SeqAppend(seq, xstrdup(optarg));
            break;

        default:
            assert(false);
            break;
        }
    }

    return seq;
}

static void test_GET_OPTIONAL_ARGUMENT(void)
{
    /* optional as middle option */
    int argc1 = 6;
    char *argv1[] =
    {
        "program",
        "-n",
        "-o", "foo",
        "-r", "bar"
    };
    Seq *seq1 = CheckOpts(argc1, argv1);
    assert_int_equal(SeqLength(seq1), 3);
    assert_string_equal((char *) SeqAt(seq1, 0), "none");
    assert_string_equal((char *) SeqAt(seq1, 1), "foo");
    assert_string_equal((char *) SeqAt(seq1, 2), "bar");
    SeqDestroy(seq1);

    /* optional as last option */
    int argc2 = 6;
    char *argv2[] =
    {
        "program",
        "-r", "bar",
        "-n",
        "-o", "foo"
    };
    Seq *seq2 = CheckOpts(argc2, argv2);
    assert_int_equal(SeqLength(seq2), 3);
    assert_string_equal((char *) SeqAt(seq2, 0), "bar");
    assert_string_equal((char *) SeqAt(seq2, 1), "none");
    assert_string_equal((char *) SeqAt(seq2, 2), "foo");
    SeqDestroy(seq2);

    /* optional as first option */
    int argc3 = 6;
    char *argv3[] =
    {
        "program",
        "-o", "foo",
        "-r", "bar",
        "-n"
    };
    Seq *seq3 = CheckOpts(argc3, argv3);
    assert_int_equal(SeqLength(seq3), 3);
    assert_string_equal((char *) SeqAt(seq3, 0), "foo");
    assert_string_equal((char *) SeqAt(seq3, 1), "bar");
    assert_string_equal((char *) SeqAt(seq3, 2), "none");
    SeqDestroy(seq3);

    /* optional with no argument */
    int argc4 = 5;
    char *argv4[] =
    {
        "program",
        "-n",
        "-o",
        "-r", "bar"
    };
    Seq *seq4 = CheckOpts(argc4, argv4);
    assert_int_equal(SeqLength(seq4), 3);
    assert_string_equal((char *) SeqAt(seq4, 0), "none");
    assert_string_equal((char *) SeqAt(seq4, 1), "default");
    assert_string_equal((char *) SeqAt(seq4, 2), "bar");
    SeqDestroy(seq4);

    /* optional with argument immediately after */
    int argc5 = 5;
    char *argv5[] =
    {
        "program",
        "-n",
        "-ofoo",
        "-r", "bar"
    };
    Seq *seq5 = CheckOpts(argc5, argv5);
    assert_int_equal(SeqLength(seq5), 3);
    assert_string_equal((char *) SeqAt(seq5, 0), "none");
    assert_string_equal((char *) SeqAt(seq5, 1), "foo");
    assert_string_equal((char *) SeqAt(seq5, 2), "bar");
    SeqDestroy(seq5);

    /* optional as only option with argument */
    int argc6 = 3;
    char *argv6[] =
    {
        "program",
        "-o", "foo"
    };
    Seq *seq6 = CheckOpts(argc6, argv6);
    assert_int_equal(SeqLength(seq6), 1);
    assert_string_equal((char *) SeqAt(seq6, 0), "foo");
    SeqDestroy(seq6);

    /* optional as only option with argument immediately after */
    int argc7 = 2;
    char *argv7[] =
    {
        "program",
        "-ofoo"
    };
    Seq *seq7 = CheckOpts(argc7, argv7);
    assert_int_equal(SeqLength(seq7), 1);
    assert_string_equal((char *) SeqAt(seq7, 0), "foo");
    SeqDestroy(seq7);

    /* optinal as only option with no argument */
    int argc8 = 2;
    char *argv8[] =
    {
        "program",
        "-o"
    };
    Seq *seq8 = CheckOpts(argc8, argv8);
    assert_int_equal(SeqLength(seq8), 1);
    assert_string_equal((char *) SeqAt(seq8, 0), "default");
    SeqDestroy(seq8);
}

int main()
{
    srand48(time(NULL));

    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_GET_OPTIONAL_ARGUMENT),
    };

    return run_tests(tests);
}
