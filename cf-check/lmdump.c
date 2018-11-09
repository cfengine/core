#include <platform.h>
#include <stdio.h>
#include <lmdump.h>

#ifdef LMDB
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <lmdb.h>

static void lmdump_print_hex(const char *s, size_t len)
{
    for (int i = 0; i < len; i++)
    {
        printf("%02x", s[i]);
    }
}

static void lmdump_print_usage(void)
{
    printf("Lmdb database dumper\n");
    printf("Usage: lmdump -d|-x|-a|-A filename\n\n");
    printf("Has three modes :\n");
    printf("    -A : print keys in ascii form\n");
    printf("    -a : print keys and values in ascii form\n");
    printf("    -x : print keys and values in hexadecimal form\n");
    printf("    -d : print only the size of keys and values\n");
}

lmdump_mode lmdump_char_to_mode(char mode)
{
    switch (mode) {
        case 'A':
            return LMDUMP_KEYS_ASCII;
        case 'a':
            return LMDUMP_VALUES_ASCII;
        case 'x':
            return LMDUMP_VALUES_HEX;
        case 'd':
            return LMDUMP_SIZES;
        default:
            break;
    }
    return LMDUMP_UNKNOWN;
}

static int lmdump_report_error(int rc)
{
    printf("err(%d): %s\n", rc, mdb_strerror(rc));
    return rc;
}

void lmdump_print_line(lmdump_mode mode, MDB_val key, MDB_val data)
{
    assert(mode >= 0 && mode < LMDUMP_UNKNOWN);

    switch (mode) {
        case LMDUMP_KEYS_ASCII:
            printf("key: %p[%d] %.*s\n",
                key.mv_data, (int) key.mv_size, (int) key.mv_size, (char *) key.mv_data);
            break;
        case LMDUMP_VALUES_ASCII:
            printf("key: %p[%d] %.*s, data: %p[%d] %.*s\n",
                key.mv_data, (int) key.mv_size, (int) key.mv_size, (char *) key.mv_data,
                data.mv_data, (int) data.mv_size, (int) data.mv_size, (char *) data.mv_data);
            break;
        case LMDUMP_VALUES_HEX:
            printf("key: %p[%d] ", key.mv_data,  (int) key.mv_size);
            lmdump_print_hex(key.mv_data,  (int) key.mv_size);
            printf(" ,data: %p[%d] ", data.mv_data,  (int) data.mv_size);
            lmdump_print_hex(data.mv_data,  (int) data.mv_size);
            printf("\n");
            break;
        case LMDUMP_SIZES:
            printf("key: %p[%d] ,data: %p[%d]\n",
                key.mv_data,  (int) key.mv_size,
                data.mv_data, (int) data.mv_size);
            break;
        default:
            break;
    }
}

int lmdump(lmdump_mode mode, const char *file)
{
    assert(mode >= 0 && mode < LMDUMP_UNKNOWN);
    assert(file != NULL);

    int rc;

    MDB_env *env;
    rc = mdb_env_create(&env);
    if (rc) return lmdump_report_error(rc);

    rc = mdb_env_open(env, file, MDB_NOSUBDIR | MDB_RDONLY, 0644);
    if (rc) return lmdump_report_error(rc);

    MDB_txn *txn;
    rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc) return lmdump_report_error(rc);

    MDB_dbi dbi;
    rc = mdb_open(txn, NULL, 0, &dbi);
    if (rc) return lmdump_report_error(rc);

    MDB_cursor *cursor;
    rc = mdb_cursor_open(txn, dbi, &cursor);
    if (rc) return lmdump_report_error(rc);

    MDB_val key, data;
    while ( (rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == MDB_SUCCESS )
    {
        lmdump_print_line(mode, key, data);
    }
    if (rc != MDB_NOTFOUND)
    {
        // At this point, not found is expected, anything else is an error
        return lmdump_report_error(rc);
    }
    mdb_cursor_close(cursor);
    mdb_close(env, dbi);

    mdb_txn_abort(txn);
    mdb_env_close(env);

    return 0;
}

int lmdump_main(int argc, char * argv[])
{
    assert(argv != NULL);

    if (argc != 3 || argv[1][0] != '-')
    {
        lmdump_print_usage();
        return 1;
    }

    const char *filename = argv[2];
    const lmdump_mode mode = lmdump_char_to_mode(argv[1][1]);

    if (mode == LMDUMP_UNKNOWN)
    {
        lmdump_print_usage();
        return 1;
    }

    return lmdump(mode, filename);
}

#else
int lmdump_main(ARG_UNUSED int argc, ARG_UNUSED char * argv[])
{
    printf("lmdump only implemented for LMDB.\n");
    return 1;
}
#endif
