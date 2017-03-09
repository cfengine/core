/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
#include <mutex.h>
#include <string_lib.h>
#include <files_interfaces.h>
#include <files_lib.h>
#include <atexit.h>
#include <policy.h>
#include <files_hashes.h>
#include <rb-tree.h>
#include <files_names.h>
#include <rlist.h>
#include <process_lib.h>
#include <fncall.h>
#include <eval_context.h>
#include <misc_lib.h>
#include <known_dirs.h>
#include <sysinfo.h>

#define CFLOGSIZE 1048576       /* Size of lock-log before rotation */
#define CF_LOCKHORIZON ((time_t)(SECONDS_PER_WEEK * 4))

#define CF_CRITIAL_SECTION "CF_CRITICAL_SECTION"

#define LOG_LOCK_ENTRY(__lock, __lock_sum, __lock_data)         \
    log_lock("Entering", __FUNCTION__, __lock, __lock_sum, __lock_data)
#define LOG_LOCK_EXIT(__lock, __lock_sum, __lock_data)          \
    log_lock("Exiting", __FUNCTION__, __lock, __lock_sum, __lock_data)
#define LOG_LOCK_OP(__lock, __lock_sum, __lock_data)            \
    log_lock("Performing", __FUNCTION__, __lock, __lock_sum, __lock_data)

typedef struct CfLockStack_ {
    char lock[CF_BUFSIZE];
    char last[CF_BUFSIZE];
    struct CfLockStack_ *previous;
} CfLockStack;

static CfLockStack *LOCK_STACK = NULL;

static void PushLock(char *lock, char *last)
{
    CfLockStack *new_lock = malloc(sizeof(CfLockStack));
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

static void GenerateMd5Hash(const char *istring, char *ohash)
{
    if (!strcmp(istring, "CF_CRITICAL_SECTION"))
    {
        strcpy(ohash, istring);
        return;
    }

    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    HashString(istring, strlen(istring), digest, HASH_METHOD_MD5);

    const char lookup[]="0123456789abcdef";
    for (int i=0; i<16; i++)
    {
        ohash[i*2]   = lookup[digest[i] >> 4];
        ohash[i*2+1] = lookup[digest[i] & 0xf];
    }
    ohash[16*2] = '\0';

    if (!strncmp(istring, "lock.track_license_bundle.track_license", 39))
    {
        ohash[0] = 'X';
    }
}
#endif

static bool WriteLockData(CF_DB *dbp, const char *lock_id, LockData *lock_data)
{
    bool ret;

#ifdef LMDB
    unsigned char digest2[EVP_MAX_MD_SIZE*2 + 1];

    if (!strcmp(lock_id, "CF_CRITICAL_SECTION"))
    {
        strcpy(digest2, lock_id);
    }
    else
    {
        GenerateMd5Hash(lock_id, digest2);
    }

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
    LockData lock_data = {
        .pid = getpid(),
        .time = time(NULL),
        .process_start_time = GetProcessStartTime(getpid()),
    };

    return WriteLockData(dbp, lock_id, &lock_data);
}

time_t FindLockTime(const char *name)
{
    bool ret;
    CF_DB *dbp;
    LockData entry = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

    if ((dbp = OpenLock()) == NULL)
    {
        return -1;
    }

#ifdef LMDB
    unsigned char ohash[EVP_MAX_MD_SIZE*2 + 1];
    GenerateMd5Hash(name, ohash);

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
    char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Canonifies or blanks our times/dates for locks where there would be an explosion of state

    /* Has s always been generated by something that uses two-digit hh:mm:ss ?
     * Are there any time-zones whose abbreviations are shorter than three letters ?
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
    CF_DB *dbp;

    if ((dbp = OpenLock()) == NULL)
    {
        return -1;
    }

    ThreadLock(cft_lock);
#ifdef LMDB
    unsigned char digest2[EVP_MAX_MD_SIZE*2 + 1];

    if (!strcmp(name, "CF_CRITICAL_SECTION"))
    {
        strcpy(digest2, name);
    }
    else
    {
        GenerateMd5Hash(name, digest2);
    }

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

void WaitForCriticalSection(const char *section_id)
{
    time_t now = time(NULL), then = FindLockTime(section_id);

/* Another agent has been waiting more than a minute, it means there
   is likely crash detritus to clear up... After a minute we take our
   chances ... */

    while ((then != -1) && (now - then < 60))
    {
        sleep(1);
        now = time(NULL);
        then = FindLockTime(section_id);
    }

    WriteLock(section_id);
}

void ReleaseCriticalSection(const char *section_id)
{
    RemoveLock(section_id);
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

static pid_t FindLockPid(char *name)
{
    bool ret;
    CF_DB *dbp;
    LockData entry = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

    if ((dbp = OpenLock()) == NULL)
    {
        return -1;
    }

#ifdef LMDB
    unsigned char ohash[EVP_MAX_MD_SIZE*2 + 1];
    GenerateMd5Hash(name, ohash);

    LOG_LOCK_ENTRY(name, ohash, &entry);
    ret = ReadDB(dbp, ohash, &entry, sizeof(entry));
    LOG_LOCK_EXIT(name, ohash, &entry);
#else
    ret = ReadDB(dbp, name, &entry, sizeof(entry));
#endif

    if (ret)
    {
        CloseLock(dbp);
        return entry.pid;
    }
    else
    {
        CloseLock(dbp);
        return -1;
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
    RegisterAtExitFunction(&LocksCleanup);
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
    char *sp       = pp->parent_promise_type->name;
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

#ifdef __MINGW32__

static bool KillLockHolder(ARG_UNUSED const char *lock)
{
    Log(LOG_LEVEL_VERBOSE,
          "Process is not running - ignoring lock (Windows does not support graceful processes termination)");
    return true;
}

#else

static bool KillLockHolder(const char *lock)
{
    bool ret;
    CF_DB *dbp = OpenLock();
    if (dbp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open locks database");
        return false;
    }

    LockData lock_data = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

#ifdef LMDB
    unsigned char ohash[EVP_MAX_MD_SIZE*2 + 1];
    GenerateMd5Hash(lock, ohash);

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

    return GracefulTerminate(lock_data.pid, lock_data.process_start_time);
}

#endif

void PromiseRuntimeHash(const Promise *pp, const char *salt, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    static const char PACK_UPIFELAPSED_SALT[] = "packageuplist";

    EVP_MD_CTX context;
    int md_len;
    const EVP_MD *md = NULL;
    Rlist *rp;
    FnCall *fp;

    char *noRvalHash[] = { "mtime", "atime", "ctime", NULL };
    int doHash;

    md = EVP_get_digestbyname(HashNameFromId(type));

    EVP_DigestInit(&context, md);

// multiple packages (promisers) may share same package_list_update_ifelapsed lock
    if ( (!salt) || strcmp(salt, PACK_UPIFELAPSED_SALT) )
    {
        EVP_DigestUpdate(&context, pp->promiser, strlen(pp->promiser));
    }

    if (pp->comment)
    {
        EVP_DigestUpdate(&context, pp->comment, strlen(pp->comment));
    }

    if (pp->parent_promise_type && pp->parent_promise_type->parent_bundle)
    {
        if (pp->parent_promise_type->parent_bundle->ns)
        {
            EVP_DigestUpdate(&context, pp->parent_promise_type->parent_bundle->ns, strlen(pp->parent_promise_type->parent_bundle->ns));
        }

        if (pp->parent_promise_type->parent_bundle->name)
        {
            EVP_DigestUpdate(&context, pp->parent_promise_type->parent_bundle->name, strlen(pp->parent_promise_type->parent_bundle->name));
        }
    }

    // Unused: pp start, end, and line attributes (describing source position).

    if (salt)
    {
        EVP_DigestUpdate(&context, salt, strlen(salt));
    }

    if (pp->conlist)
    {
        for (size_t i = 0; i < SeqLength(pp->conlist); i++)
        {
            Constraint *cp = SeqAt(pp->conlist, i);

            EVP_DigestUpdate(&context, cp->lval, strlen(cp->lval));

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
                EVP_DigestUpdate(&context, cp->rval.item, strlen(cp->rval.item));
                break;

            case RVAL_TYPE_LIST:
                for (rp = cp->rval.item; rp != NULL; rp = rp->next)
                {
                    EVP_DigestUpdate(&context, RlistScalarValue(rp), strlen(RlistScalarValue(rp)));
                }
                break;

            case RVAL_TYPE_FNCALL:

                /* Body or bundle */

                fp = (FnCall *) cp->rval.item;

                EVP_DigestUpdate(&context, fp->name, strlen(fp->name));

                for (rp = fp->args; rp != NULL; rp = rp->next)
                {
                    switch (rp->val.type)
                    {
                    case RVAL_TYPE_SCALAR:
                        EVP_DigestUpdate(&context, RlistScalarValue(rp), strlen(RlistScalarValue(rp)));
                        break;

                    case RVAL_TYPE_FNCALL:
                        EVP_DigestUpdate(&context, RlistFnCallValue(rp)->name, strlen(RlistFnCallValue(rp)->name));
                        break;

                    default:
                        ProgrammingError("Unhandled case in switch");
                        break;
                    }
                }
                break;

            default:
                break;
            }
        }
    }

    EVP_DigestFinal(&context, digest, &md_len);

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

CfLock AcquireLock(EvalContext *ctx, const char *operand, const char *host, time_t now,
                   TransactionContext tc, const Promise *pp, bool ignoreProcesses)
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
        cc_operator, cc_operand, tc.expireafter, tc.ifelapsed);

    int sum = 0;
    for (int i = 0; cc_operator[i] != '\0'; i++)
    {
        sum = (CF_MACROALPHABET * sum + cc_operator[i]) % CF_HASHTABLESIZE;
    }

    for (int i = 0; cc_operand[i] != '\0'; i++)
    {
        sum = (CF_MACROALPHABET * sum + cc_operand[i]) % CF_HASHTABLESIZE;
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
    if (tc.ifelapsed != 0)
    {
        if (elapsedtime < 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                "XX Another cf-agent seems to have done this since I started (elapsed=%jd)",
                (intmax_t) elapsedtime);
            ReleaseCriticalSection(CF_CRITIAL_SECTION);
            return CfLockNull();
        }

        if (elapsedtime < tc.ifelapsed)
        {
            Log(LOG_LEVEL_VERBOSE,
                "XX Nothing promised here [%.40s] (%jd/%u minutes elapsed)",
                cflast, (intmax_t) elapsedtime, tc.ifelapsed);
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
            if (elapsedtime >= tc.expireafter)
            {
                Log(LOG_LEVEL_INFO, "Lock %s expired (after %jd/%u minutes)",
                    cflock, (intmax_t) elapsedtime, tc.expireafter);

                pid_t pid = FindLockPid(cflock);

                if (KillLockHolder(cflock))
                {
                    Log(LOG_LEVEL_INFO,
                        "Lock expired, process with PID %jd killed",
                        (intmax_t) pid);
                    unlink(cflock);
                }
                else
                {
                    Log(LOG_LEVEL_ERR,
                        "Unable to kill expired process %jd from lock %s"
                        " (probably process not found or permission denied)",
                        (intmax_t) pid, cflock);
                }
            }
            else
            {
                ReleaseCriticalSection(CF_CRITIAL_SECTION);
                Log(LOG_LEVEL_VERBOSE,
                    "Couldn't obtain lock for %s (already running!)", cflock);
                return CfLockNull();
            }
        }

        int ret = WriteLock(cflock);
        if (ret != -1)
        {
            /* Register a cleanup handler *after* having opened the DB, so that
             * CloseAllDB() atexit() handler is registered in advance, and it is
             * called after removing this lock.

             * There is a small race condition here that we'll leave a stale lock
             * if we exit before the following line. */
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
        Log(LOG_LEVEL_ERR, "Unable to create '%s'. (creat: %s)", lock.last, GetErrorStr());
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


void GetLockName(char *lockname, const char *locktype, const char *base, const Rlist *params)
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

static void CopyLockDatabaseAtomically(const char *from, const char *to,
                                       const char *from_pretty_name, const char *to_pretty_name)
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
        Log(LOG_LEVEL_WARNING, "Could not move '%s' into place (rename: %s)",
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

    CopyLockDatabaseAtomically(db_path, db_path_backup, "lock database", "lock database backup");

    free(db_path);
    free(db_path_backup);

    ReleaseCriticalSection(CF_CRITIAL_SECTION);
}

static void RestoreLockDatabase(void)
{
    // We don't do any locking here (since we can't trust the database), but
    // this should be right after bootup, so we should be the only one.
    // Worst case someone else will just copy the same file to the same
    // location.
    char *db_path = DBIdToPath(dbid_locks);
    char *db_path_backup;
    xasprintf(&db_path_backup, "%s.backup", db_path);

    CopyLockDatabaseAtomically(db_path_backup, db_path, "lock database backup", "lock database");

    free(db_path);
    free(db_path_backup);
}

void PurgeLocks(void)
{
    CF_DBC *dbcp;
    char *key;
    int ksize, vsize;
    LockData lock_horizon;
    LockData *entry = NULL;
    time_t now = time(NULL);

    CF_DB *dbp = OpenLock();

    if(!dbp)
    {
        return;
    }

    memset(&lock_horizon, 0, sizeof(lock_horizon));

    if (ReadDB(dbp, "lock_horizon", &lock_horizon, sizeof(lock_horizon)))
    {
        if (now - lock_horizon.time < SECONDS_PER_WEEK * 4)
        {
            Log(LOG_LEVEL_VERBOSE, "No lock purging scheduled");
            CloseLock(dbp);
            return;
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Looking for stale locks to purge");

    if (!NewDBCursor(dbp, &dbcp))
    {
        char *db_path = DBIdToPath(dbid_locks);
        Log(LOG_LEVEL_ERR, "Unable to get cursor for locks database '%s'", db_path);
        free(db_path);
        CloseLock(dbp);
        return;
    }

    while (NextDB(dbcp, &key, &ksize, (void **)&entry, &vsize))
    {
#ifdef LMDB
        LOG_LOCK_OP("<unknown>", key, entry);

        if (key[0] == 'X')
        {
            continue;
        }
#else
        if (STARTSWITH(key, "last.internal_bundle.track_license.handle"))
        {
            continue;
        }
#endif

        if (now - entry->time > (time_t) CF_LOCKHORIZON)
        {
            Log(LOG_LEVEL_VERBOSE, "Purging lock (%jd s elapsed): %s",
                (intmax_t) (now - entry->time), key);
            DBCursorDeleteEntry(dbcp);
        }
    }

    Log(LOG_LEVEL_DEBUG, "Finished purging locks");

    lock_horizon.time = now;
    DeleteDBCursor(dbcp);

    WriteDB(dbp, "lock_horizon", &lock_horizon, sizeof(lock_horizon));
    CloseLock(dbp);
}

int WriteLock(const char *name)
{
    CF_DB *dbp;

    ThreadLock(cft_lock);
    if ((dbp = OpenLock()) == NULL)
    {
        ThreadUnlock(cft_lock);
        return -1;
    }

    WriteLockDataCurrent(dbp, name);

    CloseLock(dbp);
    ThreadUnlock(cft_lock);

    return 0;
}

static void VerifyThatDatabaseIsNotCorrupt_once(void)
{
    int uptime = GetUptimeSeconds(time(NULL));
    if (uptime <= 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Not able to determine uptime when verifying lock database. "
            "Will assume the database is in order.");
        return;
    }

    char *db_path = DBIdToPath(dbid_locks);
    struct stat statbuf;
    if (stat(db_path, &statbuf) == 0)
    {
        if (statbuf.st_mtime < time(NULL) - uptime)
        {
            // We have rebooted since the database was last updated.
            // Restore it from our backup.
            RestoreLockDatabase();
        }
    }
    free(db_path);
}

static void VerifyThatDatabaseIsNotCorrupt(void)
{
    static pthread_once_t uptime_verified = PTHREAD_ONCE_INIT;
    pthread_once(&uptime_verified, &VerifyThatDatabaseIsNotCorrupt_once);
}

CF_DB *OpenLock()
{
    CF_DB *dbp;

    VerifyThatDatabaseIsNotCorrupt();

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
