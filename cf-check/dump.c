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
    DUMP_MODE_NICE, // Print strings in a nice way, and file specific structs
    DUMP_MODE_PORTABLE, // Portable and unambiguous, structs and raw strings
    DUMP_MODE_SIMPLE, // Fallback mode for arbitrary / unrecognized data
    DUMP_MODE_KEYS,
    DUMP_MODE_VALUES,
} dump_mode;

static void print_usage(void)
{
    printf("Usage: cf-check dump [OPTION] [FILE ...]\n");
    printf("Example: cf-check dump /var/cfengine/state/cf_lastseen.lmdb\n");
}

static int dump_report_error(int rc)
{
    printf("err(%d): %s\n", rc, mdb_strerror(rc));
    return rc;
}

void dump_print_json_string(
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
        const size_t length = strnlen(data, size);

        // Unfortunately, the packages_installed_apt_get.lmdb database
        // has unterminated strings:
        assert((size == 1) || data[size -1] == '\n' || (length == (size - 1)));

        // Most of what we store are C strings
        // except 1 byte '0' or '1', and some structs
        // So in nice mode, we try to default to printing C strings in a nice way
        // This means it can be ambiguous (we chop off the NUL byte sometimes)
        // Use --simple for correct, unambiguous, uglier output
        if (size > 1 && length == (size - 1))
        {
            // Looks like a normal string, let's remove NUL byte (nice mode)
            size = length;
        }
    }

    Slice unescaped_data = {.data = (void *) data, .size = size};
    char *escaped_data = Json5EscapeData(unescaped_data);
    printf("%s", escaped_data);
    free(escaped_data);

    printf("\"");
}

void dump_print_opening_bracket(const dump_mode mode)
{
    if (mode == DUMP_MODE_VALUES || mode == DUMP_MODE_KEYS)
    {
        printf("[\n");
    }
    else
    {
        printf("{\n");
    }
}

void print_lastseen_quality(const MDB_val value, const bool strip_strings)
{
    assert(sizeof(KeyHostSeen) == value.mv_size);
    if (sizeof(KeyHostSeen) != value.mv_size)
    {
        // Fall back to simple printing in release builds:
        dump_print_json_string(value.mv_data, value.mv_size, strip_strings);
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

void print_lock_data(const MDB_val value, const bool strip_strings)
{
    assert(sizeof(LockData) == value.mv_size);
    if (sizeof(LockData) != value.mv_size)
    {
        // Fall back to simple printing in release builds:
        dump_print_json_string(value.mv_data, value.mv_size, strip_strings);
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
        JsonObjectAppendInteger(json, "process_start_time", process_start_time);

        Writer *w = FileWriter(stdout);
        JsonWriteCompact(w, json);
        FileWriterDetach(w);
        JsonDestroy(json);
    }
}

// Used to print values in /var/cfengine/state/cf_observations.lmdb:
void print_averages(const MDB_val value, const bool strip_strings)
{
    assert(sizeof(Averages) == value.mv_size);
    if (sizeof(Averages) != value.mv_size)
    {
        // Fall back to simple printing in release builds:
        dump_print_json_string(value.mv_data, value.mv_size, strip_strings);
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

void print_persistent_class(const MDB_val value, const bool strip_strings)
{
    // Value from db should always be bigger than sizeof struct
    // Since tags is not counted in sizeof. `tags` is variable size,
    // and should be at least size 1 (NUL byte) for data to make sense.
    assert(value.mv_size > sizeof(PersistentClassInfo));
    if (value.mv_size <= sizeof(PersistentClassInfo))
    {
        // Fall back to simple printing in release builds:
        dump_print_json_string(value.mv_data, value.mv_size, strip_strings);
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
            dump_print_json_string(value.mv_data, value.mv_size, strip_strings);
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

void dump_print_value(
    const MDB_val key,
    const MDB_val value,
    const char *const file,
    const bool strip_strings,
    const bool structs)
{
    bool fallback = false;
    if (structs)
    {
        if (StringEndsWith(file, "cf_lastseen.lmdb")
            && StringStartsWith(key.mv_data, "q"))
        {
            print_lastseen_quality(value, strip_strings);
        }
        else if (StringEndsWith(file, "cf_lock.lmdb"))
        {
            print_lock_data(value, strip_strings);
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
                print_averages(value, strip_strings);
            }
        }
        else if (StringEndsWith(file, "history.lmdb"))
        {
            print_averages(value, strip_strings);
        }
        else if (StringEndsWith(file, "cf_state.lmdb"))
        {
            print_persistent_class(value, strip_strings);
        }
        else
        {
            fallback = true;
        }
    }

    const bool was_printed = (structs && !fallback);
    if (!was_printed)
    {
        dump_print_json_string(value.mv_data, value.mv_size, strip_strings);
    }
}

void dump_print_object_line(
    const MDB_val key,
    const MDB_val value,
    const char *const file,
    const bool strip_strings,
    const bool structs)
{
    printf("\t");
    dump_print_json_string(key.mv_data, key.mv_size, strip_strings);
    printf(": ");
    dump_print_value(key, value, file, strip_strings, structs);
    printf(",\n");
}

void dump_print_array_line(const MDB_val value, const bool strip_strings)
{
    printf("\t");
    dump_print_json_string(value.mv_data, value.mv_size, strip_strings);
    printf(",\n");
}

void dump_print_closing_bracket(const dump_mode mode)
{
    if (mode == DUMP_MODE_KEYS || mode == DUMP_MODE_VALUES)
    {
        printf("]\n");
    }
    else
    {
        printf("}\n");
    }
}

int dump_db(const char *file, const dump_mode mode)
{
    assert(file != NULL);

    const bool strip_strings = ((mode == DUMP_MODE_NICE) ? true : false);
    const bool structs =
        ((mode == DUMP_MODE_NICE || mode == DUMP_MODE_PORTABLE) ? true : false);

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
    dump_print_opening_bracket(mode);
    while ((r = mdb_cursor_get(cursor, &key, &value, MDB_NEXT)) == MDB_SUCCESS)
    {
        switch (mode) {
            case DUMP_MODE_NICE:
            case DUMP_MODE_PORTABLE:
            case DUMP_MODE_SIMPLE:
                dump_print_object_line(key, value, file, strip_strings, structs);
                break;
            case DUMP_MODE_KEYS:
                dump_print_array_line(key, strip_strings);
                break;
            case DUMP_MODE_VALUES:
                dump_print_array_line(value, strip_strings);
                break;
                break;
            default:
                debug_abort_if_reached();
                break;
        }
    }
    dump_print_closing_bracket(mode);

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

int dump_dbs(Seq *const files, const dump_mode mode)
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

static bool matches_option(
    const char *const supplied,
    const char *const longopt,
    const char *const shortopt)
{
    assert(supplied != NULL);
    assert(shortopt != NULL);
    assert(longopt != NULL);
    assert(strlen(shortopt) == 2);
    assert(strlen(longopt) >= 3);
    assert(shortopt[0] == '-' && shortopt[1] != '-');
    assert(longopt[0] == '-' && longopt[1] == '-' && longopt[2] != '-');

    const size_t length = strlen(supplied);
    if (length <= 1)
    {
        return false;
    }
    else if (length == 2)
    {
        return StringSafeEqual(supplied, shortopt);
    }
    return StringSafeEqualN_IgnoreCase(supplied, longopt, length);
}

int dump_main(int argc, const char *const *const argv)
{
    assert(argv != NULL);
    assert(argc >= 1);

    dump_mode mode = DUMP_MODE_NICE;
    const char *const *filenames = argv + 1;
    size_t filenames_len = argc - 1;
    if (filenames_len > 0 && filenames[0] != NULL && filenames[0][0] == '-')
    {
        const char *const option = filenames[0];

        filenames += 1;
        filenames_len -= 1;

        if (matches_option(option, "--keys", "-k"))
        {
            mode = DUMP_MODE_KEYS;
        }
        else if (matches_option(option, "--values", "-v"))
        {
            mode = DUMP_MODE_VALUES;
        }
        else if (matches_option(option, "--nice", "-n"))
        {
            mode = DUMP_MODE_NICE;
        }
        else if (matches_option(option, "--simple", "-s"))
        {
            mode = DUMP_MODE_SIMPLE;
        }
        else if (matches_option(option, "--portable", "-p"))
        {
            mode = DUMP_MODE_PORTABLE;
        }
        else
        {
            print_usage();
            printf("Unrecognized option: '%s'\n", option);
            return 1;
        }
    }

    if (filenames_len > 0 && filenames[0] != NULL && filenames[0][0] == '-')
    {
        print_usage();
        printf("Only one option supported!\n");
        return 1;
    }

    Seq *files = argv_to_lmdb_files(filenames_len, filenames, 0);
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
