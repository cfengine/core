/*
   Copyright 2019 Northern.tech AS

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

#include <string_lib.h>
#include <files_names.h>
#include <mon_cumulative.h>
#include <monitoring.h>
#include <probes.h>



#define SYSFSBLOCK "/sys/class/block/"

#define KB 1024

/************************************************************************/

static void SysfsizeDeviceName(char *name)
{
    while ((name = strchr(name, '/')))
    {
        *name = '!';
    }
}

static int DiskSectorSize(const char *sysfsname)
{
    char sysfspath[CF_BUFSIZE];
    FILE *fh;
    int size = 512;             /* Long-time default value */

    if (snprintf(sysfspath, CF_BUFSIZE, SYSFSBLOCK "%s/queue/logical_block_size", sysfsname) >= CF_BUFSIZE)
    {
        /* FIXME: report overlong string */
        return -1;
    }

    if (!(fh = fopen(sysfspath, "r")))
    {
        /*
         * Older Linux systems don't work correctly with new 4K drives. It's safe to
         * report 512 here
         */
    }
    else
    {
        if (fscanf(fh, "%d", &size) != 1)
        {
            if (ferror(fh))
            {
                Log(LOG_LEVEL_ERR, "Unable to read sector size for '%s'. Assuming 512 bytes. (fscanf: %s)",
                    sysfsname, GetErrorStr());
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Unable to read sector size for %s. Assuming 512 bytes.", sysfsname);
            }
        }
        fclose(fh);
    }

    return size;
}

static bool IsPartition(const char *sysfsname)
{
    char sysfspath[CF_BUFSIZE];

    if (snprintf(sysfspath, CF_BUFSIZE, "%s/partition", SYSFSBLOCK) >= CF_BUFSIZE)
    {
        /* FIXME: report overlong string */
        return false;
    }

    if (access(sysfspath, F_OK) == 0)
    {
        return true;
    }

    if (errno != ENOENT)
    {
        Log(LOG_LEVEL_ERR, "Unable to detect whether block device %s is a partition. (access: %s)",
            sysfsname, GetErrorStr());
    }

    return false;
}

static bool IsStackedDevice(const char *sysfsname)
{
/* "Stacked" as LVM or cryptfs volume */
    char sysfspath[CF_BUFSIZE];
    DIR *dir;
    struct dirent *dirent;

    if (snprintf(sysfspath, CF_BUFSIZE, "%s/slaves", SYSFSBLOCK) >= CF_BUFSIZE)
    {
        /* FIXME: report overlong string */
        return false;
    }

    if (!(dir = opendir(sysfspath)))
    {
        /* We don't have any information */
        return false;
    }

    errno = 0;
    while ((dirent = readdir(dir)))
    {
        if (strcmp(dirent->d_name, ".") && strcmp(dirent->d_name, ".."))
        {
            /* Found a file inside. It's a stacked device. */
            closedir(dir);
            return true;
        }
    }

    if (errno)
    {
        Log(LOG_LEVEL_ERR, "Unable to read 'slaves' sysfs subdirectory for '%s'. (readdir: %s)",
            sysfsname, GetErrorStr());
    }

    closedir(dir);
    return false;
}

static bool IsPhysicalDevice(const char *sysfsname)
{
    return !IsPartition(sysfsname) && !IsStackedDevice(sysfsname);
}

/************************************************************************/

static void MonIoDiskstatsGatherData(double *cf_this)
{
    char buf[CF_BUFSIZE];
    FILE *fh;
    time_t now = time(NULL);

    unsigned long long totalreads = 0, totalwrites = 0;
    unsigned long long totalreadbytes = 0, totalwrittenbytes = 0;

    if (!(fh = fopen("/proc/diskstats", "r")))
    {
        Log(LOG_LEVEL_ERR, "Error trying to open /proc/diskstats. (fopen: %s)", GetErrorStr());
        return;
    }

/* Read per-disk statistics */

    while (fgets(buf, CF_BUFSIZE, fh))
    {
        unsigned long reads, writes, readsectors, writtensectors;

        /* Format sanity check */
        if (!strchr(buf, '\n'))
        {
            Log(LOG_LEVEL_ERR,
                  "/proc/diskstats format error: read overlong string (> " TOSTRING(CF_BUFSIZE - 1) " bytes)");
            goto err;
        }

        if (StripTrailingNewline(buf, CF_EXPANDSIZE) == -1)
        {
            Log(LOG_LEVEL_ERR, "StripTrailingNewline was called on an overlong string");
        }

        char diskname[256];
        if (sscanf(buf, "%*u %*u %255s %lu %*u %lu %*u %lu %*u %lu",
                   diskname, &reads, &readsectors, &writes, &writtensectors) != 5)
        {
            Log(LOG_LEVEL_ERR, "Wrong /proc/diskstats line format: %s", buf);
            continue;
        }

        SysfsizeDeviceName(diskname);

        if (!reads && !readsectors && !writes && !writtensectors)
        {
            /* Not a disk. Something virtual: loopback or similar */
            continue;
        }

        if (!IsPhysicalDevice(diskname))
        {
            continue;
        }

        reads = GetInstantUint32Value(diskname, "reads", reads, now);
        writes = GetInstantUint32Value(diskname, "writes", writes, now);
        readsectors = GetInstantUint32Value(diskname, "readsectors", readsectors, now);
        writtensectors = GetInstantUint32Value(diskname, "writtensectors", writtensectors, now);

        if (reads != (unsigned) -1)
        {
            int sectorsize = DiskSectorSize(diskname);

            totalreads += reads;
            totalwrites += writes;
            totalreadbytes += ((unsigned long long) readsectors) * sectorsize;
            totalwrittenbytes += ((unsigned long long) writtensectors) * sectorsize;
        }
    }

    if (ferror(fh))
    {
        Log(LOG_LEVEL_ERR, "Error reading /proc/diskstats. (fgets: %s)", GetErrorStr());
        goto err;
    }

/* Summarize */
    int reads_slot = NovaRegisterSlot(MON_IO_READS, "Number of I/O reads", "reads per second",
                                      0.0, 1000.0, true);
    int writes_slot = NovaRegisterSlot(MON_IO_WRITES, "Number of I/O writes", "writes per second",
                                       0.0f, 1000.0f, true);
    int readdata_slot = NovaRegisterSlot(MON_IO_READDATA, "Amount of data read", "megabytes/s",
                                         0.0, 1000.0, true);
    int writtendata_slot = NovaRegisterSlot(MON_IO_WRITTENDATA, "Amount of data written", "megabytes",
                                            0.0, 1000.0, true);

    if (reads_slot != -1 && totalreads != 0)
    {
        cf_this[reads_slot] = totalreads;
    }
    if (writes_slot != -1 && totalwrites != 0)
    {
        cf_this[writes_slot] = totalwrites;
    }
    if (readdata_slot != -1 && totalreadbytes != 0)
    {
        cf_this[readdata_slot] = ((double) totalreadbytes) / KB / KB;
    }
    if (writtendata_slot != -1 && totalwrittenbytes != 0)
    {
        cf_this[writtendata_slot] = ((double) totalwrittenbytes) / KB / KB;
    }

  err:
    fclose(fh);
}

/************************************************************************/

static void MonIoPartitionsGatherData(ARG_UNUSED double *cf_this)
{
    /* FIXME: Not implemented */
}

/************************************************************************/

ProbeGatherData MonIoInit(const char **name, const char **error)
{
    if (access("/proc/diskstats", R_OK) == 0)
    {
        *name = "Linux 2.6 /proc/diskstats statistics";
        *error = NULL;
        return &MonIoDiskstatsGatherData;
    }
    else if (access("/proc/partitions", R_OK) == 0)
    {
        *name = "Linux 2.4 /proc/partitions statistics";
        *error = NULL;
        return &MonIoPartitionsGatherData;
    }
    else
    {
        *name = NULL;
        *error = "/proc/diskstats and /proc/partitions are not available";
        return NULL;
    }
}
