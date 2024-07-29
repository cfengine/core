/*
  Copyright 2024 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; version 3.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef __ANDROID__
#include <platform.h>
#include <stdlib.h>             /* lrand48_r() */
#include <unistd.h>             /* usleep(), syscall()/gettid() */
#include <alloc.h>              /* xstrndup() */
#include <dbm_api.h>
#include <logging.h>
#include <sequence.h>
#include <set.h>
#include <string_lib.h>         /* StringFormat() */

#include <dbm_test_api.h>

#if !HAVE_DECL_GETTID
/* Older versions of glibc don't provide a wrapper function for the gettid()
 * syscall. */
#include <sys/syscall.h>
static inline pid_t gettid()
{
    return syscall(SYS_gettid);
}
#endif

typedef struct DBItem {
    char *key;
    size_t val_size;
    void *val;
} DBItem;

DBItem *DBItemNew(const char *key, size_t val_size, void *val)
{
    DBItem *ret = xmalloc(sizeof(DBItem));
    ret->key = xstrdup(key);
    ret->val_size = val_size;
    ret->val = xmemdup(val, val_size);

    return ret;
}

static void DBItemDestroy(DBItem *item)
{
    if (item != NULL)
    {
        free(item->key);
        free(item->val);
        free(item);
    }
}

static __thread struct drand48_data rng_data;

static void InitializeRNG(long int seed)
{
    srand48_r(seed, &rng_data);
}

static long GetRandomNumber(long limit)
{
    long rnd_val;
    lrand48_r(&rng_data, &rnd_val); /* generates a value in the [0, 2^31) interval */
    const long random_max = ((0x80000000 - 1) / limit) * limit;
    while (rnd_val > random_max)
    {
        /* got a bad value past the greatest multiple of the limit interval,
         * retry (see "modulo bias" if this is unclear) */
        lrand48_r(&rng_data, &rnd_val);
    }
    return rnd_val % limit;
}


static Seq *GetDBKeys(DBHandle *db)
{
    DBCursor *cur;
    bool success = NewDBCursor(db, &cur);
    if (!success)
    {
        return NULL;
    }

    Seq *ret = SeqNew(16, free);
    while (success)
    {
        int key_size;
        char *key;
        int val_size;
        void *val;
        success = NextDB(cur, &key, &key_size, &val, &val_size);
        if (success)
        {
            SeqAppend(ret, xstrndup(key, key_size));
        }
    }
    DeleteDBCursor(cur);

    return ret;
}

/* TODO: LoadDBIntoMap() */
static Seq *LoadDB(DBHandle *db, size_t limit)
{
    DBCursor *cur;
    bool success = NewDBCursor(db, &cur);
    if (!success)
    {
        return NULL;
    }

    Seq *ret = SeqNew(16, DBItemDestroy);
    size_t remaining = (limit != 0) ? limit : SIZE_MAX;
    while (success && (remaining > 0))
    {
        int key_size;
        char *key;
        int val_size;
        void *val;
        success = NextDB(cur, &key, &key_size, &val, &val_size);
        if (success)
        {
            SeqAppend(ret, DBItemNew(key, val_size, val));
            remaining--;
        }
    }
    DeleteDBCursor(cur);

    return ret;
}


void DoRandomReads(dbid db_id,
                   int keys_refresh_s, long min_interval_ms, long max_interval_ms,
                   bool *terminate)
{
    assert(terminate != NULL);

    InitializeRNG((long) gettid());

    DBHandle *db;
    bool success = OpenDB(&db, db_id);
    assert(success);

    Seq *keys = GetDBKeys(db);
    assert(keys != NULL);
    CloseDB(db);

    size_t n_keys = SeqLength(keys);
    unsigned long acc_sleeps_ms = 0;
    while (!*terminate)
    {
        long rnd = GetRandomNumber(max_interval_ms - min_interval_ms);
        unsigned long sleep_for_ms = min_interval_ms + rnd;
        int ret = usleep(sleep_for_ms * 1000);
        acc_sleeps_ms += sleep_for_ms;
        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            /* else */
            /* Should never happen. */
            Log(LOG_LEVEL_ERR, "Failed to usleep() for %ld ms: %s",
                (min_interval_ms + rnd) * 1000, GetErrorStr());
            SeqDestroy(keys);
            return;
        }
        rnd = GetRandomNumber(n_keys);
        unsigned char val[1024];
        char *key = SeqAt(keys, rnd);

        bool success = OpenDB(&db, db_id);
        assert(success);

        success = ReadDB(db, key, val, sizeof(val));
        assert(success || !HasKeyDB(db, key, strlen(key)));

        if (acc_sleeps_ms > (keys_refresh_s * 1000))
        {
            SeqDestroy(keys);
            keys = GetDBKeys(db);
            assert(keys != NULL);
            n_keys = SeqLength(keys);
            acc_sleeps_ms = 0;
        }
        CloseDB(db);
    }
    SeqDestroy(keys);
}

struct ReadParams {
    dbid db_id;
    int keys_refresh_s; long min_interval_ms; long max_interval_ms;
    bool *terminate;
};

static void *DoRandomReadsRoutine(void *data)
{
    struct ReadParams *params = data;
    DoRandomReads(params->db_id,
                  params->keys_refresh_s, params->min_interval_ms, params->max_interval_ms,
                  params->terminate);
    /* Always return NULL, pthread_create() requires a function which returns
       (void *) */
    return NULL;
}


static bool PruneDB(DBHandle *db, const char *prefix, StringSet *written_keys)
{
    DBCursor *cur;
    bool success = NewDBCursor(db, &cur);
    assert(success);

    bool have_next = success;
    while (success && have_next)
    {
        int key_size;
        char *key;
        int val_size;
        void *val;
        have_next = NextDB(cur, &key, &key_size, &val, &val_size);
        if (have_next && StringStartsWith(key, prefix))
        {
            success = DBCursorDeleteEntry(cur);
            if (success)
            {
                StringSetRemove(written_keys, key);
            }
        }
    }
    DeleteDBCursor(cur);

    return success;
}

void DoRandomWrites(dbid db_id,
                    int sample_size_pct,
                    int prune_interval_s, long min_interval_ms, long max_interval_ms,
                    bool *terminate)
{
    assert(terminate != NULL);

    InitializeRNG((long) gettid());

    DBHandle *db;
    bool success = OpenDB(&db, db_id);
    assert(success);

    Seq *items = LoadDB(db, 0);
    assert(items != NULL);
    CloseDB(db);

    const size_t n_items = SeqLength(items);
    const size_t n_samples = (n_items * sample_size_pct) / 100;
    assert(n_samples > 0);
    const size_t sample_nths = n_items / n_samples; /* see below */
    StringSet *written_keys = StringSetNew();
    unsigned long acc_sleeps_ms = 0;
    while (!*terminate)
    {
        long rnd = GetRandomNumber(max_interval_ms - min_interval_ms);
        unsigned long sleep_for_ms = min_interval_ms + rnd;
        int ret = usleep(sleep_for_ms * 1000);
        acc_sleeps_ms += sleep_for_ms;
        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            /* else */
            /* Should never happen. */
            Log(LOG_LEVEL_ERR, "Failed to usleep() for %ld ms: %s",
                (min_interval_ms + rnd) * 1000, GetErrorStr());
            SeqDestroy(items);
            return;
        }

        /* We only want to use a sample_size portion of all the items given by
         * sample_size_pct. However, instead of picking a random number in the
         * 0-sample_size range, we split the full range of all items into
         * (n_items / n_samples)-ths and then use the first item from the
         * respective nth. For example, if using 30% of 10 items, we would only
         * use the items at indices 0, 3 and 6.
         *
         * This potentially gives us better sample from the original set where
         * similar items are likely to be next to each other. */
        rnd = GetRandomNumber(n_items);
        size_t idx = ((rnd * n_samples) / n_items) * sample_nths;
        DBItem *item = SeqAt(items, idx);

        success = OpenDB(&db, db_id);
        assert(success);

        /* Derive a key for our new item from the thread ID and the original
         * item's key so that we don't mess with the original item and we can
         * clean after ourselves (LMDB has a limit of 511 bytes for the key). */
        char *key = StringFormat("test_%ju_%.400s", (uintmax_t) gettid(), item->key);
        success = WriteDB(db, key, item->val, item->val_size);
        assert(success);
        StringSetAdd(written_keys, key); /* takes ownership of key */

        if (acc_sleeps_ms > (prune_interval_s * 1000))
        {
            char *key_prefix = StringFormat("test_%ju_", (uintmax_t) gettid());
            success = PruneDB(db, key_prefix, written_keys);
            free(key_prefix);
            assert(success);
            acc_sleeps_ms = 0;
        }
        CloseDB(db);
    }

    success = OpenDB(&db, db_id);
    assert(success);

    /* Clean after ourselves. */
    SetIterator iter = StringSetIteratorInit(written_keys);
    char *key;
    while ((key = StringSetIteratorNext(&iter)) != NULL)
    {
        success = DeleteDB(db, key);
        assert(success);
    }
    CloseDB(db);

    SeqDestroy(items);
}

struct WriteParams {
    dbid db_id;
    int sample_size_pct;
    int prune_interval_s; long min_interval_ms; long max_interval_ms;
    bool *terminate;
};

static void *DoRandomWritesRoutine(void *data)
{
    struct WriteParams *params = data;
    DoRandomWrites(params->db_id,
                   params->sample_size_pct,
                   params->prune_interval_s, params->min_interval_ms, params->max_interval_ms,
                   params->terminate);
    /* Always return NULL, pthread_create() requires a function which returns
       (void *) */
    return NULL;
}


void DoRandomIterations(dbid db_id,
                        long min_interval_ms, long max_interval_ms,
                        bool *terminate)
{
    assert(terminate != NULL);

    InitializeRNG((long)gettid());

    while (!terminate)
    {
        long rnd = GetRandomNumber(max_interval_ms - min_interval_ms);
        unsigned long sleep_for_ms = min_interval_ms + rnd;
        int ret = usleep(sleep_for_ms * 1000);
        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            /* else */
            /* Should never happen. */
            Log(LOG_LEVEL_ERR, "Failed to usleep() for %ld ms: %s",
                (min_interval_ms + rnd) * 1000, GetErrorStr());
            return;
        }

        DBHandle *db;
        bool success = OpenDB(&db, db_id);
        assert(success);

        DBCursor *cur;
        success = NewDBCursor(db, &cur);
        assert(success);

        while (success)
        {
            int key_size;
            char *key;
            int val_size;
            void *val;
            success = NextDB(cur, &key, &key_size, &val, &val_size);
        }
        DeleteDBCursor(cur);
        CloseDB(db);
    }
}

struct IterParams {
    dbid db_id;
    long min_interval_ms; long max_interval_ms;
    bool *terminate;
};

static void *DoRandomIterationsRoutine(void *data)
{
    struct IterParams *params = data;
    DoRandomIterations(params->db_id,
                       params->min_interval_ms, params->max_interval_ms,
                       params->terminate);
    return NULL;
}


struct DBLoadSimulation_ {
    struct ReadParams read_params;
    pthread_t read_th;
    bool read_th_started;
    bool read_th_terminate;

    struct WriteParams write_params;
    pthread_t write_th;
    bool write_th_started;
    bool write_th_terminate;

    struct IterParams iter_params;
    pthread_t iter_th;
    bool iter_th_started;
    bool iter_th_terminate;
};

DBLoadSimulation *SimulateDBLoad(dbid db_id,
                                 int read_keys_refresh_s, long read_min_interval_ms, long read_max_interval_ms,
                                 int write_sample_size_pct,
                                 int write_prune_interval_s, long write_min_interval_ms, long write_max_interval_ms,
                                 long iter_min_interval_ms, long iter_max_interval_ms)
{
    /* Try to open the DB as a safety check. */
    DBHandle *db;
    bool success = OpenDB(&db, db_id);
    if (!success)
    {
        /* Not a nice log message, but this is testing/debugging code normal
         * users should never run and face. */
        Log(LOG_LEVEL_ERR, "Failed to open DB with ID %d", db_id);
        return NULL;
    }
    CloseDB(db);

    DBLoadSimulation *simulation = xcalloc(sizeof(DBLoadSimulation), 1);
    simulation->read_params.db_id = db_id;
    simulation->read_params.terminate = &(simulation->read_th_terminate);
    simulation->read_params.keys_refresh_s = read_keys_refresh_s;
    simulation->read_params.min_interval_ms = read_min_interval_ms;
    simulation->read_params.max_interval_ms = read_max_interval_ms;

    simulation->write_params.db_id = db_id;
    simulation->write_params.terminate = &(simulation->write_th_terminate);
    simulation->write_params.sample_size_pct = write_sample_size_pct;
    simulation->write_params.prune_interval_s = write_prune_interval_s;
    simulation->write_params.min_interval_ms = write_min_interval_ms;
    simulation->write_params.max_interval_ms = write_max_interval_ms;

    simulation->iter_params.db_id = db_id;
    simulation->iter_params.terminate = &(simulation->iter_th_terminate);
    simulation->iter_params.min_interval_ms = iter_min_interval_ms;
    simulation->iter_params.max_interval_ms = iter_max_interval_ms;

    if ((simulation->read_params.keys_refresh_s  != 0) ||
        (simulation->read_params.min_interval_ms != 0) ||
        (simulation->read_params.max_interval_ms != 0))
    {
        int ret = pthread_create(&(simulation->read_th), NULL,
                                 DoRandomReadsRoutine,
                                 &(simulation->read_params));
        simulation->read_th_started = (ret == 0);
        if (!simulation->read_th_started)
        {
            Log(LOG_LEVEL_ERR, "Failed to start read simulation thread: %s",
                GetErrorStrFromCode(ret));
        }
    }

    if ((simulation->write_params.prune_interval_s != 0) ||
        (simulation->write_params.min_interval_ms  != 0) ||
        (simulation->write_params.max_interval_ms  != 0))
    {
        int ret = pthread_create(&(simulation->write_th), NULL,
                                 DoRandomWritesRoutine,
                                 &(simulation->write_params));
        simulation->write_th_started = (ret == 0);
        if (!simulation->write_th_started)
        {
            Log(LOG_LEVEL_ERR, "Failed to start write simulation thread: %s",
                GetErrorStrFromCode(ret));
        }
    }

    if ((simulation->iter_params.min_interval_ms  != 0) ||
        (simulation->iter_params.max_interval_ms  != 0))
    {
        int ret = pthread_create(&(simulation->iter_th), NULL,
                                 DoRandomIterationsRoutine,
                                 &(simulation->iter_params));
        simulation->iter_th_started = (ret == 0);
        if (!simulation->iter_th_started)
        {
            Log(LOG_LEVEL_ERR, "Failed to start iteration simulation thread: %s",
                GetErrorStrFromCode(ret));
        }
    }

    if (!simulation->read_th_started  &&
        !simulation->write_th_started &&
        !simulation->iter_th_started)
    {
        Log(LOG_LEVEL_ERR, "No simulation running");
        free(simulation);
        return NULL;
    }

    return simulation;
}

void StopSimulation(DBLoadSimulation *simulation)
{
    /* Signal threads to terminate. */
    simulation->read_th_terminate = true;
    simulation->write_th_terminate = true;
    simulation->iter_th_terminate = true;

    int ret;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to get real time clock: %s", GetErrorStr());
        Log(LOG_LEVEL_NOTICE, "Joining simulation threads with no timeout");
        if (simulation->read_th_started)
        {
            ret = pthread_join(simulation->read_th, NULL);
            simulation->read_th_started = (ret == 0);
            if (ret != 0)
            {
                Log(LOG_LEVEL_ERR, "Failed to join read simulation thread: %s",
                    GetErrorStrFromCode(ret));
            }
        }
        if (simulation->write_th_started)
        {
            ret = pthread_join(simulation->write_th, NULL);
            simulation->write_th_started = (ret == 0);
            if (ret != 0)
            {
                Log(LOG_LEVEL_ERR, "Failed to join write simulation thread: %s",
                    GetErrorStrFromCode(ret));
            }
        }
        if (simulation->iter_th_started)
        {
            ret = pthread_join(simulation->iter_th, NULL);
            simulation->iter_th_started = (ret == 0);
            if (ret != 0)
            {
                Log(LOG_LEVEL_ERR, "Failed to join iteration simulation thread: %s",
                    GetErrorStrFromCode(ret));
            }
        }
    }

    /* Give at most 5 seconds to threads to terminate. */
    ts.tv_sec += 5;
    if (simulation->read_th_started)
    {
        ret = pthread_timedjoin_np(simulation->read_th, NULL, &ts);
        simulation->read_th_started = (ret != 0);
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR, "Failed to join read simulation thread: %s", GetErrorStrFromCode(ret));
        }
    }
    if (simulation->write_th_started)
    {
        ret = pthread_timedjoin_np(simulation->write_th, NULL, &ts);
        simulation->write_th_started = (ret != 0);
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR, "Failed to join write simulation thread: %s", GetErrorStrFromCode(ret));
        }
    }
    if (simulation->iter_th_started)
    {
        ret = pthread_timedjoin_np(simulation->iter_th, NULL, &ts);
        simulation->iter_th_started = (ret != 0);
        if (ret != 0)
        {
            Log(LOG_LEVEL_ERR, "Failed to join iteration simulation thread: %s",
                GetErrorStrFromCode(ret));
        }
    }

    if (simulation->read_th_started  ||
        simulation->write_th_started ||
        simulation->iter_th_started)
    {
        Log(LOG_LEVEL_ERR, "Failed to stop simulation, leaking simulation data");
    }
    else
    {
        free(simulation);
    }
}


struct DBFilament_ {
    dbid db_id;
    StringSet *items;
};

DBFilament *FillUpDB(dbid db_id, int usage_pct)
{
    DBHandle *db;
    bool success = OpenDB(&db, db_id);
    if (!success)
    {
        Log(LOG_LEVEL_ERR, "Failed to open DB with ID %d", db_id);
        return NULL;
    }

    int usage = GetDBUsagePercentage(db);
    if (usage == -1)
    {
        Log(LOG_LEVEL_ERR, "Cannot determine usage of the DB with ID %d", db_id);
        CloseDB(db);
        return NULL;
    }

    /* We need just one item. */
    Seq *items = LoadDB(db, 1);
    if ((items == NULL) || (SeqLength(items) == 0))
    {
        Log(LOG_LEVEL_ERR, "No DB item to use as a template for fillament");
        CloseDB(db);
        return NULL;
    }
    DBItem *item = SeqAt(items, 0);
    CloseDB(db);

    StringSet *added_keys = StringSetNew();
    size_t iter_idx = 0;
    pid_t tid = gettid();
    while (usage < usage_pct)
    {
        success = OpenDB(&db, db_id);
        assert(success);

        /* Derive a key for our new item from an index, the thread ID and the
         * original item's key so that we don't mess with the original item and
         * we can clean after ourselves (LMDB has a limit of 511 bytes for the
         * key). */
        /* Add 1000 items in each iteration so that each iteration makes some
         * difference in the DB usage and we don't have to do so many
         * iterations. */
        for (size_t i = 0; i < 1000; i++)
        {
            char *key = StringFormat("test_%ju_%.200s_%zd_%zd", (uintmax_t) tid, item->key, iter_idx, i);
            success = WriteDB(db, key, item->val, item->val_size);
            assert(success);
            StringSetAdd(added_keys, key); /* takes ownership of key */
        }
        iter_idx++;
        usage = GetDBUsagePercentage(db);
        assert(usage != -1);    /* didn't happen at first, should always work */

        CloseDB(db);
    }

    SeqDestroy(items);
    DBFilament *ret = xmalloc(sizeof(DBFilament));
    ret->db_id = db_id;
    ret->items = added_keys;
    return ret;
}

void RemoveFilament(DBFilament *filament)
{
    if (filament == NULL)
    {
        /* Nothing to do. */
        return;
    }
    if (StringSetSize(filament->items) == 0)
    {
        StringSetDestroy(filament->items);
        free(filament);
    }

    DBHandle *db;
    bool success = OpenDB(&db, filament->db_id);
    if (!success)
    {
        Log(LOG_LEVEL_ERR, "Failed to open DB with ID %d", filament->db_id);
        return;
    }

    SetIterator iter = StringSetIteratorInit(filament->items);
    char *key;
    while ((key = StringSetIteratorNext(&iter)) != NULL)
    {
        success = DeleteDB(db, key);
        assert(success);
    }

    StringSetDestroy(filament->items);
    free(filament);
    CloseDB(db);
}
#endif /* not __ANDROID__ */
