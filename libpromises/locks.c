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

#include <locks.h>
#include <global_mutex.h>
#include <mutex.h>
#include <string_lib.h>
#include <files_interfaces.h>
#include <files_lib.h>
#include <cleanup.h>
#include <policy.h>
#include <hash.h>
#include <rb-tree.h>
#include <files_names.h>
#include <rlist.h>
#include <process_lib.h>
#include <fncall.h>
#include <eval_context.h>
#include <misc_lib.h>
#include <known_dirs.h>
#include <sysinfo.h>
#include <openssl/evp.h>

#ifdef LMDB
// Be careful if you want to change this,
// it must match mdb_env_get_maxkeysize(env)
#define LMDB_MAX_KEY_SIZE 511
#endif

#define CFLOGSIZE 1048576       /* Size of lock-log before rotation */
#define CF_MAXLOCKNUM 8192

#define CF_CRITIAL_SECTION "CF_CRITICAL_SECTION"

#define LOG_LOCK_ENTRY(__lock, __lock_sum, __lock_data)         \
    log_lock("Entering", __FUNCTION__, __lock, __lock_sum, __lock_data)
#define LOG_LOCK_EXIT(__lock, __lock_sum, __lock_data)          \
    log_lock("Exiting", __FUNCTION__, __lock, __lock_sum, __lock_data)
#define LOG_LOCK_OP(__lock, __lock_sum, __lock_data)            \
    log_lock("Performing", __FUNCTION__, __lock, __lock_sum, __lock_data)

/**
 * Map the locks DB usage percentage to the lock horizon interval (how old locks
 * we want to keep).
 */
#define N_LOCK_HORIZON_USAGE_INTERVALS 4 /* 0-25, 26-50,... */
static const time_t LOCK_HORIZON_USAGE_INTERVALS[N_LOCK_HORIZON_USAGE_INTERVALS] = {
    0,                    /* plenty of space, no cleanup needed (0 is a special
                           * value) */
    4 * SECONDS_PER_WEEK, /* used to be the fixed value */
    2 * SECONDS_PER_WEEK, /* a bit more aggressive, but still reasonable  */
    SECONDS_PER_WEEK,     /* as far as we want to go to avoid making long locks
                           * unreliable and practically non-functional */
};

typedef struct CfLockStack_ {
    char lock[CF_BUFSIZE];
    char last[CF_BUFSIZE];
    struct CfLockStack_ *previous;
} CfLockStack;

static CfLockStack *LOCK_STACK = NULL;

static void PushLock(char *lock, char *last)
{
    CfLockStack *new_lock = xmalloc(sizeof(CfLockStack));
    strlcpy(new_lock->lock, lock, CF_BUFSIZE);
    strlcpy(new_lock->last, last, CF_BUFSIZE);

    new_lock->previous = LOCK_STACK;
    LOCK_STACK = new_lock;
}

static CfLockStack *PopLock()
{
    if (!LOCK_STACK)
    {
        return NULL;
    }
    CfLockStack *lock = LOCK_STACK;
    LOCK_STACK = lock->previous;
    return lock;
}

static void CopyLockDatabaseAtomically(const char *from, const char *to,
                                       const char *from_pretty_name,
                                       const char *to_pretty_name);

CF_DB *OpenLock()
{
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_locks))
    {
        return NULL;
    }

    return dbp;
}

void CloseLock(CF_DB *dbp)
{
    if (dbp)
    {
        CloseDB(dbp);
    }
}

static pthread_once_t lock_cleanup_once = PTHREAD_ONCE_INIT; /* GLOBAL_X */

#ifdef LMDB
static inline void log_lock(const char *op,
                            const char *function,
                            const char *lock,
                            const char *lock_sum,
                            const LockData *lock_data)
{
    /* Check log level first to save cycles when not in debug mode. */
    if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
    {
        if (lock_data)
        {
            LogDebug(LOG_MOD_LOCKS, "%s lock operation in '%s()': "
                     "lock_id = '%s', lock_checksum = '%s', "
                     "lock.pid = '%d', lock.time = '%d', "
                     "lock.process_start_time = '%d'",
                     op, function, lock, lock_sum,
                     (int)lock_data->pid, (int)lock_data->time,
                     (int)lock_data->process_start_time);
        }
        else
        {
            LogDebug(LOG_MOD_LOCKS, "%s lock operation in '%s()'. "
                     "lock_id = '%s', lock_checksum = '%s'",
                     op, function, lock, lock_sum);
        }
    }
}

static void HashLockKeyIfNecessary(const char *const istring, char *const ohash)
{
    assert(strlen("CF_CRITICAL_SECTION") < LMDB_MAX_KEY_SIZE);
    assert(strlen("lock.track_license_bundle.track_license") < LMDB_MAX_KEY_SIZE);
    StringCopyTruncateAndHashIfNecessary(istring, ohash, LMDB_MAX_KEY_SIZE);
}
#endif

static bool WriteLockData(CF_DB *dbp, const char *lock_id, LockData *lock_data)
{
    bool ret;

#ifdef LMDB
    unsigned char digest2[LMDB_MAX_KEY_SIZE];

    HashLockKeyIfNecessary(lock_id, digest2);

    LOG_LOCK_ENTRY(lock_id, digest2, lock_data);
    ret = WriteDB(dbp, digest2, lock_data, sizeof(LockData));
    LOG_LOCK_EXIT(lock_id, digest2, lock_data);
#else
    ret = WriteDB(dbp, lock_id, lock_data, sizeof(LockData));
#endif

    return ret;
}

static bool WriteLockDataCurrent(CF_DB *dbp, const char *lock_id)
{
    LockData lock_data = { 0 };
    lock_data.pid = getpid();
    lock_data.time = time(NULL);
    lock_data.process_start_time = GetProcessStartTime(getpid());

    return WriteLockData(dbp, lock_id, &lock_data);
}

static int WriteLock(const char *name)
{
    CF_DB *dbp = OpenLock();

    if (dbp == NULL)
    {
        return -1;
    }

    ThreadLock(cft_lock);
    WriteLockDataCurrent(dbp, name);

    CloseLock(dbp);
    ThreadUnlock(cft_lock);

    return 0;
}

static time_t FindLockTime(const char *name)
{
    bool ret;
    CF_DB *dbp = OpenLock();
    if (dbp == NULL)
    {
        return -1;
    }

    LockData entry = { 0 };
    entry.process_start_time = PROCESS_START_TIME_UNKNOWN;

#ifdef LMDB
    unsigned char ohash[LMDB_MAX_KEY_SIZE];
    HashLockKeyIfNecessary(name, ohash);

    LOG_LOCK_ENTRY(name, ohash, &entry);
    ret = ReadDB(dbp, ohash, &entry, sizeof(entry));
    LOG_LOCK_EXIT(name, ohash, &entry);
#else
    ret = ReadDB(dbp, name, &entry, sizeof(entry));
#endif

    if (ret)
    {
        CloseLock(dbp);
        return entry.time;
    }
    else
    {
        CloseLock(dbp);
        return -1;
    }
}

static void RemoveDates(char *s)
{
    int i, a = 0, b = 0, c = 0, d = 0;
    char *dayp = NULL, *monthp = NULL, *sp;
    char *days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Canonifies or blanks our times/dates for locks where there would be an explosion of state

    /* Has s always been generated by something that uses two-digit hh:mm:ss?
     * Are there any time-zones whose abbreviations are shorter than three
     * letters?
     */
    if (strlen(s) < sizeof("Fri Oct 1 15:15:23 EST 2010") - 1)
    {
        // Probably not a full date
        return;
    }

    for (i = 0; i < 7; i++)
    {
        if ((dayp = strstr(s, days[i])))
        {
            *dayp = 'D';
            *(dayp + 1) = 'A';
            *(dayp + 2) = 'Y';
            break;
        }
    }

    for (i = 0; i < 12; i++)
    {
        if ((monthp = strstr(s, months[i])))
        {
            *monthp = 'M';
            *(monthp + 1) = 'O';
            *(monthp + 2) = 'N';
            break;
        }
    }

    if (dayp && monthp)         // looks like a full date
    {
        sscanf(monthp + 4, "%d %d:%d:%d", &a, &b, &c, &d);

        if (a * b * c * d == 0)
        {
            // Probably not a date
            return;
        }

        for (sp = monthp + 4; *sp != '\0'; sp++)
        {
            if (sp > monthp + 15)
            {
                break;
            }

            if (isdigit((int)*sp))
            {
                *sp = 't';
            }
        }
    }
}

static int RemoveLock(const char *name)
{
    CF_DB *dbp = OpenLock();
    if (dbp == NULL)
    {
        return -1;
    }

    ThreadLock(cft_lock);
#ifdef LMDB
    unsigned char digest2[LMDB_MAX_KEY_SIZE];

    HashLockKeyIfNecessary(name, digest2);

    LOG_LOCK_ENTRY(name, digest2, NULL);
    DeleteDB(dbp, digest2);
    LOG_LOCK_EXIT(name, digest2, NULL);
#else
    DeleteDB(dbp, name);
#endif
    ThreadUnlock(cft_lock);

    CloseLock(dbp);
    return 0;
}

static bool NoOrObsoleteLock(LockData *entry, ARG_UNUSED size_t entry_size, size_t *max_old)
{
    assert((entry == NULL) || (entry_size == sizeof(LockData)));

    if (entry == NULL)
    {
        return true;
    }

    time_t now = time(NULL);
    if ((now - entry->time) <= (time_t) *max_old)
    {
        Log(LOG_LEVEL_DEBUG, "Giving time to process '%d' (holding lock for %ld s)", entry->pid, (now - entry->time));
    }
    return ((now - entry->time) > (time_t) *max_old);
}

void WaitForCriticalSection(const char *section_id)
{
    ThreadLock(cft_lock);

    CF_DB *dbp = OpenLock();
    if (dbp == NULL)
    {
        Log(LOG_LEVEL_CRIT, "Failed to open lock database when waiting for critical section");
        ThreadUnlock(cft_lock);
        return;
    }

    time_t started = time(NULL);
    LockData entry = { 0 };
    entry.pid = getpid();
    entry.process_start_time = PROCESS_START_TIME_UNKNOWN;

#ifdef LMDB
    unsigned char ohash[LMDB_MAX_KEY_SIZE];
    HashLockKeyIfNecessary(section_id, ohash);
    Log(LOG_LEVEL_DEBUG, "Hashed critical section lock '%s' to '%s'", section_id, ohash);
    section_id = ohash;
#endif

    /* If another agent has been waiting more than a minute, it means there
       is likely crash detritus to clear up... After a minute we take our
       chances ... */
    size_t max_old = 60;

    Log(LOG_LEVEL_DEBUG, "Acquiring critical section lock '%s'", section_id);
    bool got_lock = false;
    while (!got_lock && ((time(NULL) - started) <= (time_t) max_old))
    {
        entry.time = time(NULL);
        got_lock = OverwriteDB(dbp, section_id, &entry, sizeof(entry),
                               (OverwriteCondition) NoOrObsoleteLock, &max_old);
        if (!got_lock)
        {
            Log(LOG_LEVEL_DEBUG, "Waiting for critical section lock '%s'", section_id);
            sleep(1);
        }
    }

    /* If we still haven't gotten the lock, let's try the biggest hammer we
     * have. */
    if (!got_lock)
    {
        Log(LOG_LEVEL_NOTICE, "Failed to wait for critical section lock '%s', force-writing new lock", section_id);
        if (!WriteDB(dbp, section_id, &entry, sizeof(entry)))
        {
            Log(LOG_LEVEL_CRIT, "Failed to force-write critical section lock '%s'", section_id);
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Acquired critical section lock '%s'", section_id);
    }

    CloseLock(dbp);
    ThreadUnlock(cft_lock);
}

void ReleaseCriticalSection(const char *section_id)
{
    Log(LOG_LEVEL_DEBUG, "Releasing critical section lock '%s'", section_id);
    if (RemoveLock(section_id) == 0)
    {
        Log(LOG_LEVEL_DEBUG, "Released critical section lock '%s'", section_id);
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Failed to release critical section lock '%s'", section_id);
    }
}

static time_t FindLock(char *last)
{
    time_t mtime;

    if ((mtime = FindLockTime(last)) == -1)
    {
        /* Do this to prevent deadlock loops from surviving if IfElapsed > T_sched */

        if (WriteLock(last) == -1)
        {
            Log(LOG_LEVEL_ERR, "Unable to lock %s", last);
            return 0;
        }

        return 0;
    }
    else
    {
        return mtime;
    }
}

static void LocksCleanup(void)
{
    CfLockStack *lock;
    while ((lock = PopLock()) != NULL)
    {
        CfLock best_guess = {
            .lock = xstrdup(lock->lock),
            .last = xstrdup(lock->last),
        };
        YieldCurrentLock(best_guess);
        free(lock);
    }
}

static void RegisterLockCleanup(void)
{
    RegisterCleanupFunction(&LocksCleanup);
}

/**
 * Return a type template for the promise for lock-type
 * identification. WARNING: instead of truncation, it does not include any
 * parts (i.e. constraints or promise type) that don't fit.
 *
 * @WARNING currently it only prints up to the 5 first constraints (WHY?)
 */
static void PromiseTypeString(char *dst, size_t dst_size, const Promise *pp)
{
    const char *sp = PromiseGetPromiseType(pp);
    size_t sp_len  = strlen(sp);

    dst[0]         = '\0';
    size_t dst_len = 0;

    if (sp_len + 1 < dst_size)
    {
        strcpy(dst, sp);
        strcat(dst, ".");
        dst_len += sp_len + 1;
    }

    if (pp->conlist != NULL)
    {
        /* Number of constraints (attributes) of that promise. */
        size_t cons_num = SeqLength(pp->conlist);
        for (size_t i = 0;  (i < 5) && (i < cons_num);  i++)
        {
            Constraint *cp  = SeqAt(pp->conlist, i);
            const char *con = cp->lval;                    /* the constraint */

            /* Exception for args (promise type commands),
               by symmetry, for locking. */
            if (strcmp(con, "args") == 0)
            {
                continue;
            }

            /* Exception for arglist (promise type commands),
               by symmetry, for locking. */
            if (strcmp(con, "arglist") == 0)
            {
                continue;
            }

            size_t con_len = strlen(con);
            if (dst_len + con_len + 1 < dst_size)
            {
                strcat(dst, con);
                strcat(dst, ".");
                dst_len += con_len + 1;
            }
        }
    }
}

/**
 * A helper best-effort function to prevent us from killing non CFEngine
 * processes with matching PID-start_time combinations **when/where it's easy to
 * check**.
 */
static bool IsCfengineLockHolder(pid_t pid)
{
    char procfile[PATH_MAX];
    snprintf(procfile, PATH_MAX, "/proc/%ju/comm", (uintmax_t) pid);
    int f = open(procfile, O_RDONLY);
    /* On platforms where /proc doesn't exist, we would have to do a more
       complicated check probably not worth it in this helper best-effort
       function. */
    if (f == -1)
    {
        /* assume true where we cannot check */
        return true;
    }

    /* more than any possible CFEngine lock holder's name */
    char command[32] = {0};
    ssize_t n_read = FullRead(f, command, sizeof(command));
    close(f);
    if ((n_read <= 1) || (n_read == sizeof(command)))
    {
        Log(LOG_LEVEL_VERBOSE, "Failed to get command for process %ju", (uintmax_t) pid);
        /* assume true where we cannot check */
        return true;
    }
    if (command[n_read - 1] == '\n')
    {
        command[n_read - 1] = '\0';
    }

    /* potential CFEngine lock holders (others like cf-net, cf-key,... are not
     * supposed/expected to be lock holders) */
    const char *const cfengine_procs[] = {
        "cf-promises",
        "lt-cf-agent",  /* when running from a build with 'libtool --mode=execute' */
        "cf-agent",
        "cf-execd",
        "cf-serverd",
        "cf-monitord",
        "cf-hub",
        NULL
    };
    for (size_t i = 0; cfengine_procs[i] != NULL; i++)
    {
        if (StringEqual(cfengine_procs[i], command))
        {
            return true;
        }
    }
    Log(LOG_LEVEL_DEBUG, "'%s' not considered a CFEngine process", command);
    return false;
}

static bool KillLockHolder(const char *lock)
{
    bool ret;
    CF_DB *dbp = OpenLock();
    if (dbp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open locks database");
        return false;
    }

    LockData lock_data = { 0 };
    lock_data.process_start_time = PROCESS_START_TIME_UNKNOWN;

#ifdef LMDB
    unsigned char ohash[LMDB_MAX_KEY_SIZE];
    HashLockKeyIfNecessary(lock, ohash);

    LOG_LOCK_ENTRY(lock, ohash, &lock_data);
    ret = ReadDB(dbp, ohash, &lock_data, sizeof(lock_data));
    LOG_LOCK_EXIT(lock, ohash, &lock_data);
#else
    ret = ReadDB(dbp, lock, &lock_data, sizeof(lock_data));
#endif

    if (!ret)
    {
        /* No lock found */
        CloseLock(dbp);
        return true;
    }

    CloseLock(dbp);

    if (!IsCfengineLockHolder(lock_data.pid)) {
        Log(LOG_LEVEL_VERBOSE,
            "Lock holder with PID %ju was replaced by a non CFEngine process, ignoring request to kill it",
            (uintmax_t) lock_data.pid);
        return true;
    }

    if (GracefulTerminate(lock_data.pid, lock_data.process_start_time))
    {
        Log(LOG_LEVEL_INFO,
            "Process with PID %jd successfully killed",
            (intmax_t) lock_data.pid);
        return true;
    }
    else
    {
        if (errno == ESRCH)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Process with PID %jd has already been killed",
                (intmax_t) lock_data.pid);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_ERR,
                "Failed to kill process with PID: %jd (kill: %s)",
                (intmax_t) lock_data.pid, GetErrorStr());
            return false;
        }
    }
}

static void RvalDigestUpdate(EVP_MD_CTX *context, Rlist *rp)
{
    assert(context != NULL);
    assert(rp != NULL);

    switch (rp->val.type)
    {
    case RVAL_TYPE_SCALAR:
        EVP_DigestUpdate(context, RlistScalarValue(rp),
                         strlen(RlistScalarValue(rp)));
        break;

    case RVAL_TYPE_FNCALL:
        // TODO: This should be recursive and not just hash the function name
        EVP_DigestUpdate(context, RlistFnCallValue(rp)->name,
                         strlen(RlistFnCallValue(rp)->name));
        break;

    default:
        ProgrammingError("Unhandled case in switch");
        break;
    }
}

void PromiseRuntimeHash(const Promise *pp, const char *salt,
                        unsigned char digest[EVP_MAX_MD_SIZE + 1],
                        HashMethod type)
{
    static const char PACK_UPIFELAPSED_SALT[] = "packageuplist";

    int md_len;
    const EVP_MD *md = NULL;
    Rlist *rp;
    FnCall *fp;

    char *noRvalHash[] = { "mtime", "atime", "ctime", "stime_range", "ttime_range", "log_string", "template_data", NULL };
    int doHash;

    md = HashDigestFromId(type);
    if (md == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "Could not determine function for file hashing (type=%d)",
            (int) type);
        return;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (context == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not allocate openssl hash context");
        return;
    }

    EVP_DigestInit(context, md);

// multiple packages (promisers) may share same package_list_update_ifelapsed lock
    if ( (!salt) || strcmp(salt, PACK_UPIFELAPSED_SALT) )
    {
        EVP_DigestUpdate(context, pp->promiser, strlen(pp->promiser));
    }

    if (pp->comment)
    {
        EVP_DigestUpdate(context, pp->comment, strlen(pp->comment));
    }

    if (pp->parent_section && pp->parent_section->parent_bundle)
    {
        if (pp->parent_section->parent_bundle->ns)
        {
            EVP_DigestUpdate(context,
                             pp->parent_section->parent_bundle->ns,
                             strlen(pp->parent_section->parent_bundle->ns));
        }

        if (pp->parent_section->parent_bundle->name)
        {
            EVP_DigestUpdate(context,
                             pp->parent_section->parent_bundle->name,
                             strlen(pp->parent_section->parent_bundle->name));
        }
    }

    // Unused: pp start, end, and line attributes (describing source position).

    if (salt)
    {
        EVP_DigestUpdate(context, salt, strlen(salt));
    }

    if (pp->conlist)
    {
        for (size_t i = 0; i < SeqLength(pp->conlist); i++)
        {
            Constraint *cp = SeqAt(pp->conlist, i);

            EVP_DigestUpdate(context, cp->lval, strlen(cp->lval));

            // don't hash rvals that change (e.g. times)
            doHash = true;

            for (int j = 0; noRvalHash[j] != NULL; j++)
            {
                if (strcmp(cp->lval, noRvalHash[j]) == 0)
                {
                    doHash = false;
                    break;
                }
            }

            if (!doHash)
            {
                continue;
            }

            switch (cp->rval.type)
            {
            case RVAL_TYPE_SCALAR:
                EVP_DigestUpdate(context, cp->rval.item, strlen(cp->rval.item));
                break;

            case RVAL_TYPE_LIST:
                for (rp = cp->rval.item; rp != NULL; rp = rp->next)
                {
                    RvalDigestUpdate(context, rp);
                }
                break;

            case RVAL_TYPE_CONTAINER:
                {
                    const JsonElement *rval_json = RvalContainerValue(cp->rval);
                    Writer *writer = StringWriter();
                    JsonWriteCompact(writer, rval_json); /* sorts elements and produces canonical form */
                    EVP_DigestUpdate(context, StringWriterData(writer), StringWriterLength(writer));
                    WriterClose(writer);
                    break;
                }

            case RVAL_TYPE_FNCALL:

                /* Body or bundle */

                fp = (FnCall *) cp->rval.item;

                EVP_DigestUpdate(context, fp->name, strlen(fp->name));

                for (rp = fp->args; rp != NULL; rp = rp->next)
                {
                    RvalDigestUpdate(context, rp);
                }
                break;

            default:
                break;
            }
        }
    }

    EVP_DigestFinal(context, digest, &md_len);
    EVP_MD_CTX_free(context);

/* Digest length stored in md_len */
}

static CfLock CfLockNew(const char *last, const char *lock, bool is_dummy)
{
    return (CfLock) {
        .last = last ? xstrdup(last) : NULL,
        .lock = lock ? xstrdup(lock) : NULL,
        .is_dummy = is_dummy
    };
}

static CfLock CfLockNull(void)
{
    return (CfLock) {
        .last = NULL,
        .lock = NULL,
        .is_dummy = false
    };
}

CfLock AcquireLock(EvalContext *ctx, const char *operand, const char *host,
                   time_t now, int ifelapsed, int expireafter, const Promise *pp,
                   bool ignoreProcesses)
{
    if (now == 0)
    {
        return CfLockNull();
    }

    char str_digest[CF_HOSTKEY_STRING_SIZE];
    {
        unsigned char digest[EVP_MAX_MD_SIZE + 1];
        PromiseRuntimeHash(pp, operand, digest, CF_DEFAULT_DIGEST);
        HashPrintSafe(str_digest, sizeof(str_digest), digest,
                      CF_DEFAULT_DIGEST, true);
    }

    if (EvalContextPromiseLockCacheContains(ctx, str_digest))
    {
//        Log(LOG_LEVEL_DEBUG, "This promise has already been verified");
        return CfLockNull();
    }

    EvalContextPromiseLockCachePut(ctx, str_digest);

    // Finally if we're supposed to ignore locks ... do the remaining stuff
    if (EvalContextIsIgnoringLocks(ctx))
    {
        return CfLockNew(NULL, "dummy", true);
    }

    char cc_operator[CF_MAXVARSIZE];
    {
        char promise[CF_MAXVARSIZE - CF_BUFFERMARGIN];
        PromiseTypeString(promise, sizeof(promise), pp);
        snprintf(cc_operator, sizeof(cc_operator), "%s-%s", promise, host);
    }

    char cc_operand[CF_BUFSIZE];
    strlcpy(cc_operand, operand, CF_BUFSIZE);
    CanonifyNameInPlace(cc_operand);
    RemoveDates(cc_operand);


    Log(LOG_LEVEL_DEBUG,
        "AcquireLock(%s,%s), ExpireAfter = %d, IfElapsed = %d",
        cc_operator, cc_operand, expireafter, ifelapsed);

    int sum = 0;
    for (int i = 0; cc_operator[i] != '\0'; i++)
    {
        sum = (CF_MACROALPHABET * sum + cc_operator[i]) % CF_MAXLOCKNUM;
    }

    for (int i = 0; cc_operand[i] != '\0'; i++)
    {
        sum = (CF_MACROALPHABET * sum + cc_operand[i]) % CF_MAXLOCKNUM;
    }

    const char *bundle_name = PromiseGetBundle(pp)->name;

    char cflock[CF_BUFSIZE] = "";
    snprintf(cflock, CF_BUFSIZE, "lock.%.100s.%s.%.100s_%d_%s",
             bundle_name, cc_operator, cc_operand, sum, str_digest);

    char cflast[CF_BUFSIZE] = "";
    snprintf(cflast, CF_BUFSIZE, "last.%.100s.%s.%.100s_%d_%s",
             bundle_name, cc_operator, cc_operand, sum, str_digest);

    Log(LOG_LEVEL_DEBUG, "Locking bundle '%s' with lock '%s'",
        bundle_name, cflock);

    // Now see if we can get exclusivity to edit the locks
    WaitForCriticalSection(CF_CRITIAL_SECTION);

    // Look for non-existent (old) processes
    time_t lastcompleted = FindLock(cflast);
    time_t elapsedtime = (time_t) (now - lastcompleted) / 60;

    // For promises/locks with ifelapsed == 0, skip all detection logic of
    // previously acquired locks, whether in this agent or a parallel one.
    if (ifelapsed != 0)
    {
        if (elapsedtime < 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Another cf-agent seems to have done this since I started "
                "(elapsed=%jd)",
                (intmax_t) elapsedtime);
            ReleaseCriticalSection(CF_CRITIAL_SECTION);
            return CfLockNull();
        }

        if (elapsedtime < ifelapsed)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Nothing promised here [%.40s] (%jd/%u minutes elapsed)",
                cflast, (intmax_t) elapsedtime, ifelapsed);
            ReleaseCriticalSection(CF_CRITIAL_SECTION);
            return CfLockNull();
        }
    }

    // Look for existing (current) processes
    lastcompleted = FindLock(cflock);
    if (!ignoreProcesses)
    {
        elapsedtime = (time_t) (now - lastcompleted) / 60;

        if (lastcompleted != 0)
        {
            if (elapsedtime >= expireafter)
            {
                Log(LOG_LEVEL_INFO,
                    "Lock expired after %jd/%u minutes: %s",
                    (intmax_t) elapsedtime, expireafter, cflock);

                if (KillLockHolder(cflock))
                {
                    Log(LOG_LEVEL_VERBOSE,
                        "Lock successfully expired: %s", cflock);
                    unlink(cflock);
                }
                else
                {
                    Log(LOG_LEVEL_ERR, "Failed to expire lock: %s", cflock);
                }
            }
            else
            {
                ReleaseCriticalSection(CF_CRITIAL_SECTION);
                Log(LOG_LEVEL_VERBOSE,
                    "Couldn't obtain lock for %s (already running!)",
                    cflock);
                return CfLockNull();
            }
        }

        int ret = WriteLock(cflock);
        if (ret != -1)
        {
            /* Register a cleanup handler *after* having opened the DB, so that
             * CloseAllDB() atexit() handler is registered in advance, and it
             * is called after removing this lock.

             * There is a small race condition here that we'll leave a stale
             * lock if we exit before the following line. */
            pthread_once(&lock_cleanup_once, &RegisterLockCleanup);
        }
    }

    ReleaseCriticalSection(CF_CRITIAL_SECTION);

    // Keep this as a global for signal handling
    PushLock(cflock, cflast);

    return CfLockNew(cflast, cflock, false);
}

void YieldCurrentLock(CfLock lock)
{
    if (lock.is_dummy)
    {
        free(lock.lock);        /* allocated in AquireLock as a special case */
        return;
    }

    if (lock.lock == (char *) CF_UNDEFINED)
    {
        return;
    }

    Log(LOG_LEVEL_DEBUG, "Yielding lock '%s'", lock.lock);

    if (RemoveLock(lock.lock) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Unable to remove lock %s", lock.lock);
        free(lock.last);
        free(lock.lock);
        return;
    }

    if (WriteLock(lock.last) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to create '%s'. (create: %s)",
            lock.last, GetErrorStr());
        free(lock.last);
        free(lock.lock);
        return;
    }

    /* This lock has ben yield'ed, don't try to yield it again in case process
     * is terminated abnormally.
     */
    CfLockStack *stack = LOCK_STACK;
    CfLockStack *last = NULL;
    while (stack)
    {
        if ((strcmp(stack->lock, lock.lock) == 0)
         && (strcmp(stack->last, lock.last) == 0))
        {
            CfLockStack *delete_me = stack;
            stack = stack->previous;
            if (!last)
            {
                assert(delete_me == LOCK_STACK);
                LOCK_STACK = stack;
            } else {
                last->previous = stack;
            }
            free(delete_me);
            continue;
        }
        last = stack;
        stack = stack->previous;
    }

    free(lock.last);
    free(lock.lock);
}

void YieldCurrentLockAndRemoveFromCache(EvalContext *ctx, CfLock lock,
                                        const char *operand, const Promise *pp)
{
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    PromiseRuntimeHash(pp, operand, digest, CF_DEFAULT_DIGEST);
    char str_digest[CF_HOSTKEY_STRING_SIZE];
    HashPrintSafe(str_digest, sizeof(str_digest), digest,
                  CF_DEFAULT_DIGEST, true);

    YieldCurrentLock(lock);
    EvalContextPromiseLockCacheRemove(ctx, str_digest);
}


void GetLockName(char *lockname, const char *locktype,
                 const char *base, const Rlist *params)
{
    int max_sample, count = 0;

    for (const Rlist *rp = params; rp != NULL; rp = rp->next)
    {
        count++;
    }

    if (count)
    {
        max_sample = CF_BUFSIZE / (2 * count);
    }
    else
    {
        max_sample = 0;
    }

    strlcpy(lockname, locktype, CF_BUFSIZE / 10);
    strlcat(lockname, "_", CF_BUFSIZE / 10);
    strlcat(lockname, base, CF_BUFSIZE / 10);
    strlcat(lockname, "_", CF_BUFSIZE / 10);

    for (const Rlist *rp = params; rp != NULL; rp = rp->next)
    {
        switch (rp->val.type)
        {
        case RVAL_TYPE_SCALAR:
            strncat(lockname, RlistScalarValue(rp), max_sample);
            break;

        case RVAL_TYPE_FNCALL:
            strncat(lockname, RlistFnCallValue(rp)->name, max_sample);
            break;

        default:
            ProgrammingError("Unhandled case in switch %d", rp->val.type);
            break;
        }
    }
}

void RestoreLockDatabase(void)
{
    // We don't do any locking here (since we can't trust the database), but
    // worst case someone else will just copy the same file to the same
    // location.
    char *db_path = DBIdToPath(dbid_locks);
    char *db_path_backup;
    xasprintf(&db_path_backup, "%s.backup", db_path);

    CopyLockDatabaseAtomically(db_path_backup, db_path, "lock database backup",
                               "lock database");

    free(db_path);
    free(db_path_backup);
}

static void CopyLockDatabaseAtomically(const char *from, const char *to,
                                       const char *from_pretty_name,
                                       const char *to_pretty_name)
{
    char *tmp_file_name;
    xasprintf(&tmp_file_name, "%s.tmp", to);

    int from_fd = open(from, O_RDONLY | O_BINARY);
    if (from_fd < 0)
    {
        Log(LOG_LEVEL_WARNING,
            "Could not open '%s' (open: %s)",
            from_pretty_name, GetErrorStr());
        goto cleanup;
    }

    int to_fd = open(tmp_file_name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
    if (to_fd < 0)
    {
        Log(LOG_LEVEL_WARNING,
            "Could not open '%s' temporary file (open: %s)",
            to_pretty_name, GetErrorStr());
        goto cleanup;
    }

    size_t total_bytes_written;
    bool   last_write_was_hole;
    bool ok1 = FileSparseCopy(from_fd, from_pretty_name,
                              to_fd,   to_pretty_name, DEV_BSIZE,
                              &total_bytes_written, &last_write_was_hole);

    /* Make sure changes are persistent on disk, so database cannot get
     * corrupted at system crash. */
    bool do_sync = true;
    bool ok2 = FileSparseClose(to_fd, to_pretty_name, do_sync,
                               total_bytes_written, last_write_was_hole);

    if (!ok1 || !ok2)
    {
        Log(LOG_LEVEL_WARNING,
            "Error while moving database from '%s' to '%s'",
            from_pretty_name, to_pretty_name);
    }

    if (rename(tmp_file_name, to) != 0)
    {
        Log(LOG_LEVEL_WARNING,
            "Could not move '%s' into place (rename: %s)",
            to_pretty_name, GetErrorStr());
    }

  cleanup:
    if (from_fd != -1)
    {
        close(from_fd);
    }
    unlink(tmp_file_name);
    free(tmp_file_name);
}

void BackupLockDatabase(void)
{
    WaitForCriticalSection(CF_CRITIAL_SECTION);

    char *db_path = DBIdToPath(dbid_locks);
    char *db_path_backup;
    xasprintf(&db_path_backup, "%s.backup", db_path);

    CopyLockDatabaseAtomically(db_path, db_path_backup, "lock database",
                               "lock database backup");

    free(db_path);
    free(db_path_backup);

    ReleaseCriticalSection(CF_CRITIAL_SECTION);
}

void PurgeLocks(void)
{
    DBHandle *db = OpenLock();
    if (db == NULL)
    {
        return;
    }

    time_t now = time(NULL);

    int usage_pct = GetDBUsagePercentage(db);
    if (usage_pct == -1)
    {
        /* error already logged */
        /* no usage info, assume 50% */
        usage_pct = 50;
    }

    unsigned short interval_idx = MIN(usage_pct / (100 / N_LOCK_HORIZON_USAGE_INTERVALS),
                                      N_LOCK_HORIZON_USAGE_INTERVALS - 1);
    const time_t lock_horizon_interval = LOCK_HORIZON_USAGE_INTERVALS[interval_idx];
    if (lock_horizon_interval == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "No lock purging needed (lock DB usage: %d %%)", usage_pct);
        CloseLock(db);
        return;
    }
    const time_t purge_horizon = now - lock_horizon_interval;

    LockData lock_horizon;
    memset(&lock_horizon, 0, sizeof(lock_horizon));
    if (ReadDB(db, "lock_horizon", &lock_horizon, sizeof(lock_horizon)))
    {
        if (lock_horizon.time > purge_horizon)
        {
            Log(LOG_LEVEL_VERBOSE, "No lock purging scheduled");
            CloseLock(db);
            return;
        }
    }

    Log(LOG_LEVEL_VERBOSE,
        "Looking for stale locks (older than %jd seconds) to purge",
        (intmax_t) lock_horizon_interval);

    DBCursor *cursor;
    if (!NewDBCursor(db, &cursor))
    {
        char *db_path = DBIdToPath(dbid_locks);
        Log(LOG_LEVEL_ERR, "Unable to get cursor for locks database '%s'",
            db_path);
        free(db_path);
        CloseLock(db);
        return;
    }

    char *key;
    int ksize, vsize;
    LockData *entry = NULL;
    while (NextDB(cursor, &key, &ksize, (void **)&entry, &vsize))
    {
#ifdef LMDB
        LOG_LOCK_OP("<unknown>", key, entry);
#endif
        if (StringStartsWith(key, "last.internal_bundle.track_license.handle"))
        {
            continue;
        }

        if (entry->time < purge_horizon)
        {
            Log(LOG_LEVEL_VERBOSE, "Purging lock (%jd s elapsed): %s",
                (intmax_t) (now - entry->time), key);
            DBCursorDeleteEntry(cursor);
        }
    }
    DeleteDBCursor(cursor);

    Log(LOG_LEVEL_DEBUG, "Finished purging locks");

    lock_horizon.time = now;
    WriteDB(db, "lock_horizon", &lock_horizon, sizeof(lock_horizon));
    CloseLock(db);
}
