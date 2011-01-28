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


/*****************************************************************************/
/*                                                                           */
/* File: env_monitor.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef HAVE_SYS_LOADAVG_H
# include <sys/loadavg.h>
#else
# define LOADAVG_5MIN    1
#endif

#include <math.h>

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

double HISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS];

int HISTO = true;

/* persistent observations */

double CF_THIS[CF_OBSERVABLES]; /* New from 2.1.21 replacing above - current observation */

/* Work */

int SLEEPTIME = 2.5 * 60;    /* Should be a fraction of 5 minutes */
int BATCH_MODE = false;

double ITER = 0.0;           /* Iteration since start */
double AGE,WAGE;             /* Age and weekly age of database */

char BATCHFILE[CF_BUFSIZE];
char STATELOG[CF_BUFSIZE];
char ENVFILE_NEW[CF_BUFSIZE];
char ENVFILE[CF_BUFSIZE];

short ACPI = false;
short LMSENSORS = false;
short SCLI = false;
short TCPDUMP = false;
short TCPPAUSE = false;
FILE *TCPPIPE;

struct Averages LOCALAV;

struct Item *ALL_INCOMING = NULL;
struct Item *ALL_OUTGOING = NULL;
struct Item *NETIN_DIST[CF_NETATTR];
struct Item *NETOUT_DIST[CF_NETATTR];

/* Leap Detection vars */

double LDT_BUF[CF_OBSERVABLES][LDT_BUFSIZE];
double LDT_SUM[CF_OBSERVABLES];
double LDT_AVG[CF_OBSERVABLES];
double CHI_LIMIT[CF_OBSERVABLES];
double CHI[CF_OBSERVABLES];
double LDT_MAX[CF_OBSERVABLES];
int LDT_POS = 0;
int LDT_FULL = false;

/* Entropy etc */

double ENTROPY = 0.0;
double LAST_HOUR_ENTROPY = 0.0;
double LAST_DAY_ENTROPY = 0.0;

struct Item *PREVIOUS_STATE = NULL;
struct Item *ENTROPIES = NULL;

int NO_FORK = false;
double LASTQ[CF_OBSERVABLES];
long LASTT[CF_OBSERVABLES];

/*******************************************************************/
/* Prototypes                                                      */
/*******************************************************************/

void MonInitialize(void);
void StartServer (int argc, char **argv);
void yyerror (char *s);
void RotateFiles (char *s, int n);

void GetDatabaseAge (void);
void LoadHistogram  (void);
void GetQ (void);
char *GetTimeKey (void);
struct Averages EvalAvQ (char *timekey);
void ArmClasses (struct Averages newvals,char *timekey);
void OpenSniffer(void);
void Sniff(void);

void GatherProcessData (void);
void GatherCPUData (void);
void Unix_GatherCPUData(void);
void GatherDiskData (void);
void GatherLoadData (void);
void GatherSocketData (void);
void GatherSensorData(void);
void GatherPromisedMeasures(void);

int GatherProcessUsers(struct Item **userList, int *userListSz, int *numRootProcs, int *numOtherProcs);
void LeapDetection (void);
struct Averages *GetCurrentAverages (char *timekey);
void UpdateAverages (char *timekey, struct Averages newvals);
void UpdateDistributions (char *timekey, struct Averages *av);
double WAverage (double newvals,double oldvals, double age);
double SetClasses (char *name,double variable,double av_expect,double av_var,double localav_expect,double localav_var,struct Item **classlist,char *timekey);
void SetVariable (char *name,double now, double average, double stddev, struct Item **list);
void RecordChangeOfState  (struct Item *list,char *timekey);
double RejectAnomaly (double new,double av,double var,double av2,double var2);
void SetEntropyClasses (char *service,struct Item *list,char *inout);
void AnalyzeArrival (char *tcpbuffer);
void ZeroArrivals (void);
void CfenvTimeOut (void);
void IncrementCounter (struct Item **list,char *name);
void SaveTCPEntropyData (struct Item *list,int i, char *inout);
int GetFileGrowth(char *filename,enum observables index);
int GetAcpi(void);
int GetLMSensors(void);
void KeepMonitorPromise(struct Promise *pp);

#ifndef MINGW
int Unix_GatherProcessUsers(struct Item **userList, int *userListSz, int *numRootProcs, int *numOtherProcs);
void Unix_GatherProcessData (void);
#endif /* NOT MINGW */

/****************************************************************/

void MonInitialize()
   
{ int i,j,k;
 struct stat statbuf;
 char vbuff[CF_BUFSIZE];
  
 for (i = 0; i < ATTR; i++)
    {
    sprintf(vbuff,"%s/state/cf_incoming.%s",CFWORKDIR,ECGSOCKS[i].name);
    MapName(vbuff);
    CreateEmptyFile(vbuff);
   
    sprintf(vbuff,"%s/state/cf_outgoing.%s",CFWORKDIR,ECGSOCKS[i].name);
    MapName(vbuff);
    CreateEmptyFile(vbuff);
    }

 for (i = 0; i < CF_NETATTR; i++)
    {
    NETIN_DIST[i] = NULL;
    NETOUT_DIST[i] = NULL;
    }
 
 sprintf(vbuff,"%s/state/cf_users",CFWORKDIR);
 MapName(vbuff);
 CreateEmptyFile(vbuff);
 
 snprintf(AVDB,CF_MAXVARSIZE,"%s/state/%s",CFWORKDIR,CF_AVDB_FILE);
 MapName(AVDB);

 snprintf(STATELOG,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATELOG_FILE);
 MapName(STATELOG);

 snprintf(ENVFILE_NEW,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENVNEW_FILE);
 MapName(ENVFILE_NEW);

 snprintf(ENVFILE,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENV_FILE);
 MapName(ENVFILE);

 if (!BATCH_MODE)
    {
    GetDatabaseAge();

    for (i = 0; i < CF_OBSERVABLES; i++)
       {
       LOCALAV.Q[i].expect = 0.0;
       LOCALAV.Q[i].var = 0.0;
       LOCALAV.Q[i].q = 0.0;
       }
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
    LASTQ[i] = 0;
    LASTT[i] = 0;
   
    for (j = 0; j < LDT_BUFSIZE; j++)
       {
       LDT_BUF[i][j];
       }
    }

 srand((unsigned int)time(NULL)); 
 LoadHistogram();

/* Look for local sensors - this is unfortunately linux-centric */

 if (cfstat("/proc/acpi/thermal_zone",&statbuf) != -1)
    {
    Debug("Found an acpi service\n");
    ACPI = true;
    }

 if (cfstat("/usr/bin/sensors",&statbuf) != -1)
    {
    if (statbuf.st_mode & 0111)
       {
       Debug("Found an lmsensor system\n");
       LMSENSORS = true;
       }
    }

 Debug("Finished with initialization.\n");
}

/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

void GetDatabaseAge()

{ int err_no;
 CF_DB *dbp;

 if (!OpenDB(AVDB,&dbp))
    {
    return;
    }
 
 cf_chmod(AVDB,0644); 

 if (ReadDB(dbp,"DATABASE_AGE",&AGE,sizeof(double)))
    {
    WAGE = AGE / CF_WEEK * CF_MEASURE_INTERVAL;
    Debug("\n\nPrevious DATABASE_AGE %f\n\n",AGE);
    }
 else
    {
    Debug("No previous AGE\n");
    AGE = 0.0;
    }

 CloseDB(dbp);
}

/*********************************************************************/

void LoadHistogram()

{ FILE *fp;
  int i,day,position;
  double maxval[CF_OBSERVABLES];

if (HISTO)
   {
   char filename[CF_BUFSIZE];
   
   snprintf(filename,CF_BUFSIZE,"%s/state/histograms",CFWORKDIR);
   
   if ((fp = fopen(filename,"r")) == NULL)
      {
      CfOut(cf_verbose,"fopen","Unable to load histogram data");
      return;
      }
   
   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      maxval[i] = 1.0;
      }
  
   for (position = 0; position < CF_GRAINS; position++)
      {
      fscanf(fp,"%d ",&position);
      
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         for (day = 0; day < 7; day++)
            {
            fscanf(fp,"%lf ",&(HISTOGRAM[i][day][position]));
            }

         if (HISTOGRAM[i][day][position] < 0)
            {
            HISTOGRAM[i][day][position] = 0;
            }

         if (HISTOGRAM[i][day][position] > maxval[i])
            {
            maxval[i] = HISTOGRAM[i][day][position];
            }

         HISTOGRAM[i][day][position] *= 1000.0/maxval[i];
         }
      }
   
   fclose(fp);
   }
} 

/*********************************************************************/

void StartServer(int argc,char **argv)

{ char *timekey;
 struct Averages averages;
 int i;
 struct Promise *pp = NewPromise("monitor_cfengine","the monitor daemon");
 struct Attributes dummyattr;
 struct CfLock thislock;

#ifdef MINGW

 if(!NO_FORK)
    {
    CfOut(cf_verbose, "", "Windows does not support starting processes in the background - starting in foreground");
    }

#else  /* NOT MINGW */  
  
 if ((!NO_FORK) && (fork() != 0))
    {
    CfOut(cf_inform,"","cf-monitord: starting\n");
    exit(0);
    }

 if (!NO_FORK)
    {
    ActAsDaemon(0);
    }

#endif  /* NOT MINGW */   
   
 memset(&dummyattr,0,sizeof(dummyattr));
 dummyattr.transaction.ifelapsed = 0;
 dummyattr.transaction.expireafter = 0;

 thislock = AcquireLock(pp->promiser,VUQNAME,CFSTARTTIME,dummyattr,pp,false);

 if (thislock.lock == NULL)
    {
    return;
    }

 WritePID("cf-monitord.pid");

 OpenSniffer();
 
 while (true)
    {
    GetQ();
    timekey = GetTimeKey();
    averages = EvalAvQ(timekey);
    LeapDetection();
    ArmClasses(averages,timekey);

    ZeroArrivals();

    if (TCPDUMP)
       {
       Sniff();
       }
    else
       {
       sleep(SLEEPTIME);
       }
   
    ITER++;
    }
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/
  
void CfenvTimeOut()
 
{
alarm(0);
TCPPAUSE = true;
CfOut(cf_verbose,"","Time out\n");
}

/*********************************************************************/

void OpenSniffer()

{ char tcpbuffer[CF_BUFSIZE];

 if (TCPDUMP)
    {
    struct stat statbuf;
    char buffer[CF_MAXVARSIZE];
    sscanf(CF_TCPDUMP_COMM,"%s",buffer);
   
    if (cfstat(buffer,&statbuf) != -1)
       {
       if ((TCPPIPE = cf_popen(CF_TCPDUMP_COMM,"r")) == NULL)
          {
          TCPDUMP = false;
          }

       /* Skip first banner */
       fgets(tcpbuffer,CF_BUFSIZE-1,TCPPIPE);
       }
    else
       {
       TCPDUMP = false;
       }
    }

}

/*********************************************************************/

void Sniff()

{ int i;
 char tcpbuffer[CF_BUFSIZE];
 
CfOut(cf_verbose,"","Reading from tcpdump...\n");
memset(tcpbuffer,0,CF_BUFSIZE);      
signal(SIGALRM,(void *)CfenvTimeOut);
alarm(SLEEPTIME);
TCPPAUSE = false;

while (!feof(TCPPIPE))
   {
   if (TCPPAUSE)
      {
      break;
      }
   
   fgets(tcpbuffer,CF_BUFSIZE-1,TCPPIPE);
   
   if (TCPPAUSE)
       {
       break;
       }
   
   if (strstr(tcpbuffer,"tcpdump:")) /* Error message protect sleeptime */
      {
      Debug("Error - (%s)\n",tcpbuffer);
      alarm(0);
      TCPDUMP = false;
      break;
      }
   
   AnalyzeArrival(tcpbuffer);
   }

signal(SIGALRM,SIG_DFL);
TCPPAUSE = false;
fflush(TCPPIPE);
}

/*********************************************************************/

void GetQ()

{ int i;

Debug("========================= GET Q ==============================\n");

ENTROPIES = NULL;

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   CF_THIS[i] = 0.0;
   }

GatherProcessData();
GatherCPUData();
#ifndef MINGW
GatherLoadData(); 
GatherDiskData();
GatherSocketData();
GatherSensorData();
#endif  /* NOT MINGW */
GatherPromisedMeasures();
}

/*********************************************************************/

char *GetTimeKey()

{ time_t now;
  char str[CF_SMALLBUF];
  
if ((now = time((time_t *)NULL)) == -1)
   {
   exit(1);
   }

sprintf(str,"%s",cf_ctime(&now));

return ConvTimeKey(str); 
}


/*********************************************************************/

struct Averages EvalAvQ(char *t)

{ struct Averages *currentvals,newvals;
  double This[CF_OBSERVABLES],delta2;
  char name[CF_MAXVARSIZE];
  int i; 

Banner("Evaluating and storing new weekly averages");
  
if ((currentvals = GetCurrentAverages(t)) == NULL)
   {
   CfOut(cf_error,"","Error reading average database");
   exit(1);
   }

/* Discard any apparently anomalous behaviour before renormalizing database */

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   double delta2;
   
   name[0] = '\0';
   CfGetClassName(i,name);
   
   /* Overflow protection */
   
   if (currentvals->Q[i].expect < 0)
      {
      currentvals->Q[i].expect = 0;
      }
   
   if (currentvals->Q[i].q < 0)
      {
      currentvals->Q[i].q = 0;
      }
   
   if (currentvals->Q[i].var < 0)
      {
      currentvals->Q[i].var = 0;
      }
   
   This[i] = RejectAnomaly(CF_THIS[i],currentvals->Q[i].expect,currentvals->Q[i].var,LOCALAV.Q[i].expect,LOCALAV.Q[i].var);

   newvals.Q[i].q = This[i];
   LOCALAV.Q[i].q = This[i];
   
   Debug("Current %s.q %lf\n",name,currentvals->Q[i].q);
   Debug("Current %s.var %lf\n",name,currentvals->Q[i].var);
   Debug("Current %s.ex %lf\n",name,currentvals->Q[i].expect);
   Debug("CF_THIS[%s] = %lf\n",name,CF_THIS[i]);
   Debug("This[%s] = %lf\n",name,This[i]);
   
   newvals.Q[i].expect = WAverage(This[i],currentvals->Q[i].expect,WAGE);
   LOCALAV.Q[i].expect = WAverage(newvals.Q[i].expect,LOCALAV.Q[i].expect,ITER);
   
   delta2 = (This[i] - currentvals->Q[i].expect)*(This[i] - currentvals->Q[i].expect);
   
   if (currentvals->Q[i].var > delta2*2.0)
      {
      /* Clean up past anomalies */
      newvals.Q[i].var = delta2;
      LOCALAV.Q[i].var = WAverage(newvals.Q[i].var,LOCALAV.Q[i].var,ITER);
      }
   else
      {
      newvals.Q[i].var = WAverage(delta2,currentvals->Q[i].var,WAGE);
      LOCALAV.Q[i].var = WAverage(newvals.Q[i].var,LOCALAV.Q[i].var,ITER);
      }
   
   CfOut(cf_verbose,"","New[%d] %s.q %lf\n",i,name,newvals.Q[i].q);
   CfOut(cf_verbose,"","New[%d] %s.var %lf\n",i,name,newvals.Q[i].var);
   CfOut(cf_verbose,"","New[%d] %s.ex %lf\n",i,name,newvals.Q[i].expect);
   
   CfOut(cf_verbose,"","%s = %lf -> (%lf#%lf) local [%lf#%lf]\n",name,This[i],newvals.Q[i].expect,sqrt(newvals.Q[i].var),LOCALAV.Q[i].expect,sqrt(LOCALAV.Q[i].var));
   
   if (This[i] > 0)
      {
      CfOut(cf_verbose,"","Storing %.2lf in %s\n",This[i],name);
      }
   }

UpdateAverages(t,newvals);
UpdateDistributions(t,currentvals);  /* Distribution about mean */

return newvals;
}

/*********************************************************************/

void LeapDetection()

{ int i,j,last_pos = LDT_POS;
  double n1,n2,d;
  double padding = 0.2;

if (++LDT_POS >= LDT_BUFSIZE)
   {
   LDT_POS = 0;
   
   if (!LDT_FULL)
      {
      Debug("LDT Buffer full at %d\n",LDT_BUFSIZE);
      LDT_FULL = true;
      }
   }

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   // First do some anomaly rejection. Sudden jumps must be numerical errors.
   
   if (LDT_BUF[i][last_pos] > 0 && CF_THIS[i]/LDT_BUF[i][last_pos] > 1000)
      {
      CF_THIS[i] = LDT_BUF[i][last_pos];
      }

   /* Note AVG should contain n+1 but not SUM, hence funny increments */

   LDT_AVG[i] = LDT_AVG[i] + CF_THIS[i]/((double)LDT_BUFSIZE + 1.0);
   
   d = (double)(LDT_BUFSIZE * (LDT_BUFSIZE + 1)) * LDT_AVG[i];
   
   if (LDT_FULL && (LDT_POS == 0))
      {
      n2 = (LDT_SUM[i] - (double)LDT_BUFSIZE * LDT_MAX[i]);
      
      if (d < 0.001)
         {
         CHI_LIMIT[i] = 0.5;
         }
      else
         {
         CHI_LIMIT[i] = padding + sqrt(n2*n2/d);
         }
      
      LDT_MAX[i] = 0.0;
      }
   
   if (CF_THIS[i] > LDT_MAX[i])
      {
      LDT_MAX[i] = CF_THIS[i];
      }
   
   n1 = (LDT_SUM[i] - (double)LDT_BUFSIZE * CF_THIS[i]);
   
   if (d < 0.001)
      {
      CHI[i] = 0.0;
      }
   else
      {
      CHI[i] = sqrt(n1*n1/d);
      }
   
   LDT_AVG[i] = LDT_AVG[i] - LDT_BUF[i][LDT_POS]/((double)LDT_BUFSIZE + 1.0);
   LDT_BUF[i][LDT_POS] = CF_THIS[i];
   LDT_SUM[i] = LDT_SUM[i] - LDT_BUF[i][LDT_POS] + CF_THIS[i];
   }
}

/*********************************************************************/

void ArmClasses(struct Averages av,char *timekey)

{ double sigma;
 struct Item *classlist = NULL, *ip;
 int i,j,k,pos;
 FILE *fp;
 char buff[CF_BUFSIZE],ldt_buff[CF_BUFSIZE],name[CF_MAXVARSIZE];
 static int anomaly[CF_OBSERVABLES][LDT_BUFSIZE];
 static double anomaly_chi[CF_OBSERVABLES];
 static double anomaly_chi_limit[CF_OBSERVABLES];

 Debug("Arm classes for %s\n",timekey);
 
 for (i = 0; i < CF_OBSERVABLES; i++)
    {
    CfGetClassName(i,name);
    sigma = SetClasses(name,CF_THIS[i],av.Q[i].expect,av.Q[i].var,LOCALAV.Q[i].expect,LOCALAV.Q[i].var,&classlist,timekey);
    SetVariable(name,CF_THIS[i],av.Q[i].expect,sigma,&classlist);

    /* LDT */

    ldt_buff[0] = '\0';

    anomaly[i][LDT_POS] = false;

    if (!LDT_FULL)
       {
       anomaly[i][LDT_POS] = false;
       anomaly_chi[i] = 0.0;
       anomaly_chi_limit[i] = 0.0;
       }
   
    if (LDT_FULL && (CHI[i] > CHI_LIMIT[i]))
       {
       anomaly[i][LDT_POS] = true;                   /* Remember the last anomaly value */
       anomaly_chi[i] = CHI[i];
       anomaly_chi_limit[i] = CHI_LIMIT[i];
      
       CfOut(cf_verbose,"","LDT(%d) in %s chi = %.2f thresh %.2f \n",LDT_POS,name,CHI[i],CHI_LIMIT[i]);

       /* Last printed element is now */
      
       for (j = LDT_POS+1, k = 0; k < LDT_BUFSIZE; j++,k++)
          {
          if (j == LDT_BUFSIZE) /* Wrap */
             {
             j = 0;
             }
         
          if (anomaly[i][j])
             {
             snprintf(buff,CF_BUFSIZE," *%.2f*",LDT_BUF[i][j]);
             }
          else
             {
             snprintf(buff,CF_BUFSIZE," %.2f",LDT_BUF[i][j]);
             }

          strcat(ldt_buff,buff);
          }

       if (CF_THIS[i] > av.Q[i].expect)
          {
          snprintf(buff,CF_BUFSIZE,"%s_high_ldt",name);
          }
       else
          {
          snprintf(buff,CF_BUFSIZE,"%s_high_ldt",name);
          }

       AppendItem(&classlist,buff,"2");
       NewPersistentContext(buff,CF_PERSISTENCE,cfpreserve); 
       }
    else
       {
       for (j = LDT_POS+1, k = 0; k < LDT_BUFSIZE; j++,k++)
          {
          if (j == LDT_BUFSIZE) /* Wrap */
             {
             j = 0;
             }

          if (anomaly[i][j])
             {
             snprintf(buff,CF_BUFSIZE," *%.2f*",LDT_BUF[i][j]);
             }
          else
             {
             snprintf(buff,CF_BUFSIZE," %.2f",LDT_BUF[i][j]);
             }
          strcat(ldt_buff,buff);
          }
       }

    /* Not using these for now
       snprintf(buff,CF_MAXVARSIZE,"ldtbuf_%s=%s",name,ldt_buff);
       AppendItem(&classlist,buff,"");

       snprintf(buff,CF_MAXVARSIZE,"ldtchi_%s=%.2f",name,anomaly_chi[i]);
       AppendItem(&classlist,buff,"");
   
       snprintf(buff,CF_MAXVARSIZE,"ldtlimit_%s=%.2f",name,anomaly_chi_limit[i]);
       AppendItem(&classlist,buff,"");
    */
    }

SetMeasurementPromises(&classlist);

/* Publish class list */

unlink(ENVFILE_NEW);

if ((fp = fopen(ENVFILE_NEW,"a")) == NULL)
   {
   DeleteItemList(PREVIOUS_STATE);
   PREVIOUS_STATE = classlist;
   return; 
   }

for (ip = classlist; ip != NULL; ip=ip->next)
   {
   fprintf(fp,"%s\n",ip->name);
   }

DeleteItemList(PREVIOUS_STATE);
PREVIOUS_STATE = classlist;

for (ip = ENTROPIES; ip != NULL; ip=ip->next)
   {
   fprintf(fp,"%s\n",ip->name);
   }

DeleteItemList(ENTROPIES); 
fclose(fp);

cf_rename(ENVFILE_NEW,ENVFILE);
}

/*********************************************************************/

void AnalyzeArrival(char *arrival)

/* This coarsely classifies TCP dump data */
    
{ char src[CF_BUFSIZE],dest[CF_BUFSIZE], flag = '.', *arr;
 int isme_dest, isme_src;

 src[0] = dest[0] = '\0';
 
 if (strstr(arrival,"listening"))
    {
    return;
    }
 
 Chop(arrival);      

 /* Most hosts have only a few dominant services, so anomalies will
    show up even in the traffic without looking too closely. This
    will apply only to the main interface .. not multifaces 

    New format in tcpdump
    
    IP (tos 0x10, ttl 64, id 14587, offset 0, flags [DF], proto TCP (6), length 692) 128.39.89.232.22 > 84.215.40.125.48022: P 1546432:1547072(640) ack 1969 win 1593 <nop,nop,timestamp 25662737 1631360>
    IP (tos 0x0, ttl 251, id 14109, offset 0, flags [DF], proto UDP (17), length 115) 84.208.20.110.53 > 192.168.1.103.32769: 45266 NXDomain 0/1/0 (87)
    arp who-has 192.168.1.1 tell 192.168.1.103
    arp reply 192.168.1.1 is-at 00:1d:7e:28:22:c6
    IP (tos 0x0, ttl 64, id 0, offset 0, flags [DF], proto ICMP (1), length 84) 192.168.1.103 > 128.39.89.10: ICMP echo request, id 48474, seq 1, length 64
    IP (tos 0x0, ttl 64, id 0, offset 0, flags [DF], proto ICMP (1), length 84) 192.168.1.103 > 128.39.89.10: ICMP echo request, id 48474, seq 2, length 64

 */

 for (arr = strstr(arrival,"length"); arr != NULL && *arr != ')'; arr++)
    {
    }

 if (arr == NULL)
    {
    arr = arrival;
    }
 else
    {
    arr++;
    }

 if (strstr(arrival,"proto TCP") || strstr(arrival,"ack"))
    {              
    sscanf(arr,"%s %*c %s %c ",src,dest,&flag);
    DePort(src);
    DePort(dest);
    isme_dest = IsInterfaceAddress(dest);
    isme_src = IsInterfaceAddress(src);
   
    switch (flag)
       {
       case 'S': Debug("%1.1lf: TCP new connection from %s to %s - i am %s\n",ITER,src,dest,VIPADDRESS);
           if (isme_dest)
              {
              CF_THIS[ob_tcpsyn_in]++;
              IncrementCounter(&(NETIN_DIST[tcpsyn]),src);
              }
           else if (isme_src)
              {
              CF_THIS[ob_tcpsyn_out]++;
              IncrementCounter(&(NETOUT_DIST[tcpsyn]),dest);
              }       
           break;
           
       case 'F': Debug("%1.1lf: TCP end connection from %s to %s\n",ITER,src,dest);
           if (isme_dest)
              {
              CF_THIS[ob_tcpfin_in]++;
              IncrementCounter(&(NETIN_DIST[tcpfin]),src);
              }
           else if (isme_src)
              {
              CF_THIS[ob_tcpfin_out]++;
              IncrementCounter(&(NETOUT_DIST[tcpfin]),dest);
              }       
           break;
           
       default: Debug("%1.1lf: TCP established from %s to %s\n",ITER,src,dest);
           
           if (isme_dest)
              {
              CF_THIS[ob_tcpack_in]++;
              IncrementCounter(&(NETIN_DIST[tcpack]),src);
              }
           else if (isme_src)
              {
              CF_THIS[ob_tcpack_out]++;
              IncrementCounter(&(NETOUT_DIST[tcpack]),dest);
              }       
           break;
       }
    
    }
 else if (strstr(arrival,".53"))
    {
    sscanf(arr,"%s %*c %s %c ",src,dest,&flag);
    DePort(src);
    DePort(dest);
    isme_dest = IsInterfaceAddress(dest);
    isme_src = IsInterfaceAddress(src);
   
    Debug("%1.1lf: DNS packet from %s to %s\n",ITER,src,dest);
    if (isme_dest)
       {
       CF_THIS[ob_dns_in]++;
       IncrementCounter(&(NETIN_DIST[dns]),src);
       }
    else if (isme_src)
       {
       CF_THIS[ob_dns_out]++;
       IncrementCounter(&(NETOUT_DIST[tcpack]),dest);
       }       
    }
 else if (strstr(arrival,"proto UDP"))
    {
    sscanf(arr,"%s %*c %s %c ",src,dest,&flag);
    DePort(src);
    DePort(dest);
    isme_dest = IsInterfaceAddress(dest);
    isme_src = IsInterfaceAddress(src);
   
    Debug("%1.1lf: UDP packet from %s to %s\n",ITER,src,dest);
    if (isme_dest)
       {
       CF_THIS[ob_udp_in]++;
       IncrementCounter(&(NETIN_DIST[udp]),src);
       }
    else if (isme_src)
       {
       CF_THIS[ob_udp_out]++;
       IncrementCounter(&(NETOUT_DIST[udp]),dest);
       }       
    }
 else if (strstr(arrival,"proto ICMP"))
    {
    sscanf(arr,"%s %*c %s %c ",src,dest,&flag);
    DePort(src);
    DePort(dest);
    isme_dest = IsInterfaceAddress(dest);
    isme_src = IsInterfaceAddress(src);
   
    Debug("%1.1lf: ICMP packet from %s to %s\n",ITER,src,dest);
   
    if (isme_dest)
       {
       CF_THIS[ob_icmp_in]++;
       IncrementCounter(&(NETIN_DIST[icmp]),src);
       }
    else if (isme_src)
       {
       CF_THIS[ob_icmp_out]++;
       IncrementCounter(&(NETOUT_DIST[icmp]),src);
       }       
    }
 else
    {
    Debug("%1.1lf: Miscellaneous undirected packet (%.100s)\n",ITER,arrival);
   
    CF_THIS[ob_tcpmisc_in]++;
   
    /* Here we don't know what source will be, but .... */
   
    sscanf(arrival,"%s",src);
   
    if (!isdigit((int)*src))
       {
       Debug("Assuming continuation line...\n");
       return;
       }
   
    DePort(src);
   
    if (strstr(arrival,".138"))
       {
       snprintf(dest,CF_BUFSIZE-1,"%s NETBIOS",src);
       }
    else if (strstr(arrival,".2049"))
       {
       snprintf(dest,CF_BUFSIZE-1,"%s NFS",src);
       }
    else
       {
       strncpy(dest,src,60);
       }
   
    IncrementCounter(&(NETIN_DIST[tcpmisc]),dest);
    }
}

/*********************************************************************/
/* Level 4                                                           */
/*********************************************************************/

void GatherProcessData()

{ struct Item *userList = NULL;
  char vbuff[CF_BUFSIZE];
  int numProcUsers = 0;
  int numRootProcs = 0;
  int numOtherProcs = 0;

if (!GatherProcessUsers(&userList, &numProcUsers, &numRootProcs, &numOtherProcs))
   {
   return;
   }

CF_THIS[ob_users] += numProcUsers;
CF_THIS[ob_rootprocs] += numRootProcs;
CF_THIS[ob_otherprocs] += numOtherProcs;

snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_users",CFWORKDIR);
MapName(vbuff);
RawSaveItemList(userList,vbuff);

DeleteItemList(userList);

CfOut(cf_verbose,"","(Users,root,other) = (%d,%d,%d)\n",CF_THIS[ob_users],CF_THIS[ob_rootprocs],CF_THIS[ob_otherprocs]);
}

/*****************************************************************************/

int GatherProcessUsers(struct Item **userList, int *userListSz, int *numRootProcs, int *numOtherProcs)
{
#ifdef MINGW
 return NovaWin_GatherProcessUsers(userList, userListSz, numRootProcs, numOtherProcs);
#else
 return Unix_GatherProcessUsers(userList, userListSz, numRootProcs, numOtherProcs);
#endif
}

/*****************************************************************************/

void GatherCPUData()
{
#ifdef MINGW
NovaWin_GatherCPUData(CF_THIS);
#else
Unix_GatherCPUData();
#endif
}

/*****************************************************************************/

void GatherDiskData()

{ char accesslog[CF_BUFSIZE];
  char errorlog[CF_BUFSIZE];
  char syslog[CF_BUFSIZE];
  char messages[CF_BUFSIZE];
 
CfOut(cf_verbose,"","Gathering disk data\n");
CF_THIS[ob_diskfree] = GetDiskUsage("/",cfpercent);
CfOut(cf_verbose,"","Disk free = %d %%\n",CF_THIS[ob_diskfree]);

/* Here would should have some detection based on OS type VSYSTEMHARDCLASS */

switch(VSYSTEMHARDCLASS)
   {
   linuxx:
   
   default:
       strcpy(accesslog,"/var/log/apache2/access_log");
       strcpy(errorlog,"/var/log/apache2/error_log");
       strcpy(syslog,"/var/log/syslog");
       strcpy(messages,"/var/log/messages");
   }

CF_THIS[ob_webaccess] = GetFileGrowth(accesslog,ob_webaccess);
CfOut(cf_verbose,"","Webaccess = %d %%\n",CF_THIS[ob_webaccess]);
CF_THIS[ob_weberrors] = GetFileGrowth(errorlog,ob_weberrors);
CfOut(cf_verbose,"","Web error = %d %%\n",CF_THIS[ob_weberrors]);
CF_THIS[ob_syslog] = GetFileGrowth(syslog,ob_syslog);
CfOut(cf_verbose,"","Syslog = %d %%\n",CF_THIS[ob_syslog]);
CF_THIS[ob_messages] = GetFileGrowth(messages,ob_messages);
CfOut(cf_verbose,"","Messages = %d %%\n",CF_THIS[ob_messages]);
}

/*****************************************************************************/

void GatherLoadData()

{ double load[4] = {0,0,0,0}, sum = 0.0; 
  int i,n = 1;

Debug("GatherLoadData\n\n");

#ifdef HAVE_GETLOADAVG 
if ((n = getloadavg(load,LOADAVG_5MIN)) == -1)
   {
   CF_THIS[ob_loadavg] = 0.0;
   }
else
   {
   for (i = 0; i < n; i++)
      {
      Debug("Found load average to be %lf of %d samples\n", load[i],n);
      sum += load[i];
      }
   }
#endif

/* Scale load average by 100 to make it visible */

CF_THIS[ob_loadavg] = (int) (100.0 * sum);
CfOut(cf_verbose,"","100 x Load Average = %d\n",CF_THIS[ob_loadavg]);
}

/*****************************************************************************/

void GatherSocketData()
    
{ FILE *pp;
  char local[CF_BUFSIZE],remote[CF_BUFSIZE],comm[CF_BUFSIZE];
  struct Item *in[ATTR],*out[ATTR];
  char *sp;
  int i;
  char vbuff[CF_BUFSIZE];
    
Debug("GatherSocketData()\n");
  
for (i = 0; i < ATTR; i++)
   {
   in[i] = out[i] = NULL;
   }

if (ALL_INCOMING != NULL)
   {
   DeleteItemList(ALL_INCOMING);
   ALL_INCOMING = NULL;
   }

if (ALL_OUTGOING != NULL)
   {
   DeleteItemList(ALL_OUTGOING);
   ALL_OUTGOING = NULL;
   } 

sscanf(VNETSTAT[VSYSTEMHARDCLASS],"%s",comm);

strcat(comm," -n"); 

if ((pp = cf_popen(comm,"r")) == NULL)
   {
   return;
   }

while (!feof(pp))
   {
   memset(local,0,CF_BUFSIZE);
   memset(remote,0,CF_BUFSIZE);
   
   CfReadLine(vbuff,CF_BUFSIZE,pp);
   
   if (strstr(vbuff,"UNIX"))
      {
      break;
      }
   
   if (!strstr(vbuff,"."))
      {
      continue;
      }
   
   /* Different formats here ... ugh.. */
   
   if (strncmp(vbuff,"tcp",3) == 0)
      {
      sscanf(vbuff,"%*s %*s %*s %s %s",local,remote); /* linux-like */
      }
   else
      {
      sscanf(vbuff,"%s %s",local,remote);             /* solaris-like */
      } 
   
   if (strlen(local) == 0)
      {
      continue;
      }
   
   for (sp = local+strlen(local); (*sp != '.') && (sp > local); sp--)
      {
      }
   
   sp++;
   
   if ((strlen(sp) < 5) &&!IsItemIn(ALL_INCOMING,sp))
      {
      PrependItem(&ALL_INCOMING,sp,NULL);
      }
   
   for (sp = remote+strlen(remote); (sp >= remote) && !isdigit((int)*sp); sp--)
      {
      }
   
   sp++;
   
   if ((strlen(sp) < 5) && !IsItemIn(ALL_OUTGOING,sp))
      {
      PrependItem(&ALL_OUTGOING,sp,NULL);
      }
   
   for (i = 0; i < ATTR; i++)
      {
      char *spend;
      
      for (spend = local+strlen(local)-1; isdigit((int)*spend); spend--)
         {
         }
      
      spend++;
      
      if (strcmp(spend,ECGSOCKS[i].portnr) == 0)
         {
         CF_THIS[ECGSOCKS[i].in]++;
         AppendItem(&in[i],vbuff,"");
         }
      
      for (spend = remote+strlen(remote)-1; (sp >= remote) && isdigit((int)*spend); spend--)
         {
         }
      
      spend++;
      
      if (strcmp(spend,ECGSOCKS[i].portnr) == 0)
         {
         CF_THIS[ECGSOCKS[i].out]++;
         AppendItem(&out[i],vbuff,"");
         }
      }
   }

cf_pclose(pp);

/* Now save the state for ShowState() cf2 version alert function IFF
   the state is not smaller than the last or at least 40 minutes
   older. This mirrors the persistence of the maxima classes */

 
for (i = 0; i < ATTR; i++)
   {
   struct stat statbuf;
   time_t now = time(NULL);
   
   Debug("save incoming %s\n",ECGSOCKS[i].name);
   snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_incoming.%s",CFWORKDIR,ECGSOCKS[i].name);
   if (cfstat(vbuff,&statbuf) != -1)
      {
      if ((ByteSizeList(in[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
         {
         CfOut(cf_verbose,"","New state %s is smaller, retaining old for 40 mins longer\n",ECGSOCKS[i].name);
         DeleteItemList(in[i]);
         continue;
         }
      }
   
   SetEntropyClasses(ECGSOCKS[i].name,in[i],"in");
   RawSaveItemList(in[i],vbuff);
   DeleteItemList(in[i]);
   Debug("Saved in netstat data in %s\n",vbuff); 
   }

for (i = 0; i < ATTR; i++)
   {
   struct stat statbuf;
   time_t now = time(NULL); 
   
   Debug("save outgoing %s\n",ECGSOCKS[i].name);
   snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_outgoing.%s",CFWORKDIR,ECGSOCKS[i].name);
   
   if (cfstat(vbuff,&statbuf) != -1)
      {       
      if ((ByteSizeList(out[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
         {
         CfOut(cf_verbose,"","New state %s is smaller, retaining old for 40 mins longer\n",ECGSOCKS[i].name);
         DeleteItemList(out[i]);
         continue;
         }
      }
   
   SetEntropyClasses(ECGSOCKS[i].name,out[i],"out");
   RawSaveItemList(out[i],vbuff);
   Debug("Saved out netstat data in %s\n",vbuff); 
   DeleteItemList(out[i]);
   }

for (i = 0; i < CF_NETATTR; i++)
   {
   struct stat statbuf;
   time_t now = time(NULL); 
   
   Debug("save incoming %s\n",TCPNAMES[i]);
   snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_incoming.%s",CFWORKDIR,TCPNAMES[i]);
   
   if (cfstat(vbuff,&statbuf) != -1)
      {       
      if ((ByteSizeList(NETIN_DIST[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
         {
         CfOut(cf_verbose,"","New state %s is smaller, retaining old for 40 mins longer\n",TCPNAMES[i]);
         DeleteItemList(NETIN_DIST[i]);
         NETIN_DIST[i] = NULL;
         continue;
         }
      }
   
   SaveTCPEntropyData(NETIN_DIST[i],i,"in");
   SetEntropyClasses(TCPNAMES[i],NETIN_DIST[i],"in");
   DeleteItemList(NETIN_DIST[i]);
   NETIN_DIST[i] = NULL;
   }


for (i = 0; i < CF_NETATTR; i++)
   {
   struct stat statbuf;
   time_t now = time(NULL); 
   
   Debug("save outgoing %s\n",TCPNAMES[i]);
   snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_outgoing.%s",CFWORKDIR,TCPNAMES[i]);
   
   if (cfstat(vbuff,&statbuf) != -1)
      {       
      if ((ByteSizeList(NETOUT_DIST[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
         {
         CfOut(cf_verbose,"","New state %s is smaller, retaining old for 40 mins longer\n",TCPNAMES[i]);
         DeleteItemList(NETOUT_DIST[i]);
         NETOUT_DIST[i] = NULL;   
         continue;
         }
      }
   
   SaveTCPEntropyData(NETOUT_DIST[i],i,"out");
   SetEntropyClasses(TCPNAMES[i],NETOUT_DIST[i],"out");
   DeleteItemList(NETOUT_DIST[i]);
   NETOUT_DIST[i] = NULL;
   }
}

/*****************************************************************************/

struct Averages *GetCurrentAverages(char *timekey)

{ int err_no;
  CF_DB *dbp;
  static struct Averages entry;

if (!OpenDB(AVDB,&dbp))
   {
   return NULL;
   }

memset(&entry,0,sizeof(entry));

AGE++;
WAGE = AGE / CF_WEEK * CF_MEASURE_INTERVAL;

if (ReadDB(dbp,timekey,&entry,sizeof(struct Averages)))
   {
   int i;
   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      Debug("Previous values (%lf,..) for time index %s\n\n",entry.Q[i].expect,timekey);
      }
   }
else
   {
   Debug("No previous value for time index %s\n",timekey);
   }

CloseDB(dbp);
return &entry;
}

/*****************************************************************************/

void UpdateAverages(char *timekey,struct Averages newvals)

{ int err_no;
  CF_DB *dbp;

if (!OpenDB(AVDB,&dbp))
   {
   return;
   }

CfOut(cf_inform,"","Updated averages at %s\n",timekey);

//WriteDB(dbp,timekey,&value,sizeof(struct Averages));

WriteDB(dbp,timekey,&newvals,sizeof(struct Averages));
WriteDB(dbp,"DATABASE_AGE",&AGE,sizeof(double)); 

CloseDB(dbp);
HistoryUpdate(newvals);
}

/*****************************************************************************/

void UpdateDistributions(char *timekey,struct Averages *av)

{ int position,day,i,time_to_update = true;
  char filename[CF_BUFSIZE];
  FILE *fp;
 
/* Take an interval of 4 standard deviations from -2 to +2, divided into CF_GRAINS
   parts. Centre each measurement on CF_GRAINS/2 and scale each measurement by the
   std-deviation for the current time.
*/

if (HISTO && IsDefinedClass("Min40_45"))
   {
   day = Day2Number(timekey);
   
   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      position = CF_GRAINS/2 + (int)(0.5+(CF_THIS[i] - av->Q[i].expect)*CF_GRAINS/(4*sqrt((av->Q[i].var))));
      
      if (0 <= position && position < CF_GRAINS)
         {
         HISTOGRAM[i][day][position]++;
         }
      }
   
   
   snprintf(filename,CF_BUFSIZE,"%s/state/histograms",CFWORKDIR);
   
   if ((fp = fopen(filename,"w")) == NULL)
      {
      CfOut(cf_error,"fopen","Unable to save histograms");
      return;
      }
   
   for (position = 0; position < CF_GRAINS; position++)
      {
      fprintf(fp,"%d ",position);
      
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         for (day = 0; day < 7; day++)
            {
            fprintf(fp,"%.0lf ",HISTOGRAM[i][day][position]);
            }
         }
      fprintf(fp,"\n");
      }
   
   fclose(fp);
   }
}

/*****************************************************************************/

double WAverage(double anew,double aold,double age)

/* For a couple of weeks, learn eagerly. Otherwise variances will
   be way too large. Then downplay newer data somewhat, and rely on
   experience of a couple of months of data ... */

{ double av,cf_sane_monitor_limit = 9999999.0;
  double wnew,wold;

// First do some database corruption self-healing
  
if (aold > cf_sane_monitor_limit && anew > cf_sane_monitor_limit)
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

// Now look at the self-learning

if (FORGETRATE > 0.9 || FORGETRATE < 0.1)
   {
   FORGETRATE = 0.6;
   }

if (age < 2.0)  /* More aggressive learning for young database */
   {
   wnew = FORGETRATE;
   wold = (1.0-FORGETRATE);
   }
else
   {
   wnew = (1.0-FORGETRATE);
   wold = FORGETRATE;
   }

if (aold == 0 && anew == 0)
   {
   return 0;
   }

av = (wnew*anew + wold*aold)/(wnew+wold); 

if (av < 0)
   {
   // Accuracy lost - something wrong
   return 0.0;
   }

return av;
}

/*****************************************************************************/

double SetClasses(char * name,double variable,double av_expect,double av_var,double localav_expect,double localav_var,struct Item **classlist,char *timekey)

{ char buffer[CF_BUFSIZE],buffer2[CF_BUFSIZE];
 double dev,delta,sigma,ldelta,lsigma,sig;

 Debug("\n SetClasses(%s,X=%lf,avX=%lf,varX=%lf,lavX=%lf,lvarX=%lf,%s)\n",name,variable,av_expect,av_var,localav_expect,localav_var,timekey);

 delta = variable - av_expect;
 sigma = sqrt(av_var);
 ldelta = variable - localav_expect;
 lsigma = sqrt(localav_var);
 sig = sqrt(sigma*sigma+lsigma*lsigma); 

 Debug(" delta = %lf,sigma = %lf, lsigma = %lf, sig = %lf\n",delta,sigma,lsigma,sig);
 
 if (sigma == 0.0 || lsigma == 0.0)
    {
    Debug(" No sigma variation .. can't measure class\n");
    return sig;
    }

 Debug("Setting classes for %s...\n",name);
 
 if (fabs(delta) < cf_noise_threshold) /* Arbitrary limits on sensitivity  */
    {
    Debug(" Sensitivity too high ..\n");

    buffer[0] = '\0';
    strcpy(buffer,name);

    if ((delta > 0) && (ldelta > 0))
       {
       strcat(buffer,"_high");
       }
    else if ((delta < 0) && (ldelta < 0))
       {
       strcat(buffer,"_low");
       }
    else
       {
       strcat(buffer,"_normal");
       }
        
    dev = sqrt(delta*delta/(1.0+sigma*sigma)+ldelta*ldelta/(1.0+lsigma*lsigma));
        
    if (dev > 2.0*sqrt(2.0))
       {
       strcpy(buffer2,buffer);
       strcat(buffer2,"_microanomaly");
       AppendItem(classlist,buffer2,"2");
       NewPersistentContext(buffer2,CF_PERSISTENCE,cfpreserve); 
       }
   
    return sig; /* Granularity makes this silly */
    }
 else
    {
    buffer[0] = '\0';
    strcpy(buffer,name);  
    
    if ((delta > 0) && (ldelta > 0))
       {
       strcat(buffer,"_high");
       }
    else if ((delta < 0) && (ldelta < 0))
       {
       strcat(buffer,"_low");
       }
    else
       {
       strcat(buffer,"_normal");
       }
    
    dev = sqrt(delta*delta/(1.0+sigma*sigma)+ldelta*ldelta/(1.0+lsigma*lsigma));
    
    if (dev <= sqrt(2.0))
       {
       strcpy(buffer2,buffer);
       strcat(buffer2,"_normal");
       AppendItem(classlist,buffer2,"0");
       }
    else
       {
       strcpy(buffer2,buffer);
       strcat(buffer2,"_dev1");
       AppendItem(classlist,buffer2,"0");
       }
    
    /* Now use persistent classes so that serious anomalies last for about
       2 autocorrelation lengths, so that they can be cross correlated and
       seen by normally scheduled cfagent processes ... */
    
    if (dev > 2.0*sqrt(2.0))
       {
       strcpy(buffer2,buffer);
       strcat(buffer2,"_dev2");
       AppendItem(classlist,buffer2,"2");
       NewPersistentContext(buffer2,CF_PERSISTENCE,cfpreserve); 
       }
    
    if (dev > 3.0*sqrt(2.0))
       {
       strcpy(buffer2,buffer);
       strcat(buffer2,"_anomaly");
       AppendItem(classlist,buffer2,"3");
       NewPersistentContext(buffer2,CF_PERSISTENCE,cfpreserve); 
       }

    return sig; 
    }
}

/*****************************************************************************/

void SetVariable(char *name,double value,double average,double stddev,struct Item **classlist)


{ char var[CF_BUFSIZE];

 snprintf(var,CF_MAXVARSIZE,"value_%s=%.0lf",name,value);
 AppendItem(classlist,var,"");

 snprintf(var,CF_MAXVARSIZE,"av_%s=%.2lf",name,average);
 AppendItem(classlist,var,"");

 snprintf(var,CF_MAXVARSIZE,"dev_%s=%.2lf",name,stddev);
 AppendItem(classlist,var,""); 
}

/*****************************************************************************/

void RecordChangeOfState(struct Item *classlist,char *timekey)

{
}

/*****************************************************************************/

void ZeroArrivals()

{ int i;
 
 for (i = 0; i < CF_OBSERVABLES; i++)
    {
    CF_THIS[i] = 0;
    }
}

/*****************************************************************************/

double RejectAnomaly(double new,double average,double variance,double localav,double localvar)

{ double dev = sqrt(variance+localvar);          /* Geometrical average dev */
  double delta;
  int bigger;

if (average == 0)
   {
   return new;
   }

if (new > big_number*4.0)
   {
   return 0.0;
   }

if (new > big_number)
   {
   return average;
   }

if ((new-average)*(new-average) < cf_noise_threshold*cf_noise_threshold)
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

delta = sqrt((new-average)*(new-average)+(new-localav)*(new-localav));

if (delta > 4.0*dev)  /* IR */
   {
   srand48((unsigned int)time(NULL)); 
   
   if (drand48() < 0.7) /* 70% chance of using full value - as in learning policy */
      {
      return new;
      }
   else
      {
      if (bigger)
         {
         return average+2.0*dev;
         }
      else
         {
         return average-2.0*dev;
         }
      }
   }
else
   {
   CfOut(cf_verbose,"","Value accepted\n");
   return new;
   }
}

/***************************************************************/
/* Level 5                                                     */
/***************************************************************/

void SetEntropyClasses(char *service,struct Item *list,char *inout)

{ struct Item *ip, *addresses = NULL;
 char local[CF_BUFSIZE],remote[CF_BUFSIZE],class[CF_BUFSIZE],vbuff[CF_BUFSIZE], *sp;
 int i = 0, min_signal_diversity = 1,total=0;
 double *p = NULL, S = 0.0, percent = 0.0;


 if (IsSocketType(service))
    {
    for (ip = list; ip != NULL; ip=ip->next)
       {   
       if (strlen(ip->name) > 0)
          {
          if (strncmp(ip->name,"tcp",3) == 0)
             {
             sscanf(ip->name,"%*s %*s %*s %s %s",local,remote); /* linux-like */
             }
          else
             {
             sscanf(ip->name,"%s %s",local,remote);             /* solaris-like */
             }
         
          strncpy(vbuff,remote,CF_BUFSIZE-1);
         
          for (sp = vbuff+strlen(vbuff)-1; isdigit((int)*sp); sp--)
             {     
             }
         
          *sp = '\0';
         
          if (!IsItemIn(addresses,vbuff))
             {
             total++;
             AppendItem(&addresses,vbuff,"");
             IncrementItemListCounter(addresses,vbuff);
             }
          else
             {
             total++;    
             IncrementItemListCounter(addresses,vbuff);
             }      
          }
       }
   
   
    if (total > min_signal_diversity)
       {
       p = (double *) malloc((total+1)*sizeof(double));
      
       for (i = 0,ip = addresses; ip != NULL; i++,ip=ip->next)
          {
          p[i] = ((double)(ip->counter))/((double)total);
         
          S -= p[i]*log(p[i]);
          }
      
       percent = S/log((double)total)*100.0;
       free(p);       
       }
    }
 else
    {
    int classes = 0;

    total = 0;
    
    for (ip = list; ip != NULL; ip=ip->next)
       {
       total += (double)(ip->counter);
       }
 
    p = (double *)malloc(sizeof(double)*total); 

    for (ip = list; ip != NULL; ip=ip->next)
       {
       p[classes++] = ip->counter/total;
       }
    
    for (i = 0; i < classes; i++)
       {
       S -= p[i] * log(p[i]);
       }
    
    if (classes > 1)
       {
       percent = S/log((double)classes)*100.0;
       }
    else
       {    
       percent = 0;
       }

    free(p);
    }
 
 if (percent > 90)
    {
    snprintf(class,CF_MAXVARSIZE,"entropy_%s_%s_high",service,inout);
    AppendItem(&ENTROPIES,class,"");
    }
 else if (percent < 20)
    {
    snprintf(class,CF_MAXVARSIZE,"entropy_%s_%s_low",service,inout);
    AppendItem(&ENTROPIES,class,"");
    }
 else
    {
    snprintf(class,CF_MAXVARSIZE,"entropy_%s_%s_medium",service,inout);
    AppendItem(&ENTROPIES,class,"");
    }
 
 DeleteItemList(addresses);
}

/***************************************************************/

void SaveTCPEntropyData(struct Item *list,int i,char *inout)

{ struct Item *ip;
 FILE *fp;
 char filename[CF_BUFSIZE];

 CfOut(cf_verbose,"","TCP Save %s\n",TCPNAMES[i]);
  
 if (list == NULL)
    {
    CfOut(cf_verbose,"","No %s-%s events\n",TCPNAMES[i],inout);
    return;
    }

 if (strncmp(inout,"in",2) == 0)
    {
    snprintf(filename,CF_BUFSIZE-1,"%s/state/cf_incoming.%s",CFWORKDIR,TCPNAMES[i]); 
    }
 else
    {
    snprintf(filename,CF_BUFSIZE-1,"%s/state/cf_outgoing.%s",CFWORKDIR,TCPNAMES[i]); 
    }

 CfOut(cf_verbose,"","TCP Save %s\n",filename);
 
 if ((fp = fopen(filename,"w")) == NULL)
    {
    CfOut(cf_verbose,"","Unable to write datafile %s\n",filename);
    return;
    }
 
 for (ip = list; ip != NULL; ip=ip->next)
    {
    fprintf(fp,"%d %s\n",ip->counter,ip->name);
    }
 
 fclose(fp);
}


/***************************************************************/

void IncrementCounter(struct Item **list,char *name)

{
 if (!IsItemIn(*list,name))
    {
    AppendItem(list,name,"");
    IncrementItemListCounter(*list,name);
    }
 else
    {
    IncrementItemListCounter(*list,name);
    } 
}

/***************************************************************/

int GetFileGrowth(char *filename,enum observables index)

{ struct stat statbuf;
  size_t q;
  double dq;

if (cfstat(filename,&statbuf) == -1)
   {
   return 0;
   }

q = statbuf.st_size;

CfOut(cf_verbose,"","GetFileGrowth(%s) = %d\n",filename,q);

dq = (double)q - LASTQ[index];

if (LASTQ[index] == 0)
   {
   LASTQ[index] = q;
   return (int)(q/100+0.5);       /* arbitrary spike mitigation */
   }

LASTQ[index] = q;
return (int)(dq+0.5);
}

/******************************************************************************/
/* Motherboard sensors - how to standardize this somehow                      *
/* We're mainly interested in temperature and power consumption, but only the */
/* temperature is generally available. Several temperatures exist too ...     */
/******************************************************************************/

void GatherSensorData()

{ char buffer[CF_BUFSIZE];

 Debug("GatherSensorData()\n");
 
 switch(VSYSTEMHARDCLASS)
    {
    case linuxx:

        /* Search for acpi first, then lmsensors */
         
        if (ACPI && GetAcpi())
           {
           return;
           }

        if (LMSENSORS && GetLMSensors())
           {
           return;
           }

        break;

    case solaris:
        break;
    }
}

/******************************************************************************/

void GatherPromisedMeasures()

{ struct Bundle *bp;
  struct SubType *sp;
  struct Promise *pp;
  char *scope;
  
for (bp = BUNDLES; bp != NULL; bp = bp->next) /* get schedule */
   {
   scope = bp->name;
   SetNewScope(bp->name);
   
   if ((strcmp(bp->type,CF_AGENTTYPES[cf_monitor]) == 0) || (strcmp(bp->type,CF_AGENTTYPES[cf_common]) == 0))
      {
      for (sp = bp->subtypes; sp != NULL; sp = sp->next) /* get schedule */
         {
         for (pp = sp->promiselist; pp != NULL; pp=pp->next)
            {
            ExpandPromise(cf_monitor,scope,pp,KeepMonitorPromise);
            }
         }
      }
   }

DeleteAllScope();
}

/******************************************************************************/

int GetAcpi()

{ DIR *dirh;
 FILE *fp;
 struct dirent *dirp;
 int count = 0;
 char path[CF_BUFSIZE],buf[CF_BUFSIZE],index[4];
 double temp = 0;
 struct Attributes attr;

 memset(&attr,0,sizeof(attr));
 attr.transaction.audit = false;

 Debug("ACPI temperature\n");

 if ((dirh = opendir("/proc/acpi/thermal_zone")) == NULL)
    {
    CfOut(cf_verbose,"opendir","Can't open directory %s\n",path);
    return false;
    }

 for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
    {
    if (!ConsiderFile(dirp->d_name,path,attr,NULL))
       {
       continue;
       }
   
    snprintf(path,CF_BUFSIZE,"/proc/acpi/thermal_zone/%s/temperature",dirp->d_name);
   
    if ((fp = fopen(path,"r")) == NULL)
       {
       printf("Couldn't open %s\n",path);
       continue;
       }

    fgets(buf,CF_BUFSIZE-1,fp);

    sscanf(buf,"%*s %lf",&temp);

    for (count = 0; count < 4; count++)
       {
       snprintf(index,2,"%d",count);

       if (strstr(dirp->d_name,index))
          {
          switch (count)
             {
             case 0: CF_THIS[ob_temp0] = temp;
                 break;
             case 1: CF_THIS[ob_temp1] = temp;
                 break;
             case 2: CF_THIS[ob_temp2] = temp;
                 break;
             case 3: CF_THIS[ob_temp3] = temp;
                 break;
             }

          Debug("Set temp%d to %lf\n",count,temp);
          }
       }
   
    fclose(fp);
    }

 closedir(dirh);

 return true;
}

/******************************************************************************/

int GetLMSensors()

{ FILE *pp;
 struct Item *ip,*list = NULL;
 double temp = 0;
 char name[CF_BUFSIZE];
 int count;
 char vbuff[CF_BUFSIZE];
  
 CF_THIS[ob_temp0] = 0.0;
 CF_THIS[ob_temp1] = 0.0;
 CF_THIS[ob_temp2] = 0.0;
 CF_THIS[ob_temp3] = 0.0;
  
 if ((pp = cf_popen("/usr/bin/sensors","r")) == NULL)
    {
    LMSENSORS = false; /* Broken */
    return false;
    }

 CfReadLine(vbuff,CF_BUFSIZE,pp); 

 while (!feof(pp))
    {
    CfReadLine(vbuff,CF_BUFSIZE,pp);

    if (strstr(vbuff,"Temp")||strstr(vbuff,"temp"))
       {
       PrependItem(&list,vbuff,NULL);
       }
    }

 cf_pclose(pp);

 if (ListLen(list) > 0)
    {
    Debug("LM Sensors seemed to return ok data\n");
    }
 else
    {
    return false;
    }

/* lmsensor names are hopelessly inconsistent - so try a few things */

 for (ip = list; ip != NULL; ip=ip->next)
    {
    for (count = 0; count < 4; count++)
       {
       snprintf(name,16,"CPU%d Temp:",count);

       if (strncmp(ip->name,name,strlen(name)) == 0)
          {
          sscanf(ip->name,"%*[^:]: %lf",&temp);
         
          switch (count)
             {
             case 0: CF_THIS[ob_temp0] = temp;
                 break;
             case 1: CF_THIS[ob_temp1] = temp;
                 break;
             case 2: CF_THIS[ob_temp2] = temp;
                 break;
             case 3: CF_THIS[ob_temp3] = temp;
                 break;
             }

          Debug("Set temp%d to %lf from what looks like cpu temperature\n",count,temp);
          }
       }
    }

 if (CF_THIS[ob_temp0] != 0)
    {
    /* We got something plausible */
    return true;
    }

/* Alternative name Core x: */

 for (ip = list; ip != NULL; ip=ip->next)
    {
    for (count = 0; count < 4; count++)
       {
       snprintf(name,16,"Core %d:",count);

       if (strncmp(ip->name,name,strlen(name)) == 0)
          {
          sscanf(ip->name,"%*[^:]: %lf",&temp);
         
          switch (count)
             {
             case 0: CF_THIS[ob_temp0] = temp;
                 break;
             case 1: CF_THIS[ob_temp1] = temp;
                 break;
             case 2: CF_THIS[ob_temp2] = temp;
                 break;
             case 3: CF_THIS[ob_temp3] = temp;
                 break;
             }

          Debug("Set temp%d to %lf from what looks like core temperatures\n",count,temp);
          }
       }
    }

 if (CF_THIS[ob_temp0] != 0)
    {
    /* We got something plausible */
    return true;
    }

 for (ip = list; ip != NULL; ip=ip->next)
    {
    if (strncmp(ip->name,"CPU Temp:",strlen("CPU Temp:")) == 0  )
       {
       sscanf(ip->name,"%*[^:]: %lf",&temp);
       Debug("Setting temp0 to CPU Temp\n");
       CF_THIS[ob_temp0] = temp;
       }

    if (strncmp(ip->name,"M/B Temp:",strlen("M/B Temp:")) == 0  )
       {
       sscanf(ip->name,"%*[^:]: %lf",&temp);
       Debug("Setting temp0 to M/B Temp\n");
       CF_THIS[ob_temp1] = temp;
       }

    if (strncmp(ip->name,"Sys Temp:",strlen("Sys Temp:")) == 0  )
       {
       sscanf(ip->name,"%*[^:]: %lf",&temp);
       Debug("Setting temp0 to Sys Temp\n");
       CF_THIS[ob_temp2] = temp;
       }

    if (strncmp(ip->name,"AUX Temp:",strlen("AUX Temp:")) == 0  )
       {
       sscanf(ip->name,"%*[^:]: %lf",&temp);
       Debug("Setting temp0 to AUX Temp\n");
       CF_THIS[ob_temp3] = temp;
       }
    }

 if (CF_THIS[ob_temp0] != 0)
    {
    /* We got something plausible */
    return true;
    }


/* Alternative name Core x: */

 for (ip = list; ip != NULL; ip=ip->next)
    {
    for (count = 0; count < 4; count++)
       {
       snprintf(name,16,"temp%d:",count);

       if (strncmp(ip->name,name,strlen(name)) == 0)
          {
          sscanf(ip->name,"%*[^:]: %lf",&temp);
         
          switch (count)
             {
             case 0: CF_THIS[ob_temp0] = temp;
                 break;
             case 1: CF_THIS[ob_temp1] = temp;
                 break;
             case 2: CF_THIS[ob_temp2] = temp;
                 break;
             case 3: CF_THIS[ob_temp3] = temp;
                 break;
             }

          Debug("Set temp%d to %lf\n",count,temp);
          }
       }
    }

/* Give up? */

 DeleteItemList(list);

 return true;
}

/*********************************************************************/

int ByteSizeList(struct Item *list)

{ int count = 0;
 struct Item *ip;
 
 for (ip = list; ip != NULL; ip=ip->next)
    {
    count+=strlen(ip->name);
    }

 return count; 
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void KeepMonitorPromise(struct Promise *pp)

{ char *sp = NULL;
 
if (!IsDefinedClass(pp->classes))
   {
   CfOut(cf_verbose,"","\n");
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   CfOut(cf_verbose,"","Skipping whole next promise (%s), as context %s is not relevant\n",pp->promiser,pp->classes);
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   return;
   }

if (VarClassExcluded(pp,&sp))
   {
   CfOut(cf_verbose,"","\n");
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   CfOut(cf_verbose,"","Skipping whole next promise (%s), as var-context %s is not relevant\n",pp->promiser,sp);
   CfOut(cf_verbose,"",". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
   return;
   }

if (strcmp("classes",pp->agentsubtype) == 0)
   {
   KeepClassContextPromise(pp);
   return;
   }

if (strcmp("measurements",pp->agentsubtype) == 0)
   {
   VerifyMeasurementPromise(CF_THIS,pp);
   *pp->donep = false;
   return;
   }
}

/*******************************************************************/
/* Unix implementations                                            */
/*******************************************************************/

#ifndef MINGW

int Unix_GatherProcessUsers(struct Item **userList, int *userListSz, int *numRootProcs, int *numOtherProcs)
    
{ FILE *pp;
  char pscomm[CF_BUFSIZE];
  char user[CF_MAXVARSIZE];
  char vbuff[CF_BUFSIZE];
 
snprintf(pscomm,CF_BUFSIZE,"%s %s",VPSCOMM[VSYSTEMHARDCLASS],VPSOPTS[VSYSTEMHARDCLASS]);

if ((pp = cf_popen(pscomm,"r")) == NULL)
   {
   return false;
   }

CfReadLine(vbuff,CF_BUFSIZE,pp); 

while (!feof(pp))
   {
   CfReadLine(vbuff,CF_BUFSIZE,pp);
   sscanf(vbuff,"%s",user);

   if (strcmp(user,"USER") == 0)
      {
      continue;
      }
   
   if (!IsItemIn(*userList,user))
      {
      PrependItem(userList,user,NULL);
      (*userListSz)++;
      }
   
   if (strcmp(user,"root") == 0)
      {
      (*numRootProcs)++;
      }
   else
      {
      (*numOtherProcs)++;
      }
   }

cf_pclose(pp);
return true;
}

/*****************************************************************************/

void Unix_GatherCPUData()

{ double q,dq;
  char name[CF_MAXVARSIZE],cpuname[CF_MAXVARSIZE],buf[CF_BUFSIZE];
  long count,userticks=0,niceticks=0,systemticks=0,idle=0,iowait=0,irq=0,softirq=0;
  long total_time = 1;
  FILE *fp;
  enum observables index = ob_spare;
  
if ((fp=fopen("/proc/stat","r")) == NULL)
   {
   CfOut(cf_verbose,"","Didn't find proc data\n");
   return;
   }

CfOut(cf_verbose,"","Reading /proc/stat utilization data -------\n");

count = 0;

while (!feof(fp))
   {
   fgets(buf,CF_BUFSIZE,fp);
   
   sscanf(buf,"%s%ld%ld%ld%ld%ld%ld%ld",&cpuname,&userticks,&niceticks,&systemticks,&idle,&iowait,&irq,&softirq);
   snprintf(name,16,"cpu%d",count);
   
   total_time = (userticks+niceticks+systemticks+idle); 
   
   q = 100.0 * (double)(total_time - idle);
   
   if (strncmp(cpuname,name,strlen(name)) == 0)
      {
      CfOut(cf_verbose,"","Found CPU %d\n",count);
      
      switch (count++)
         {
         case 0: index = ob_cpu0;
             break;
         case 1: index = ob_cpu1;
             break;
         case 2: index = ob_cpu2;
             break;
         case 3: index = ob_cpu3;
             break;
         default:
             index = ob_spare;
             CfOut(cf_verbose,"","Error reading proc/stat\n");
             continue;
         }
      }
   else if (strncmp(cpuname,"cpu",3) == 0)
      {
      CfOut(cf_verbose,"","Found aggregate CPU\n",count);
      index = ob_cpuall;
      }
   else 
      {
      CfOut(cf_verbose,"","Found nothing (%s)\n",cpuname);
      index = ob_spare;
      fclose(fp);
      return;
      }
   
   dq = (q - LASTQ[index])/(double)(total_time-LASTT[index]); /* % Utilization */
   
   if (dq > 100 || dq < 0) // Counter wrap around
      {
      dq = 50;
      }
   
   CF_THIS[index] = dq;
   LASTQ[index] = q;
   
   CfOut(cf_verbose,"","Set %s=%d to %.1lf after %d 100ths of a second \n",OBS[index][1],index,CF_THIS[index],total_time);         
   }

LASTT[index] = total_time;
fclose(fp);
}


#endif  /* NOT MINGW */
