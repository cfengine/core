/*
  Copyright 2026 Northern.tech AS

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

#include <user_watcher.h>
#include <watcher.h>
#include <logging.h>
#include <alloc.h>
#include <cf3.defs.h>
#include <prototypes3.h>

#ifdef __MINGW32__
#include <cf-windows-functions.h>
#else
#include <pwd.h>
#endif

/* Minimum time between two logs of a persisting failure to look up the user,
 * since the user is looked up on every poll */
#define CHECK_ERROR_LOG_INTERVAL_SECS 60

typedef struct
{
    char *name;
    time_t last_checked; /* Time of the last successful check, 0 if none yet */
    time_t
        last_error_logged; /* Time the last error was logged, 0 if no errors */

    /* What the last successful check observed */
    bool last_exists;
} UserWatcherState;


/**
 * @brief Creates the state of a user_added or user_removed watcher.
 *
 * @param args The arguments of the event, the name of the user to watch
 * @return The new state, to be destroyed with UserWatcherStateDestroy()
 */
void *UserWatcherStateNew(const Rlist *args)
{
    assert(args != NULL);
    const char *name = RlistScalarValue(args);

    UserWatcherState *state =
        (UserWatcherState *) xcalloc(1, sizeof(UserWatcherState));
    state->name = xstrdup(name);

    return state;
}

/**
 * @brief Destroys the state of a user watcher.
 *
 * @param state The watcher state, may be NULL
 */
void UserWatcherStateDestroy(void *_state)
{
    UserWatcherState *state = _state;
    if (state != NULL)
    {
        free(state->name);
        free(state);
    }
}

/**
 * @brief Logs the error message of a failed operation performed by the
 * watcher.
 *
 * A persisting failure is logged at most once every
 * CHECK_ERROR_LOG_INTERVAL_SECS.
 *
 * @param state The watcher state
 * @param operation The operation (e.g., LookupAccountName() or getpwnam_r())
 * @param err The error code (errno on Unix, Win32 error code on Windows)
 */
static void LogCheckError(
    UserWatcherState *state, const char *operation, int err)
{
    assert(state != NULL);

    const time_t now = time(NULL);
    if ((state->last_error_logged == 0)
        || (now - state->last_error_logged >= CHECK_ERROR_LOG_INTERVAL_SECS))
    {
        Log(LOG_LEVEL_ERR,
            "Unable to look up user '%s' while checking for user events (%s: %s)",
            state->name,
            operation,
            GetErrorStrFromCode(err));
        state->last_error_logged = now;
    }
}

/**
 * @brief Checks whether the user of the watcher exists.
 *
 * Looks up the one name only, without listing all users, so it is safe to
 * call while the main thread runs bundles. On Windows, only user accounts
 * count, not groups or built-in accounts such as SYSTEM.
 *
 * @param state The watcher state
 * @param[out] exists Whether user exists
 * @return false on error
 */
static bool UserLookup(UserWatcherState *state, bool *exists)
{
    assert(state != NULL);
    assert(exists != NULL);

#ifdef __MINGW32__
    char sid[CF_MAXSIDSIZE];
    SID_NAME_USE type;
    DWORD error = NovaWin_LookupAccountName(
        state->name, (SID *) sid, sizeof(sid), &type);
    if (error == ERROR_SUCCESS)
    {
        *exists = (type == SidTypeUser);
    }
    else if (NovaWin_IsAccountNotFound(error))
    {
        *exists = false;
    }
    else
    {
        LogCheckError(state, "LookupAccountName", error);
        return false;
    }
#else
    struct passwd pwd;
    struct passwd *result = NULL;
    char buf[GETPW_R_SIZE_MAX];
    int ret = getpwnam_r(state->name, &pwd, buf, sizeof(buf), &result);
    if (result != NULL)
    {
        *exists = true;
    }
    else if (ret == 0)
    {
        /* POSIX: not found is a NULL result with a return value of 0. Any
         * other return value is an error, so the user is not taken as
         * removed on a failing lookup. */
        *exists = false;
    }
    else
    {
        LogCheckError(state, "getpwnam_r", ret);
        return false;
    }
#endif

    state->last_error_logged = 0;
    return true;
}

/**
 * @brief Records the result of a successful check.
 *
 * The first successful check is only the reference for the next ones, so it
 * never fires.
 *
 * @param state The watcher state
 * @param exists Whether the user exists
 * @param happened Whether the event happened since the last check
 * @param event The name of the event for logging (e.g. "removal")
 * @return WATCHER_CHECK_EVENT if the event fired, WATCHER_CHECK_NO_EVENT
 *         otherwise
 */
static WatcherCheckResult UserWatcherRecordObservation(
    UserWatcherState *state, bool exists, bool happened, const char *event)
{
    assert(state != NULL);
    assert(event != NULL);

    const time_t now = time(NULL);
    happened = happened && (state->last_checked != 0);

    if (happened)
    {
        char last_checked_str[26];
        const char *last_checked_ts =
            cf_strtimestamp_local(state->last_checked, last_checked_str);

        char now_str[26];
        const char *now_ts = cf_strtimestamp_local(now, now_str);

        Log(LOG_LEVEL_VERBOSE,
            "Observed %s of user '%s' at %s (last checked at %s)",
            event,
            state->name,
            (now_ts != NULL) ? now_ts : "unknown time",
            (last_checked_ts != NULL) ? last_checked_ts : "unknown time");
    }

    state->last_checked = now;
    state->last_exists = exists;
    return happened ? WATCHER_CHECK_EVENT : WATCHER_CHECK_NO_EVENT;
}

/**
 * @brief Checks for the user_added event.
 *
 * Fires once when the user goes from absent to present.
 *
 * @param state The watcher state
 * @return WATCHER_CHECK_EVENT if the user was added since the last check,
 *         WATCHER_CHECK_ERROR if the lookup failed, WATCHER_CHECK_NO_EVENT
 *         otherwise
 */
WatcherCheckResult UserWatcherCheckUserAdded(void *_state)
{
    assert(_state != NULL);
    UserWatcherState *state = _state;

    bool exists;
    if (!UserLookup(state, &exists))
    {
        /* Unknown state, keep the last observation as is */
        return WATCHER_CHECK_ERROR;
    }

    if (!exists && (state->last_exists || state->last_checked == 0))
    {
        Log(LOG_LEVEL_VERBOSE,
            "User '%s' is absent, watching for its addition",
            state->name);
    }

    bool added = !state->last_exists && exists;
    return UserWatcherRecordObservation(state, exists, added, "addition");
}

/**
 * @brief Checks for the user_removed event.
 *
 * Fires once when the user goes from present to absent.
 *
 * @param state The watcher state
 * @return WATCHER_CHECK_EVENT if the user was removed since the last check,
 *         WATCHER_CHECK_ERROR if the lookup failed, WATCHER_CHECK_NO_EVENT
 *         otherwise
 */
WatcherCheckResult UserWatcherCheckUserRemoved(void *_state)
{
    assert(_state != NULL);
    UserWatcherState *state = _state;

    bool exists;
    if (!UserLookup(state, &exists))
    {
        /* Unknown state, keep the last observation as is */
        return WATCHER_CHECK_ERROR;
    }

    if (exists && !state->last_exists)
    {
        Log(LOG_LEVEL_VERBOSE,
            "User '%s' is present, watching for its removal",
            state->name);
    }

    bool removed = state->last_exists && !exists;
    return UserWatcherRecordObservation(state, exists, removed, "removal");
}
