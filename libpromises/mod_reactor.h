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

#ifndef CFENGINE_MOD_REACTOR_H
#define CFENGINE_MOD_REACTOR_H

#include <cf3.defs.h>
#include <policy.h>

#define REACTOR_FILE_DELETED "file_deleted"
#define REACTOR_FILE_CREATED "file_created"
#define REACTOR_FILE_MODIFIED "file_modified"
#define REACTOR_FILE_CONTENT_MODIFIED "file_content_modified"
#define REACTOR_FILE_PERMISSIONS_CHANGED "file_permissions_changed"
#define REACTOR_FILE_OWNER_CHANGED "file_owner_changed"
#define REACTOR_FILE_SIZE_EXCEEDS "file_size_exceeds"
#define REACTOR_SIZE_THRESHOLD "size_threshold"

/* Range of the 'interval' attribute of the 'when' body of events promises */
#define REACTOR_INTERVAL_RANGE "1,99999999999"

/* Types of the bundles the 'then' attribute of events promises can refer to,
 * in order of lookup, terminated by NULL */
extern const char *const REACTOR_THEN_BUNDLE_TYPES[];

/* Promise types of the reactor bundles with events promises, in order of
 * evaluation, terminated by NULL */
extern const char *const REACTOR_EVENTS_BUNDLE_PROMISE_TYPES[];

/**
 * @brief Whether the bundle is a reactor bundle with events promises.
 *
 * Reactor bundles are of two mutually exclusive kinds:
 * - with events promises, they can only have the promise types of
 *   REACTOR_EVENTS_BUNDLE_PROMISE_TYPES. cf-reactor evaluates them when it
 *   reads the policy, registering the watchers of their events promises.
 * - without events promises, they can have every promise type of agent
 *   bundles. cf-reactor runs them like cf-agent runs agent bundles, from the
 *   'then' attribute of events promises.
 */
bool BundleIsReactorEventsBundle(const Bundle *bp);

/**
 * @brief Checks that a reactor bundle with events promises has no other
 *        promise types than those of REACTOR_EVENTS_BUNDLE_PROMISE_TYPES.
 * @return false if it has, appending the errors to errors, true otherwise
 */
bool ReactorBundleCheck(const Bundle *bp, Seq *errors);

extern const PromiseTypeSyntax CF_REACTOR_PROMISE_TYPES[];

#endif
