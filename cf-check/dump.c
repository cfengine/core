#include <platform.h>
#include <dump.h>

#ifdef LMDB
#include <lmdb.h>
#include <string_lib.h>
#include <json.h>
#include <db_structs.h>
#include <utilities.h>
#include <known_dirs.h> // GetStateDir() for usage printout
#include <file_lib.h>   // FILE_SEPARATOR
#include <observables.h>

typedef enum
{
    DUMP_NICE,     // Print strings in a nice way, and file specific structs
    DUMP_PORTABLE, // Portable and unambiguous, structs and raw strings
    DUMP_SIMPLE,   // Fallback mode for arbitrary / unrecognized data
    DUMP_KEYS,
    DUMP_VALUES,
} dump_mode;

static void print_usage(void)
{
    printf("Usage: cf-check dump [-k|-v|-n|-s|-p|-t FILE] [FILE ...]\n");
    printf("\n");
    printf("\t-k|--keys        print only keys\n");
    printf("\t-v|--values      print only values\n");
    printf("\t-n|--nice        print strings in a nice way and with database specific awareness\n");
    printf("\t-s|--simple      print as simple escaped binary data\n");
    printf("\t-p|--portable    print unambiguously with structs and raw strings\n");
    printf("\t-t|--tskey FILE  use FILE as list of observables\n");
    printf("\tWill use '%s%cts_key' if not specified, or built-in list if no ts_key file is found.\n",
        GetStateDir(),
        FILE_SEPARATOR);
    printf("\n");
    printf("Example: cf-check dump /var/cfengine/state/cf_lastseen.lmdb\n");
}

static int dump_report_error(const int rc)
{
    printf("err(%d): %s\n", rc, mdb_strerror(rc));
    return rc;
}

static void print_json_string(
    const char *const data, size_t size, const bool strip_strings)
{
    assert(data != NULL);

    printf("\"");
    if (size == 0)
    {
        // size 0 is printed unambiguously as empty string ""
        // An empty C string (size 1) will be printed as "\0"
        // in all modes (including nice)
        printf("\"");
        return;
    }

    if (strip_strings)
    {
        const size_t len = strnlen(data, size);

        // We expect the data to either be single byte ("0" or "1"),
        // or a C-string
        bool known_data = ((size == 1) || (len == (size - 1)) || (data[size - 1] == '\n'));
        if (!known_data)
        {
            printf("\nError: This database contains unknown binary data - use --simple to print anyway or try on the same OS/architecture the lmdb file was generated on\n");
            exit(1);
        }

        // Most of what we store are C strings except 1 byte '0' or '1',
        // and some structs. So in nice mode, we try to default to printing
        // C strings in a nice way. This means it can be ambiguous (we chop
        // off the NUL byte sometimes). Use --simple for correct, unambiguous,
        // uglier output.
        if (size > 1 && len == (size - 1))
        {
            // Looks like a normal string, let's remove NUL byte (nice mode)
            size = len;
        }
    }

    Slice unescaped_data = {.data = (void *) data, .size = size};
    char *escaped_data = Json5EscapeData(unescaped_data);
    printf("%s", escaped_data);
    free(escaped_data);

    printf("\"");
}

static void print_struct_lastseen_quality(
    const MDB_val value, const bool strip_strings)
{
    assert(sizeof(KeyHostSeen) == value.mv_size);
    if (sizeof(KeyHostSeen) != value.mv_size)
    {
        // Fall back to simple printing in release builds:
        print_json_string(value.mv_data, value.mv_size, strip_strings);
    }
    else
    {
        // TODO: improve names of struct members in QPoint and KeyHostSeen
        KeyHostSeen quality;
        memcpy(&quality, value.mv_data, sizeof(quality));
        const time_t lastseen = quality.lastseen;
        const QPoint Q = quality.Q;

        JsonElement *q_json = JsonObjectCreate(4);
        JsonObjectAppendReal(q_json, "q", Q.q);
        JsonObjectAppendReal(q_json, "expect", Q.expect);
        JsonObjectAppendReal(q_json, "var", Q.var);
        JsonObjectAppendReal(q_json, "dq", Q.dq);

        JsonElement *top_json = JsonObjectCreate(2);
        JsonObjectAppendInteger(top_json, "lastseen", lastseen);
        JsonObjectAppendObject(top_json, "Q", q_json);

        Writer *w = FileWriter(stdout);
        JsonWriteCompact(w, top_json);
        FileWriterDetach(w);
        JsonDestroy(top_json);
    }
}

static void print_struct_lock_data(
    const MDB_val value, const bool strip_strings)
{
    assert(sizeof(LockData) == value.mv_size);
    if (sizeof(LockData) != value.mv_size)
    {
        // Fall back to simple printing in release builds:
        print_json_string(value.mv_data, value.mv_size, strip_strings);
    }
    else
    {
        // TODO: improve names of struct members in LockData
        LockData lock;
        memcpy(&lock, value.mv_data, sizeof(lock));
        const pid_t pid = lock.pid;
        const time_t time = lock.time;
        const time_t process_start_time = lock.process_start_time;

        JsonElement *json = JsonObjectCreate(3);
        JsonObjectAppendInteger(json, "pid", pid);
        JsonObjectAppendInteger(json, "time", time);
        JsonObjectAppendInteger(
            json, "process_start_time", process_start_time);

        Writer *w = FileWriter(stdout);
        JsonWriteCompact(w, json);
        FileWriterDetach(w);
        JsonDestroy(json);
    }
}

// Used to print values in /var/cfengine/state/cf_observations.lmdb:
static void print_struct_averages(
    const MDB_val value, const bool strip_strings, const char *tskey_filename)
{
    assert(sizeof(Averages) == value.mv_size);
    if (sizeof(Averages) != value.mv_size)
    {
        // Fall back to simple printing in release builds:
        print_json_string(value.mv_data, value.mv_size, strip_strings);
    }
    else
    {
        // TODO: clean up Averages
        char **obnames = NULL;
        Averages averages;
        memcpy(&averages, value.mv_data, sizeof(averages));
        const time_t last_seen = averages.last_seen;

        obnames = GetObservableNames(tskey_filename);
        JsonElement *all_observables = JsonObjectCreate(CF_OBSERVABLES);

        for (Observable i = 0; i < CF_OBSERVABLES; ++i)
        {
            char *name = obnames[i];
            JsonElement *observable = JsonObjectCreate(4);
            QPoint Q = averages.Q[i];

            JsonObjectAppendReal(observable, "q", Q.q);
            JsonObjectAppendReal(observable, "expect", Q.expect);
            JsonObjectAppendReal(observable, "var", Q.var);
            JsonObjectAppendReal(observable, "dq", Q.dq);
            JsonObjectAppendObject(all_observables, name, observable);

            free(obnames[i]);
        }

        free(obnames);

        JsonElement *top_json = JsonObjectCreate(2);
        JsonObjectAppendInteger(top_json, "last_seen", last_seen);
        JsonObjectAppendObject(top_json, "Q", all_observables);

        Writer *w = FileWriter(stdout);
        JsonWriteCompact(w, top_json);
        FileWriterDetach(w);
        JsonDestroy(top_json);
    }
}

static void print_struct_persistent_class(
    const MDB_val value, const bool strip_strings)
{
    // Value from db should always be bigger than sizeof struct
    // Since tags is not counted in sizeof. `tags` is variable size,
    // and should be at least size 1 (NUL byte) for data to make sense.
    assert(value.mv_size > sizeof(PersistentClassInfo));
    if (value.mv_size <= sizeof(PersistentClassInfo))
    {
        // Fall back to simple printing in release builds:
        print_json_string(value.mv_data, value.mv_size, strip_strings);
    }
    else
    {
        /* Make a copy to ensure proper alignment. We cannot just copy data to a
         * local PersistentClassInfo variable because it contains a
         * variable-length string at the end (see the struct definition). */
        PersistentClassInfo *class_info = malloc(value.mv_size);
        memcpy(class_info, value.mv_data, value.mv_size);
        const unsigned int expires = class_info->expires;
        const PersistentClassPolicy policy = class_info->policy;
        const char *policy_str;
        switch (policy)
        {
        case CONTEXT_STATE_POLICY_RESET:
            policy_str = "RESET";
            break;
        case CONTEXT_STATE_POLICY_PRESERVE:
            policy_str = "PRESERVE";
            break;
        default:
            debug_abort_if_reached();
            policy_str = "INTERNAL ERROR";
            break;
        }

        const char *const tags = class_info->tags;
        assert(tags > (char *) class_info);

        const size_t offset = (tags - (char *) class_info);

        /* (sizeof(unsigned int) + sizeof(PersistentClassPolicy)) is offset without the padding */
        assert(offset >= (sizeof(unsigned int) + sizeof(PersistentClassPolicy)));

        assert(value.mv_size > offset);
        const size_t str_size = value.mv_size - offset;
        assert(str_size > 0);
        if (memchr(tags, '\0', str_size) == NULL)
        {
            // String is not terminated, abort or fall back to default:
            debug_abort_if_reached();
            print_json_string(value.mv_data, value.mv_size, strip_strings);
            free(class_info);
            return;
        }

        // At this point, it should be safe to strdup(tags)

        JsonElement *top_json = JsonObjectCreate(2);
        JsonObjectAppendInteger(top_json, "expires", expires);
        JsonObjectAppendString(top_json, "policy", policy_str);
        JsonObjectAppendString(top_json, "tags", tags);

        Writer *w = FileWriter(stdout);
        JsonWriteCompact(w, top_json);
        FileWriterDetach(w);
        JsonDestroy(top_json);
        free(class_info);
    }
}

static void print_struct_or_string(
    const MDB_val key,
    const MDB_val value,
    const char *const file,
    const bool strip_strings,
    const bool structs,
    const char *tskey_filename)
{
    if (structs)
    {
        if (StringContains(file, "cf_lastseen.lmdb")
            && StringStartsWith(key.mv_data, "q"))
        {
            print_struct_lastseen_quality(value, strip_strings);
        }
        else if (StringContains(file, "cf_lock.lmdb"))
        {
            print_struct_lock_data(value, strip_strings);
        }
        else if (StringContains(file, "cf_observations.lmdb"))
        {
            if (StringEqual(key.mv_data, "DATABASE_AGE"))
            {
                assert(sizeof(double) == value.mv_size);
                double age;
                memcpy(&age, value.mv_data, sizeof(age));
                printf("%f", age);
            }
            else
            {
                print_struct_averages(value, strip_strings, tskey_filename);
            }
        }
        else if (StringEqual(file, "history.lmdb") ||
                 StringEndsWith(file, FILE_SEPARATOR_STR"history.lmdb") ||
                 StringEqual(file, "history.lmdb.backup") ||
                 StringEndsWith(file, FILE_SEPARATOR_STR"history.lmdb.backup"))
        {
            print_struct_averages(value, strip_strings, tskey_filename);
        }
        else if (StringContains(file, "cf_state.lmdb"))
        {
            print_struct_persistent_class(value, strip_strings);
        }
        else if (StringContains(file, "nova_agent_execution.lmdb"))
        {
            if (StringEqual(key.mv_data, "delta_gavr"))
            {
                assert(sizeof(double) == value.mv_size);
                double average;
                memcpy(&average, value.mv_data, sizeof(average));
                printf("%f", average);
            }
            else if (StringEqual(key.mv_data, "last_exec"))
            {
                assert(sizeof(time_t) == value.mv_size);
                time_t last_exec;
                memcpy(&last_exec, value.mv_data, sizeof(last_exec));
                printf("%ju", (uintmax_t) (last_exec));
            }
            else
            {
                debug_abort_if_reached();
            }
        }
        else
        {
            print_json_string(value.mv_data, value.mv_size, strip_strings);
        }
    }
    else
    {
        print_json_string(value.mv_data, value.mv_size, strip_strings);
    }
}

static void print_line_key_value(
    const MDB_val key,
    const MDB_val value,
    const char *const file,
    const bool strip_strings,
    const bool structs,
    const char *tskey_filename)
{
    printf("\t");
    print_json_string(key.mv_data, key.mv_size, strip_strings);
    printf(": ");
    print_struct_or_string(key, value, file, strip_strings, structs, tskey_filename);
    printf(",\n");
}

static void print_line_array_element(
    const MDB_val value, const bool strip_strings)
{
    printf("\t");
    print_json_string(value.mv_data, value.mv_size, strip_strings);
    printf(",\n");
}

static void print_opening_bracket(const dump_mode mode)
{
    if (mode == DUMP_VALUES || mode == DUMP_KEYS)
    {
        printf("[\n");
    }
    else
    {
        printf("{\n");
    }
}

static void print_closing_bracket(const dump_mode mode)
{
    if (mode == DUMP_KEYS || mode == DUMP_VALUES)
    {
        printf("]\n");
    }
    else
    {
        printf("}\n");
    }
}

static int dump_db(const char *file, const dump_mode mode, const char *tskey_filename)
{
    assert(file != NULL);

    const bool strip_strings = (mode == DUMP_NICE);
    const bool structs = (mode == DUMP_NICE || mode == DUMP_PORTABLE);

    int r;
    MDB_env *env = NULL;
    MDB_txn *txn = NULL;
    MDB_dbi dbi;
    MDB_cursor *cursor = NULL;

    if (0 != (r = mdb_env_create(&env))
        || 0 != (r = mdb_env_open(env, file, MDB_NOSUBDIR | MDB_RDONLY, 0644))
        || 0 != (r = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn))
        || 0 != (r = mdb_open(txn, NULL, 0, &dbi))
        || 0 != (r = mdb_cursor_open(txn, dbi, &cursor)))
    {
        if (env != NULL)
        {
            if (txn != NULL)
            {
                if (cursor != NULL)
                {
                    mdb_cursor_close(cursor);
                }
                mdb_txn_abort(txn);
            }
            mdb_env_close(env);
        }
        return dump_report_error(r);
    }

    MDB_val key, value;
    print_opening_bracket(mode);
    while ((r = mdb_cursor_get(cursor, &key, &value, MDB_NEXT)) == MDB_SUCCESS)
    {
        switch (mode)
        {
        case DUMP_NICE:
        case DUMP_PORTABLE:
        case DUMP_SIMPLE:
            print_line_key_value(key, value, file, strip_strings, structs, tskey_filename);
            break;
        case DUMP_KEYS:
            print_line_array_element(key, strip_strings);
            break;
        case DUMP_VALUES:
            print_line_array_element(value, strip_strings);
            break;
        default:
            debug_abort_if_reached();
            break;
        }
    }
    print_closing_bracket(mode);

    if (r != MDB_NOTFOUND)
    {
        // At this point, not found is expected, anything else is an error
        return dump_report_error(r);
    }
    mdb_cursor_close(cursor);
    mdb_close(env, dbi);

    mdb_txn_abort(txn);
    mdb_env_close(env);

    return 0;
}

static int dump_dbs(Seq *const files, const dump_mode mode, const char *tskey_filename)
{
    assert(files != NULL);
    const size_t length = SeqLength(files);
    assert(length > 0);

    if (length == 1)
    {
        return dump_db(SeqAt(files, 0), mode, tskey_filename);
    }

    int ret = 0;

    for (size_t i = 0; i < length; ++i)
    {
        const char *const filename = SeqAt(files, i);
        printf("%s:\n", filename);
        const int r = dump_db(filename, mode, tskey_filename);
        if (r != 0)
        {
            ret = r;
        }
    }
    return ret;
}

int dump_main(int argc, const char *const *const argv)
{
    assert(argv != NULL);
    assert(argc >= 1);

    dump_mode mode = DUMP_NICE;
    size_t offset = 1;
    const char *tskey_filename = NULL;

    if ((size_t) argc > offset && argv[offset] != NULL && argv[offset][0] == '-')
    {
        const char *const option = argv[offset];
        offset += 1;

        if (StringMatchesOption(option, "--keys", "-k"))
        {
            mode = DUMP_KEYS;
        }
        else if (StringMatchesOption(option, "--values", "-v"))
        {
            mode = DUMP_VALUES;
        }
        else if (StringMatchesOption(option, "--nice", "-n"))
        {
            mode = DUMP_NICE;
        }
        else if (StringMatchesOption(option, "--simple", "-s"))
        {
            mode = DUMP_SIMPLE;
        }
        else if (StringMatchesOption(option, "--portable", "-p"))
        {
            mode = DUMP_PORTABLE;
        }
        else if (StringMatchesOption(option, "--tskey", "-t"))
        {
            tskey_filename = argv[offset];
            offset += 1;
        }
        else
        {
            print_usage();
            printf("Unrecognized option: '%s'\n", option);
            return 1;
        }
    }

    if ((size_t) argc > offset && argv[offset] != NULL && argv[offset][0] == '-')
    {
        print_usage();
        printf("Only one option supported!\n");
        return 1;
    }

    Seq *files = argv_to_lmdb_files(argc, argv, offset);
    const int ret = dump_dbs(files, mode, tskey_filename);
    SeqDestroy(files);
    return ret;
}

#else
int dump_main(ARG_UNUSED int argc, ARG_UNUSED const char *const *const argv)
{
    printf("dump only implemented for LMDB.\n");
    return 1;
}
#endif
