#include <test.h>

#define TEST_UNIT_TEST

#include <processes_select.c>

/* Actual ps output witnessed. */
static void test_split_line_challenges(void)
{
    /* Collect all test data in one array to make alignments visible: */
    static const char *lines[] = {
        "USER       PID    SZ    VSZ   RSS NLWP STIME     ELAPSED     TIME COMMAND",
        "operatic 14338 1042534 4170136 2122012 9 Sep15 4-06:11:34 2-09:27:49 /usr/lib/opera/opera"
    };
    char *name[CF_PROCCOLS]; /* Headers */
    char *field[CF_PROCCOLS]; /* Content */
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    int i, user = 0, nlwp = 5;

    memset(name, 0, sizeof(name));
    memset(field, 0, sizeof(field));

    /* Prepare data needed by tests and assert things tests can then assume: */
    GetProcessColumnNames(lines[0], name, start, end);
    assert_string_equal(name[user], "USER");
    assert_string_equal(name[nlwp], "NLWP");

    assert_true(SplitProcLine(lines[1], 1, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "operatic");
    assert_string_equal(field[nlwp], "9");

    /* Finally, tidy away fields and headers: */
    for (i = 0; field[i] != NULL; i++)
    {
        free(field[i]);
    }
    for (i = 0; name[i] != NULL; i++)
    {
        free(name[i]);
    }
}

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

    assert_true(SplitProcLine(lines[2], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "block");
    /* Copes when STIME is a date with a space in it. */
    assert_string_equal(field[stime], began);

    assert_true(SplitProcLine(lines[1], pstime, name, start, end, PCA_AllColumnsPresent, field));
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

    assert_true(SplitProcLine(lines[2], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "block");
    /* Copes when STIME is a date with a space in it. */
    assert_string_equal(field[stime], "Jul02");

    assert_true(SplitProcLine(lines[1], pstime, name, start, end, PCA_AllColumnsPresent, field));
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

    assert_true(SplitProcLine(lines[1], pstime, name, start, end, PCA_AllColumnsPresent, field));
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
    assert_false(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field)); /* NULL */
    assert_false(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field)); /* empty */

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field)); /* basic */
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

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "spacey");
    /* Discards leading and dangling space in command. */
    assert_string_equal(field[command], "echo");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "inspace");
    /* Preserves spaces within a text field. */
    assert_string_equal(field[command], lines[line] + start[command]);

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    /* Handle a text field overflowing to the right. */
    assert_string_equal(field[user], "wordright");
    /* Shouldn't pollute PID: */
    assert_string_equal(field[user + 1], "4");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    /* Handle a text field overflowing under next header. */
    assert_string_equal(field[user], "wordytoright");
    /* Shouldn't pollute PID: */
    assert_string_equal(field[user + 1], "4");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "numleft");
    /* Handle numeric field overflowing under previous header. */
    assert_string_equal(field[vsz], "123432536");
    /* Shouldn't pollute STAT: */
    assert_string_equal(field[vsz - 1], "S");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "numboth");
    /* Handle numeric field overflowing under previous header. */
    assert_string_equal(field[rss], "12784321");
    /* Shouldn't pollute STAT or NI: */
    assert_string_equal(field[rss - 1], "0");
    assert_string_equal(field[rss + 1], "1");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "timeleft");
    /* Handle time fields overflowing almost under previous header. */
    assert_string_equal(field[stime + 1], "271-00:07:43");
    assert_string_equal(field[stime], "10:30");
    /* Shouldn't pollute NLWP: */
    assert_string_equal(field[stime - 1], "1");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "timesleft");
    /* Handle time fields overflowing under previous header. */
    assert_string_equal(field[stime + 1], "1271-00:07:43");
    assert_string_equal(field[stime], "10:30");
    /* Shouldn't pollute NLWP: */
    assert_string_equal(field[stime - 1], "1");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
    assert_string_equal(field[user], "timeright");
    /* Handle time field overflowing under next header. */
    assert_string_equal(field[command - 1], "1-02:07:14");
    /* Shouldn't pollute ELAPSED or COMMAND: */
    assert_string_equal(field[stime + 1], "92-21:17:55");
    assert_string_equal(field[command], "true");

    assert_true(SplitProcLine(lines[--line], pstime, name, start, end, PCA_AllColumnsPresent, field));
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

static void test_split_line_serious_overspill(void)
{
    /*
       Test that columns spilling over into other columns do not confuse the
       parser.
       Collect all test data in one array to make alignments visible:
    */
    static const char *lines[] = {
        "    USER   PID %CPU %MEM   SZ  RSS TT      S    STIME        TIME COMMAND",
        " johndoe  8263  0.0  0.2 19890 116241 ?       S   Jan_16    08:41:40 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4",
        "noaccess  8264  0.0  0.2 19890 116242 ?       S   Jan_16    08:41:40 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4",
        "noaccess  8265  0.0  0.2 19890 116243 ?       S   Jan_16    08:41:40 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4",
        NULL
    };

    char *name[CF_PROCCOLS]; /* Headers */
    char *field[CF_PROCCOLS]; /* Content */
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    int user = 0, pid = 1, sz = 4, rss = 5, command = 10;
    time_t pstime = 1410000000;

    /* Prepare data needed by tests and assert things tests can then assume: */
    GetProcessColumnNames(lines[0], name, start, end);
    assert_string_equal(name[user], "USER");
    assert_string_equal(name[pid], "PID");
    assert_string_equal(name[sz], "SZ");
    assert_string_equal(name[rss], "RSS");
    assert_string_equal(name[command], "COMMAND");
    assert_int_equal(start[command], 66);

    // Test content
    {
        assert_true(SplitProcLine(lines[1], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "johndoe");
        assert_string_equal(field[pid], "8263");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116241");
        assert_string_equal(field[command], lines[1] + 69);
    }

    {
        assert_true(SplitProcLine(lines[2], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "noaccess");
        assert_string_equal(field[pid], "8264");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116242");
        assert_string_equal(field[command], lines[2] + 69);
    }

    {
        assert_true(SplitProcLine(lines[3], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "noaccess");
        assert_string_equal(field[pid], "8265");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116243");
        assert_string_equal(field[command], lines[3] + 69);
    }
}

typedef struct
{
    FILE *fp;
    const char **lines;
} LWData;

static void *ListWriter(void *arg)
{
    LWData *data = (LWData *)arg;
    for (int i = 0; data->lines[i]; i++)
    {
        fprintf(data->fp, "%s\n", data->lines[i]);
    }
    fclose(data->fp);

    return NULL;
}

static void test_platform_extra_table(void)
{
    static const char *lines[] = {
        "    USER   PID %CPU %MEM   SZ  RSS TT      S    STIME        TIME COMMAND",
        " johndoe  8263  0.0  0.2 19890 116241 ?       S   Jan_16    08:41:40 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4",
        "noaccess  8264  0.0  0.2 19890 116242 ?       S   Jan_16    08:41:40 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4",
        "noaccess  8265  0.0  0.2 19890 116243 ?       S   Jan_16    08:41:40 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4",
        " jenkins 22306    -    -    0    0 ?       Z        -       00:00 <defunct>",
        " jenkins 22307    -    -    0    0 ?       Z        -       00:00 <defunct>",
        " jenkins 22308    -    -    0    0 ?       Z        -       00:00 <defunct>",
        NULL
    };
    static const char *ucb_lines[] = {
        "   PID TT       S  TIME COMMAND",
        "  8263 ?        S 521:40 /usr/java/bin/java blahblah",
        // Takes from Solaris 10. Yep, the line really is that long.
        "  8264 ?        S 521:40 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4 -classpath /usr/share/webconsole/private/container/bin/bootstrap.jar:/usr/share/webconsole/private/container/bin/commons-logging.jar:/usr/share/webconsole/private/container/bin/log4j.jar:/usr/java/lib/tools.jar:/usr/java/jre/lib/jsse.jar -Djava.security.manager -Djava.security.policy==/var/webconsole/domains/console/conf/console.policy -Djavax.net.ssl.trustStore=/var/webconsole/domains/console/conf/keystore.jks -Djava.security.auth.login.config=/var/webconsole/domains/console/conf/consolelogin.conf -Dcatalina.home=/usr/share/webconsole/private/container -Dcatalina.base=/var/webconsole/domains/console -Dcom.sun.web.console.home=/usr/share/webconsole -Dcom.sun.web.console.conf=/etc/webconsole/console -Dcom.sun.web.console.base=/var/webconsole/domains/console -Dcom.sun.web.console.logdir=/var/log/webconsole/console -Dcom.sun.web.console.native=/usr/lib/webconsole -Dcom.sun.web.console.appbase=/var/webconsole/domains/console/webapps -Dcom.sun.web.console.secureport=6789 -Dcom.sun.web.console.unsecureport=6788 -Dcom.sun.web.console.unsecurehost=127.0.0.1 -Dwebconsole.default.file=/etc/webconsole/console/default.properties -Dwebconsole.config.file=/etc/webconsole/console/service.properties -Dcom.sun.web.console.startfile=/var/webconsole/tmp/console_start.tmp -Djava.awt.headless=true -Dorg.apache.commons.logging.Log=org.apache.commons.logging.impl.NoOpLog org.apache.catalina.startup.Bootstrap start",
        "  8265 ?        S 521:40 /usr/java/bin/java blahblah",
        // Taken from Solaris 10, notice the missing fields.
        " 22306          Z  0:00  <defunct>",
        " 22307          Z  0:00 ",
        " 22308          Z  0:00",
        NULL
    };
    char *name[CF_PROCCOLS]; /* Headers */
    char *field[CF_PROCCOLS]; /* Content */
    for (int i = 0; i < CF_PROCCOLS; ++i)
    {
        name[i] = NULL;
        field[i] = NULL;
    }
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    int user = 0, pid = 1, sz = 4, rss = 5, command = 10;
    time_t pstime = 1410000000;

    // Prepare to fill "/usr/ucb/ps" table with data.
    ClearPlatformExtraTable();
    int pipefd[2];
    assert_int_equal(pipe(pipefd), 0);
    LWData data;
    data.fp = fdopen(pipefd[1], "w");
    data.lines = ucb_lines;

    // Feed the pipe from a separate thread.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    pthread_create(&tid, &attr, &ListWriter, &data);
    pthread_attr_destroy(&attr);

    FILE *cmd_output = fdopen(pipefd[0], "r");

    UCB_PS_MAP = StringMapNew();
    ReadFromUcbPsPipe(cmd_output);

    /* Prepare data needed by tests and assert things tests can then assume: */
    GetProcessColumnNames(lines[0], name, start, end);
    assert_string_equal(name[user], "USER");
    assert_string_equal(name[pid], "PID");
    assert_string_equal(name[sz], "SZ");
    assert_string_equal(name[rss], "RSS");
    assert_string_equal(name[command], "COMMAND");
    assert_int_equal(start[command], 66);

    // Test content
    {
        assert_true(SplitProcLine(lines[1], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "johndoe");
        assert_string_equal(field[pid], "8263");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116241");
        assert_string_equal(field[command], lines[1] + 69);

        ApplyPlatformExtraTable(name, field);
        // Now check new and corrected values.
        assert_string_equal(field[user], "johndoe");
        assert_string_equal(field[pid], "8263");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116241");
        assert_string_equal(field[command], ucb_lines[1] + 25);
    }

    {
        assert_true(SplitProcLine(lines[2], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "noaccess");
        assert_string_equal(field[pid], "8264");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116242");
        assert_string_equal(field[command], lines[2] + 69);

        ApplyPlatformExtraTable(name, field);
        // Now check new and corrected values.
        assert_string_equal(field[user], "noaccess");
        assert_string_equal(field[pid], "8264");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116242");
        assert_string_equal(field[command], ucb_lines[2] + 25);
    }

    {
        assert_true(SplitProcLine(lines[3], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "noaccess");
        assert_string_equal(field[pid], "8265");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116243");
        assert_string_equal(field[command], lines[3] + 69);

        ApplyPlatformExtraTable(name, field);
        // Now check new and corrected values.
        assert_string_equal(field[user], "noaccess");
        assert_string_equal(field[pid], "8265");
        assert_string_equal(field[sz], "19890");
        assert_string_equal(field[rss], "116243");
        assert_string_equal(field[command], ucb_lines[3] + 25);
    }

    {
        assert_true(SplitProcLine(lines[4], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "jenkins");
        assert_string_equal(field[pid], "22306");
        assert_string_equal(field[sz], "0");
        assert_string_equal(field[rss], "0");
        assert_string_equal(field[command], lines[4] + 66);

        ApplyPlatformExtraTable(name, field);
        // Now check new and corrected values.
        assert_string_equal(field[user], "jenkins");
        assert_string_equal(field[pid], "22306");
        assert_string_equal(field[sz], "0");
        assert_string_equal(field[rss], "0");
        assert_string_equal(field[command], "<defunct>");
    }

    {
        assert_true(SplitProcLine(lines[5], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "jenkins");
        assert_string_equal(field[pid], "22307");
        assert_string_equal(field[sz], "0");
        assert_string_equal(field[rss], "0");
        assert_string_equal(field[command], lines[5] + 66);

        ApplyPlatformExtraTable(name, field);
        // Now check new and corrected values.
        assert_string_equal(field[user], "jenkins");
        assert_string_equal(field[pid], "22307");
        assert_string_equal(field[sz], "0");
        assert_string_equal(field[rss], "0");
        assert_string_equal(field[command], "");
    }

    {
        assert_true(SplitProcLine(lines[6], pstime, name, start, end, PCA_AllColumnsPresent, field));
        assert_string_equal(field[user], "jenkins");
        assert_string_equal(field[pid], "22308");
        assert_string_equal(field[sz], "0");
        assert_string_equal(field[rss], "0");
        assert_string_equal(field[command], lines[6] + 66);

        ApplyPlatformExtraTable(name, field);
        // Now check new and corrected values.
        assert_string_equal(field[user], "jenkins");
        assert_string_equal(field[pid], "22308");
        assert_string_equal(field[sz], "0");
        assert_string_equal(field[rss], "0");
        assert_string_equal(field[command], "");
    }
    for (int i=0; i < CF_PROCCOLS; ++i)
    {
        free(field[i]);
        field[i] = NULL;
    }

    fclose(cmd_output);
}

static void test_platform_specific_ps_examples(void)
{
    enum {
        // Make sure the order matches the ps lines below.
        TEST_LINUX = 0,
        TEST_AIX,
        TEST_HPUX,
        TEST_SOLARIS9,
        TEST_SOLARIS10,
        TEST_SOLARIS11,
        TEST_FREEBSD11,
        TEST_ILLUMOS,
        NUM_OF_PLATFORMS
    };
    /* Simple, visual way of specifying the expected parse results, each row
       alternates between the line we want to parse (with header first) and the
       range we wish the parsing code to extract for us, denoted by '<' and '>'.
       'X' is used where they overlap, and '-' if the field is empty. '{' and
       '}' a special case and means it should get the expected output from the
       corresponding row in the exception table below. */
    const char *pslines[NUM_OF_PLATFORMS][20] = {
        {
            // Linux
            "USER       PID  PPID  PGID %CPU %MEM    VSZ  NI       RSS NLWP STIME     ELAPSED     TIME COMMAND",
            "<  >       < >  <  >  <  > <  > <  >    < >  <>       < > <  > <   >     <     >     <  > <     >",
            "a10040    4831     1  4116  5.4 17.3 2763416  0   1400812   54 Apr04  4-02:54:24 05:25:51 /usr/lib/firefox/firefox",
            "<    >    <  >     X  <  >  < > <  > <     >  X   <     >   <> {   }  <        > <      > <                      >",
            "a10040    5862  5851  4116  0.0  0.0      0   0         0    1 Apr04  4-02:38:28 00:00:00 [/usr/bin/termin] <defunct>",
            "<    >    <  >  <  >  <  >  < >  < >      X   X         X    X {   }  <        > <      > <                         >",
            NULL
        }, {
            // AIX
            "    USER     PID    PPID    PGID  %CPU  %MEM   VSZ NI ST    STIME        TIME COMMAND",
            "    <  >     < >    <  >    <  >  <  >  <  >   < > <> <>    <   >        <  > <     >",
            " jenkins 1036484  643150 1036484   0.0   0.0   584 20 A  09:29:20    00:00:00 bash",
            " <     > <     >  <    > <     >   < >   < >   < > <> X  <      >    <      > <  >",
            "          254232  729146  729146                   20 Z              00:00:00 <defunct>",
            "       -  <    >  <    >  <    >     -     -     - <> X         -    <      > <       >",
            " jenkins  205218       1  205218   0.0   7.0 125384 20 A    Mar 16    00:36:46 /usr/java6_64/bin/java -jar slave.jar -jnlpUrl http://jenkins.usdc.cfengine.com/computer/buildslave-aix-5.3-ppc64/slave-agent.jnlp",
            " <     >  <    >       X  <    >   < >   < > <    > <> X    <    >    <      > <                                                                                                                                >",
            // Extreme case, a lot of fields missing. This only works when the
            // process is a zombie and the platform uses the
            // PCA_ZombieSkipEmptyColumns algorithm (which AIX does).
            "          205218       1  205218                   20 Z",
            "       -  <    >       X  <    >     -     -     - <> X        -           - -",
            NULL
        }, {
            // HPUX
            "     UID   PID  PPID  C    STIME TTY       TIME COMMAND",
            "     < >   < >  <  >  X    <   > < >       <  > <     >",
            " jenkins 17345 17109  0 09:31:39 pts/0     0:00 perl -e my $v = fork(); if ($v != 0) { sleep(3600); }",
            " <     > <   > <   >  X <      > <   >     <  > <                                                   >",
            " jenkins 17374 17345  0 09:31:40 pts/0     0:00 <defunct>",
            " <     > <   > <   >  X <      > <   >     <  > <       >",
            "    root   832     1  0  May  4  ?         0:01 /usr/sbin/automountd",
            "    <  >   < >     X  X  <    >  X         <  > <                  >",
            NULL
        }, {
            // Solaris 9
            "    USER   PID %CPU %MEM   SZ  RSS TT      S    STIME        TIME COMMAND",
            "    <  >   < > <  > <  >   <>  < > <>      X    <   >        <  > <     >",
            " jenkins 29769  0.0  0.0  810 2976 pts/1   S 07:22:43        0:00 /usr/bin/perl ../../ps.pl",
            " <     > <   >  < >  < >  < > <  > <   >   X <      >        <  > <                       >",
            " jenkins 29835    -    -    0    0 ?       Z        -        0:00 <defunct>",
            " <     > <   >    X    X    X    X X       X        X        <  > <       >",
            " jenkins 10026  0.0  0.3 30927 143632 ?       S   Jan_21    01:18:58 /usr/jdk/jdk1.6.0_45/bin/java -jar slave.jar",
            " <     > <   >  < >  < > <   > <    > X       X   <    >    <      > <                                          >",
            NULL
        }, {
            // Solaris 10
            "    USER   PID %CPU %MEM   SZ  RSS TT      S    STIME        TIME COMMAND",
            "    <  >   < > <  > <  >   <>  < > <>      X    <   >        <  > <     >",
            "    root 19553  0.0  0.0  743 4680 ?       S 04:03:10       00:00 /usr/lib/ssh/sshd",
            "    <  > <   >  < >  < >  < > <  > X       X <      >       <   > <               >",
            " jenkins 29770    -    -    0    0 ?       Z        -       00:00 <defunct>",
            " <     > <   >    X    X    X    X X       X        X       <   > <       >",
            "noaccess  8264  0.0  0.2 19890 116240 ?       S   Jan_16    08:49:25 /usr/java/bin/java -server -Xmx128m -XX:+UseParallelGC -XX:ParallelGCThreads=4 ",
            "<      >  <  >  < >  < > <   > <    > X       X   <    >    <      > <                                                                            >",
            NULL
        }, {
            // Solaris 11
            "    USER   PID %CPU %MEM   SZ  RSS TT      S    STIME        TIME COMMAND",
            "    <  >   < > <  > <  >   <>  < > <>      X    <   >        <  > <     >",
            "    root 15449  0.0  0.0  835 4904 ?       S 04:03:29       00:00 /usr/lib/ssh/sshd",
            "    <  > <   >  < >  < >  < > <  > X       X <      >       <   > <               >",
            " jenkins  5409    -    -    0    0 ?       Z        -       00:00 <defunct>",
            " <     >  <  >    X    X    X    X X       X        X       <   > <       >",
            " jenkins 29997  0.0  0.5 51661 312120 ?       S   Jan_21    01:18:53 /usr/jdk/jdk1.6.0_45/bin/java -jar slave.jar",
            " <     > <   >  < >  < > <   > <    > X       X   <    >    <      > <                                          >",
            NULL
        }, {
            // FreeBSD 11
            "USER         PID  %CPU %MEM     VSZ    RSS TT  STAT STARTED        TIME COMMAND",
            "<  >         < >  <  > <  >     < >    < > <>  <  > <     >        <  > <     >",
            "skreuzer    6506   5.0  0.2   57192  25764  0- S+   12:39      10:13.37 mosh-server new -c 256 -s -l LANG=en_US.UTF-8",
            "<      >    <  >   < >  < >   <   >  <   >  <> <>   <   >      <      > <                                           >",
            "zookeeper    646   0.0  1.0 4538080 162140  -  I    28Feb16    21:46.80 /usr/local/openjdk7/bin/java -Dzookeeper.log.dir=/var/log/zookeeper -Dlog4j.configuration=file:/usr/local/etc/zookeeper/l",
            "<       >    < >   < >  < > <     > <    >  X  X    <     >    <      > <                                                                                                                       >",
            "skreuzer   40046   0.0  0.0       0      0  6  Z+   21:34       0:00.00 <defunct>",
            "<      >   <   >   < >  < >       X      X  X  <>   <   >       <     > <       >",
            "skreuzer   50293   0.0  0.0   20612   5332  3  Is    2Mar16     0:00.62 -zsh (zsh)",
            "<      >   <   >   < >  < >   <   >   <  >  X  <>    <    >     <     > <        >",
            NULL
        }, {
            // Illumos
            "    USER   PID %CPU %MEM   SZ  RSS TT      S    STIME        TIME COMMAND",
            "    <  >   < > <  > <  >   <>  < > <>      X    <   >        <  > <     >",
            " bahamat 63679  0.0  0.1 1831 4340 pts/13  S 00:15:39       00:00 perl -e $p = fork(); if ($p != 0) { sleep(60); }",
            " <     > <   >  < >  < > <  > <  > <    >  X <      >       <   > <                                              >",
            " bahamat 63680    -    -    0    0 ?       Z        -       00:00 <defunct>",
            " <     > <   >    X    X    X    X X       X        X       <   > <       >",
            "    root 72601  0.3  0.4 291631 1050796 ?       S   Feb_25    06:35:07 /opt/local/java/openjdk7/bin/java -Dhudson.DNSMultiCast.disabled=true -Xmx2048m",
            "    <  > <   >  < >  < > <    > <     > X       X   <    >    <      > <                                                                             >",
            NULL
        }
    };

    // Only half as many elements here, since there is only expected output.
    const char *exceptions[NUM_OF_PLATFORMS][10] = {
        {
            // Linux
            NULL,
            "1459762277",
            "1459763233",
            NULL
        }, {
            // AIX
            NULL
        }, {
            // HPUX
            NULL
        }, {
            // Solaris 9
            NULL
        }, {
            // Solaris 10
            NULL
        }, {
            // Solaris 11
            NULL
        }, {
            // FreeBSD 11
            NULL
        }, {
            // Illumos
            NULL
        }
    };

    char *names[CF_PROCCOLS];
    char *fields[CF_PROCCOLS];
    int start[CF_PROCCOLS];
    int end[CF_PROCCOLS];

    memset(names, 0, sizeof(names));
    memset(fields, 0, sizeof(fields));

    for (int platform = 0; platform < NUM_OF_PLATFORMS; platform++)
    {
        PsColumnAlgorithm pca;
        if (platform == TEST_AIX)
        {
            pca = PCA_ZombieSkipEmptyColumns;
        }
        else
        {
            pca = PCA_AllColumnsPresent;
        }

        for (int linenum = 0; pslines[platform][linenum]; linenum += 2)
        {
            char **fields_to_check;
            if (linenum == 0)
            {
                GetProcessColumnNames(pslines[platform][linenum], names, start, end);
                fields_to_check = names;
            }
            else
            {
                assert_true(SplitProcLine(pslines[platform][linenum], 1460118341, names, start, end, pca, fields));
                fields_to_check = fields;
            }

            bool in_field = false;
            int field_start = -1;
            int field_end = -1;
            int field_num = 0;
            bool exception = false;
            for (int pos = 0;; pos++)
            {
                if (!pslines[platform][linenum + 1][pos])
                {
                    // There should be no excess space at the end of the
                    // expected replies.
                    assert_int_not_equal(pslines[platform][linenum + 1][pos - 1], ' ');
                    assert_false(in_field);
                    // We should have reached the end of both arrays.
                    assert_int_equal(names[field_num], NULL);
                    if (linenum != 0)
                    {
                        assert_int_equal(fields[field_num], NULL);
                    }
                    break;
                }

                switch (pslines[platform][linenum + 1][pos])
                {
                case ' ':
                    break;
                case '{':
                    exception = true;
                    // fallthrough
                case '<':
                    assert_false(in_field);
                    in_field = true;
                    field_start = pos;
                    break;
                case '}':
                case '>':
                    assert_true(in_field);
                    in_field = false;
                    // + 1 because we want it to be the next byte.
                    field_end = pos + 1;
                    break;
                case 'X':
                    field_start = pos;
                    // + 1 because we want it to be the next byte.
                    field_end = pos + 1;
                    break;
                case '-':
                    field_start = pos;
                    // This corresponds to an empty string.
                    field_end = pos;
                    break;
                default:
                    assert_true(false);
                    break;
                }

                if (field_start != -1 && field_end != -1)
                {
                    if (exception)
                    {
                        assert_string_equal(fields_to_check[field_num],
                                            exceptions[platform][linenum / 2]);
                        exception = false;
                    }
                    else
                    {
                        int field_len = field_end - field_start;

#if 0                   /* DEBUG OUTPUT */
                        printf("Checking '%s' against '", fields_to_check[field_num]);
                        fwrite(pslines[platform][linenum] + field_start, field_len, 1, stdout);
                        printf("'\n");
#endif                  /* DEBUG OUTPUT */

                        // Check if fields are identical.
                        assert_memory_equal(fields_to_check[field_num],
                                            pslines[platform][linenum] + field_start,
                                            field_len);
                        // And that it's not longer than what we expect.
                        assert_int_equal(fields_to_check[field_num][field_len], '\0');
                    }
                    field_start = -1;
                    field_end = -1;
                    field_num++;
                }
            }

            for (int i = 0; i < CF_PROCCOLS; i++)
            {
                free(fields[i]);
                fields[i] = NULL;
            }
        }

        for (int i = 0; i < CF_PROCCOLS; i++)
        {
            free(names[i]);
            names[i] = NULL;
        }
    }
}

int main(void)
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
        {
            unit_test(test_split_line_challenges),
            unit_test(test_split_line_noelapsed),
            unit_test(test_split_line_elapsed),
            unit_test(test_split_line_longcmd),
            unit_test(test_split_line),
            unit_test(test_split_line_serious_overspill),
            unit_test(test_platform_extra_table),
            unit_test(test_platform_specific_ps_examples),
        };

    return run_tests(tests);
}
