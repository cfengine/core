#include <platform.h>
#include <dump.h>

#ifdef LMDB
#include <lmdb.h>
#include <string_lib.h>
#include <json.h>

typedef enum
{
    DUMP_MODE_SIMPLE,
    DUMP_MODE_KEYS,
    DUMP_MODE_VALUES,
} dump_mode;

static void print_usage(void)
{
    printf("Usage: cf-check dump FILE\n");
    printf("Example: cf-check dump /var/cfengine/state/cf_lastseen.lmdb\n");
}

static int dump_report_error(int rc)
{
    printf("err(%d): %s\n", rc, mdb_strerror(rc));
    return rc;
}

void dump_print_json_string(const char *const data, const size_t size)
{
    printf("\"");

    Slice unescaped_data = {.data = (void *) data, .size = size};
    // TODO: should probably change Slice in libntech to take (const void *)
    // TODO: Expose Json5EscapeDataWriter and use it instead
    char *escaped_data = Json5EscapeData(unescaped_data);
    printf("%s", escaped_data);
    free(escaped_data);

    printf("\"");
}

void dump_print_opening_bracket(const dump_mode mode)
{
    if (mode == DUMP_MODE_SIMPLE)
    {
        printf("{\n");
    }
    else
    {
        printf("[\n");
    }
}

void dump_print_object_line(const MDB_val key, const MDB_val value)
{
    printf("\t");
    dump_print_json_string(key.mv_data, key.mv_size);
    printf(": ");
    dump_print_json_string(value.mv_data, value.mv_size);
    printf(",\n");
}

void dump_print_array_line(const MDB_val value)
{
    printf("\t");
    dump_print_json_string(value.mv_data, value.mv_size);
    printf(",\n");
}

void dump_print_closing_bracket(const dump_mode mode)
{
    if (mode == DUMP_MODE_SIMPLE)
    {
        printf("}\n");
    }
    else
    {
        printf("]\n");
    }
}

int dump_db(const char *file, const dump_mode mode)
{
    assert(file != NULL);

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
            case DUMP_MODE_SIMPLE:
                dump_print_object_line(key, value);
                break;
            case DUMP_MODE_KEYS:
                dump_print_array_line(key);
                break;
            case DUMP_MODE_VALUES:
                dump_print_array_line(value);
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

int dump_main(int argc, const char *const *const argv)
{
    assert(argv != NULL);
    if (argc < 2 || argc > 3)
    {
        print_usage();
        return EXIT_FAILURE;
    }
    dump_mode mode = DUMP_MODE_SIMPLE;
    const char *filename = argv[1];
    if (argv[1][0] == '-')
    {
        if (argc != 3)
        {
            print_usage();
            return EXIT_FAILURE;
        }
        const char *const option = argv[1];
        filename = argv[2];
        if (StringSafeEqual(option, "--keys")
            || StringSafeEqual(option, "-k"))
        {
            mode = DUMP_MODE_KEYS;
        }
        else if (
            StringSafeEqual(option, "--values")
            || StringSafeEqual(option, "-v"))
        {
            mode = DUMP_MODE_VALUES;
        }
    }
    return dump_db(filename, mode);
}

#else
int dump_main(ARG_UNUSED int argc, ARG_UNUSED const char *const *const argv)
{
    printf("dump only implemented for LMDB.\n");
    return 1;
}
#endif
