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

#include <env_monitor.h>

#include <eval_context.h>
#include <mon.h>
#include <granules.h>
#include <dbm_api.h>
#include <policy.h>
#include <promises.h>
#include <item_lib.h>
#include <conversion.h>
#include <ornaments.h>
#include <expand.h>
#include <scope.h>
#include <sysinfo.h>
#include <signals.h>
#include <locks.h>
#include <exec_tools.h>
#include <generic_agent.h> // WritePID
#include <files_lib.h>
#include <unix.h>
#include <verify_measurements.h>
#include <verify_classes.h>
#include <cf-monitord-enterprise-stubs.h>
#include <known_dirs.h>

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

#define CF_ENVNEW_FILE   "env_data.new"
#define cf_noise_threshold 6    /* number that does not warrant large anomaly status */
#define MON_THRESHOLD_HIGH 1000000      // samples should stay below this threshold
#define LDT_BUFSIZE 10

double FORGETRATE = 0.7;

static char ENVFILE_NEW[CF_BUFSIZE] = "";
static char ENVFILE[CF_BUFSIZE] = "";

static double HISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS] = { { { 0.0 } } };

/* persistent observations */

static double CF_THIS[CF_OBSERVABLES] = { 0.0 };

/* Work */

static long ITER = 0;               /* Iteration since start */
static double AGE = 0.0, WAGE = 0.0;        /* Age and weekly age of database */

static Averages LOCALAV = { 0 };

/* Leap Detection vars */

static double LDT_BUF[CF_OBSERVABLES][LDT_BUFSIZE] = { { 0 } };
static double LDT_SUM[CF_OBSERVABLES] = { 0 };
static double LDT_AVG[CF_OBSERVABLES] = { 0 };
static double CHI_LIMIT[CF_OBSERVABLES] = { 0 };
static double CHI[CF_OBSERVABLES] = { 0 };
static double LDT_MAX[CF_OBSERVABLES] = { 0 };
static int LDT_POS = 0;
static int LDT_FULL = false;

int NO_FORK = false;

/*******************************************************************/
/* Prototypes                                                      */
/*******************************************************************/

static void GetDatabaseAge(void);
static void LoadHistogram(void);
static void GetQ(EvalContext *ctx, const Policy *policy);
static Averages EvalAvQ(EvalContext *ctx, char *timekey);
static void ArmClasses(EvalContext *ctx, Averages newvals);
static void GatherPromisedMeasures(EvalContext *ctx, const Policy *policy);

static void LeapDetection(void);
static Averages *GetCurrentAverages(char *timekey);
static void UpdateAverages(EvalContext *ctx, char *timekey, Averages newvals);
static void UpdateDistributions(EvalContext *ctx, char *timekey, Averages *av);
static double WAverage(double newvals, double oldvals, double age);
static double SetClasses(EvalContext *ctx, char *name, double variable, double av_expect, double av_var, double localav_expect,
                         double localav_var, Item **classlist);
static void SetVariable(char *name, double now, double average, double stddev, Item **list);
static double RejectAnomaly(double new, double av, double var, double av2, double var2);
static void ZeroArrivals(void);
static PromiseResult KeepMonitorPromise(EvalContext *ctx, const Promise *pp, void *param);

/****************************************************************/

void MonitorInitialize(void)
{
    int i, j, k;
    char vbuff[CF_BUFSIZE];
    const char* const statedir = GetStateDir();

    snprintf(vbuff, sizeof(vbuff), "%s/cf_users", statedir);
    MapName(vbuff);
    CreateEmptyFile(vbuff);

    snprintf(ENVFILE_NEW, CF_BUFSIZE, "%s/%s", statedir, CF_ENVNEW_FILE);
    MapName(ENVFILE_NEW);

    snprintf(ENVFILE, CF_BUFSIZE, "%s/%s", statedir, CF_ENV_FILE);
    MapName(ENVFILE);

    MonEntropyClassesInit();

    GetDatabaseAge();

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        LOCALAV.Q[i] = QDefinite(0.0);
    }

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        for (j = 0; j < 7; j++)
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

    Log(LOG_LEVEL_DEBUG, "Finished with monitor initialization");
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
        Log(LOG_LEVEL_DEBUG, "Previous DATABASE_AGE %f", AGE);
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "No previous DATABASE_AGE");
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

    snprintf(filename, CF_BUFSIZE, "%s%chistograms", GetStateDir(), FILE_SEPARATOR);

    if ((fp = fopen(filename, "r")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Unable to load histogram data from '%s' (fopen: %s)",
            filename, GetErrorStr());
        return;
    }

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        maxval[i] = 1.0;
    }

    for (position = 0; position < CF_GRAINS; position++)
    {
        if (fscanf(fp, "%d ", &position) != 1)
        {
            Log(LOG_LEVEL_ERR, "Format error in histogram file '%s' - aborting", filename);
            break;
        }

        for (i = 0; i < CF_OBSERVABLES; i++)
        {
            for (day = 0; day < 7; day++)
            {
                if (fscanf(fp, "%lf ", &(HISTOGRAM[i][day][position])) != 1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Format error in histogram file '%s'. (fscanf: %s)", filename, GetErrorStr());
                    HISTOGRAM[i][day][position] = 0;
                }

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

void MonitorStartServer(EvalContext *ctx, const Policy *policy)
{
    char timekey[CF_SMALLBUF];
    Averages averages;

    Policy *monitor_cfengine_policy = PolicyNew();
    Promise *pp = NULL;
    {
        Bundle *bp = PolicyAppendBundle(monitor_cfengine_policy, NamespaceDefault(), "monitor_cfengine_bundle", "agent", NULL, NULL);
        PromiseType *tp = BundleAppendPromiseType(bp, "monitor_cfengine");

        pp = PromiseTypeAppendPromise(tp, "the monitor daemon", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, NULL, NULL);
    }
    assert(pp);

    CfLock thislock;

#ifdef __MINGW32__

    if (!NO_FORK)
    {
        Log(LOG_LEVEL_VERBOSE, "Windows does not support starting processes in the background - starting in foreground");
    }

#else /* !__MINGW32__ */

    if ((!NO_FORK) && (fork() != 0))
    {
        Log(LOG_LEVEL_INFO, "cf-monitord: starting");
        _exit(EXIT_SUCCESS);
    }

    if (!NO_FORK)
    {
        ActAsDaemon();
    }

#endif /* !__MINGW32__ */

    TransactionContext tc = {
        .ifelapsed = 0,
        .expireafter = 0,
    };

    thislock = AcquireLock(ctx, pp->promiser, VUQNAME, CFSTARTTIME, tc, pp, false);
    if (thislock.lock == NULL)
    {
        PolicyDestroy(monitor_cfengine_policy);
        return;
    }

    WritePID("cf-monitord.pid");

    MonNetworkSnifferOpen();

    while (!IsPendingTermination())
    {
        GetQ(ctx, policy);
        snprintf(timekey, sizeof(timekey), "%s", GenTimeKey(time(NULL)));
        averages = EvalAvQ(ctx, timekey);
        LeapDetection();
        ArmClasses(ctx, averages);

        ZeroArrivals();

        MonNetworkSnifferSniff(EvalContextGetIpAddresses(ctx), ITER, CF_THIS);

        ITER++;
    }

    PolicyDestroy(monitor_cfengine_policy);
    YieldCurrentLock(thislock);
}

/*********************************************************************/

static void GetQ(EvalContext *ctx, const Policy *policy)
{
    MonEntropyClassesReset();

    ZeroArrivals();

    MonProcessesGatherData(CF_THIS);
    MonDiskGatherData(CF_THIS);
#ifndef __MINGW32__
    MonCPUGatherData(CF_THIS);
    MonLoadGatherData(CF_THIS);
    MonNetworkGatherData(CF_THIS);
    MonNetworkSnifferGatherData();
    MonTempGatherData(CF_THIS);
#endif /* !__MINGW32__ */
    MonOtherGatherData(CF_THIS);
    GatherPromisedMeasures(ctx, policy);
}

/*********************************************************************/

static Averages EvalAvQ(EvalContext *ctx, char *t)
{
    Averages *lastweek_vals, newvals;
    double last5_vals[CF_OBSERVABLES];
    char name[CF_MAXVARSIZE];
    time_t now = time(NULL);
    int i;

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        last5_vals[i] = 0.0;
    }

    Banner("Evaluating and storing new weekly averages");

    if ((lastweek_vals = GetCurrentAverages(t)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Error reading average database");
        exit(EXIT_FAILURE);
    }

/* Discard any apparently anomalous behaviour before renormalizing database */

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        double delta2;
        char desc[CF_BUFSIZE];
        double This;
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
        
        This =
            RejectAnomaly(CF_THIS[i], lastweek_vals->Q[i].expect, lastweek_vals->Q[i].var, LOCALAV.Q[i].expect,
                          LOCALAV.Q[i].var);

        newvals.Q[i].q = This;
        newvals.last_seen = now;  // Record the freshness of this slot
        
        LOCALAV.Q[i].q = This;

        Log(LOG_LEVEL_DEBUG, "Previous week's '%s.q' %lf", name, lastweek_vals->Q[i].q);
        Log(LOG_LEVEL_DEBUG, "Previous week's '%s.var' %lf", name, lastweek_vals->Q[i].var);
        Log(LOG_LEVEL_DEBUG, "Previous week's '%s.ex' %lf", name, lastweek_vals->Q[i].expect);

        Log(LOG_LEVEL_DEBUG, "Just measured: CF_THIS[%s] = %lf", name, CF_THIS[i]);
        Log(LOG_LEVEL_DEBUG, "Just sanitized: This[%s] = %lf", name, This);

        newvals.Q[i].expect = WAverage(This, lastweek_vals->Q[i].expect, WAGE);
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

        delta2 = (This - lastweek_vals->Q[i].expect) * (This - lastweek_vals->Q[i].expect);

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

        Log(LOG_LEVEL_VERBOSE, "[%d] %s q=%lf, var=%lf, ex=%lf", i, name,
            newvals.Q[i].q, newvals.Q[i].var, newvals.Q[i].expect);

        Log(LOG_LEVEL_VERBOSE, "[%d] = %lf -> (%lf#%lf) local [%lf#%lf]", i, This, newvals.Q[i].expect,
            sqrt(newvals.Q[i].var), LOCALAV.Q[i].expect, sqrt(LOCALAV.Q[i].var));

        if (This > 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Storing %.2lf in %s", This, name);
        }
    }

    UpdateAverages(ctx, t, newvals);
    UpdateDistributions(ctx, t, lastweek_vals);        /* Distribution about mean */

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
            Log(LOG_LEVEL_DEBUG, "LDT Buffer full at %d", LDT_BUFSIZE);
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

    rename(ENVFILE_NEW, ENVFILE);
}

/*********************************************************************/

static void AddOpenPorts(const char *name, const Item *value, Item **mon_data)
{
    Writer *w = StringWriter();
    WriterWriteF(w, "@%s=", name);
    PrintItemList(value, w);
    if (StringWriterLength(w) <= 1500)
    {
        AppendItem(mon_data, StringWriterData(w), NULL);
    }
    WriterClose(w);
}

static void ArmClasses(EvalContext *ctx, Averages av)
{
    double sigma;
    Item *ip, *mon_data = NULL;
    int i, j, k;
    char buff[CF_BUFSIZE], ldt_buff[CF_BUFSIZE], name[CF_MAXVARSIZE];
    static int anomaly[CF_OBSERVABLES][LDT_BUFSIZE] = { { 0 } };
    extern Item *ALL_INCOMING;
    extern Item *MON_UDP4, *MON_UDP6, *MON_TCP4, *MON_TCP6;

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        char desc[CF_BUFSIZE];

        GetObservable(i, name, desc);
        sigma = SetClasses(ctx, name, CF_THIS[i], av.Q[i].expect, av.Q[i].var, LOCALAV.Q[i].expect, LOCALAV.Q[i].var, &mon_data);
        SetVariable(name, CF_THIS[i], av.Q[i].expect, sigma, &mon_data);

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

            Log(LOG_LEVEL_VERBOSE, "LDT(%d) in %s chi = %.2f thresh %.2f ", LDT_POS, name, CHI[i], CHI_LIMIT[i]);

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

            AppendItem(&mon_data, buff, "2");
            EvalContextHeapPersistentSave(ctx, buff, CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
            EvalContextClassPutSoft(ctx, buff, CONTEXT_SCOPE_NAMESPACE, "");
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

    SetMeasurementPromises(&mon_data);

    // Report on the open ports, in various ways

    AddOpenPorts("listening_ports", ALL_INCOMING, &mon_data);
    AddOpenPorts("listening_udp6_ports", MON_UDP6, &mon_data);
    AddOpenPorts("listening_udp4_ports", MON_UDP4, &mon_data);
    AddOpenPorts("listening_tcp6_ports", MON_TCP6, &mon_data);
    AddOpenPorts("listening_tcp4_ports", MON_TCP4, &mon_data);

    // Port addresses

    if (ListLen(MON_TCP6) + ListLen(MON_TCP4) > 512)
    {
        Log(LOG_LEVEL_INFO, "Disabling address information of TCP ports in LISTEN state: more than 512 listening ports are detected");
    }
    else
    {
        for (ip = MON_TCP6; ip != NULL; ip=ip->next)
        {
            snprintf(buff,CF_BUFSIZE,"tcp6_port_addr[%s]=%s",ip->name,ip->classes);
            AppendItem(&mon_data, buff, NULL);
        }

        for (ip = MON_TCP4; ip != NULL; ip=ip->next)
        {
            snprintf(buff,CF_BUFSIZE,"tcp4_port_addr[%s]=%s",ip->name,ip->classes);
            AppendItem(&mon_data, buff, NULL);
        }
    }

    for (ip = MON_UDP6; ip != NULL; ip=ip->next)
    {
        snprintf(buff,CF_BUFSIZE,"udp6_port_addr[%s]=%s",ip->name,ip->classes);
        AppendItem(&mon_data, buff, NULL);
    }
    
    for (ip = MON_UDP4; ip != NULL; ip=ip->next)
    {
        snprintf(buff,CF_BUFSIZE,"udp4_port_addr[%s]=%s",ip->name,ip->classes);
        AppendItem(&mon_data, buff, NULL);
    }
    
    PublishEnvironment(mon_data);

    DeleteItemList(mon_data);
}

/*****************************************************************************/

static Averages *GetCurrentAverages(char *timekey)
{
    CF_DB *dbp;
    static Averages entry; /* No need to initialize */

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
            Log(LOG_LEVEL_DEBUG, "Previous values (%lf,..) for time index '%s'", entry.Q[i].expect, timekey);
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "No previous value for time index '%s'", timekey);
    }

    CloseDB(dbp);
    return &entry;
}

/*****************************************************************************/

static void UpdateAverages(EvalContext *ctx, char *timekey, Averages newvals)
{
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_observations))
    {
        return;
    }

    Log(LOG_LEVEL_INFO, "Updated averages at '%s'", timekey);

    WriteDB(dbp, timekey, &newvals, sizeof(Averages));
    WriteDB(dbp, "DATABASE_AGE", &AGE, sizeof(double));

    CloseDB(dbp);
    HistoryUpdate(ctx, newvals);
}

static int Day2Number(const char *datestring)
{
    int i = 0;

    for (i = 0; i < 7; i++)
    {
        if (strncmp(datestring, DAY_TEXT[i], 3) == 0)
        {
            return i;
        }
    }

    return -1;
}

static void UpdateDistributions(EvalContext *ctx, char *timekey, Averages *av)
{
    int position, day, i;
    char filename[CF_BUFSIZE];
    FILE *fp;

/* Take an interval of 4 standard deviations from -2 to +2, divided into CF_GRAINS
   parts. Centre each measurement on CF_GRAINS/2 and scale each measurement by the
   std-deviation for the current time.
*/

    if (IsDefinedClass(ctx, "Min40_45"))
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

        snprintf(filename, CF_BUFSIZE, "%s%chistograms", GetStateDir(), FILE_SEPARATOR);

        if ((fp = fopen(filename, "w")) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Unable to save histograms to '%s' (fopen: %s)", filename, GetErrorStr());
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

static double SetClasses(EvalContext *ctx, char *name, double variable, double av_expect, double av_var, double localav_expect,
                         double localav_var, Item **classlist)
{
    char buffer[CF_BUFSIZE], buffer2[CF_BUFSIZE];
    double dev, delta, sigma, ldelta, lsigma, sig;

    delta = variable - av_expect;
    sigma = sqrt(av_var);
    ldelta = variable - localav_expect;
    lsigma = sqrt(localav_var);
    sig = sqrt(sigma * sigma + lsigma * lsigma);

    Log(LOG_LEVEL_DEBUG, "delta = %lf, sigma = %lf, lsigma = %lf, sig = %lf", delta, sigma, lsigma, sig);

    if ((sigma == 0.0) || (lsigma == 0.0))
    {
        Log(LOG_LEVEL_DEBUG, "No sigma variation .. can't measure class");

        snprintf(buffer, CF_MAXVARSIZE, "entropy_%s.*", name);
        MonEntropyPurgeUnused(buffer);

        return sig;
    }

    Log(LOG_LEVEL_DEBUG, "Setting classes for '%s'...", name);

    if (fabs(delta) < cf_noise_threshold)       /* Arbitrary limits on sensitivity  */
    {
        Log(LOG_LEVEL_DEBUG, "Sensitivity too high");

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
            EvalContextHeapPersistentSave(ctx, buffer2, CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
            EvalContextClassPutSoft(ctx, buffer2, CONTEXT_SCOPE_NAMESPACE, "");
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
            EvalContextHeapPersistentSave(ctx, buffer2, CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
            EvalContextClassPutSoft(ctx, buffer2, CONTEXT_SCOPE_NAMESPACE, "");
        }

        if (dev > 3.0 * sqrt(2.0))
        {
            strcpy(buffer2, buffer);
            strcat(buffer2, "_anomaly");
            AppendItem(classlist, buffer2, "3");
            EvalContextHeapPersistentSave(ctx, buffer2, CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
            EvalContextClassPutSoft(ctx, buffer2, CONTEXT_SCOPE_NAMESPACE, "");
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
        Log(LOG_LEVEL_VERBOSE, "Value accepted");
        return new;
    }
}

/***************************************************************/
/* Level 5                                                     */
/***************************************************************/

static void GatherPromisedMeasures(EvalContext *ctx, const Policy *policy)
{
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        const Bundle *bp = SeqAt(policy->bundles, i);
        EvalContextStackPushBundleFrame(ctx, bp, NULL, false);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_MONITOR]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            for (size_t j = 0; j < SeqLength(bp->promise_types); j++)
            {
                PromiseType *sp = SeqAt(bp->promise_types, j);

                EvalContextStackPushPromiseTypeFrame(ctx, sp);
                for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
                {
                    Promise *pp = SeqAt(sp->promises, ppi);
                    ExpandPromise(ctx, pp, KeepMonitorPromise, NULL);
                }
                EvalContextStackPopFrame(ctx);
            }
        }

        EvalContextStackPopFrame(ctx);
    }

    EvalContextClear(ctx);
    DetectEnvironment(ctx);
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static PromiseResult KeepMonitorPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);

    if (strcmp("vars", pp->parent_promise_type->name) == 0)
    {
        return PROMISE_RESULT_NOOP;
    }
    else if (strcmp("classes", pp->parent_promise_type->name) == 0)
    {
        return VerifyClassPromise(ctx, pp, NULL);
    }
    else if (strcmp("measurements", pp->parent_promise_type->name) == 0)
    {
        PromiseResult result = VerifyMeasurementPromise(ctx, CF_THIS, pp);
        return result;
    }
    else if (strcmp("reports", pp->parent_promise_type->name) == 0)
    {
        return PROMISE_RESULT_NOOP;
    }

    assert(false && "Unknown promise type");
    return PROMISE_RESULT_NOOP;
}
