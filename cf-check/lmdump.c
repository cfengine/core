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
    printf("    -a : print keys in ascii form\n");
    printf("    -A : print keys and values in ascii form\n");
    printf("    -x : print keys and values in hexadecimal form\n");
    printf("    -d : print only the size of keys and values\n");
}

static int lmdump_report_error(int rc)
{
    printf("err(%d): %s\n", rc, mdb_strerror(rc));
    return rc;
}

int lmdump_main(int argc, char * argv[])
{
    int rc;
    MDB_env *env;
    MDB_dbi dbi;
    MDB_val key, data;
    MDB_txn *txn;
    MDB_cursor *cursor;

    if (argc < 3)
    {
        lmdump_print_usage();
        return 1;
    }
    int mode = -1;
    if (strcmp(argv[1],"-a") == 0)
    {
        mode = 'a';
    }
    else if (strcmp(argv[1], "-A") == 0)
    {
        mode = 'A';
    }
    else if (strcmp(argv[1], "-x") == 0)
    {
        mode = 'x';
    }
    else if (strcmp(argv[1], "-d") == 0)
    {
        mode = 'd';
    }
    else
    {
        lmdump_print_usage();
        return 1;
    }
    rc = mdb_env_create(&env);
    if (rc) return lmdump_report_error(rc);

    rc = mdb_env_open(env, argv[2], MDB_NOSUBDIR, 0644);
    if (rc) return lmdump_report_error(rc);

    rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc) return lmdump_report_error(rc);

    rc = mdb_open(txn, NULL, 0, &dbi);
    if (rc) return lmdump_report_error(rc);

    rc = mdb_cursor_open(txn, dbi, &cursor);
    if (rc) return lmdump_report_error(rc);

    while ( (rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == MDB_SUCCESS )
    {
        if (mode == 'A')
        {
            printf("key: %p[%d] %.*s\n",
                key.mv_data, (int) key.mv_size, (int) key.mv_size, (char *) key.mv_data);
        }
        else if (mode == 'a')
        {
            printf("key: %p[%d] %.*s, data: %p[%d] %.*s\n",
                key.mv_data, (int) key.mv_size, (int) key.mv_size, (char *) key.mv_data,
                data.mv_data, (int) data.mv_size, (int) data.mv_size, (char *) data.mv_data);
        }
        else if (mode == 'd')
        {
            printf("key: %p[%d] ,data: %p[%d]\n",
                key.mv_data,  (int) key.mv_size,
                data.mv_data, (int) data.mv_size);
        }
        else if (mode == 'x')
        {
            printf("key: %p[%d] ", key.mv_data,  (int) key.mv_size);
            lmdump_print_hex(key.mv_data,  (int) key.mv_size);
            printf(" ,data: %p[%d] ", data.mv_data,  (int) data.mv_size);
            lmdump_print_hex(data.mv_data,  (int) data.mv_size);
            printf("\n");
        }
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

#else
int lmdump_main(ARG_UNUSED int argc, ARG_UNUSED char * argv[])
{
    printf("lmdump only implemented for LMDB.\n");
    return 1;
}
#endif
