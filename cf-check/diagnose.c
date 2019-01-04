#include <platform.h>
#include <diagnose.h>

#if defined (__MINGW32__)

int diagnose_main(int argc, char **argv)
{
    printf("diagnose not supported on Windows\n");
    return 1;
}

#elif ! defined (LMDB)

int diagnose_main(int argc, char **argv)
{
    printf("diagnose only implemented for LMDB.\n");
    return 1;
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

#define CF_CHECK_RUN_CODES(macro)                         \
    macro(OK)                                             \
    macro(SIGNAL_HANGUP)                                  \
    macro(SIGNAL_INTERRUPT)                               \
    macro(SIGNAL_QUIT)                                    \
    macro(SIGNAL_ILLEGAL_INSTRUCTION)                     \
    macro(SIGNAL_TRACE_TRAP)                              \
    macro(SIGNAL_ABORT)                                   \
    macro(SIGNAL_EMULATE_INSTRUCTION)                     \
    macro(SIGNAL_FLOATING_POINT_EXCEPTION)                \
    macro(SIGNAL_KILL)                                    \
    macro(SIGNAL_BUS_ERROR)                               \
    macro(SIGNAL_SEGFAULT)                                \
    macro(SIGNAL_NON_EXISTENT_SYSCALL)                    \
    macro(SIGNAL_INVALID_PIPE)                            \
    macro(SIGNAL_TIMER_EXPIRED)                           \
    macro(SIGNAL_TERMINATE)                               \
    macro(SIGNAL_URGENT_SOCKET_CONDITION)                 \
    macro(SIGNAL_STOP)                                    \
    macro(SIGNAL_KEYBOARD_STOP)                           \
    macro(SIGNAL_CONTINUE)                                \
    macro(SIGNAL_CHILD_STATUS_CHANGE)                     \
    macro(SIGNAL_BACKGROUND_READ_ATTEMPT)                 \
    macro(SIGNAL_BACKGROUND_WRITE_ATTEMPT)                \
    macro(SIGNAL_IO_POSSIBLE_ON_DESCRIPTOR)               \
    macro(SIGNAL_CPU_TIME_EXCEEDED)                       \
    macro(SIGNAL_FILE_SIZE_EXCEEDED)                      \
    macro(SIGNAL_VIRTUAL_TIME_ALARM)                      \
    macro(SIGNAL_PROFILING_TIMER_ALARM)                   \
    macro(SIGNAL_WINDOW_SIZE_CHANGE)                      \
    macro(SIGNAL_STATUS_REQUEST)                          \
    macro(SIGNAL_OTHER)                                   \
    macro(LMDB_KEY_EXISTS)                                \
    macro(LMDB_KEY_NOT_FOUND)                             \
    macro(LMDB_PAGE_NOT_FOUND)                            \
    macro(LMDB_CORRUPT_PAGE)                              \
    macro(LMDB_PANIC_FATAL_ERROR)                         \
    macro(LMDB_VERSION_MISMATCH)                          \
    macro(LMDB_INVALID_DATABASE)                          \
    macro(LMDB_MAP_FULL)                                  \
    macro(LMDB_DBS_FULL)                                  \
    macro(LMDB_READERS_FULL)                              \
    macro(LMDB_TLS_KEYS_FULL)                             \
    macro(LMDB_TRANSACTION_FULL)                          \
    macro(LMDB_CURSOR_STACK_TOO_DEEP)                     \
    macro(LMDB_PAGE_FULL)                                 \
    macro(LMDB_MAP_RESIZE_BEYOND_SIZE)                    \
    macro(LMDB_INCOMPATIBLE_OPERATION)                    \
    macro(LMDB_INVALID_REUSE_OF_READER_LOCKTABLE_SLOT)    \
    macro(LMDB_BAD_OR_INVALID_TRANSACTION)                \
    macro(LMDB_WRONG_KEY_OR_VALUE_SIZE)                   \
    macro(LMDB_BAD_DBI)                                   \
    macro(LMDUMP_UNKNOWN_ERROR)                           \
    macro(PID_ERROR)                                      \
    macro(PERMISSION_ERROR)                               \
    macro(DOES_NOT_EXIST)                                 \
    macro(UNKNOWN)

#define CF_CHECK_MAX CF_CHECK_UNKNOWN

#define CF_CHECK_CREATE_ENUM(name) \
  CF_CHECK_##name,

#define CF_CHECK_CREATE_STRING(name) \
  #name,

typedef enum {
    CF_CHECK_RUN_CODES(CF_CHECK_CREATE_ENUM)
} CFCheckCode;

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

static int signal_to_code(int sig)
{
    switch (sig) {
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

static int lmdump_errno_to_code(int r)
{
    switch (r) {
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

static int diagnose(const char *path)
{
    freopen("/dev/null", "w", stdout);
    return lmdump(LMDUMP_VALUES_ASCII, path);
}

static int fork_and_diagnose(const char *path)
{
    const pid_t child_pid = fork();
    if (child_pid == 0)
    {
        // Child
        exit(diagnose(path));
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
            return lmdump_errno_to_code(WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status))
        {
            return signal_to_code(WTERMSIG(status));
        }
    }
    return CF_CHECK_OK;
}

size_t diagnose_files(Seq* filenames, Seq** corrupt)
{
    assert(corrupt == NULL || *corrupt == NULL);
    size_t corruptions = 0;
    const size_t length = SeqLength(filenames);
    for (int i = 0; i < length; ++i)
    {
        const char *filename = SeqAt(filenames, i);
        const int r = fork_and_diagnose(filename);
        printf("Status of '%s': %s\n", filename, CF_CHECK_STRING(r));

        if (r != CF_CHECK_OK)
        {
            ++corruptions;
            if (corrupt != NULL)
            {
                if (*corrupt == NULL)
                {
                    *corrupt = SeqNew(length, free);
                }
                SeqAppend(*corrupt, xstrdup(filename));
            }
        }
    }
    if (corruptions == 0)
    {
        printf("All %zu databases healthy\n", length);
    }
    else
    {
        printf("Problems detected in %zu/%zu databases\n", corruptions, length);
    }
    return corruptions;
}

int diagnose_main(int argc, char **argv)
{
    Seq *files = argv_to_lmdb_files(argc, argv);
    const int ret = diagnose_files(files, NULL);
    SeqDestroy(files);
    return ret;
}

#endif
