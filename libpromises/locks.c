/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.

*/

#include "locks.h"
#include "mutex.h"
#include "logging.h"
#include "string_lib.h"
#include "files_interfaces.h"
#include "files_lib.h"
#include "atexit.h"
#include "policy.h"
#include "files_hashes.h"
#include "item_lib.h"
#include "files_names.h"
#include "rlist.h"
#include "process_lib.h"
#include "fncall.h"
#include "env_context.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

#define CFLOGSIZE 1048576       /* Size of lock-log before rotation */

static Item *DONELIST = NULL;

static pthread_once_t lock_cleanup_once = PTHREAD_ONCE_INIT;

static bool WriteLockData(CF_DB *dbp, const char *lock_id, LockData *lock_data)
{
    if(WriteDB(dbp, lock_id, lock_data, sizeof(LockData)))
    {
        return true;
    }
    else
    {
        return false;
    }
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

/*
 * Much simpler than AcquireLock. Useful when you just want to check
 * if a certain amount of time has elapsed for an action since last
 * time you checked.  No need to clean up after calling this
 * (e.g. like YieldCurrentLock()).
 *
 * WARNING: Is prone to race-conditions, both on the thread and
 *          process level.
 */

bool AcquireLockByID(const char *lock_id, int acquire_after_minutes)
{
    CF_DB *dbp = OpenLock();

    if(dbp == NULL)
    {
        return false;
    }

    bool result;
    LockData lock_data = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

    if (ReadDB(dbp, lock_id, &lock_data, sizeof(lock_data)))
    {
        if(lock_data.time + (acquire_after_minutes * SECONDS_PER_MINUTE) < time(NULL))
        {
            result = WriteLockDataCurrent(dbp, lock_id);
        }
        else
        {
            result = false;
        }
    }
    else
    {
        result = WriteLockDataCurrent(dbp, lock_id);
    }

    CloseLock(dbp);

    return result;
}

time_t FindLockTime(char *name)
{
    CF_DB *dbp;
    LockData entry = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

    CfDebug("FindLockTime(%s)\n", name);

    if ((dbp = OpenLock()) == NULL)
    {
        return -1;
    }

    if (ReadDB(dbp, name, &entry, sizeof(entry)))
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

bool InvalidateLockTime(const char *lock_id)
{
    time_t epoch = 0;

    CF_DB *dbp = OpenLock();

    if (dbp == NULL)
    {
        return false;
    }

    LockData lock_data = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

    if(!ReadDB(dbp, lock_id, &lock_data, sizeof(lock_data)))
    {
        CloseLock(dbp);
        return true;  /* nothing to invalidate */
    }

    lock_data.time = epoch;

    bool result = WriteLockData(dbp, lock_id, &lock_data);

    CloseLock(dbp);

    return result;
}


static void RemoveDates(char *s)
{
    int i, a = 0, b = 0, c = 0, d = 0;
    char *dayp = NULL, *monthp = NULL, *sp;
    char *days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Canonifies or blanks our times/dates for locks where there would be an explosion of state

    if (strlen(s) < strlen("Fri Oct 1 15:15:23 EST 2010"))
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

static int RemoveLock(char *name)
{
    CF_DB *dbp;

    if ((dbp = OpenLock()) == NULL)
    {
        return -1;
    }

    ThreadLock(cft_lock);
    DeleteDB(dbp, name);
    ThreadUnlock(cft_lock);

    CloseLock(dbp);
    return 0;
}

static void WaitForCriticalSection()
{
    time_t now = time(NULL), then = FindLockTime("CF_CRITICAL_SECTION");

/* Another agent has been waiting more than a minute, it means there
   is likely crash detritus to clear up... After a minute we take our
   chances ... */

    while ((then != -1) && (now - then < 60))
    {
        sleep(1);
        now = time(NULL);
        then = FindLockTime("CF_CRITICAL_SECTION");
    }

    WriteLock("CF_CRITICAL_SECTION");
}

static void ReleaseCriticalSection()
{
    RemoveLock("CF_CRITICAL_SECTION");
}

static time_t FindLock(char *last)
{
    time_t mtime;

    if ((mtime = FindLockTime(last)) == -1)
    {
        /* Do this to prevent deadlock loops from surviving if IfElapsed > T_sched */

        if (WriteLock(last) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to lock %s\n", last);
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
    CF_DB *dbp;
    LockData entry = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

    if ((dbp = OpenLock()) == NULL)
    {
        return -1;
    }

    if (ReadDB(dbp, name, &entry, sizeof(entry)))
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

static void LogLockCompletion(char *cflog, int pid, char *str, char *operator, char *operand)
{
    FILE *fp;
    char buffer[CF_MAXVARSIZE];
    struct stat statbuf;
    time_t tim;

    CfDebug("LockLogCompletion(%s)\n", str);

    if (cflog == NULL)
    {
        return;
    }

    if ((fp = fopen(cflog, "a")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Can't open lock-log file %s\n", cflog);
        exit(1);
    }

    if ((tim = time((time_t *) NULL)) == -1)
    {
        CfDebug("Cfengine: couldn't read system clock\n");
    }

    sprintf(buffer, "%s", cf_ctime(&tim));

    if (Chop(buffer, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }

    fprintf(fp, "%s:%s:pid=%d:%s:%s\n", buffer, str, pid, operator, operand);

    fclose(fp);

    if (cfstat(cflog, &statbuf) != -1)
    {
        if (statbuf.st_size > CFLOGSIZE)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Rotating lock-runlog file\n");
            RotateFiles(cflog, 2);
        }
    }
}

static void LocksCleanup(void)
{
    if (strlen(CFLOCK) > 0)
    {
        CfLock best_guess;
        best_guess.lock = xstrdup(CFLOCK);
        best_guess.last = xstrdup(CFLAST);
        best_guess.log = xstrdup(CFLOG);
        YieldCurrentLock(best_guess);
    }
}

static void RegisterLockCleanup(void)
{
    RegisterAtExitFunction(&LocksCleanup);
}

static char *BodyName(const Promise *pp)
{
    char *name, *sp;
    int size = 0;

/* Return a type template for the promise body for lock-type identification */

    name = xmalloc(CF_MAXVARSIZE);

    sp = pp->parent_promise_type->name;

    if (size + strlen(sp) < CF_MAXVARSIZE - CF_BUFFERMARGIN)
    {
        strcpy(name, sp);
        strcat(name, ".");
        size += strlen(sp);
    }

    for (size_t i = 0; (i < 5) && i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, "args") == 0)      /* Exception for args, by symmetry, for locking */
        {
            continue;
        }

        if (size + strlen(cp->lval) < CF_MAXVARSIZE - CF_BUFFERMARGIN)
        {
            strcat(name, cp->lval);
            strcat(name, ".");
            size += strlen(cp->lval);
        }
    }

    return name;
}

#ifdef __MINGW32__

static bool KillLockHolder(ARG_UNUSED const char *lock)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "",
          "Process is not running - ignoring lock (Windows does not support graceful processes termination)\n");
    return true;
}

#else

static bool KillLockHolder(const char *lock)
{
    CF_DB *dbp = OpenLock();
    if (dbp == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to open locks database");
        return false;
    }

    LockData lock_data = {
        .process_start_time = PROCESS_START_TIME_UNKNOWN,
    };

    if (!ReadDB(dbp, lock, &lock_data, sizeof(lock_data)))
    {
        /* No lock found */
        CloseLock(dbp);
        return true;
    }

    CloseLock(dbp);

    return GracefulTerminate(lock_data.pid, lock_data.process_start_time);
}

#endif

static void PromiseHash(const Promise *pp, const char *salt, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    static const char *PACK_UPIFELAPSED_SALT = "packageuplist";

    EVP_MD_CTX context;
    int md_len;
    const EVP_MD *md = NULL;
    Rlist *rp;
    FnCall *fp;

    char *noRvalHash[] = { "mtime", "atime", "ctime", NULL };
    int doHash;

    md = EVP_get_digestbyname(FileHashName(type));

    EVP_DigestInit(&context, md);

// multiple packages (promisers) may share same package_list_update_ifelapsed lock
    if (!(salt && (strncmp(salt, PACK_UPIFELAPSED_SALT, sizeof(PACK_UPIFELAPSED_SALT) - 1) == 0)))
    {
        EVP_DigestUpdate(&context, pp->promiser, strlen(pp->promiser));
    }

    if (pp->comment)
    {
        EVP_DigestUpdate(&context, pp->comment, strlen(pp->comment));
    }

    if (salt)
    {
        EVP_DigestUpdate(&context, salt, strlen(salt));
    }

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
                EVP_DigestUpdate(&context, rp->item, strlen(rp->item));
            }
            break;

        case RVAL_TYPE_FNCALL:

            /* Body or bundle */

            fp = (FnCall *) cp->rval.item;

            EVP_DigestUpdate(&context, fp->name, strlen(fp->name));

            for (rp = fp->args; rp != NULL; rp = rp->next)
            {
                EVP_DigestUpdate(&context, rp->item, strlen(rp->item));
            }
            break;

        default:
            break;
        }
    }

    EVP_DigestFinal(&context, digest, &md_len);

/* Digest length stored in md_len */
}

CfLock AcquireLock(EvalContext *ctx, char *operand, char *host, time_t now, TransactionContext tc, const Promise *pp, int ignoreProcesses)
{
    int i, sum = 0;
    time_t lastcompleted = 0, elapsedtime;
    char *promise, cc_operator[CF_BUFSIZE], cc_operand[CF_BUFSIZE];
    char cflock[CF_BUFSIZE], cflast[CF_BUFSIZE], cflog[CF_BUFSIZE];
    char str_digest[CF_BUFSIZE];
    CfLock this;
    unsigned char digest[EVP_MAX_MD_SIZE + 1];

    /* Register a cleanup handler */
    pthread_once(&lock_cleanup_once, &RegisterLockCleanup);

    this.last = (char *) CF_UNDEFINED;
    this.lock = (char *) CF_UNDEFINED;
    this.log = (char *) CF_UNDEFINED;

    if (now == 0)
    {
        return this;
    }

    this.last = NULL;
    this.lock = NULL;
    this.log = NULL;

/* Indicate as done if we tried ... as we have passed all class
   constraints now but we should only do this for level 0
   promises. Sub routine bundles cannot be marked as done or it will
   disallow iteration over bundles */

    if (EvalContextPromiseIsDone(ctx, pp))
    {
        return this;
    }

    if (RlistLen(CF_STCK) == 1)
    {
        /* Must not set promise to be done for editfiles etc */
        EvalContextMarkPromiseDone(ctx, pp);
    }

    PromiseHash(pp, operand, digest, CF_DEFAULT_DIGEST);
    HashPrintSafe(CF_DEFAULT_DIGEST, digest, str_digest);

/* As a backup to "done" we need something immune to re-use */

    if (THIS_AGENT_TYPE == AGENT_TYPE_AGENT)
    {
        if (IsItemIn(DONELIST, str_digest))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> This promise has already been verified");
            return this;
        }

        PrependItem(&DONELIST, str_digest, NULL);
    }

/* Finally if we're supposed to ignore locks ... do the remaining stuff */

    if (IGNORELOCK)
    {
        this.lock = xstrdup("dummy");
        return this;
    }

    promise = BodyName(pp);
    snprintf(cc_operator, CF_MAXVARSIZE - 1, "%s-%s", promise, host);
    strncpy(cc_operand, operand, CF_BUFSIZE - 1);
    CanonifyNameInPlace(cc_operand);
    RemoveDates(cc_operand);

    free(promise);

    CfDebug("AcquireLock(%s,%s), ExpireAfter=%d, IfElapsed=%d\n", cc_operator, cc_operand, tc.expireafter,
            tc.ifelapsed);

    for (i = 0; cc_operator[i] != '\0'; i++)
    {
        sum = (CF_MACROALPHABET * sum + cc_operator[i]) % CF_HASHTABLESIZE;
    }

    for (i = 0; cc_operand[i] != '\0'; i++)
    {
        sum = (CF_MACROALPHABET * sum + cc_operand[i]) % CF_HASHTABLESIZE;
    }

    snprintf(cflog, CF_BUFSIZE, "%s/cf3.%.40s.runlog", CFWORKDIR, host);
    snprintf(cflock, CF_BUFSIZE, "lock.%.100s.%s.%.100s_%d_%s", PromiseGetBundle(pp)->name, cc_operator, cc_operand, sum, str_digest);
    snprintf(cflast, CF_BUFSIZE, "last.%.100s.%s.%.100s_%d_%s", PromiseGetBundle(pp)->name, cc_operator, cc_operand, sum, str_digest);

    CfDebug("LOCK(%s)[%s]\n", PromiseGetBundle(pp)->name, cflock);

// Now see if we can get exclusivity to edit the locks

    CFINITSTARTTIME = time(NULL);

    WaitForCriticalSection();

/* Look for non-existent (old) processes */

    lastcompleted = FindLock(cflast);
    elapsedtime = (time_t) (now - lastcompleted) / 60;

    if (elapsedtime < 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " XX Another cf-agent seems to have done this since I started (elapsed=%jd)\n",
              (intmax_t) elapsedtime);
        ReleaseCriticalSection();
        return this;
    }

    if (elapsedtime < tc.ifelapsed)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " XX Nothing promised here [%.40s] (%jd/%u minutes elapsed)\n", cflast,
              (intmax_t) elapsedtime, tc.ifelapsed);
        ReleaseCriticalSection();
        return this;
    }

/* Look for existing (current) processes */

    if (!ignoreProcesses)
    {
        lastcompleted = FindLock(cflock);
        elapsedtime = (time_t) (now - lastcompleted) / 60;

        if (lastcompleted != 0)
        {
            if (elapsedtime >= tc.expireafter)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "Lock %s expired (after %jd/%u minutes)\n", cflock, (intmax_t) elapsedtime,
                      tc.expireafter);

                pid_t pid = FindLockPid(cflock);

                if (KillLockHolder(cflock))
                {
                    LogLockCompletion(cflog, pid, "Lock expired, process killed", cc_operator, cc_operand);
                    unlink(cflock);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to kill expired process %d from lock %s", (int)pid, cflock);
                }
            }
            else
            {
                ReleaseCriticalSection();
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Couldn't obtain lock for %s (already running!)\n", cflock);
                return this;
            }
        }

        WriteLock(cflock);
    }

    ReleaseCriticalSection();

    this.lock = xstrdup(cflock);
    this.last = xstrdup(cflast);
    this.log = xstrdup(cflog);

/* Keep this as a global for signal handling */
    strcpy(CFLOCK, cflock);
    strcpy(CFLAST, cflast);
    strcpy(CFLOG, cflog);

    return this;
}

void YieldCurrentLock(CfLock this)
{
    if (IGNORELOCK)
    {
        free(this.lock);        /* allocated in AquireLock as a special case */
        return;
    }

    if (this.lock == (char *) CF_UNDEFINED)
    {
        return;
    }

    CfDebug("Yielding lock %s\n", this.lock);

    if (RemoveLock(this.lock) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Unable to remove lock %s\n", this.lock);
        free(this.last);
        free(this.lock);
        free(this.log);
        return;
    }

    if (WriteLock(this.last) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "creat", "Unable to create %s\n", this.last);
        free(this.last);
        free(this.lock);
        free(this.log);
        return;
    }

    /* This lock has ben yield'ed, don't try to yield it again in case process
     * is terminated abnormally.
     */
    strcpy(CFLOCK, "");
    strcpy(CFLAST, "");
    strcpy(CFLOG, "");

    LogLockCompletion(this.log, getpid(), "Lock removed normally ", this.lock, "");

    free(this.last);
    free(this.lock);
    free(this.log);
}

void GetLockName(char *lockname, char *locktype, char *base, Rlist *params)
{
    Rlist *rp;
    int max_sample, count = 0;

    for (rp = params; rp != NULL; rp = rp->next)
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

    strncpy(lockname, locktype, CF_BUFSIZE / 10);
    strcat(lockname, "_");
    strncat(lockname, base, CF_BUFSIZE / 10);
    strcat(lockname, "_");

    for (rp = params; rp != NULL; rp = rp->next)
    {
        strncat(lockname, (char *) rp->item, max_sample);
    }
}

void PurgeLocks(void)
{
    CF_DBC *dbcp;
    char *key;
    int ksize, vsize;
    LockData entry;
    time_t now = time(NULL);

    CF_DB *dbp = OpenLock();

    if(!dbp)
    {
        return;
    }

    memset(&entry, 0, sizeof(entry));

    if (ReadDB(dbp, "lock_horizon", &entry, sizeof(entry)))
    {
        if (now - entry.time < SECONDS_PER_WEEK * 4)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> No lock purging scheduled");
            CloseLock(dbp);
            return;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Looking for stale locks to purge");

    if (!NewDBCursor(dbp, &dbcp))
    {
        CloseLock(dbp);
        return;
    }

    while (NextDB(dbcp, &key, &ksize, (void *) &entry, &vsize))
    {
        if (strncmp(key, "last.internal_bundle.track_license.handle",
                    strlen("last.internal_bundle.track_license.handle")) == 0)
        {
            continue;
        }

        if (now - entry.time > (time_t) CF_LOCKHORIZON)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " --> Purging lock (%jd) %s", (intmax_t)(now - entry.time), key);
            DBCursorDeleteEntry(dbcp);
        }
    }

    entry.time = now;
    DeleteDBCursor(dbcp);

    WriteDB(dbp, "lock_horizon", &entry, sizeof(entry));
    CloseLock(dbp);
}

int WriteLock(char *name)
{
    CF_DB *dbp;

    CfDebug("WriteLock(%s)\n", name);

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
