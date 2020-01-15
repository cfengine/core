#include <platform.h>
#include <diagnose.h>
#include <logging.h>

#if defined(__MINGW32__) || !defined(LMDB)

int diagnose_main(
    ARG_UNUSED int argc, ARG_UNUSED const char *const *const argv)
{
    Log(LOG_LEVEL_ERR,
        "cf-check diagnose not available on this platform/build");
    return 1;
}

size_t diagnose_files(
    ARG_UNUSED const Seq *filenames,
    ARG_UNUSED Seq **corrupt,
    ARG_UNUSED bool foreground,
    ARG_UNUSED bool validate)
{
    Log(LOG_LEVEL_INFO,
        "database diagnosis not available on this platform/build");
    return 0;
}

#else

#include <stdio.h>
#include <lmdump.h>
#include <lmdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <utilities.h>
#include <sequence.h>
#include <alloc.h>
#include <string_lib.h>
#include <unistd.h>
#include <validate.h>

#define CF_CHECK_CREATE_STRING(name) \
  #name,

static const char *CF_CHECK_STR[] = {
    CF_CHECK_RUN_CODES(CF_CHECK_CREATE_STRING)
};

static bool code_is_errno(int r)
{
    return (r > CF_CHECK_MAX);
}

// Better strerror, returns NULL if it doesn't know.
static const char *strerror_or_null(int r)
{
    const char *strerror_string = strerror(r);
    if (strerror_string == NULL)
    {
        return NULL;
    }

    const char *unknown = "Unknown error";
    if (strncmp(strerror_string, unknown, strlen(unknown)) == 0)
    {
        return NULL;
    }

    return strerror_string;
}

static int errno_to_code(int r)
{
    assert(r != 0);
    return r + CF_CHECK_MAX;
}

static int code_to_errno(int r)
{
    assert(code_is_errno(r));
    return r - CF_CHECK_MAX;
}

static const char *CF_CHECK_STRING(int code)
{
    static char unknown[1024];
    if (code <= 0 || code < CF_CHECK_MAX)
    {
        return CF_CHECK_STR[code];
    }
    else if (code_is_errno(code)) // code > CF_CHECK_MAX
    {
        code = code_to_errno(code);
        const char *str = strerror_or_null(code);
        if (str == NULL)
        {
            str = "Unknown";
        }
        snprintf(unknown, sizeof(unknown), "SYSTEM_ERROR %d - %s", code, str);
        return unknown;
    }
    return CF_CHECK_STR[CF_CHECK_UNKNOWN];
}

int signal_to_cf_check_code(int sig)
{
    switch (sig)
    {
    case SIGHUP:
        return CF_CHECK_SIGNAL_HANGUP;
    case SIGINT:
        return CF_CHECK_SIGNAL_INTERRUPT;
    case SIGQUIT:
        return CF_CHECK_SIGNAL_QUIT;
    case SIGILL:
        return CF_CHECK_SIGNAL_ILLEGAL_INSTRUCTION;
    case SIGTRAP:
        return CF_CHECK_SIGNAL_TRACE_TRAP;
    case SIGABRT:
        return CF_CHECK_SIGNAL_ABORT;
    case SIGFPE:
        return CF_CHECK_SIGNAL_FLOATING_POINT_EXCEPTION;
    case SIGKILL:
        return CF_CHECK_SIGNAL_KILL;
    case SIGBUS:
        return CF_CHECK_SIGNAL_BUS_ERROR;
    case SIGSEGV:
        return CF_CHECK_SIGNAL_SEGFAULT;
    case SIGSYS:
        return CF_CHECK_SIGNAL_NON_EXISTENT_SYSCALL;
    case SIGPIPE:
        return CF_CHECK_SIGNAL_INVALID_PIPE;
    case SIGALRM:
        return CF_CHECK_SIGNAL_TIMER_EXPIRED;
    case SIGTERM:
        return CF_CHECK_SIGNAL_TERMINATE;
    case SIGURG:
        return CF_CHECK_SIGNAL_URGENT_SOCKET_CONDITION;
    case SIGSTOP:
        return CF_CHECK_SIGNAL_STOP;
    case SIGTSTP:
        return CF_CHECK_SIGNAL_KEYBOARD_STOP;
    case SIGCONT:
        return CF_CHECK_SIGNAL_CONTINUE;
    case SIGCHLD:
        return CF_CHECK_SIGNAL_CHILD_STATUS_CHANGE;
    case SIGTTIN:
        return CF_CHECK_SIGNAL_BACKGROUND_READ_ATTEMPT;
    case SIGTTOU:
        return CF_CHECK_SIGNAL_BACKGROUND_WRITE_ATTEMPT;
    case SIGIO:
        return CF_CHECK_SIGNAL_IO_POSSIBLE_ON_DESCRIPTOR;
    case SIGXCPU:
        return CF_CHECK_SIGNAL_CPU_TIME_EXCEEDED;
    case SIGXFSZ:
        return CF_CHECK_SIGNAL_FILE_SIZE_EXCEEDED;
    case SIGVTALRM:
        return CF_CHECK_SIGNAL_VIRTUAL_TIME_ALARM;
    case SIGPROF:
        return CF_CHECK_SIGNAL_PROFILING_TIMER_ALARM;
    case SIGWINCH:
        return CF_CHECK_SIGNAL_WINDOW_SIZE_CHANGE;
    // Some signals are present on OS X / BSD but not Ubuntu 14, omitting:
    // case SIGEMT:
    //     return CF_CHECK_SIGNAL_EMULATE_INSTRUCTION;
    // case SIGINFO:
    //     return CF_CHECK_SIGNAL_STATUS_REQUEST;
    default:
        break;
    }
    return CF_CHECK_SIGNAL_OTHER;
}

int lmdb_errno_to_cf_check_code(int r)
{
    switch (r)
    {
    case 0:
        return CF_CHECK_OK;
    // LMDB-specific error codes:
    case MDB_KEYEXIST:
        return CF_CHECK_LMDB_KEY_EXISTS;
    case MDB_NOTFOUND:
        return CF_CHECK_LMDB_KEY_NOT_FOUND;
    case MDB_PAGE_NOTFOUND:
        return CF_CHECK_LMDB_PAGE_NOT_FOUND;
    case MDB_CORRUPTED:
        return CF_CHECK_LMDB_CORRUPT_PAGE;
    case MDB_PANIC:
        return CF_CHECK_LMDB_PANIC_FATAL_ERROR;
    case MDB_VERSION_MISMATCH:
        return CF_CHECK_LMDB_VERSION_MISMATCH;
    case MDB_INVALID:
        return CF_CHECK_LMDB_INVALID_DATABASE;
    case MDB_MAP_FULL:
        return CF_CHECK_LMDB_MAP_FULL;
    case MDB_DBS_FULL:
        return CF_CHECK_LMDB_DBS_FULL;
    case MDB_READERS_FULL:
        return CF_CHECK_LMDB_READERS_FULL;
    case MDB_TLS_FULL:
        return CF_CHECK_LMDB_TLS_KEYS_FULL;
    case MDB_TXN_FULL:
        return CF_CHECK_LMDB_TRANSACTION_FULL;
    case MDB_CURSOR_FULL:
        return CF_CHECK_LMDB_CURSOR_STACK_TOO_DEEP;
    case MDB_PAGE_FULL:
        return CF_CHECK_LMDB_PAGE_FULL;
    case MDB_MAP_RESIZED:
        return CF_CHECK_LMDB_MAP_RESIZE_BEYOND_SIZE;
    case MDB_INCOMPATIBLE:
        return CF_CHECK_LMDB_INCOMPATIBLE_OPERATION;
    case MDB_BAD_RSLOT:
        return CF_CHECK_LMDB_INVALID_REUSE_OF_READER_LOCKTABLE_SLOT;
    case MDB_BAD_TXN:
        return CF_CHECK_LMDB_BAD_OR_INVALID_TRANSACTION;
    case MDB_BAD_VALSIZE:
        return CF_CHECK_LMDB_WRONG_KEY_OR_VALUE_SIZE;
    // Doesn't exist in earlier versions of LMDB:
    // case MDB_BAD_DBI:
    //     return CF_CHECK_LMDB_BAD_DBI;
    default:
        break;
    }
    const int s = errno_to_code(r);
    if (s == CF_CHECK_UNKNOWN)
    {
        return CF_CHECK_LMDUMP_UNKNOWN_ERROR;
    }
    return s;
}

static int diagnose(const char *path, bool temporary_redirect, bool validate)
{
    // At this point we are already forked, we just need to decide 2 things:
    // * Should output be redirected (to prevent spam)?
    // * Which inner diagnose / validation function should be called?
    int ret;
    if (validate)
    {
        // --validate has meaningful output, so we don't want to redirect
        // regardless of whether it's foreground or forked.
        ret = CFCheck_Validate(path);
    }
    else if (temporary_redirect)
    {
        // --no-fork mode: temporarily redirect output to /dev/null & restore
        // Only done when necessary as it might not be so portable (/dev/fd)
        int saved_stdout = dup(STDOUT_FILENO);
        FILE *const f_result = freopen("/dev/null", "w", stdout);
        if (f_result == NULL)
        {
            return errno;
        }
        assert(f_result == stdout);
        ret = lmdump(LMDUMP_VALUES_ASCII, path);

        fflush(stdout);
        dup2(saved_stdout, STDOUT_FILENO);
    }
    else
    {
        // Normal mode: redirect to /dev/null permanently (forked process)
        FILE *const f_result = freopen("/dev/null", "w", stdout);
        if (f_result == NULL)
        {
            return errno;
        }
        assert(f_result == stdout);
        ret = lmdump(LMDUMP_VALUES_ASCII, path);
    }
    return ret;
}

static int fork_and_diagnose(const char *path, bool validate)
{
    const pid_t child_pid = fork();
    if (child_pid == 0)
    {
        // Child
        exit(diagnose(path, false, validate));
    }
    else
    {
        // Parent
        int status;
        pid_t pid = waitpid(child_pid, &status, 0);
        if (pid != child_pid)
        {
            return CF_CHECK_PID_ERROR;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != CF_CHECK_OK)
        {
            return lmdb_errno_to_cf_check_code(WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status))
        {
            return signal_to_cf_check_code(WTERMSIG(status));
        }
    }
    return CF_CHECK_OK;
}

/**
 * @param[in]  filenames  DB files to diagnose/check
 * @param[out] corrupt    place to store the resulting sequence of corrupted
 *                        files or %NULL (to only get the number of corrupted
 *                        files)
 * @param[in]  foreground whether to run in foreground or fork (safer)
 * @return                the number of the corrupted files
 */
size_t diagnose_files(
    const Seq *filenames, Seq **corrupt, bool foreground, bool validate)
{
    size_t corruptions = 0;
    const size_t length = SeqLength(filenames);

    if (corrupt != NULL)
    {
        *corrupt = SeqNew(length, free);
    }

    for (int i = 0; i < length; ++i)
    {
        const char *filename = SeqAt(filenames, i);
        int r;
        if (foreground)
        {
            r = lmdb_errno_to_cf_check_code(
                diagnose(filename, true, validate));
        }
        else
        {
            r = fork_and_diagnose(filename, validate);
        }
        Log(LOG_LEVEL_INFO,
            "Status of '%s': %s\n",
            filename,
            CF_CHECK_STRING(r));

        if (r != CF_CHECK_OK)
        {
            ++corruptions;
            if (corrupt != NULL)
            {
                SeqAppend(*corrupt, xstrdup(filename));
            }
        }
    }
    if (corruptions == 0)
    {
        Log(LOG_LEVEL_INFO, "All %zu databases healthy", length);
    }
    else
    {
        Log(LOG_LEVEL_ERR,
            "Problems detected in %zu/%zu databases",
            corruptions,
            length);
    }
    return corruptions;
}

int diagnose_main(int argc, const char *const *const argv)
{
    size_t offset = 1;
    bool foreground = false;
    bool validate = false;
    for (int i = offset; i < argc && argv[i][0] == '-'; ++i)
    {
        if (StringMatchesOption(argv[i], "--no-fork", "-F"))
        {
            foreground = true;
            offset += 1;
        }
        else if (StringMatchesOption(argv[i], "--validate", "-v"))
        {
            validate = true;
            offset += 1;
        }
        else
        {
            assert(argv[i][0] == '-'); // For-loop condition
            Log(LOG_LEVEL_ERR, "Unrecognized option: '%s'", argv[i]);
            return 2;
        }
    }
    Seq *files = argv_to_lmdb_files(argc, argv, offset);
    if (files == NULL || SeqLength(files) == 0)
    {
        Log(LOG_LEVEL_ERR, "No database files to diagnose");
        return 1;
    }
    const int ret = diagnose_files(files, NULL, foreground, validate);
    SeqDestroy(files);
    return ret;
}

#endif
