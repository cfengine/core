/*
   Copyright 2017 Northern.tech AS

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

#include <cf3.defs.h>

#include <mon.h>
#include <files_interfaces.h>

/* Globals */

static double LASTQ[CF_OBSERVABLES] = { 0.0 };

/* Prototypes */

static int GetFileGrowth(const char *filename, enum observables index);

/* Implementation */

void MonDiskGatherData(double *cf_this)
{
    char accesslog[CF_BUFSIZE];
    char errorlog[CF_BUFSIZE];
    char syslog[CF_BUFSIZE];
    char messages[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Gathering disk data");
    cf_this[ob_diskfree] = GetDiskUsage("/", CF_SIZE_PERCENT);
    Log(LOG_LEVEL_VERBOSE, "Disk free = %.0lf%%", cf_this[ob_diskfree]);

/* Here would should have some detection based on OS type VSYSTEMHARDCLASS */

    strcpy(accesslog, "/var/log/apache2/access_log");
    strcpy(errorlog, "/var/log/apache2/error_log");
    strcpy(syslog, "/var/log/syslog");
    strcpy(messages, "/var/log/messages");

    cf_this[ob_webaccess] = GetFileGrowth(accesslog, ob_webaccess);
    Log(LOG_LEVEL_VERBOSE, "Webaccess = %.2lf%%", cf_this[ob_webaccess]);
    cf_this[ob_weberrors] = GetFileGrowth(errorlog, ob_weberrors);
    Log(LOG_LEVEL_VERBOSE, "Web error = %.2lf%%", cf_this[ob_weberrors]);
    cf_this[ob_syslog] = GetFileGrowth(syslog, ob_syslog);
    Log(LOG_LEVEL_VERBOSE, "Syslog = %.2lf%%", cf_this[ob_syslog]);
    cf_this[ob_messages] = GetFileGrowth(messages, ob_messages);
    Log(LOG_LEVEL_VERBOSE, "Messages = %.2lf%%", cf_this[ob_messages]);
}

/****************************************************************************/

static int GetFileGrowth(const char *filename, enum observables index)
{
    struct stat statbuf;
    size_t q;
    double dq;

    if (stat(filename, &statbuf) == -1)
    {
        return 0;
    }

    q = statbuf.st_size;

    Log(LOG_LEVEL_VERBOSE, "GetFileGrowth(%s) = %zu", filename, q);

    dq = (double) q - LASTQ[index];

    if (LASTQ[index] == 0)
    {
        LASTQ[index] = q;
        return (int) (q / 100 + 0.5);   /* arbitrary spike mitigation */
    }

    LASTQ[index] = q;
    return (int) (dq + 0.5);
}
