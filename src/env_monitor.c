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

#include "env_context.h"
#include "monitoring.h"
#include "granules.h"
#include "dbm_api.h"
#include "policy.h"
#include "promises.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "scope.h"
#include "sysinfo.h"
#include "files_operators.h"
#include "cfstream.h"

#include <math.h>

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

#define CF_ENVNEW_FILE   "env_data.new"
#define cf_noise_threshold 6    /* number that does not warrent large anomaly status */
#define MON_THRESHOLD_HIGH 1000000      // samples should stay below this threshold
#define LDT_BUFSIZE 10

static char ENVFILE_NEW[CF_BUFSIZE];
static char ENVFILE[CF_BUFSIZE];

static double HISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS];

/* persistent observations */

static double CF_THIS[CF_OBSERVABLES];  /* New from 2.1.21 replacing above - current observation */

/* Work */

static long ITER;               /* Iteration since start */
static double AGE, WAGE;        /* Age and weekly age of database */

static Averages LOCALAV;

/* Leap Detection vars */

static double LDT_BUF[CF_OBSERVABLES][LDT_BUFSIZE];
static double LDT_SUM[CF_OBSERVABLES];
static double LDT_AVG[CF_OBSERVABLES];
static double CHI_LIMIT[CF_OBSERVABLES];
static double CHI[CF_OBSERVABLES];
static double LDT_MAX[CF_OBSERVABLES];
static int LDT_POS = 0;
static int LDT_FULL = false;

int NO_FORK = false;

/*******************************************************************/
/* Prototypes                                                      */
/*******************************************************************/

static void GetDatabaseAge(void);
static void LoadHistogram(void);
static void GetQ(const Policy *policy, const ReportContext *report_context);
static Averages EvalAvQ(char *timekey);
static void ArmClasses(Averages newvals, char *timekey);
static void GatherPromisedMeasures(const Policy *policy, const ReportContext *report_context);

static void LeapDetection(void);
static Averages *GetCurrentAverages(char *timekey);
static void UpdateAverages(char *timekey, Averages newvals);
static void UpdateDistributions(char *timekey, Averages *av);
static double WAverage(double newvals, double oldvals, double age);
static double SetClasses(char *name, double variable, double av_expect, double av_var, double localav_expect,
                         double localav_var, Item **classlist, char *timekey);
static void SetVariable(char *name, double now, double average, double stddev, Item **list);
static double RejectAnomaly(double new, double av, double var, double av2, double var2);
static void ZeroArrivals(void);
static void KeepMonitorPromise(Promise *pp);

/****************************************************************/

void MonitorInitialize(void)
{
    int i, j, k;
    char vbuff[CF_BUFSIZE];

    sprintf(vbuff, "%s/state/cf_users", CFWORKDIR);
    MapName(vbuff);
    CreateEmptyFile(vbuff);

    snprintf(ENVFILE_NEW, CF_BUFSIZE, "%s/state/%s", CFWORKDIR, CF_ENVNEW_FILE);
    MapName(ENVFILE_NEW);

    snprintf(ENVFILE, CF_BUFSIZE, "%s/state/%s", CFWORKDIR, CF_ENV_FILE);
    MapName(ENVFILE);

    MonEntropyClassesInit();

    GetDatabaseAge();

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        LOCALAV.Q[i] = QDefinite(0.0);
    }

    for (i = 0; i < 7; i++)
    {
        for (j = 0; j < CF_OBSERVABLES; j++)
        {
            for (k = 0; k < CF_GRAINS; k++)
            {
                HISTOGRAM[i][j][k] = 0;
            }
        }
    }

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        CHI[i] = 0;
        CHI_LIMIT[i] = 0.1;
        LDT_AVG[i] = 0;
        LDT_SUM[i] = 0;
    }

    srand((unsigned int) time(NULL));
    LoadHistogram();

/* Look for local sensors - this is unfortunately linux-centric */

    MonNetworkInit();
    MonTempInit();
    MonOtherInit();

    CfDebug("Finished with initialization.\n");
}

/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

static void GetDatabaseAge()
{
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_observations))
    {
        return;
    }

    if (ReadDB(dbp, "DATABASE_AGE", &AGE, sizeof(double)))
    {
        WAGE = AGE / SECONDS_PER_WEEK * CF_MEASURE_INTERVAL;
        CfDebug("\n\nPrevious DATABASE_AGE %f\n\n", AGE);
    }
    else
    {
        CfDebug("No previous AGE\n");
        AGE = 0.0;
    }

    CloseDB(dbp);
}

/*********************************************************************/

static void LoadHistogram(void)
{
    FILE *fp;
    int i, day, position;
    double maxval[CF_OBSERVABLES];

    char filename[CF_BUFSIZE];

    snprintf(filename, CF_BUFSIZE, "%s/state/histograms", CFWORKDIR);

    if ((fp = fopen(filename, "r")) == NULL)
    {
        CfOut(cf_verbose, "fopen", "Unable to load histogram data");
        return;
    }

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        maxval[i] = 1.0;
    }

    for (position = 0; position < CF_GRAINS; position++)
    {
        fscanf(fp, "%d ", &position);

        for (i = 0; i < CF_OBSERVABLES; i++)
        {
            for (day = 0; day < 7; day++)
            {
                fscanf(fp, "%lf ", &(HISTOGRAM[i][day][position]));

                if (HISTOGRAM[i][day][position] < 0)
                {
                    HISTOGRAM[i][day][position] = 0;
                }

                if (HISTOGRAM[i][day][position] > maxval[i])
                {
                    maxval[i] = HISTOGRAM[i][day][position];
                }

                HISTOGRAM[i][day][position] *= 1000.0 / maxval[i];
            }
        }
    }

    fclose(fp);
}

/*********************************************************************/

void MonitorStartServer(const Policy *policy, const ReportContext *report_context)
{
    char timekey[CF_SMALLBUF];
    Averages averages;
    Promise *pp = NewPromise("monitor_cfengine", "the monitor daemon");
    Attributes dummyattr;
    CfLock thislock;

#ifdef MINGW

    if (!NO_FORK)
    {
        CfOut(cf_verbose, "", "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* NOT MINGW */

    if ((!NO_FORK) && (fork() != 0))
    {
        CfOut(cf_inform, "", "cf-monitord: starting\n");
        _exit(0);
    }

    if (!NO_FORK)
    {
        ActAsDaemon(0);
    }

#endif /* NOT MINGW */

    memset(&dummyattr, 0, sizeof(dummyattr));
    dummyattr.transaction.ifelapsed = 0;
    dummyattr.transaction.expireafter = 0;

    thislock = AcquireLock(pp->promiser, VUQNAME, CFSTARTTIME, dummyattr, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    WritePID("cf-monitord.pid");

    MonNetworkSnifferOpen();

    while (true)
    {
        GetQ(policy, report_context);
        snprintf(timekey, sizeof(timekey), "%s", GenTimeKey(time(NULL)));
        averages = EvalAvQ(timekey);
        LeapDetection();
        ArmClasses(averages, timekey);

        ZeroArrivals();

        MonNetworkSnifferSniff(ITER, CF_THIS);

        ITER++;
    }
}

/*********************************************************************/

static void GetQ(const Policy *policy, const ReportContext *report_context)
{
    CfDebug("========================= GET Q ==============================\n");

    MonEntropyClassesReset();

    ZeroArrivals();

    MonProcessesGatherData(CF_THIS);
#ifndef MINGW
    MonCPUGatherData(CF_THIS);
    MonLoadGatherData(CF_THIS);
    MonDiskGatherData(CF_THIS);
    MonNetworkGatherData(CF_THIS);
    MonNetworkSnifferGatherData(CF_THIS);
    MonTempGatherData(CF_THIS);
#endif /* NOT MINGW */
    MonOtherGatherData(CF_THIS);
    GatherPromisedMeasures(policy, report_context);
}

/*********************************************************************/

static Averages EvalAvQ(char *t)
{
    Averages *lastweek_vals, newvals;
    double last5_vals[CF_OBSERVABLES];
    double This[CF_OBSERVABLES];
    char name[CF_MAXVARSIZE];
    time_t now = time(NULL);
    int i;

    Banner("Evaluating and storing new weekly averages");

    if ((lastweek_vals = GetCurrentAverages(t)) == NULL)
    {
        CfOut(cf_error, "", "Error reading average database");
        exit(1);
    }

/* Discard any apparently anomalous behaviour before renormalizing database */

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        double delta2;
        char desc[CF_BUFSIZE];

        name[0] = '\0';
        GetObservable(i, name, desc);

        /* Overflow protection */

        if (lastweek_vals->Q[i].expect < 0)
        {
            lastweek_vals->Q[i].expect = 0;
        }

        if (lastweek_vals->Q[i].q < 0)
        {
            lastweek_vals->Q[i].q = 0;
        }

        if (lastweek_vals->Q[i].var < 0)
        {
            lastweek_vals->Q[i].var = 0;
        }

        // lastweek_vals is last week's stored data
        
        This[i] =
            RejectAnomaly(CF_THIS[i], lastweek_vals->Q[i].expect, lastweek_vals->Q[i].var, LOCALAV.Q[i].expect,
                          LOCALAV.Q[i].var);

        newvals.Q[i].q = This[i];
        newvals.last_seen = now;  // Record the freshness of this slot
        
        LOCALAV.Q[i].q = This[i];

        CfDebug("Previous week's %s.q %lf\n", name, lastweek_vals->Q[i].q);
        CfDebug("Previous week's %s.var %lf\n", name, lastweek_vals->Q[i].var);
        CfDebug("Previous week's %s.ex %lf\n", name, lastweek_vals->Q[i].expect);

        CfDebug("Just measured: CF_THIS[%s] = %lf\n", name, CF_THIS[i]);
        CfDebug("Just sanitized: This[%s] = %lf\n", name, This[i]);

        newvals.Q[i].expect = WAverage(This[i], lastweek_vals->Q[i].expect, WAGE);
        LOCALAV.Q[i].expect = WAverage(newvals.Q[i].expect, LOCALAV.Q[i].expect, ITER);

        if (last5_vals[i] > 0)
        {
            newvals.Q[i].dq = newvals.Q[i].q - last5_vals[i];
            LOCALAV.Q[i].dq = newvals.Q[i].q - last5_vals[i];
        }
        else
        {
            newvals.Q[i].dq = 0;
            LOCALAV.Q[i].dq = 0;           
        }

        // Save the last measured value as the value "from five minutes ago" to get the gradient
        last5_vals[i] = newvals.Q[i].q;

        delta2 = (This[i] - lastweek_vals->Q[i].expect) * (This[i] - lastweek_vals->Q[i].expect);

        if (lastweek_vals->Q[i].var > delta2 * 2.0)
        {
            /* Clean up past anomalies */
            newvals.Q[i].var = delta2;
            LOCALAV.Q[i].var = WAverage(newvals.Q[i].var, LOCALAV.Q[i].var, ITER);
        }
        else
        {
            newvals.Q[i].var = WAverage(delta2, lastweek_vals->Q[i].var, WAGE);
            LOCALAV.Q[i].var = WAverage(newvals.Q[i].var, LOCALAV.Q[i].var, ITER);
        }

        CfOut(cf_verbose, "", "[%d] %s q=%lf, var=%lf, ex=%lf", i, name,
              newvals.Q[i].q, newvals.Q[i].var, newvals.Q[i].expect);

        CfOut(cf_verbose, "", "[%d] = %lf -> (%lf#%lf) local [%lf#%lf]\n", i, This[i], newvals.Q[i].expect,
              sqrt(newvals.Q[i].var), LOCALAV.Q[i].expect, sqrt(LOCALAV.Q[i].var));

        if (This[i] > 0)
        {
            CfOut(cf_verbose, "", "Storing %.2lf in %s\n", This[i], name);
        }
    }

    UpdateAverages(t, newvals);
    UpdateDistributions(t, lastweek_vals);        /* Distribution about mean */

    return newvals;
}

/*********************************************************************/

static void LeapDetection(void)
{
    int i, last_pos = LDT_POS;
    double n1, n2, d;
    double padding = 0.2;

    if (++LDT_POS >= LDT_BUFSIZE)
    {
        LDT_POS = 0;

        if (!LDT_FULL)
        {
            CfDebug("LDT Buffer full at %d\n", LDT_BUFSIZE);
            LDT_FULL = true;
        }
    }

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        /* First do some anomaly rejection. Sudden jumps must be numerical errors. */

        if ((LDT_BUF[i][last_pos] > 0) && ((CF_THIS[i] / LDT_BUF[i][last_pos]) > 1000))
        {
            CF_THIS[i] = LDT_BUF[i][last_pos];
        }

        /* Note AVG should contain n+1 but not SUM, hence funny increments */

        LDT_AVG[i] = LDT_AVG[i] + CF_THIS[i] / ((double) LDT_BUFSIZE + 1.0);

        d = (double) (LDT_BUFSIZE * (LDT_BUFSIZE + 1)) * LDT_AVG[i];

        if (LDT_FULL && (LDT_POS == 0))
        {
            n2 = (LDT_SUM[i] - (double) LDT_BUFSIZE * LDT_MAX[i]);

            if (d < 0.001)
            {
                CHI_LIMIT[i] = 0.5;
            }
            else
            {
                CHI_LIMIT[i] = padding + sqrt(n2 * n2 / d);
            }

            LDT_MAX[i] = 0.0;
        }

        if (CF_THIS[i] > LDT_MAX[i])
        {
            LDT_MAX[i] = CF_THIS[i];
        }

        n1 = (LDT_SUM[i] - (double) LDT_BUFSIZE * CF_THIS[i]);

        if (d < 0.001)
        {
            CHI[i] = 0.0;
        }
        else
        {
            CHI[i] = sqrt(n1 * n1 / d);
        }

        LDT_AVG[i] = LDT_AVG[i] - LDT_BUF[i][LDT_POS] / ((double) LDT_BUFSIZE + 1.0);
        LDT_BUF[i][LDT_POS] = CF_THIS[i];
        LDT_SUM[i] = LDT_SUM[i] - LDT_BUF[i][LDT_POS] + CF_THIS[i];
    }
}

/*********************************************************************/

static void PublishEnvironment(Item *classes)
{
    FILE *fp;
    Item *ip;

    unlink(ENVFILE_NEW);

    if ((fp = fopen(ENVFILE_NEW, "a")) == NULL)
    {
        return;
    }

    for (ip = classes; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%s\n", ip->name);
    }

    MonEntropyClassesPublish(fp);

    fclose(fp);

    cf_rename(ENVFILE_NEW, ENVFILE);
}

/*********************************************************************/

static void ArmClasses(Averages av, char *timekey)
{
    double sigma;
    Item *ip,*classlist = NULL;
    int i, j, k;
    char buff[CF_BUFSIZE], ldt_buff[CF_BUFSIZE], name[CF_MAXVARSIZE];
    static int anomaly[CF_OBSERVABLES][LDT_BUFSIZE];
    extern Item *ALL_INCOMING;
    extern Item *MON_UDP4, *MON_UDP6, *MON_TCP4, *MON_TCP6;

    CfDebug("Arm classes for %s\n", timekey);

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        char desc[CF_BUFSIZE];

        GetObservable(i, name, desc);
        sigma = SetClasses(name, CF_THIS[i], av.Q[i].expect, av.Q[i].var, LOCALAV.Q[i].expect, LOCALAV.Q[i].var, &classlist, timekey);
        SetVariable(name, CF_THIS[i], av.Q[i].expect, sigma, &classlist);

        /* LDT */

        ldt_buff[0] = '\0';

        anomaly[i][LDT_POS] = false;

        if (!LDT_FULL)
        {
            anomaly[i][LDT_POS] = false;
        }

        if (LDT_FULL && (CHI[i] > CHI_LIMIT[i]))
        {
            anomaly[i][LDT_POS] = true; /* Remember the last anomaly value */

            CfOut(cf_verbose, "", "LDT(%d) in %s chi = %.2f thresh %.2f \n", LDT_POS, name, CHI[i], CHI_LIMIT[i]);

            /* Last printed element is now */

            for (j = LDT_POS + 1, k = 0; k < LDT_BUFSIZE; j++, k++)
            {
                if (j == LDT_BUFSIZE)   /* Wrap */
                {
                    j = 0;
                }

                if (anomaly[i][j])
                {
                    snprintf(buff, CF_BUFSIZE, " *%.2f*", LDT_BUF[i][j]);
                }
                else
                {
                    snprintf(buff, CF_BUFSIZE, " %.2f", LDT_BUF[i][j]);
                }

                strcat(ldt_buff, buff);
            }

            if (CF_THIS[i] > av.Q[i].expect)
            {
                snprintf(buff, CF_BUFSIZE, "%s_high_ldt", name);
            }
            else
            {
                snprintf(buff, CF_BUFSIZE, "%s_high_ldt", name);
            }

            AppendItem(&classlist, buff, "2");
            NewPersistentContext(buff, "measurements", CF_PERSISTENCE, cfpreserve);
        }
        else
        {
            for (j = LDT_POS + 1, k = 0; k < LDT_BUFSIZE; j++, k++)
            {
                if (j == LDT_BUFSIZE)   /* Wrap */
                {
                    j = 0;
                }

                if (anomaly[i][j])
                {
                    snprintf(buff, CF_BUFSIZE, " *%.2f*", LDT_BUF[i][j]);
                }
                else
                {
                    snprintf(buff, CF_BUFSIZE, " %.2f", LDT_BUF[i][j]);
                }
                strcat(ldt_buff, buff);
            }
        }
    }

    SetMeasurementPromises(&classlist);

    // Report on the open ports, in various ways

    ldt_buff[0] = '\0';
    PrintItemList(ldt_buff,CF_BUFSIZE,ALL_INCOMING);

    if (strlen(ldt_buff) < 1500)
    {
        snprintf(buff,CF_BUFSIZE,"@listening_ports=%s",ldt_buff);
        AppendItem(&classlist,buff,NULL);
    }

    ldt_buff[0] = '\0';
    PrintItemList(ldt_buff,CF_BUFSIZE,MON_UDP6);

    if (strlen(ldt_buff) < 1500)
    {
        snprintf(buff,CF_BUFSIZE,"@listening_udp6_ports=%s",ldt_buff);
        AppendItem(&classlist,buff,NULL);
    }

    ldt_buff[0] = '\0';
    PrintItemList(ldt_buff,CF_BUFSIZE,MON_UDP4);

    if (strlen(ldt_buff) < 1500)
    {
        snprintf(buff,CF_BUFSIZE,"@listening_udp4_ports=%s",ldt_buff);
        AppendItem(&classlist,buff,NULL);
    }

    ldt_buff[0] = '\0';
    PrintItemList(ldt_buff,CF_BUFSIZE,MON_TCP6);

    if (strlen(ldt_buff) < 1500)
    {
        snprintf(buff,CF_BUFSIZE,"@listening_tcp6_ports=%s",ldt_buff);
        AppendItem(&classlist,buff,NULL);
    }

    ldt_buff[0] = '\0';
    PrintItemList(ldt_buff,CF_BUFSIZE,MON_TCP4);

    if (strlen(ldt_buff) < 1500)
    {
        snprintf(buff,CF_BUFSIZE,"@listening_tcp4_ports=%s",ldt_buff);
        AppendItem(&classlist,buff,NULL);
    }

    // Port addresses

    if (ListLen(MON_TCP6) + ListLen(MON_TCP4) > 512)
    {
        CfOut(cf_inform, "", "Disabling address information of TCP ports in LISTEN state: more than 512 listening ports are detected");
    }
    else
    {
        for (ip = MON_TCP6; ip != NULL; ip=ip->next)
        {
            snprintf(buff,CF_BUFSIZE,"tcp6_port_addr[%s]=%s",ip->name,ip->classes);
            AppendItem(&classlist,buff,NULL);       
        }

        for (ip = MON_TCP4; ip != NULL; ip=ip->next)
        {
            snprintf(buff,CF_BUFSIZE,"tcp4_port_addr[%s]=%s",ip->name,ip->classes);
            AppendItem(&classlist,buff,NULL);       
        }
    }

    for (ip = MON_UDP6; ip != NULL; ip=ip->next)
    {
        snprintf(buff,CF_BUFSIZE,"udp6_port_addr[%s]=%s",ip->name,ip->classes);
        AppendItem(&classlist,buff,NULL);       
    }
    
    for (ip = MON_UDP4; ip != NULL; ip=ip->next)
    {
        snprintf(buff,CF_BUFSIZE,"udp4_port_addr[%s]=%s",ip->name,ip->classes);
        AppendItem(&classlist,buff,NULL);       
    }
    
    PublishEnvironment(classlist);

    DeleteItemList(classlist);
}

/*****************************************************************************/

static Averages *GetCurrentAverages(char *timekey)
{
    CF_DB *dbp;
    static Averages entry;

    if (!OpenDB(&dbp, dbid_observations))
    {
        return NULL;
    }

    memset(&entry, 0, sizeof(entry));

    AGE++;
    WAGE = AGE / SECONDS_PER_WEEK * CF_MEASURE_INTERVAL;

    if (ReadDB(dbp, timekey, &entry, sizeof(Averages)))
    {
        int i;

        for (i = 0; i < CF_OBSERVABLES; i++)
        {
            CfDebug("Previous values (%lf,..) for time index %s\n\n", entry.Q[i].expect, timekey);
        }
    }
    else
    {
        CfDebug("No previous value for time index %s\n", timekey);
    }

    CloseDB(dbp);
    return &entry;
}

/*****************************************************************************/

static void UpdateAverages(char *timekey, Averages newvals)
{
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_observations))
    {
        return;
    }

    CfOut(cf_inform, "", "Updated averages at %s\n", timekey);

    WriteDB(dbp, timekey, &newvals, sizeof(Averages));
    WriteDB(dbp, "DATABASE_AGE", &AGE, sizeof(double));

    CloseDB(dbp);
    HistoryUpdate(newvals);
}

/*****************************************************************************/

static void UpdateDistributions(char *timekey, Averages *av)
{
    int position, day, i;
    char filename[CF_BUFSIZE];
    FILE *fp;

/* Take an interval of 4 standard deviations from -2 to +2, divided into CF_GRAINS
   parts. Centre each measurement on CF_GRAINS/2 and scale each measurement by the
   std-deviation for the current time.
*/

    if (IsDefinedClass("Min40_45", NULL))
    {
        day = Day2Number(timekey);

        for (i = 0; i < CF_OBSERVABLES; i++)
        {
            position =
                CF_GRAINS / 2 + (int) (0.5 + (CF_THIS[i] - av->Q[i].expect) * CF_GRAINS / (4 * sqrt((av->Q[i].var))));

            if ((0 <= position) && (position < CF_GRAINS))
            {
                HISTOGRAM[i][day][position]++;
            }
        }

        snprintf(filename, CF_BUFSIZE, "%s/state/histograms", CFWORKDIR);

        if ((fp = fopen(filename, "w")) == NULL)
        {
            CfOut(cf_error, "fopen", "Unable to save histograms");
            return;
        }

        for (position = 0; position < CF_GRAINS; position++)
        {
            fprintf(fp, "%d ", position);

            for (i = 0; i < CF_OBSERVABLES; i++)
            {
                for (day = 0; day < 7; day++)
                {
                    fprintf(fp, "%.0lf ", HISTOGRAM[i][day][position]);
                }
            }
            fprintf(fp, "\n");
        }

        fclose(fp);
    }
}

/*****************************************************************************/

/* For a couple of weeks, learn eagerly. Otherwise variances will
   be way too large. Then downplay newer data somewhat, and rely on
   experience of a couple of months of data ... */

static double WAverage(double anew, double aold, double age)
{
    double av, cf_sane_monitor_limit = 9999999.0;
    double wnew, wold;

/* First do some database corruption self-healing */

    if ((aold > cf_sane_monitor_limit) && (anew > cf_sane_monitor_limit))
    {
        return 0;
    }

    if (aold > cf_sane_monitor_limit)
    {
        return anew;
    }

    if (aold > cf_sane_monitor_limit)
    {
        return aold;
    }

/* Now look at the self-learning */

    if ((FORGETRATE > 0.9) || (FORGETRATE < 0.1))
    {
        FORGETRATE = 0.6;
    }

    if (age < 2.0)              /* More aggressive learning for young database */
    {
        wnew = FORGETRATE;
        wold = (1.0 - FORGETRATE);
    }
    else
    {
        wnew = (1.0 - FORGETRATE);
        wold = FORGETRATE;
    }

    if ((aold == 0) && (anew == 0))
    {
        return 0;
    }

/*
 * AV = (Wnew*Anew + Wold*Aold) / (Wnew + Wold).
 *
 * Wnew + Wold always equals to 1, so we omit it for better precision and
 * performance.
 */

    av = (wnew * anew + wold * aold);

    if (av < 0)
    {
        /* Accuracy lost - something wrong */
        return 0.0;
    }

    return av;
}

/*****************************************************************************/

static double SetClasses(char *name, double variable, double av_expect, double av_var, double localav_expect,
                         double localav_var, Item **classlist, char *timekey)
{
    char buffer[CF_BUFSIZE], buffer2[CF_BUFSIZE];
    double dev, delta, sigma, ldelta, lsigma, sig;

    CfDebug("\n SetClasses(%s,X=%lf,avX=%lf,varX=%lf,lavX=%lf,lvarX=%lf,%s)\n", name, variable, av_expect, av_var,
            localav_expect, localav_var, timekey);

    delta = variable - av_expect;
    sigma = sqrt(av_var);
    ldelta = variable - localav_expect;
    lsigma = sqrt(localav_var);
    sig = sqrt(sigma * sigma + lsigma * lsigma);

    CfDebug(" delta = %lf,sigma = %lf, lsigma = %lf, sig = %lf\n", delta, sigma, lsigma, sig);

    if ((sigma == 0.0) || (lsigma == 0.0))
    {
        CfDebug(" No sigma variation .. can't measure class\n");

        snprintf(buffer, CF_MAXVARSIZE, "entropy_%s.*", name);
        MonEntropyPurgeUnused(buffer);

        return sig;
    }

    CfDebug("Setting classes for %s...\n", name);

    if (fabs(delta) < cf_noise_threshold)       /* Arbitrary limits on sensitivity  */
    {
        CfDebug(" Sensitivity too high ..\n");

        buffer[0] = '\0';
        strcpy(buffer, name);

        if ((delta > 0) && (ldelta > 0))
        {
            strcat(buffer, "_high");
        }
        else if ((delta < 0) && (ldelta < 0))
        {
            strcat(buffer, "_low");
        }
        else
        {
            strcat(buffer, "_normal");
        }

        AppendItem(classlist, buffer, "0");

        dev = sqrt(delta * delta / (1.0 + sigma * sigma) + ldelta * ldelta / (1.0 + lsigma * lsigma));

        if (dev > 2.0 * sqrt(2.0))
        {
            strcpy(buffer2, buffer);
            strcat(buffer2, "_microanomaly");
            AppendItem(classlist, buffer2, "2");
            NewPersistentContext(buffer2, "measurements", CF_PERSISTENCE, cfpreserve);
        }

        return sig;             /* Granularity makes this silly */
    }
    else
    {
        buffer[0] = '\0';
        strcpy(buffer, name);

        if ((delta > 0) && (ldelta > 0))
        {
            strcat(buffer, "_high");
        }
        else if ((delta < 0) && (ldelta < 0))
        {
            strcat(buffer, "_low");
        }
        else
        {
            strcat(buffer, "_normal");
        }

        dev = sqrt(delta * delta / (1.0 + sigma * sigma) + ldelta * ldelta / (1.0 + lsigma * lsigma));

        if (dev <= sqrt(2.0))
        {
            strcpy(buffer2, buffer);
            strcat(buffer2, "_normal");
            AppendItem(classlist, buffer2, "0");
        }
        else
        {
            strcpy(buffer2, buffer);
            strcat(buffer2, "_dev1");
            AppendItem(classlist, buffer2, "0");
        }

        /* Now use persistent classes so that serious anomalies last for about
           2 autocorrelation lengths, so that they can be cross correlated and
           seen by normally scheduled cfagent processes ... */

        if (dev > 2.0 * sqrt(2.0))
        {
            strcpy(buffer2, buffer);
            strcat(buffer2, "_dev2");
            AppendItem(classlist, buffer2, "2");
            NewPersistentContext(buffer2, "measurements", CF_PERSISTENCE, cfpreserve);
        }

        if (dev > 3.0 * sqrt(2.0))
        {
            strcpy(buffer2, buffer);
            strcat(buffer2, "_anomaly");
            AppendItem(classlist, buffer2, "3");
            NewPersistentContext(buffer2, "measurements", CF_PERSISTENCE, cfpreserve);
        }

        return sig;
    }
}

/*****************************************************************************/

static void SetVariable(char *name, double value, double average, double stddev, Item **classlist)
{
    char var[CF_BUFSIZE];

    snprintf(var, CF_MAXVARSIZE, "value_%s=%.2lf", name, value);
    AppendItem(classlist, var, "");

    snprintf(var, CF_MAXVARSIZE, "av_%s=%.2lf", name, average);
    AppendItem(classlist, var, "");

    snprintf(var, CF_MAXVARSIZE, "dev_%s=%.2lf", name, stddev);
    AppendItem(classlist, var, "");
}

/*****************************************************************************/

static void ZeroArrivals()
{
    memset(CF_THIS, 0, sizeof(CF_THIS));
}

/*****************************************************************************/

static double RejectAnomaly(double new, double average, double variance, double localav, double localvar)
{
    double dev = sqrt(variance + localvar);     /* Geometrical average dev */
    double delta;
    int bigger;

    if (average == 0)
    {
        return new;
    }

    if (new > MON_THRESHOLD_HIGH * 4.0)
    {
        return 0.0;
    }

    if (new > MON_THRESHOLD_HIGH)
    {
        return average;
    }

    if ((new - average) * (new - average) < cf_noise_threshold * cf_noise_threshold)
    {
        return new;
    }

    if (new - average > 0)
    {
        bigger = true;
    }
    else
    {
        bigger = false;
    }

/* This routine puts some inertia into the changes, so that the system
   doesn't respond to every little change ...   IR and UV cutoff */

    delta = sqrt((new - average) * (new - average) + (new - localav) * (new - localav));

    if (delta > 4.0 * dev)      /* IR */
    {
        srand48((unsigned int) time(NULL));

        if (drand48() < 0.7)    /* 70% chance of using full value - as in learning policy */
        {
            return new;
        }
        else
        {
            if (bigger)
            {
                return average + 2.0 * dev;
            }
            else
            {
                return average - 2.0 * dev;
            }
        }
    }
    else
    {
        CfOut(cf_verbose, "", "Value accepted\n");
        return new;
    }
}

/***************************************************************/
/* Level 5                                                     */
/***************************************************************/

static void GatherPromisedMeasures(const Policy *policy, const ReportContext *report_context)
{
    SubType *sp;
    Promise *pp;
    char *scope;

    for (const Bundle *bp = policy->bundles; bp != NULL; bp = bp->next)       /* get schedule */
    {
        scope = bp->name;
        SetNewScope(bp->name);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_MONITOR]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            for (sp = bp->subtypes; sp != NULL; sp = sp->next)  /* get schedule */
            {
                for (pp = sp->promiselist; pp != NULL; pp = pp->next)
                {
                    ExpandPromise(AGENT_TYPE_MONITOR, scope, pp, KeepMonitorPromise, report_context);
                }
            }
        }
    }

    DeleteAllScope();
    NewScope("const");
    NewScope("control_monitor");
    NewScope("control_common");
    NewScope("mon");
    NewScope("sys");
    GetNameInfo3();
    GetInterfacesInfo(AGENT_TYPE_MONITOR);
    Get3Environment();
    OSClasses();
    BuiltinClasses();
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void KeepMonitorPromise(Promise *pp)
{
    char *sp = NULL;

    if (!IsDefinedClass(pp->classes, pp->namespace))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(cf_verbose, "", "Skipping whole next promise (%s), as context %s is not relevant\n", pp->promiser,
              pp->classes);
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(cf_verbose, "", "Skipping whole next promise (%s), as var-context %s is not relevant\n", pp->promiser,
              sp);
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    if (strcmp("classes", pp->agentsubtype) == 0)
    {
        KeepClassContextPromise(pp);
        return;
    }

    if (strcmp("measurements", pp->agentsubtype) == 0)
    {
        VerifyMeasurementPromise(CF_THIS, pp);
        *pp->donep = false;
        return;
    }
}

/*****************************************************************************/

void MonOtherInit(void)
{
#ifdef HAVE_NOVA
    Nova_MonOtherInit();
#endif
}

/*********************************************************************/

void MonOtherGatherData(double *cf_this)
{
#ifdef HAVE_NOVA
    Nova_MonOtherGatherData(cf_this);
#endif
}

