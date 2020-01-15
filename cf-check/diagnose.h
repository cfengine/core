#ifndef __DIAGNOSE_H__
#define __DIAGNOSE_H__

#include <sequence.h>

// Extensions of the errno range, for mixing with lmdb
// and operating system error codes
#define CF_CHECK_ERRNO_VALIDATE_FAILED -1

// cf-check has one canonical list of error codes
// which combines signals, system errno, lmdb errnos and
// cf-check specific errors:
// clang-format off
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
    macro(VALIDATE_FAILED)                                \
    macro(UNKNOWN)

#define CF_CHECK_MAX CF_CHECK_UNKNOWN

#define CF_CHECK_CREATE_ENUM(name) \
  CF_CHECK_##name,

typedef enum {
    CF_CHECK_RUN_CODES(CF_CHECK_CREATE_ENUM)
} CFCheckCode;
// clang-format on

int lmdb_errno_to_cf_check_code(int r);
int signal_to_cf_check_code(int sig);

size_t diagnose_files(
    const Seq *filenames, Seq **corrupt, bool foreground, bool validate);
int diagnose_main(int argc, const char *const *argv);

#endif
