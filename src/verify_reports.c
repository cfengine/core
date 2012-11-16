/* 
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.

*/

#include "cf3.defs.h"

#include "dbm_api.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "item_lib.h"
#include "vars.h"
#include "sort.h"
#include "vars.h"
#include "attributes.h"
#include "cfstream.h"
#include "communication.h"

static void ShowState(char *type);
static void PrintFile(Attributes a, Promise *pp);

/*******************************************************************/
/* Agent reporting                                                 */
/*******************************************************************/

void VerifyReportPromise(Promise *pp)
{
    Attributes a = { {0} };
    CfLock thislock;
    Rlist *rp;
    char unique_name[CF_EXPANDSIZE];

    a = GetReportsAttributes(pp);

    snprintf(unique_name, CF_EXPANDSIZE - 1, "%s_%zu", pp->promiser, pp->offset.line);
    thislock = AcquireLock(unique_name, VUQNAME, CFSTARTTIME, a, pp, false);

    // Handle return values before locks, as we always do this

    if (a.report.result)
    {
        // User-unwritable value last-result contains the useresult
        if (strlen(a.report.result) > 0)
        {
            snprintf(unique_name, CF_BUFSIZE, "last-result[%s]", a.report.result);
        }
        else
        {
            snprintf(unique_name, CF_BUFSIZE, "last-result");
        }

        NewScalar(pp->bundle, unique_name, pp->promiser, cf_str);
        return;
    }
       
    // Now do regular human reports
    
    if (thislock.lock == NULL)
    {
        return;
    }

    PromiseBanner(pp);

    cfPS(cf_verbose, CF_CHG, "", pp, a, "Report: %s", pp->promiser);

    if (a.report.to_file)
    {
        CfFOut(a.report.to_file, cf_error, "", "%s", pp->promiser);
    }
    else
    {
        CfOut(cf_reporting, "", "R: %s", pp->promiser);
    }

    if (a.report.haveprintfile)
    {
        PrintFile(a, pp);
    }

    if (a.report.showstate)
    {
        for (rp = a.report.showstate; rp != NULL; rp = rp->next)
        {
            ShowState(rp->item);
        }
    }

    if (a.report.havelastseen)
    {
        /* Do nothing. Deprecated. */
    }

    YieldCurrentLock(thislock);
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static void PrintFile(Attributes a, Promise *pp)
{
    FILE *fp;
    char buffer[CF_BUFSIZE];
    int lines = 0;

    if (a.report.filename == NULL)
    {
        CfOut(cf_verbose, "", "Printfile promise was incomplete, with no filename.\n");
        return;
    }

    if ((fp = fopen(a.report.filename, "r")) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "fopen", pp, a, " !! Printing of file %s was not possible.\n", a.report.filename);
        return;
    }

    while ((!feof(fp)) && (lines < a.report.numlines))
    {
        buffer[0] = '\0';
        fgets(buffer, CF_BUFSIZE, fp);
        CfOut(cf_error, "", "R: %s", buffer);
        lines++;
    }

    fclose(fp);
}

/*********************************************************************/

static void ShowState(char *type)
{
    struct stat statbuf;
    char buffer[CF_BUFSIZE], vbuff[CF_BUFSIZE], assemble[CF_BUFSIZE];
    Item *addresses = NULL, *saddresses = NULL, *ip;
    int i = 0, tot = 0, min_signal_diversity = 1, conns = 1;
    int maxlen = 0, count;
    double *dist = NULL, S = 0.0;
    char *offset = NULL;
    FILE *fp;

    CfDebug("ShowState(%s)\n", type);

    snprintf(buffer, CF_BUFSIZE - 1, "%s/state/cf_%s", CFWORKDIR, type);

    if (cfstat(buffer, &statbuf) == 0)
    {
        if ((fp = fopen(buffer, "r")) == NULL)
        {
            CfOut(cf_inform, "fopen", "Could not open state memory %s\n", buffer);
            return;
        }

        while (!feof(fp))
        {
            char local[CF_BUFSIZE], remote[CF_BUFSIZE];

            buffer[0] = local[0] = remote[0] = '\0';

            memset(vbuff, 0, CF_BUFSIZE);
            fgets(buffer, CF_BUFSIZE, fp);

            if (strlen(buffer) > 0)
            {
                CfOut(cf_verbose, "", "(%2d) %s", conns, buffer);

                if (IsSocketType(type))
                {
                    if ((strncmp(type, "incoming", 8) == 0) || (strncmp(type, "outgoing", 8) == 0))
                    {
                        if (strncmp(buffer, "tcp", 3) == 0)
                        {
                            sscanf(buffer, "%*s %*s %*s %s %s", local, remote); /* linux-like */
                        }
                        else
                        {
                            sscanf(buffer, "%s %s", local, remote);     /* solaris-like */
                        }

                        strncpy(vbuff, remote, CF_BUFSIZE - 1);
                        DePort(vbuff);
                    }
                }
                else if (IsTCPType(type))
                {
                    count = 1;
                    sscanf(buffer, "%d %[^\n]", &count, remote);
                    AppendItem(&addresses, remote, "");
                    SetItemListCounter(addresses, remote, count);
                    conns += count;
                    continue;
                }
                else
                {
                    /* If we get here this is a process thing */
                    if (offset == NULL)
                    {
                        if ((offset = strstr(buffer, "CMD")))
                        {
                        }
                        else if ((offset = strstr(buffer, "COMMAND")))
                        {
                        }

                        if (offset == NULL)
                        {
                            continue;
                        }
                    }

                    strncpy(vbuff, offset, CF_BUFSIZE - 1);
                    Chop(vbuff);
                }

                if (!IsItemIn(addresses, vbuff))
                {
                    conns++;
                    AppendItem(&addresses, vbuff, "");
                    IncrementItemListCounter(addresses, vbuff);
                }
                else
                {
                    conns++;
                    IncrementItemListCounter(addresses, vbuff);
                }
            }
        }

        fclose(fp);
        conns--;

        CfOut(cf_error, "", "\n");
        CfOut(cf_error, "", "R: The peak measured state was q = %d:\n", conns);

        if ((IsSocketType(type)) || (IsTCPType(type)))
        {
            for (ip = addresses; ip != NULL; ip = ip->next)
            {
                tot += ip->counter;

                buffer[0] = '\0';
                sscanf(ip->name, "%s", buffer);

                if ((!IsIPV4Address(buffer)) && (!IsIPV6Address(buffer)))
                {
                    CfOut(cf_verbose, "", "Rejecting address %s\n", ip->name);
                    continue;
                }

                CfOut(cf_error, "", "R: DNS key: %s = %s (%d/%d)\n", buffer, IPString2Hostname(buffer), ip->counter,
                      conns);

                if (strlen(ip->name) > maxlen)
                {
                    maxlen = strlen(ip->name);
                }
            }

            if (addresses != NULL)
            {
                printf("R: -\n");
            }
        }
        else
        {
            for (ip = addresses; ip != NULL; ip = ip->next)
            {
                tot += ip->counter;
            }
        }

        addresses = SortItemListCounters(addresses);
        saddresses = addresses;

        for (ip = saddresses; ip != NULL; ip = ip->next)
        {
            int s;

            if (maxlen > 17)    /* ipv6 */
            {
                snprintf(assemble, CF_BUFSIZE, "Frequency: %-40s|", ip->name);
            }
            else
            {
                snprintf(assemble, CF_BUFSIZE, "Frequency: %-17s|", ip->name);
            }

            for (s = 0; (s < ip->counter) && (s < 50); s++)
            {
                if (s < 48)
                {
                    strcat(assemble, "*");
                }
                else
                {
                    strcat(assemble, "+");
                }
            }

            CfOut(cf_error, "", "R: %s \t(%d/%d)\n", assemble, ip->counter, conns);
        }

        dist = xmalloc((tot + 1) * sizeof(double));

        if (conns > min_signal_diversity)
        {
            for (i = 0, ip = addresses; ip != NULL; i++, ip = ip->next)
            {
                dist[i] = ((double) (ip->counter)) / ((double) tot);

                S -= dist[i] * log(dist[i]);
            }

            CfOut(cf_error, "", "R: Variability/entropy of addresses = %.1f %%\n", S / log((double) tot) * 100.0);
            CfOut(cf_error, "", "R: (Entropy = 0 for single source, 100 for flatly distributed source)\n -\n");
        }

        CfOut(cf_error, "", "\n");
        CfOut(cf_error, "", "R: State of %s peaked at %s\n", type, cf_ctime(&statbuf.st_mtime));
    }
    else
    {
        CfOut(cf_inform, "", "R: State parameter %s is not known or recorded\n", type);
    }

    DeleteItemList(addresses);

    if (dist)
    {
        free((char *) dist);
    }
}
