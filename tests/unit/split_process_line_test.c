#include <test.h>

#include <processes_select.c>

static void test_split_line_elapsed(void)
{
    /* Collect all test data in one array to make alignments visible: */
    static const char *lines[] = {
        "USER       PID STAT    VSZ  NI   RSS NLWP STIME     ELAPSED     TIME COMMAND",
        "space    14604 S         0   0     0    1 Sep 02 4-03:40:00 00:00:01 true",
        "block    14604 S         0   0     0    1 Sep02  4-03:40:00 00:00:01 true"
    };
    char *name[CF_PROCCOLS]; /* Headers */
    char *field[CF_PROCCOLS]; /* Content */
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    int i, user = 0, stime = 7;
    time_t pstime = 1410000000;        /* 2014-09-06 12:40 */
    const char began[] = "1409641200"; /* 2014-09-02  9:0 */

    /* Prepare data needed by tests and assert things tests can then assume: */
    GetProcessColumnNames(lines[0], name, start, end);
    assert_string_equal(name[user], "USER");
    assert_string_equal(name[stime], "STIME");

    assert_true(SplitProcLine(lines[2], pstime, name, start, end, field));
    assert_string_equal(field[user], "block");
    /* Copes when STIME is a date with a space in it. */
    assert_string_equal(field[stime], began);

    assert_true(SplitProcLine(lines[1], pstime, name, start, end, field));
    assert_string_equal(field[user], "space");
    /* Copes when STIME is a date with a space in it. */
    assert_string_equal(field[stime], began);

    /* Finally, tidy away headers: */
    for (i = 0; name[i] != NULL; i++)
    {
        free(name[i]);
    }
}

static void test_split_line_noelapsed(void)
{
    /* Collect all test data in one array to make alignments visible: */
    static const char *lines[] = {
        "USER       PID STAT    VSZ  NI   RSS NLWP STIME     TIME COMMAND",
        "space    14604 S         0   0     0   1 Jul 02 00:00:01 true",
        "block    14604 S         0   0     0   1  Jul02 00:00:01 true"
    };
    char *name[CF_PROCCOLS]; /* Headers */
    char *field[CF_PROCCOLS]; /* Content */
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    int i, user = 0, stime = 7;
    time_t pstime = 1410000000;

    /* Prepare data needed by tests and assert things tests can then assume: */
    GetProcessColumnNames(lines[0], name, start, end);
    assert_string_equal(name[user], "USER");
    assert_string_equal(name[stime], "STIME");

    assert_true(SplitProcLine(lines[2], pstime, name, start, end, field));
    assert_string_equal(field[user], "block");
    /* Copes when STIME is a date with a space in it. */
    assert_string_equal(field[stime], "Jul02");

    assert_true(SplitProcLine(lines[1], pstime, name, start, end, field));
    assert_string_equal(field[user], "space");
    /* Copes when STIME is a date with a space in it. */
    assert_string_equal(field[stime], "Jul 02");

    /* Finally, tidy away headers: */
    for (i = 0; name[i] != NULL; i++)
    {
        free(name[i]);
    }
}

static void test_split_line_longcmd(void)
{
    static const char *lines[] = {
        "USER       PID STAT    VSZ  NI   RSS NLWP STIME     ELAPSED     TIME COMMAND",
        "longcmd    923 S     32536   0   784    1 10:30 71-00:07:43 00:01:49 "
          "java can end up with some insanely long command-lines, so we need to "
          "be sure that we don't artificially limit the length of the command "
          "field that we see when looking at the process table to match for the "
          "details that the user might be interested in - so here we have an "
          "absurdly long 'command' field just for the sake of testing that it "
          "does not get truncated - see RedMine ticket 3974 for working round the "
          "problems when ps itself does such truncation, but this test is not about "
          "that so much as our own code not doing such truncation - and now for "
          "some random drivel repeated a lot to bulk this command line length out "
          "to more than 4k:"
          /* 638 bytes thus far, +72 per repeat, * 50 for 4238 > 4096: */
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens"
          " the quick brown fox jumped over the lazy dogs after eating the chickens",
    };
    char *name[CF_PROCCOLS]; /* Headers */
    char *field[CF_PROCCOLS]; /* Content */
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    int i, user = 0, command = 10;
    time_t pstime = 1410000000;

    /* Prepare data needed by tests and assert things assumed by test: */
    GetProcessColumnNames(lines[0], name, start, end);
    assert_string_equal(name[user], "USER");
    assert_string_equal(name[command], "COMMAND");

    assert_true(SplitProcLine(lines[1], pstime, name, start, end, field));
    assert_string_equal(field[user], "longcmd");
    /* Does not truncate the command. */
    assert_string_equal(field[command], lines[1] + start[command]);

    /* Finally, tidy away headers: */
    for (i = 0; name[i] != NULL; i++)
    {
        free(name[i]);
    }
}

static void test_split_line(void)
{
  /* Collect all test data in one array to make alignments visible: */
    static const char *lines[] = {
        /* Indent any continuation lines so that it's clear that's what they are. */
        /* Use the username field as a test name to confirm tests are in sync. */

        /* TODO: gather real pathological cases from Solaris (known to
         * produce fields that abut, with no space in between) and any
         * other platforms with similar "quirks". */

        "USER       PID STAT    VSZ  NI   RSS NLWP STIME     ELAPSED     TIME COMMAND",
        "timeboth   923 S     32536   0   784    1 10:30 1271-00:07:43 00:01:49 true"
          " to itself",
        /* timeright: adapted from a Jenkins run that failed. */
        "timeright  941 S     39752   0  4552    1 May20 92-21:17:55 1-02:07:14 true",
        "timesleft  923 S     32536   0   784  1 10:30 1271-00:07:43 00:01:49 true",
        "timeleft   923 S     32536   0   784   1 10:30 271-00:07:43 00:01:49 true",
        "numboth    923 S     32536   0 12784321 1 10:30 71-00:07:43 00:01:49 true",
        "numleft    923 S 123432536   0   784    1 10:30 71-00:07:43 00:01:49 true",
        "wordytoright 4 S         0   0     0    1 10:30       54:29 00:00:01 true",
        "wordright    4 S         0   0     0    1 10:30       54:29 00:00:01 true",

        /* Long-standing: */
        "inspace    923 S     32536   0   784    1 10:30 71-00:07:43 00:01:49 echo"
          " preserve\t embedded   spaces in   a text   field",
        "spacey     923 S     32536   0   784    1 10:30 71-00:07:43 00:01:49    echo "
          "\t \t                     \r\n\f\n\v\n",
        "basic      923 S     32536   0   784    1 10:30 71-00:07:43 00:01:49 true",
        "",
        NULL
    };
    char *name[CF_PROCCOLS]; /* Headers */
    char *field[CF_PROCCOLS]; /* Content */
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    int i, user = 0, vsz = 3, rss = 5, stime = 7, command = 10;
    time_t pstime = 1410000000;

    /* Prepare data needed by tests and assert things tests can then assume: */
    GetProcessColumnNames(lines[0], name, start, end);
    assert_string_equal(name[user], "USER");
    assert_string_equal(name[vsz], "VSZ");
    assert_string_equal(name[rss], "RSS");
    assert_string_equal(name[stime], "STIME");
    assert_string_equal(name[command], "COMMAND");
    assert_int_equal(start[command], 69);

    size_t line = sizeof(lines) / sizeof(const char *);
    /* Higher indexed tests first; test lines[line] then decrement line. */
    assert_false(SplitProcLine(lines[--line], pstime, name, start, end, field)); /* NULL */
    assert_false(SplitProcLine(lines[--line], pstime, name, start, end, field)); /* empty */

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field)); /* basic */
    {
        /* Each field is as expected: */
        const char *each[] = {
            "basic", "923", "S", "32536", "0", "784", "1",
            "10:30", "71-00:07:43", "00:01:49", "true"
        };
        for (i = 0; name[i] != NULL; i++)
        {
            assert_in_range(i, 0, sizeof(each) / sizeof(char *) - 1);
            bool valid = field[i] != NULL && each[i] != NULL;
            assert_true(valid);
            assert_string_equal(field[i], each[i]);
        }
        /* That incidentally covers numeric (VSZ) and time (ELAPSED,
         * TIME) fields overflowing to the left (but not so far as to
         * go under the previous field's header). */
    }
    /* See field[user] checks for names of remaining tests. */

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "spacey");
    /* Discards leading and dangling space in command. */
    assert_string_equal(field[command], "echo");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "inspace");
    /* Preserves spaces within a text field. */
    assert_string_equal(field[command], lines[line] + start[command]);

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    /* Handle a text field overflowing to the right. */
    assert_string_equal(field[user], "wordright");
    /* Shouldn't pollute PID: */
    assert_string_equal(field[user + 1], "4");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    /* Handle a text field overflowing under next header. */
    assert_string_equal(field[user], "wordytoright");
    /* Shouldn't pollute PID: */
    assert_string_equal(field[user + 1], "4");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "numleft");
    /* Handle numeric field overflowing under previous header. */
    assert_string_equal(field[vsz], "123432536");
    /* Shouldn't pollute STAT: */
    assert_string_equal(field[vsz - 1], "S");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "numboth");
    /* Handle numeric field overflowing under previous header. */
    assert_string_equal(field[rss], "12784321");
    /* Shouldn't pollute STAT or NI: */
    assert_string_equal(field[rss - 1], "0");
    assert_string_equal(field[rss + 1], "1");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "timeleft");
    /* Handle time fields overflowing almost under previous header. */
    assert_string_equal(field[stime + 1], "271-00:07:43");
    assert_string_equal(field[stime], "10:30");
    /* Shouldn't pollute NLWP: */
    assert_string_equal(field[stime - 1], "1");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "timesleft");
    /* Handle time fields overflowing under previous header. */
    assert_string_equal(field[stime + 1], "1271-00:07:43");
    assert_string_equal(field[stime], "10:30");
    /* Shouldn't pollute NLWP: */
    assert_string_equal(field[stime - 1], "1");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "timeright");
    /* Handle time field overflowing under next header. */
    assert_string_equal(field[command - 1], "1-02:07:14");
    /* Shouldn't pollute ELAPSED or COMMAND: */
    assert_string_equal(field[stime + 1], "92-21:17:55");
    assert_string_equal(field[command], "true");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, field));
    assert_string_equal(field[user], "timeboth");
    assert_int_equal(command, stime + 3); /* with elapsed and time between */
    /* Handle a time field overflowing almost under previous header
     * while also overflowing right and thus causing the next to
     * overflow under the field beyond it. */
    assert_string_equal(field[command - 1], "00:01:49");
    assert_string_equal(field[stime + 1], "1271-00:07:43");
    /* Should shunt COMMAND two bytes to the right: */
    assert_string_equal(field[command], lines[line] + 2 + start[command]);
    /* Shouldn't pollute COMMAND, NLWP or STIME: */
    assert_string_equal(field[stime], "10:30");
    assert_string_equal(field[stime - 1], "1");

    assert_int_equal(line, 1);
    /* Finally, tidy away headers: */
    for (i = 0; name[i] != NULL; i++)
    {
        free(name[i]);
    }
}

int main(void)
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
        {
            unit_test(test_split_line_noelapsed),
            unit_test(test_split_line_elapsed),
            unit_test(test_split_line_longcmd),
            unit_test(test_split_line)
        };

    return run_tests(tests);
}
