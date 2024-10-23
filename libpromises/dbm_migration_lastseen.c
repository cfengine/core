/*
  Copyright 2022 Northern.tech AS

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

#include <dbm_migration.h>

#include <lastseen.h>
#include <logging.h>

typedef struct
{
    double q;
    double expect;
    double var;
} QPoint0;

#define QPOINT0_OFFSET 128

/*
 * Structure of version 0 lastseen entry:
 *
 * flag | hostkey -> address | QPoint
 *  |        |          |         \- 3*double
 *  |        |          \- 128 chars
 *  |        \- N*chars
 *  \- 1 byte, '+' or '-'
 */

static bool LastseenMigrationVersion0(DBHandle *db)
{
    /* For some reason DB migration for LMDB was disabled in 2014 (in commit
     * 8611970bfa33be7b3cf0724eb684833e08582850). Unfortunately there is no
     * mention as to why this was done. Maybe it was not working?
     *
     * However, we're re-enabling it now (10 years later). Since this
     * migration function has not been active for the last 10 years, the
     * safest thing is to remove the migration logic, and only update the
     * version number.
     *
     * If you have not upgraded CFEngine in the last 10 years, this will be
     * the least of your problems.
     */
    return WriteDB(db, "version", "1", sizeof("1"));
}

const DBMigrationFunction dbm_migration_plan_lastseen[] =
{
    LastseenMigrationVersion0,
    NULL
};
