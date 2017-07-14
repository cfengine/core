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
#include <dir.h>
#include <item_lib.h>
#include <files_interfaces.h>
#include <pipes.h>

/* Globals */

static bool ACPI       = false;
static bool SYSTHERMAL = false;
static bool LMSENSORS  = false;

/* Prototypes */

#if defined(__linux__)
static bool GetAcpi(double *cf_this);
static bool GetSysThermal(double *cf_this);
static bool GetLMSensors(double *cf_this);
#endif

/* Implementation */

/******************************************************************************
 * Motherboard sensors - how to standardize this somehow
 * We're mainly interested in temperature and power consumption, but only the
 * temperature is generally available. Several temperatures exist too ...
 ******************************************************************************/

#if defined(__linux__)

void MonTempGatherData(double *cf_this)
{
    if (ACPI && GetAcpi(cf_this))
    {
        return;
    }

    if (SYSTHERMAL && GetSysThermal(cf_this))
    {
        return;
    }

    if (LMSENSORS && GetLMSensors(cf_this))
    {
        return;
    }
}

#else

void MonTempGatherData(ARG_UNUSED double *cf_this)
{
}

#endif

/******************************************************************************/

void MonTempInit(void)
{
    struct stat statbuf;

    for (int i = 0; i < 4; i++)
    {
        char s[128];
        xsnprintf(s, sizeof(s),
                  "/sys/devices/virtual/thermal/thermal_zone%d", i);

        if (stat(s, &statbuf) != -1)
        {
            Log(LOG_LEVEL_DEBUG, "Found a thermal device in /sys");
            SYSTHERMAL = true;
        }
    }

    if (stat("/proc/acpi/thermal_zone", &statbuf) != -1)
    {
        Log(LOG_LEVEL_DEBUG, "Found an acpi service");
        ACPI = true;
    }

    if (stat("/usr/bin/sensors", &statbuf) != -1)
    {
        if (statbuf.st_mode & 0111)
        {
            Log(LOG_LEVEL_DEBUG, "Found an lmsensor system");
            LMSENSORS = true;
        }
    }
}

/******************************************************************************/

#if defined(__linux__)
static bool GetAcpi(double *cf_this)
{
    Dir *dirh;
    FILE *fp;
    const struct dirent *dirp;
    int count;
    char path[CF_BUFSIZE], buf[CF_BUFSIZE], index[4];
    double temp;

    if ((dirh = DirOpen("/proc/acpi/thermal_zone")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Can't open directory '%s'. (opendir: %s)", path, GetErrorStr());
        return false;
    }

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
        {
            continue;
        }

        snprintf(path, CF_BUFSIZE, "/proc/acpi/thermal_zone/%s/temperature", dirp->d_name);

        if ((fp = fopen(path, "r")) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Couldn't open '%s' to gather temperature data (fopen: %s)", path, GetErrorStr());
            continue;
        }

        if (fgets(buf, sizeof(buf), fp) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Failed to read line from stream '%s'", path);
            fclose(fp);
            continue;
        }

        temp = 0.0;
        sscanf(buf, "%*s %lf", &temp);

        for (count = 0; count < 4; count++)
        {
            snprintf(index, 2, "%d", count);

            if (strstr(dirp->d_name, index))
            {
                switch (count)
                {
                case 0:
                    cf_this[ob_temp0] = temp;
                    break;
                case 1:
                    cf_this[ob_temp1] = temp;
                    break;
                case 2:
                    cf_this[ob_temp2] = temp;
                    break;
                case 3:
                    cf_this[ob_temp3] = temp;
                    break;
                }

                Log(LOG_LEVEL_DEBUG, "Set temp%d to %lf", count, temp);
            }
        }
        fclose(fp);
    }

    DirClose(dirh);
    return true;
}

/******************************************************************************/

static bool GetSysThermal(double *cf_this)
{
    FILE *fp;
    int count;
    bool retval = false;

    for (count = 0; count < 4; count++)
    {
        double temp = 0;

        char path[128];
        xsnprintf(path, sizeof(path),
                  "/sys/devices/virtual/thermal/thermal_zone%d/temp", count);

        if ((fp = fopen(path, "r")) == NULL)
        {
            Log(LOG_LEVEL_INFO, "Couldn't open '%s'", path);
            continue;
        }

        char buf[128];
        if (fgets(buf, sizeof(buf), fp) == NULL)
        {
            Log(LOG_LEVEL_INFO, "Failed to read line from stream '%s'", path);
            fclose(fp);
            continue;
        }

        int ret = sscanf(buf, "%lf", &temp);
        if (ret == 1)
        {
            switch (count)
            {
            case 0:
                cf_this[ob_temp0] = temp;
                break;
            case 1:
                cf_this[ob_temp1] = temp;
                break;
            case 2:
                cf_this[ob_temp2] = temp;
                break;
            case 3:
                cf_this[ob_temp3] = temp;
                break;
            }

            Log(LOG_LEVEL_DEBUG, "Set temp%d to %lf", count, temp);
            retval = true;
        }
        else
        {
            Log(LOG_LEVEL_INFO, "Failed to read number from: %s", path);
        }

        fclose(fp);
    }

    return retval;
}

/******************************************************************************/

static bool GetLMSensors(double *cf_this)
{
    FILE *pp;
    Item *ip, *list = NULL;
    double temp = 0;
    char name[CF_BUFSIZE];
    int count;

    cf_this[ob_temp0] = 0.0;
    cf_this[ob_temp1] = 0.0;
    cf_this[ob_temp2] = 0.0;
    cf_this[ob_temp3] = 0.0;

    if ((pp = cf_popen("/usr/bin/sensors", "r", true)) == NULL)
    {
        LMSENSORS = false;      /* Broken */
        return false;
    }

    {
        size_t vbuff_size = CF_BUFSIZE;
        char *vbuff = xmalloc(vbuff_size);

        ssize_t res = CfReadLine(&vbuff, &vbuff_size, pp);
        if (res <= 0)
        {
            /* FIXME: do we need to log anything here? */
            cf_pclose(pp);
            free(vbuff);
            return false;
        }

        for (;;)
        {
            ssize_t res = CfReadLine(&vbuff, &vbuff_size, pp);
            if (res == -1)
            {
                if (!feof(pp))
                {
                    /* FIXME: Do we need to log anything here? */
                    cf_pclose(pp);
                    free(vbuff);
                    return false;
                }
                else
                {
                    break;
                }
            }

            if (strstr(vbuff, "Temp") || strstr(vbuff, "temp"))
            {
                PrependItem(&list, vbuff, NULL);
            }
        }

        cf_pclose(pp);
        free(vbuff);
    }

    if (ListLen(list) > 0)
    {
        Log(LOG_LEVEL_DEBUG, "LM Sensors seemed to return ok data");
    }
    else
    {
        return false;
    }

/* lmsensor names are hopelessly inconsistent - so try a few things */

    for (ip = list; ip != NULL; ip = ip->next)
    {
        for (count = 0; count < 4; count++)
        {
            snprintf(name, 16, "CPU%d Temp:", count);

            if (strncmp(ip->name, name, strlen(name)) == 0)
            {
                sscanf(ip->name, "%*[^:]: %lf", &temp);

                switch (count)
                {
                case 0:
                    cf_this[ob_temp0] = temp;
                    break;
                case 1:
                    cf_this[ob_temp1] = temp;
                    break;
                case 2:
                    cf_this[ob_temp2] = temp;
                    break;
                case 3:
                    cf_this[ob_temp3] = temp;
                    break;
                }

                Log(LOG_LEVEL_DEBUG, "Set temp%d to %lf from what looks like cpu temperature", count, temp);
            }
        }
    }

    if (cf_this[ob_temp0] != 0)
    {
        /* We got something plausible */
        return true;
    }

/* Alternative name Core x: */

    for (ip = list; ip != NULL; ip = ip->next)
    {
        for (count = 0; count < 4; count++)
        {
            snprintf(name, 16, "Core %d:", count);

            if (strncmp(ip->name, name, strlen(name)) == 0)
            {
                sscanf(ip->name, "%*[^:]: %lf", &temp);

                switch (count)
                {
                case 0:
                    cf_this[ob_temp0] = temp;
                    break;
                case 1:
                    cf_this[ob_temp1] = temp;
                    break;
                case 2:
                    cf_this[ob_temp2] = temp;
                    break;
                case 3:
                    cf_this[ob_temp3] = temp;
                    break;
                }

                Log(LOG_LEVEL_DEBUG, "Set temp%d to %lf from what looks like core temperatures", count, temp);
            }
        }
    }

    if (cf_this[ob_temp0] != 0)
    {
        /* We got something plausible */
        return true;
    }

    for (ip = list; ip != NULL; ip = ip->next)
    {
        if (strncmp(ip->name, "CPU Temp:", strlen("CPU Temp:")) == 0)
        {
            sscanf(ip->name, "%*[^:]: %lf", &temp);
            Log(LOG_LEVEL_DEBUG, "Setting temp0 to CPU Temp");
            cf_this[ob_temp0] = temp;
        }

        if (strncmp(ip->name, "M/B Temp:", strlen("M/B Temp:")) == 0)
        {
            sscanf(ip->name, "%*[^:]: %lf", &temp);
            Log(LOG_LEVEL_DEBUG, "Setting temp0 to M/B Temp");
            cf_this[ob_temp1] = temp;
        }

        if (strncmp(ip->name, "Sys Temp:", strlen("Sys Temp:")) == 0)
        {
            sscanf(ip->name, "%*[^:]: %lf", &temp);
            Log(LOG_LEVEL_DEBUG, "Setting temp0 to Sys Temp");
            cf_this[ob_temp2] = temp;
        }

        if (strncmp(ip->name, "AUX Temp:", strlen("AUX Temp:")) == 0)
        {
            sscanf(ip->name, "%*[^:]: %lf", &temp);
            Log(LOG_LEVEL_DEBUG, "Setting temp0 to AUX Temp");
            cf_this[ob_temp3] = temp;
        }
    }

    if (cf_this[ob_temp0] != 0)
    {
        /* We got something plausible */
        return true;
    }

/* Alternative name Core x: */

    for (ip = list; ip != NULL; ip = ip->next)
    {
        for (count = 0; count < 4; count++)
        {
            snprintf(name, 16, "temp%d:", count);

            if (strncmp(ip->name, name, strlen(name)) == 0)
            {
                sscanf(ip->name, "%*[^:]: %lf", &temp);

                switch (count)
                {
                case 0:
                    cf_this[ob_temp0] = temp;
                    break;
                case 1:
                    cf_this[ob_temp1] = temp;
                    break;
                case 2:
                    cf_this[ob_temp2] = temp;
                    break;
                case 3:
                    cf_this[ob_temp3] = temp;
                    break;
                }

                Log(LOG_LEVEL_DEBUG, "Set temp%d to %lf", count, temp);
            }
        }
    }

/* Give up? */
    DeleteItemList(list);
    return true;
}

#endif
