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

#include <package_watcher.h>
#include <package_module_wrapper.h>
#include <stoppable_thread.h>
#include <signal_lib.h>
#include <eval_context.h>
#include <logging.h>
#include <alloc.h>
#include <rlist.h>
#include <set.h>
#include <sequence.h>
#include <mutex.h>
#include <string_lib.h>

/* Timeout of a run of the package module. Only the listing thread of the
 * module waits for it. Tests can define it before including this file. */
#ifndef LISTING_TIMEOUT_SECS
#define LISTING_TIMEOUT_SECS 60
#endif

/* Minimum time between the starts of two listings of a package module, since
 * a listing can take a while. Tests can define it before including this file.
 */
#ifndef LISTING_INTERVAL_SECS
#define LISTING_INTERVAL_SECS 10
#endif

/* Minimum time between two attempts to create the wrapper of a package
 * module that can't be run. Longer, since each attempt logs errors. Tests can
 * define it before including this file. */
#ifndef LISTING_RETRY_INTERVAL_SECS
#define LISTING_RETRY_INTERVAL_SECS 60
#endif

/* How long to wait for a listing thread to stop before giving up on it */
#define LISTING_THREAD_STOP_TIMEOUT_SECS 5

/* Minimum time between two logs of a persisting failure to list packages */
#define LISTING_ERROR_LOG_INTERVAL_SECS 60

/* The installed packages of a package module, listed by its own thread.
 * Shared by all watchers of the package module. */
typedef struct
{
    char *key;                 /* see PackageModuleKey() */
    PackageModuleBody *module; /* copy of the package module body */
    size_t refcount;           /* watchers using it, under listings_mutex */
    StoppableThread *thread;
    time_t last_error_logged; /* only used by the listing thread */

    pthread_mutex_t mutex; /* protects the fields below */
    StringSet *installed;  /* names, NULL if the listing failed */
    time_t listed_at;      /* start of the latest listing, 0 if none */
} PackageListing;

/* All listings, under listings_mutex. Only changed on the main thread. */
static pthread_mutex_t listings_mutex = PTHREAD_MUTEX_INITIALIZER;
static Seq *listings = NULL;

typedef struct
{
    char *name;
    PackageListing *listing;
    bool observed;       /* whether a listing was observed yet */
    bool last_installed; /* what the last observed listing said */
} PackageWatcherState;

/**
 * @brief Returns what tells two package modules apart when listing.
 *
 * Two bodies with the same name, path, interpreter and options list the
 * same packages, so their watchers share a package listing.
 *
 * @param module The package module body
 * @return The key, to be freed by the caller
 */
static char *PackageModuleKey(const PackageModuleBody *module)
{
    assert(module != NULL);

    char *options = PackageModuleFormatOptions(module->options);
    char *key = StringFormat(
        "%s\n%s\n%s\n%s",
        module->name,
        (module->module_path != NULL) ? module->module_path : "",
        (module->interpreter != NULL) ? module->interpreter : "",
        options);
    free(options);
    return key;
}

/**
 * @brief Logs a problem listing the packages of a package module.
 *
 * The caller logs the actual problem at verbose level. A persisting problem is
 * logged at most once every LISTING_ERROR_LOG_INTERVAL_SECS. Only called by
 * the listing thread.
 *
 * @param listing The package listing
 */
static void LogListingError(PackageListing *listing)
{
    assert(listing != NULL);

    const time_t now = time(NULL);
    if ((listing->last_error_logged == 0)
        || (now - listing->last_error_logged
            >= LISTING_ERROR_LOG_INTERVAL_SECS))
    {
        Log(LOG_LEVEL_ERR,
            "Problem listing installed packages with package module '%s' while checking for package events, run with verbose logging for details",
            listing->module->name);
        listing->last_error_logged = now;
    }
}

/**
 * @brief Adds the name of a record of a 'list-installed' reply.
 *
 * @param installed The names of the installed packages
 * @param name The name of the record, may be NULL
 * @param version The version of the record, may be NULL
 * @param arch The architecture of the record, may be NULL
 */
static void AddInstalledPackage(
    StringSet *installed,
    const char *name,
    const char *version,
    const char *arch)
{
    assert(installed != NULL);

    if (name == NULL)
    {
        /* No record yet, i.e. before the first Name= line or an empty reply.
         * Skipped, so that callers don't need to check. */
        return;
    }
    if (version == NULL || arch == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Malformed response from package module for package '%s'",
            name);
        return;
    }
    if (!StringSetContains(installed, name))
    {
        StringSetAdd(installed, xstrdup(name));
    }
}

/**
 * @brief Parses a 'list-installed' reply of a package module.
 *
 * @param response The lines of the reply
 * @param module_name The name of the package module, for logging
 * @return The names of the installed packages, or NULL if the reply has
 *         errors
 */
static StringSet *ParseInstalledPackages(
    const Rlist *response, const char *module_name)
{
    assert(module_name != NULL);

    StringSet *installed = StringSetNew();
    const char *name = NULL;
    const char *version = NULL;
    const char *arch = NULL;
    bool failed = false;
    for (const Rlist *rp = response; rp != NULL; rp = rp->next)
    {
        const char *line = RlistScalarValue(rp);
        if (StringStartsWith(line, "Name="))
        {
            /* A new record, meaning the previous one, is complete. If there is
             * no previous one, then it's skipped. */
            AddInstalledPackage(installed, name, version, arch);
            name = line + strlen("Name=");
            version = NULL;
            arch = NULL;
        }
        else if (StringStartsWith(line, "Version="))
        {
            version = line + strlen("Version=");
        }
        else if (StringStartsWith(line, "Architecture="))
        {
            arch = line + strlen("Architecture=");
        }
        else if (
            StringStartsWith(line, "Error=")
            || StringStartsWith(line, "ErrorMessage="))
        {
            Log(LOG_LEVEL_VERBOSE,
                "Package module '%s' replied with an error: %s",
                module_name,
                line);
            /* Not returning yet, so that all error lines get logged, e.g. an
             * ErrorMessage= line with the details after an Error= line */
            failed = true;
        }
    }
    /* The last record, if any */
    AddInstalledPackage(installed, name, version, arch);

    if (failed)
    {
        /* Never taken as "nothing installed" */
        StringSetDestroy(installed);
        return NULL;
    }
    return installed;
}

/**
 * @brief Lists the installed packages with the package module.
 *
 * @param listing The listing
 * @param wrapper The wrapper of the package module
 * @return The names of the installed packages, or NULL on failure
 */
static StringSet *ListInstalledPackages(
    PackageListing *listing, const PackageModuleWrapper *wrapper)
{
    assert(listing != NULL);
    assert(wrapper != NULL);

    char *options = PackageModuleFormatOptions(listing->module->options);
    Rlist *response = NULL;
    const int ret = PackageWrapperCommunicate(
        wrapper, "list-installed", options, &response);
    free(options);

    if (ret != 0)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Package module '%s' failed to list installed packages",
            listing->module->name);
        LogListingError(listing);
        return NULL;
    }

    StringSet *installed =
        ParseInstalledPackages(response, listing->module->name);
    RlistDestroy(response);

    if (installed == NULL)
    {
        LogListingError(listing);
    }
    return installed;
}

/**
 * @brief The routine of the listing thread of a package module.
 *
 * Lists the installed packages ahead of the checks, every
 * LISTING_INTERVAL_SECS, so that a check sees a recent listing whatever its
 * interval.
 *
 * @param thread The listing thread
 * @param arg The listing
 */
static void ListingThreadMain(StoppableThread *thread, void *arg)
{
#ifndef __MINGW32__
    MaskTerminationSignalsInThread();
#endif

    PackageListing *listing = arg;
    assert(listing != NULL);

    /* Created on the first listing, so that negotiating the API version
     * runs the package module on this thread, not on the main one */
    PackageModuleWrapper *wrapper = NULL;

    while (!StoppableThreadShouldStop(thread))
    {
        const time_t start = time(NULL);
        if (wrapper == NULL)
        {
            wrapper =
                NewPackageModuleWrapper(listing->module, LISTING_TIMEOUT_SECS);
        }
        const time_t wait_secs = (wrapper != NULL)
                                     ? LISTING_INTERVAL_SECS
                                     : LISTING_RETRY_INTERVAL_SECS;

        StringSet *installed = NULL;
        if (wrapper != NULL)
        {
            installed = ListInstalledPackages(listing, wrapper);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE,
                "Package module '%s' can't be run",
                listing->module->name);
            LogListingError(listing);
        }

        ThreadLock(&listing->mutex);
        StringSetDestroy(listing->installed);
        listing->installed = installed;
        listing->listed_at = start;
        ThreadUnlock(&listing->mutex);

        /* Counted from the start, so that a slow listing doesn't delay the
         * next one. Sleeps in a loop, since a sleep can end early. */
        const time_t next = start + wait_secs;
        time_t now;
        while (!StoppableThreadShouldStop(thread)
               && ((now = time(NULL)) < next))
        {
            StoppableThreadSleep(thread, next - now);
        }
    }

    DeletePackageModuleWrapper(wrapper);
}

/**
 * @brief Destroys a listing whose thread has stopped.
 *
 * @param listing The listing
 */
static void PackageListingDestroy(PackageListing *listing)
{
    assert(listing != NULL);

    free(listing->key);
    PackageModuleBodyDestroy(listing->module);
    StringSetDestroy(listing->installed);
    pthread_mutex_destroy(&listing->mutex);
    free(listing);
}

/**
 * @brief Gets the listing of a package module, shared by its watchers.
 *
 * Creates it, and starts its thread, if no watcher uses the package module
 * yet. Only called on the main thread.
 *
 * @param module The package module body, copied
 * @return The listing, to be released with PackageListingRelease(), or NULL
 *         if its thread can't be started
 */
static PackageListing *PackageListingAcquire(const PackageModuleBody *module)
{
    assert(module != NULL);

    char *key = PackageModuleKey(module);

    ThreadLock(&listings_mutex);
    if (listings == NULL)
    {
        listings = SeqNew(1, NULL);
    }

    for (size_t i = 0; i < SeqLength(listings); i++)
    {
        PackageListing *listing = SeqAt(listings, i);
        if (StringEqual(listing->key, key))
        {
            listing->refcount++;
            ThreadUnlock(&listings_mutex);
            free(key);
            return listing;
        }
    }

    PackageListing *listing = xcalloc(1, sizeof(PackageListing));
    listing->key = key;
    listing->module = PackageModuleBodyCopy(module);
    pthread_mutex_init(&listing->mutex, NULL);
    listing->refcount = 1;

    listing->thread = StoppableThreadStart(ListingThreadMain, listing);
    if (listing->thread == NULL)
    {
        ThreadUnlock(&listings_mutex);
        PackageListingDestroy(listing);
        return NULL;
    }

    SeqAppend(listings, listing);
    ThreadUnlock(&listings_mutex);
    return listing;
}

/**
 * @brief Releases a listing got with PackageListingAcquire().
 *
 * The last watcher to release it stops its thread and destroys it. If the
 * thread doesn't stop in time, e.g. because the package module hangs, it's
 * given up on, and the listing is leaked on purpose, since the thread may
 * still use it. It's then only freed when cf-reactor exits. Only called on
 * the main thread.
 *
 * @param listing The listing
 */
static void PackageListingRelease(PackageListing *listing)
{
    assert(listing != NULL);

    ThreadLock(&listings_mutex);
    assert(listing->refcount > 0);
    listing->refcount--;
    if (listing->refcount > 0)
    {
        ThreadUnlock(&listings_mutex);
        return;
    }

    for (size_t i = 0; i < SeqLength(listings); i++)
    {
        if (SeqAt(listings, i) == listing)
        {
            SeqSoftRemove(listings, i);
            break;
        }
    }
    ThreadUnlock(&listings_mutex);

    if (StoppableThreadStop(listing->thread, LISTING_THREAD_STOP_TIMEOUT_SECS))
    {
        PackageListingDestroy(listing);
    }
}

/**
 * @brief Creates the state of a package watcher for the given package module.
 *
 * @param name The name of the package to watch
 * @param module The package module body, copied
 * @return The new state, or NULL if the listing thread can't be started
 */
static PackageWatcherState *PackageWatcherStateNewFromModule(
    const char *name, const PackageModuleBody *module)
{
    assert(name != NULL);
    assert(module != NULL);

    PackageListing *listing = PackageListingAcquire(module);
    if (listing == NULL)
    {
        return NULL;
    }

    PackageWatcherState *state = xcalloc(1, sizeof(PackageWatcherState));
    state->name = xstrdup(name);
    state->listing = listing;
    return state;
}

void *PackageWatcherStateNew(const EvalContext *ctx, const Rlist *args)
{
    assert(ctx != NULL);
    assert(args != NULL);

    const char *name = RlistScalarValue(args);
    const PackageModuleBody *module = NULL;
    if (args->next != NULL)
    {
        const char *module_name = RlistScalarValue(args->next);
        module = GetPackageModuleFromContext(ctx, module_name);
        if (module == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "No package module body '%s' to watch package '%s' with",
                module_name,
                name);
            return NULL;
        }
    }
    else
    {
        module = GetDefaultPackageModuleFromContext(ctx);
        if (module == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "No default package module to watch package '%s' with, "
                "set 'package_module' in the 'when' body or in body common control",
                name);
            return NULL;
        }
    }

    return PackageWatcherStateNewFromModule(name, module);
}

void PackageWatcherStateDestroy(void *_state)
{
    PackageWatcherState *state = _state;
    if (state != NULL)
    {
        PackageListingRelease(state->listing);
        free(state->name);
        free(state);
    }
}

/**
 * @brief The body of PackageLookup(), only called by it, with the mutex of
 *        the listing held.
 *
 * @param listing The listing
 * @param name The name of the package
 * @param[out] listed Whether there is a listing yet
 * @param[out] installed Whether the package is installed, only set if listed
 * @return false if the latest listing failed
 */
static bool _PackageLookupInner(
    const PackageListing *listing,
    const char *name,
    bool *listed,
    bool *installed)
{
    assert(listing != NULL);
    assert(name != NULL);
    assert(listed != NULL);
    assert(installed != NULL);

    *listed = (listing->listed_at != 0);
    if (!*listed)
    {
        return true;
    }
    if (listing->installed == NULL)
    {
        return false;
    }
    *installed = StringSetContains(listing->installed, name);
    return true;
}

/**
 * @brief Looks up the package in the latest listing of its package module.
 *
 * @param state The watcher state
 * @param[out] listed Whether there is a listing yet
 * @param[out] installed Whether the package is installed, only set if listed
 * @return false if the latest listing failed
 */
static bool PackageLookup(
    PackageWatcherState *state, bool *listed, bool *installed)
{
    assert(state != NULL);

    PackageListing *listing = state->listing;
    ThreadLock(&listing->mutex);
    const bool ok =
        _PackageLookupInner(listing, state->name, listed, installed);
    ThreadUnlock(&listing->mutex);
    return ok;
}

/**
 * @brief Records an observed listing.
 *
 * The first observed listing is only the reference for the next ones, so it
 * never fires.
 *
 * @param state The watcher state
 * @param installed Whether the package is installed
 * @param happened Whether the event happened since the last observation
 * @param event The name of the event for logging (e.g. "installation")
 * @return WATCHER_CHECK_EVENT if the event fired, WATCHER_CHECK_NO_EVENT
 *         otherwise
 */
static WatcherCheckResult PackageWatcherRecordObservation(
    PackageWatcherState *state,
    bool installed,
    bool happened,
    const char *event)
{
    assert(state != NULL);
    assert(event != NULL);

    happened = happened && state->observed;
    if (happened)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Observed %s of package '%s'",
            event,
            state->name);
    }

    state->observed = true;
    state->last_installed = installed;
    return happened ? WATCHER_CHECK_EVENT : WATCHER_CHECK_NO_EVENT;
}

WatcherCheckResult PackageWatcherCheckPackageInstalled(void *_state)
{
    assert(_state != NULL);
    PackageWatcherState *state = _state;

    bool listed = false;
    bool installed = false;
    if (!PackageLookup(state, &listed, &installed))
    {
        /* Unknown state, keep the last observation as is */
        return WATCHER_CHECK_ERROR;
    }
    if (!listed)
    {
        return WATCHER_CHECK_NO_EVENT;
    }

    /* Logs only if the package was uninstalled, or on the first observation.
     */
    if (!installed && (state->last_installed || !state->observed))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Package '%s' is not installed, watching for its installation",
            state->name);
    }

    const bool happened = !state->last_installed && installed;
    return PackageWatcherRecordObservation(
        state, installed, happened, "installation");
}

WatcherCheckResult PackageWatcherCheckPackageUninstalled(void *_state)
{
    assert(_state != NULL);
    PackageWatcherState *state = _state;

    bool listed = false;
    bool installed = false;
    if (!PackageLookup(state, &listed, &installed))
    {
        /* Unknown state, keep the last observation as is */
        return WATCHER_CHECK_ERROR;
    }
    if (!listed)
    {
        return WATCHER_CHECK_NO_EVENT;
    }

    if (installed && !state->last_installed)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Package '%s' is installed, watching for its uninstallation",
            state->name);
    }

    const bool happened = state->last_installed && !installed;
    return PackageWatcherRecordObservation(
        state, installed, happened, "uninstallation");
}
