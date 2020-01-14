#include <platform.h>
#include <dump.h>

#ifdef LMDB
#include <lmdb.h>
#include <string_lib.h>
#include <json.h>
#include <db_structs.h>
#include <utilities.h>

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
    printf("Usage: cf-check dump [-k|-v|-n|-s|-p] [FILE ...]\n");
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
    assert(size != 0); // Don't know of anything we store which can be size 0

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
            printf("\nError: This database contains unknown binary data - use --simple to print anyway\n");
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
        const KeyHostSeen *const quality = value.mv_data;
        const time_t lastseen = quality->lastseen;
        const QPoint Q = quality->Q;

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
        const LockData *const lock = value.mv_data;
        const pid_t pid = lock->pid;
        const time_t time = lock->time;
        const time_t process_start_time = lock->process_start_time;

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
    const MDB_val value, const bool strip_strings)
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
        const Averages *const averages = value.mv_data;
        const time_t last_seen = averages->last_seen;

        JsonElement *all_observables = JsonObjectCreate(observables_max);
        assert(observables_max <= CF_OBSERVABLES);

        for (Observable i = 0; i < observables_max; ++i)
        {
            JsonElement *observable = JsonObjectCreate(4);
            QPoint Q = averages->Q[i];
            const char *const name = observable_strings[i];

            JsonObjectAppendReal(observable, "q", Q.q);
            JsonObjectAppendReal(observable, "expect", Q.expect);
            JsonObjectAppendReal(observable, "var", Q.var);
            JsonObjectAppendReal(observable, "dq", Q.dq);
            JsonObjectAppendObject(all_observables, name, observable);
        }

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
        const PersistentClassInfo *const class_info = value.mv_data;
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
        const size_t offset_no_padding =
            (sizeof(unsigned int) + sizeof(PersistentClassPolicy));
        assert(offset >= offset_no_padding);

        assert(value.mv_size > offset);
        const size_t str_size = value.mv_size - offset;
        assert(str_size > 0);
        if (memchr(tags, '\0', str_size) == NULL)
        {
            // String is not terminated, abort or fall back to default:
            debug_abort_if_reached();
            print_json_string(value.mv_data, value.mv_size, strip_strings);
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
    }
}

static void print_struct_or_string(
    const MDB_val key,
    const MDB_val value,
    const char *const file,
    const bool strip_strings,
    const bool structs)
{
    if (structs)
    {
        if (StringEndsWith(file, "cf_lastseen.lmdb")
            && StringStartsWith(key.mv_data, "q"))
        {
            print_struct_lastseen_quality(value, strip_strings);
        }
        else if (StringEndsWith(file, "cf_lock.lmdb"))
        {
            print_struct_lock_data(value, strip_strings);
        }
        else if (StringEndsWith(file, "cf_observations.lmdb"))
        {
            if (StringSafeEqual(key.mv_data, "DATABASE_AGE"))
            {
                assert(sizeof(double) == value.mv_size);
                const double *const age = value.mv_data;
                printf("%f", *age);
            }
            else
            {
                print_struct_averages(value, strip_strings);
            }
        }
        else if (StringEndsWith(file, "history.lmdb"))
        {
            print_struct_averages(value, strip_strings);
        }
        else if (StringEndsWith(file, "cf_state.lmdb"))
        {
            print_struct_persistent_class(value, strip_strings);
        }
        else if (StringEndsWith(file, "nova_agent_execution.lmdb"))
        {
            if (StringSafeEqual(key.mv_data, "delta_gavr"))
            {
                assert(sizeof(double) == value.mv_size);
                const double *const average = value.mv_data;
                printf("%f", *average);
            }
            else if (StringSafeEqual(key.mv_data, "last_exec"))
            {
                assert(sizeof(time_t) == value.mv_size);
                const time_t *const last_exec = value.mv_data;
                printf("%ju", (uintmax_t) (*last_exec));
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
    const bool structs)
{
    printf("\t");
    print_json_string(key.mv_data, key.mv_size, strip_strings);
    printf(": ");
    print_struct_or_string(key, value, file, strip_strings, structs);
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

static int dump_db(const char *file, const dump_mode mode)
{
    assert(file != NULL);

    const bool strip_strings = (mode == DUMP_NICE);
    const bool structs = (mode == DUMP_NICE || mode == DUMP_PORTABLE);

    int r;
    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_cursor *cursor;

    if (0 != (r = mdb_env_create(&env))
        || 0 != (r = mdb_env_open(env, file, MDB_NOSUBDIR | MDB_RDONLY, 0644))
        || 0 != (r = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn))
        || 0 != (r = mdb_open(txn, NULL, 0, &dbi))
        || 0 != (r = mdb_cursor_open(txn, dbi, &cursor)))
    {
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
            print_line_key_value(key, value, file, strip_strings, structs);
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

static int dump_dbs(Seq *const files, const dump_mode mode)
{
    assert(files != NULL);
    const size_t length = SeqLength(files);
    assert(length > 0);

    if (length == 1)
    {
        return dump_db(SeqAt(files, 0), mode);
    }

    int ret = 0;

    for (size_t i = 0; i < length; ++i)
    {
        const char *const filename = SeqAt(files, i);
        printf("%s:\n", filename);
        const int r = dump_db(filename, mode);
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

    if (argc > offset && argv[offset] != NULL && argv[offset][0] == '-')
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
        else
        {
            print_usage();
            printf("Unrecognized option: '%s'\n", option);
            return 1;
        }
    }

    if (argc > offset && argv[offset] != NULL && argv[offset][0] == '-')
    {
        print_usage();
        printf("Only one option supported!\n");
        return 1;
    }

    Seq *files = argv_to_lmdb_files(argc, argv, offset);
    const int ret = dump_dbs(files, mode);
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
