/* 

        Copyright (C) 2001-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: env_monitor.c                                                       */
/*                                                                           */
/*****************************************************************************/

/* This file is hard-linked in cf2 and cf3 - for consistent maintenance */

#include "cf.defs.h"
#include "cf.extern.h"
#ifdef HAVE_SYS_LOADAVG_H
# include <sys/loadavg.h>
#else
# define LOADAVG_5MIN    1
#endif

#include <math.h>

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

unsigned int HISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS];

int HISTO = false;

/* persistent observations */

double THIS[CF_OBSERVABLES]; /* New from 2.1.21 replacing above - current observation */

/* Work */

int SLEEPTIME = 2.5 * 60;    /* Should be a fraction of 5 minutes */
int BATCH_MODE = false;

double ITER = 0.0;           /* Iteration since start */
double AGE,WAGE;             /* Age and weekly age of database */

char OUTPUT[CF_BUFSIZE*2];
char BATCHFILE[CF_BUFSIZE];
char STATELOG[CF_BUFSIZE];
char ENV_NEW[CF_BUFSIZE];
char ENV[CF_BUFSIZE];

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

short NO_FORK = false;
double FORGETRATE = 0.7;

int LASTQ[CF_OBSERVABLES];

/*******************************************************************/
/* Prototypes                                                      */
/*******************************************************************/

void MonInitialize(void);
void StartServer (int argc, char **argv);
void *ExitCleanly (void);
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
void GatherDiskData (void);
void GatherLoadData (void);
void GatherSocketData (void);
void GatherSNMPData(void);
void GatherSensorData(void);
    
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
void ConvertDatabase(void);
int GetFileGrowth(char *filename,enum observables index);

int GetAcpi(void);
int GetLMSensors(void);

/****************************************************************/

void MonInitialize()
   
{ int i,j,k;
  struct stat statbuf;
 
/* XXX Initialize workdir for non privileged users */

strcpy(CFWORKDIR,WORKDIR);

if (getuid() > 0)
   {
   char *homedir;
   if ((homedir = getenv("HOME")) != NULL)
      {
      strcpy(CFWORKDIR,homedir);
      strcat(CFWORKDIR,"/.cfagent");
      }
   }
 
sprintf(VBUFF,"%s/test",CFWORKDIR);
MakeDirectoriesFor(VBUFF,'y');
sprintf(VBUFF,"%s/state/test",CFWORKDIR);
MakeDirectoriesFor(VBUFF,'y');
strncpy(VLOCKDIR,CFWORKDIR,CF_BUFSIZE-1);
strncpy(VLOGDIR,CFWORKDIR,CF_BUFSIZE-1);

for (i = 0; i < ATTR; i++)
   {
   sprintf(VBUFF,"%s/state/cf_incoming.%s",CFWORKDIR,ECGSOCKS[i].name);
   CreateEmptyFile(VBUFF);
   sprintf(VBUFF,"%s/state/cf_outgoing.%s",CFWORKDIR,ECGSOCKS[i].name);
   CreateEmptyFile(VBUFF);
   }

for (i = 0; i < CF_NETATTR; i++)
   {
   NETIN_DIST[i] = NULL;
   NETOUT_DIST[i] = NULL;
   }
 
sprintf(VBUFF,"%s/state/cf_users",CFWORKDIR);
CreateEmptyFile(VBUFF);
 
snprintf(AVDB,CF_MAXVARSIZE,"%s/state/%s",CFWORKDIR,CF_AVDB_FILE);
snprintf(STATELOG,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATELOG_FILE);
snprintf(ENV_NEW,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENVNEW_FILE);
snprintf(ENV,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENV_FILE);

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
   
   for (j = 0; j < LDT_BUFSIZE; j++)
      {
      LDT_BUF[i][j];
      }
   }

srand((unsigned int)time(NULL)); 
LoadHistogram();
ConvertDatabase();

/* Look for local sensors */

if (stat("/proc/acpi/thermal_zone",&statbuf) != -1)
   {
   Debug("Found an acpi service\n");
   ACPI = true;
   }

if (stat("/usr/bin/sensors",&statbuf) != -1)
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
  DBT key,value;
  DB *dbp;

if ((err_no = db_create(&dbp,NULL,0)) != 0)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Couldn't initialize average database %s\n",AVDB);
   CfLog(cferror,OUTPUT,"db_open");
   return;
   }

#ifdef CF_OLD_DB
if ((err_no = (dbp->open)(dbp,AVDB,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((err_no = (dbp->open)(dbp,NULL,AVDB,NULL,DB_BTREE,DB_CREATE,0644)) != 0)    
#endif
   {
   AGE = WAGE = 0;
   snprintf(OUTPUT,CF_BUFSIZE,"Couldn't open average database %s\n",AVDB);
   CfLog(cferror,OUTPUT,"db_open");
   return;
   }

chmod(AVDB,0644); 

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = "DATABASE_AGE";
key.size = strlen("DATABASE_AGE")+1;

if ((err_no = dbp->get(dbp,NULL,&key,&value,0)) != 0)
   {
   if (err_no != DB_NOTFOUND)
      {
      dbp->err(dbp,err_no,NULL);
      dbp->close(dbp,0);
      return;
      }
   }
 
if (value.data != NULL)
   {
   AGE = *(double *)(value.data);
   WAGE = AGE / CF_WEEK * CF_MEASURE_INTERVAL;
   Debug("\n\nPrevious DATABASE_AGE %f\n\n",AGE);
   }
else
   {
   Debug("No previous AGE\n");
   AGE = 0.0;
   }

dbp->close(dbp,0);
}

/*********************************************************************/

void LoadHistogram()

{ FILE *fp;
  int position,i,day; 

if (HISTO)
   {
   char filename[CF_BUFSIZE];
   
   snprintf(filename,CF_BUFSIZE,"%s/state/histograms",CFWORKDIR);
   
   if ((fp = fopen(filename,"r")) == NULL)
      {
      CfLog(cfverbose,"Unable to load histogram data","fopen");
      return;
      }

   for (position = 0; position < CF_GRAINS; position++)
      {
      fscanf(fp,"%d ",&position);
      
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         for (day = 0; day < 7; day++)
            {
            fscanf(fp,"%d ",&(HISTOGRAM[i][day][position]));
            }
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

if ((!NO_FORK) && (fork() != 0))
   {
   sprintf(OUTPUT,"cfenvd: starting\n");
   CfLog(cfinform,OUTPUT,"");
   exit(0);
   }

if (!NO_FORK)
   {
   ActAsDaemon(0);
   }

WritePID("cfenvd.pid");

VCANONICALFILE = strdup("db");
 
if (!GetLock("cfenvd","daemon",0,1,"localhost",(time_t)time(NULL)))
   {
   return;
   }

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
Verbose("Time out\n");
}

/*********************************************************************/

void ConvertDatabase()

/* 2.1.11 introduces a new db format - this converts */
    
{ struct stat statbuf;
 char filename[CF_BUFSIZE],new[CF_BUFSIZE],timekey[CF_BUFSIZE];
 int i,errno,now;
  DBT key,value;
  DB *dbp;
  static struct OldAverages oldentry;
  static struct Averages newentry;
    
snprintf(filename,CF_BUFSIZE-1,"%s/state/%s",CFWORKDIR,CF_OLDAVDB_FILE);
snprintf(new,CF_BUFSIZE-1,"%s/state/%s",CFWORKDIR,CF_AVDB_FILE);

if (stat(new,&statbuf) != -1)
   {
   return;
   }

if (stat(filename,&statbuf) == -1)
   {
   return;
   }

printf("Found old database %s\n",filename);
  
if ((errno = db_create(&dbp,NULL,0)) != 0)
   {
   printf("Couldn't create average database %s\n",filename);
   exit(1);
   }

#ifdef CF_OLD_DB 
if ((errno = (dbp->open)(dbp,filename,NULL,DB_BTREE,DB_RDONLY,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,filename,NULL,DB_BTREE,DB_RDONLY,0644)) != 0)    
#endif
   {
   printf("Couldn't open average database %s\n",filename);
   dbp->err(dbp,errno,NULL);
   exit(1);
   }

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));

for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
   {
   memset(&key,0,sizeof(key));       
   memset(&value,0,sizeof(value));
   memset(&oldentry,0,sizeof(oldentry));
   memset(&newentry,0,sizeof(newentry));

   strcpy(timekey,GenTimeKey(now));

   key.data = timekey;
   key.size = strlen(timekey)+1;
   
   if ((errno = dbp->get(dbp,NULL,&key,&value,0)) != 0)
      {
      if (errno != DB_NOTFOUND)
         {
         dbp->err(dbp,errno,NULL);
         exit(1);
         }
      }
   
   if (value.data != NULL)
      {
      memcpy(&oldentry,value.data,sizeof(oldentry));

      newentry.Q[ob_users].expect = oldentry.expect_number_of_users;
      newentry.Q[ob_rootprocs].expect = oldentry.expect_rootprocs;
      newentry.Q[ob_otherprocs].expect = oldentry.expect_otherprocs;
      newentry.Q[ob_diskfree].expect = oldentry.expect_diskfree;
      newentry.Q[ob_loadavg].expect = oldentry.expect_loadavg;
      newentry.Q[ob_rootprocs].expect = oldentry.expect_rootprocs;
      newentry.Q[ob_netbiosns_in].expect = oldentry.expect_incoming[0];
      newentry.Q[ob_netbiosns_out].expect = oldentry.expect_outgoing[0];
      newentry.Q[ob_netbiosdgm_in].expect = oldentry.expect_incoming[1];
      newentry.Q[ob_netbiosdgm_out].expect = oldentry.expect_outgoing[1];
      newentry.Q[ob_netbiosssn_in].expect = oldentry.expect_incoming[2];
      newentry.Q[ob_netbiosssn_out].expect = oldentry.expect_outgoing[2];
      newentry.Q[ob_irc_in].expect = oldentry.expect_incoming[3];
      newentry.Q[ob_irc_out].expect = oldentry.expect_outgoing[3];
      newentry.Q[ob_cfengine_in].expect = oldentry.expect_incoming[4];
      newentry.Q[ob_cfengine_out].expect = oldentry.expect_outgoing[4];
      newentry.Q[ob_nfsd_in].expect = oldentry.expect_incoming[5];
      newentry.Q[ob_nfsd_out].expect = oldentry.expect_outgoing[5];
      newentry.Q[ob_smtp_in].expect = oldentry.expect_incoming[6];
      newentry.Q[ob_smtp_out].expect = oldentry.expect_outgoing[6];
      newentry.Q[ob_www_in].expect = oldentry.expect_incoming[7];
      newentry.Q[ob_www_out].expect = oldentry.expect_outgoing[7];
      newentry.Q[ob_ftp_in].expect = oldentry.expect_incoming[8];
      newentry.Q[ob_ftp_out].expect = oldentry.expect_outgoing[8];
      newentry.Q[ob_ssh_in].expect = oldentry.expect_incoming[9];
      newentry.Q[ob_ssh_out].expect = oldentry.expect_outgoing[9];
      newentry.Q[ob_wwws_in].expect = oldentry.expect_incoming[10];
      newentry.Q[ob_wwws_out].expect = oldentry.expect_outgoing[10];
      newentry.Q[ob_icmp_in].expect = oldentry.expect_incoming[11];
      newentry.Q[ob_icmp_out].expect = oldentry.expect_outgoing[11];
      newentry.Q[ob_udp_in].expect = oldentry.expect_incoming[12];
      newentry.Q[ob_udp_out].expect = oldentry.expect_outgoing[12];
      newentry.Q[ob_dns_in].expect = oldentry.expect_incoming[13];
      newentry.Q[ob_dns_out].expect = oldentry.expect_outgoing[13];
      newentry.Q[ob_tcpsyn_in].expect = oldentry.expect_incoming[14];
      newentry.Q[ob_tcpsyn_out].expect = oldentry.expect_outgoing[14];
      newentry.Q[ob_tcpack_in].expect = oldentry.expect_incoming[15];
      newentry.Q[ob_tcpack_out].expect = oldentry.expect_outgoing[15];
      newentry.Q[ob_tcpfin_in].expect = oldentry.expect_incoming[16];
      newentry.Q[ob_tcpfin_out].expect = oldentry.expect_outgoing[16];
      newentry.Q[ob_tcpmisc_in].expect = oldentry.expect_incoming[17];
      newentry.Q[ob_tcpmisc_out].expect = oldentry.expect_outgoing[17];

      newentry.Q[ob_users].var = oldentry.var_number_of_users;
      newentry.Q[ob_rootprocs].var = oldentry.var_rootprocs;
      newentry.Q[ob_otherprocs].var = oldentry.var_otherprocs;
      newentry.Q[ob_diskfree].var = oldentry.var_diskfree;
      newentry.Q[ob_loadavg].var = oldentry.var_loadavg;
      newentry.Q[ob_rootprocs].var = oldentry.var_rootprocs;
      newentry.Q[ob_netbiosns_in].var = oldentry.var_incoming[0];
      newentry.Q[ob_netbiosns_out].var = oldentry.var_outgoing[0];
      newentry.Q[ob_netbiosdgm_in].var = oldentry.var_incoming[1];
      newentry.Q[ob_netbiosdgm_out].var = oldentry.var_outgoing[1];
      newentry.Q[ob_netbiosssn_in].var = oldentry.var_incoming[2];
      newentry.Q[ob_netbiosssn_out].var = oldentry.var_outgoing[2];
      newentry.Q[ob_irc_in].var = oldentry.var_incoming[3];
      newentry.Q[ob_irc_out].var = oldentry.var_outgoing[3];
      newentry.Q[ob_cfengine_in].var = oldentry.var_incoming[4];
      newentry.Q[ob_cfengine_out].var = oldentry.var_outgoing[4];
      newentry.Q[ob_nfsd_in].var = oldentry.var_incoming[5];
      newentry.Q[ob_nfsd_out].var = oldentry.var_outgoing[5];
      newentry.Q[ob_smtp_in].var = oldentry.var_incoming[6];
      newentry.Q[ob_smtp_out].var = oldentry.var_outgoing[6];
      newentry.Q[ob_www_in].var = oldentry.var_incoming[7];
      newentry.Q[ob_www_out].var = oldentry.var_outgoing[7];
      newentry.Q[ob_ftp_in].var = oldentry.var_incoming[8];
      newentry.Q[ob_ftp_out].var = oldentry.var_outgoing[8];
      newentry.Q[ob_ssh_in].var = oldentry.var_incoming[9];
      newentry.Q[ob_ssh_out].var = oldentry.var_outgoing[9];
      newentry.Q[ob_wwws_in].var = oldentry.var_incoming[10];
      newentry.Q[ob_wwws_out].var = oldentry.var_outgoing[10];
      newentry.Q[ob_icmp_in].var = oldentry.var_incoming[11];
      newentry.Q[ob_icmp_out].var = oldentry.var_outgoing[11];
      newentry.Q[ob_udp_in].var = oldentry.var_incoming[12];
      newentry.Q[ob_udp_out].var = oldentry.var_outgoing[12];
      newentry.Q[ob_dns_in].var = oldentry.var_incoming[13];
      newentry.Q[ob_dns_out].var = oldentry.var_outgoing[13];
      newentry.Q[ob_tcpsyn_in].var = oldentry.var_incoming[14];
      newentry.Q[ob_tcpsyn_out].var = oldentry.var_outgoing[14];
      newentry.Q[ob_tcpack_in].var = oldentry.var_incoming[15];
      newentry.Q[ob_tcpack_out].var = oldentry.var_outgoing[15];
      newentry.Q[ob_tcpfin_in].var = oldentry.var_incoming[16];
      newentry.Q[ob_tcpfin_out].var = oldentry.var_outgoing[16];
      newentry.Q[ob_tcpmisc_in].var = oldentry.var_incoming[17];
      newentry.Q[ob_tcpmisc_out].var = oldentry.var_outgoing[17];

      UpdateAverages(timekey,newentry);
      }
   }
 
dbp->close(dbp,0);
unlink(filename);
printf("Converted and removed old database\n");
}


/*********************************************************************/

void OpenSniffer()

{ char tcpbuffer[CF_BUFSIZE];

if (TCPDUMP)
   {
   struct stat statbuf;
   char buffer[CF_MAXVARSIZE];
   sscanf(CF_TCPDUMP_COMM,"%s",buffer);
   
   if (stat(buffer,&statbuf) != -1)
      {
      if ((TCPPIPE = cfpopen(CF_TCPDUMP_COMM,"r")) == NULL)
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
 
Verbose("Reading from tcpdump...\n");
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

GatherProcessData();
GatherCPUData();
GatherLoadData(); 
GatherDiskData();
GatherSocketData();
GatherSNMPData();
GatherSensorData();
}

/*********************************************************************/

char *GetTimeKey()

{ time_t now;
  char str[CF_SMALLBUF];
  
if ((now = time((time_t *)NULL)) == -1)
   {
   exit(1);
   }

sprintf(str,"%s",ctime(&now));

return ConvTimeKey(str); 
}


/*********************************************************************/

struct Averages EvalAvQ(char *t)

{ struct Averages *currentvals,newvals;
  int i; 
  double This[CF_OBSERVABLES],delta2;
  
if ((currentvals = GetCurrentAverages(t)) == NULL)
   {
   CfLog(cferror,"Error reading average database","");
   exit(1);
   }

/* Discard any apparently anomalous behaviour before renormalizing database */

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   double delta2;

   newvals.Q[i].q = THIS[i];
   LOCALAV.Q[i].q = THIS[i];

   /* Periodic */
       
   This[i] = RejectAnomaly(THIS[i],currentvals->Q[i].expect,currentvals->Q[i].var,LOCALAV.Q[i].expect,LOCALAV.Q[i].var);

   Debug("Current %s.q %lf\n",OBS[i][0],currentvals->Q[i].q);
   Debug("Current %s.var %lf\n",OBS[i][0],currentvals->Q[i].var);
   Debug("Current %s.ex %lf\n",OBS[i][0],currentvals->Q[i].expect);
   Debug("THIS[%s] = %lf\n",OBS[i][0],THIS[i]);
   Debug("This[%s] = %lf\n",OBS[i][0],This[i]);

   newvals.Q[i].expect = WAverage(This[i],currentvals->Q[i].expect,WAGE);
   LOCALAV.Q[i].expect = WAverage(newvals.Q[i].expect,LOCALAV.Q[i].expect,ITER);
   
   delta2 = (This[i] - currentvals->Q[i].expect)*(This[i] - currentvals->Q[i].expect);

   newvals.Q[i].var = WAverage(delta2,currentvals->Q[i].var,WAGE);
   LOCALAV.Q[i].var = WAverage(newvals.Q[i].var,LOCALAV.Q[i].var,ITER);

   Verbose("New %s.q %lf\n",OBS[i][0],newvals.Q[i].q);
   Verbose("New %s.var %lf\n",OBS[i][0],newvals.Q[i].var);
   Verbose("New %s.ex %lf\n",OBS[i][0],newvals.Q[i].expect);

   Verbose("%s = %lf -> (%f#%f) local [%f#%f]\n",OBS[i][0],This[i],newvals.Q[i].expect,sqrt(newvals.Q[i].var),LOCALAV.Q[i].expect,sqrt(LOCALAV.Q[i].var));

   if (This[i] > 0)
      {
      Verbose("Storing %.2f in %s\n",This[i],OBS[i][0]);
      }
   }
   
UpdateAverages(t,newvals);
 
if (WAGE > CFGRACEPERIOD)
   {
   UpdateDistributions(t,currentvals);  /* Distribution about mean */
   }
 
return newvals;
}

/*********************************************************************/

void LeapDetection()

{ int i,j;
  double n1,n2,d;
  double padding = 0.2;

if (++LDT_POS >= LDT_BUFSIZE)
   {
   LDT_POS = 0;
   
   if (!LDT_FULL)
      {
      snprintf(OUTPUT,CF_BUFSIZE,"LDT Buffer full at %d\n",LDT_BUFSIZE);
      CfLog(cfloginform,OUTPUT,"");
      LDT_FULL = true;
      }
   }

  
for (i = 0; i < CF_OBSERVABLES; i++)
   {
   /* Note AVG should contain n+1 but not SUM, hence funny increments */
   
   LDT_AVG[i] = LDT_AVG[i] + THIS[i]/((double)LDT_BUFSIZE + 1.0);

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

   if (THIS[i] > LDT_MAX[i])
      {
      LDT_MAX[i] = THIS[i];
      }
   
   n1 = (LDT_SUM[i] - (double)LDT_BUFSIZE * THIS[i]);

   if (d < 0.001)
      {
      CHI[i] = 0.0;
      }
   else
      {
      CHI[i] = sqrt(n1*n1/d);
      }

   LDT_AVG[i] = LDT_AVG[i] - LDT_BUF[i][LDT_POS]/((double)LDT_BUFSIZE + 1.0);
   LDT_BUF[i][LDT_POS] = THIS[i];
   LDT_SUM[i] = LDT_SUM[i] - LDT_BUF[i][LDT_POS] + THIS[i];
   }
}

/*********************************************************************/

void ArmClasses(struct Averages av,char *timekey)

{ double sigma;
  struct Item *classlist = NULL, *ip;
  int i,j,k,pos;
  FILE *fp;
  char buff[CF_BUFSIZE],ldt_buff[CF_BUFSIZE];
  static int anomaly[CF_OBSERVABLES][LDT_BUFSIZE];
  static double anomaly_chi[CF_OBSERVABLES];
  static double anomaly_chi_limit[CF_OBSERVABLES];

Debug("Arm classes for %s\n",timekey);
 
for (i = 0; i < CF_OBSERVABLES; i++)
   {
   sigma = SetClasses(OBS[i][0],THIS[i],av.Q[i].expect,av.Q[i].var,LOCALAV.Q[i].expect,LOCALAV.Q[i].var,&classlist,timekey);
   SetVariable(OBS[i][0],THIS[i],av.Q[i].expect,sigma,&classlist);

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
      
      snprintf(OUTPUT,CF_BUFSIZE,"LDT(%d) in %s chi = %.2f thresh %.2f \n",LDT_POS,OBS[i][0],CHI[i],CHI_LIMIT[i]);

      if (VERBOSE)
         {
         CfLog(cfloginform,OUTPUT,"");
         }
      Verbose(OUTPUT);
      
      snprintf(OUTPUT,CF_BUFSIZE,"LDT_BUF (%s): Rot ",OBS[i][0]);

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

         strcat(OUTPUT,buff);
         strcat(ldt_buff,buff);
         }

      strcat(OUTPUT,"\n");

      if (VERBOSE)
         {
         CfLog(cfloginform,OUTPUT,"");
         }
      Verbose(OUTPUT);

      if (THIS[i] > av.Q[i].expect)
         {
         snprintf(OUTPUT,CF_BUFSIZE,"%s_high_ldt",OBS[i][0]);
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE,"%s_high_ldt",OBS[i][0]);
         }

      AppendItem(&classlist,OUTPUT,"2");
      AddPersistentClass(OUTPUT,CF_PERSISTENCE,cfpreserve); 
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

   snprintf(buff,CF_MAXVARSIZE,"ldtbuf_%s=%s",OBS[i][0],ldt_buff);
   AppendItem(&classlist,buff,"");

   snprintf(buff,CF_MAXVARSIZE,"ldtchi_%s=%.2f",OBS[i][0],anomaly_chi[i]);
   AppendItem(&classlist,buff,"");
   
   snprintf(buff,CF_MAXVARSIZE,"ldtlimit_%s=%.2f",OBS[i][0],anomaly_chi_limit[i]);
   AppendItem(&classlist,buff,"");
   }

/* Publish class list */

unlink(ENV_NEW);
 
if ((fp = fopen(ENV_NEW,"a")) == NULL)
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

rename(ENV_NEW,ENV);
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
       case 'S': Debug("%1.1f: TCP new connection from %s to %s - i am %s\n",ITER,src,dest,VIPADDRESS);
           if (isme_dest)
              {
              THIS[ob_tcpsyn_in]++;
              IncrementCounter(&(NETIN_DIST[tcpsyn]),src);
              }
           else if (isme_src)
              {
              THIS[ob_tcpsyn_out]++;
              IncrementCounter(&(NETOUT_DIST[tcpsyn]),dest);
              }       
           break;
           
       case 'F': Debug("%1.1f: TCP end connection from %s to %s\n",ITER,src,dest);
           if (isme_dest)
              {
              THIS[ob_tcpfin_in]++;
              IncrementCounter(&(NETIN_DIST[tcpfin]),src);
              }
           else if (isme_src)
              {
              THIS[ob_tcpfin_out]++;
              IncrementCounter(&(NETOUT_DIST[tcpfin]),dest);
              }       
           break;
           
       default: Debug("%1.1f: TCP established from %s to %s\n",ITER,src,dest);
           
           if (isme_dest)
              {
              THIS[ob_tcpack_in]++;
              IncrementCounter(&(NETIN_DIST[tcpack]),src);
              }
           else if (isme_src)
              {
              THIS[ob_tcpack_out]++;
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
   
   Debug("%1.1f: DNS packet from %s to %s\n",ITER,src,dest);
   if (isme_dest)
      {
      THIS[ob_dns_in]++;
      IncrementCounter(&(NETIN_DIST[dns]),src);
      }
   else if (isme_src)
      {
      THIS[ob_dns_out]++;
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
   
   Debug("%1.1f: UDP packet from %s to %s\n",ITER,src,dest);
   if (isme_dest)
      {
      THIS[ob_udp_in]++;
      IncrementCounter(&(NETIN_DIST[udp]),src);
      }
   else if (isme_src)
      {
      THIS[ob_udp_out]++;
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
   
   Debug("%1.1f: ICMP packet from %s to %s\n",ITER,src,dest);
   
   if (isme_dest)
      {
      THIS[ob_icmp_in]++;
      IncrementCounter(&(NETIN_DIST[icmp]),src);
      }
   else if (isme_src)
      {
      THIS[ob_icmp_out]++;
      IncrementCounter(&(NETOUT_DIST[icmp]),src);
      }       
   }
else
   {
   Debug("%1.1f: Miscellaneous undirected packet (%.100s)\n",ITER,arrival);
   
   THIS[ob_tcpmisc_in]++;
   
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

{ FILE *pp;
  char pscomm[CF_BUFSIZE];
  char user[CF_MAXVARSIZE];
  struct Item *list = NULL;
  
snprintf(pscomm,CF_BUFSIZE,"%s %s",VPSCOMM[VSYSTEMHARDCLASS],VPSOPTS[VSYSTEMHARDCLASS]);

if ((pp = cfpopen(pscomm,"r")) == NULL)
   {
   return;
   }

ReadLine(VBUFF,CF_BUFSIZE,pp); 

while (!feof(pp))
   {
   ReadLine(VBUFF,CF_BUFSIZE,pp);
   sscanf(VBUFF,"%s",user);
   if (!IsItemIn(list,user))
      {
      PrependItem(&list,user,NULL);
      THIS[ob_users]++;
      }

   if (strcmp(user,"root") == 0)
      {
      THIS[ob_rootprocs]++;
      }
   else
      {
      THIS[ob_otherprocs]++;
      }
   }

cfpclose(pp);

snprintf(VBUFF,CF_MAXVARSIZE,"%s/state/cf_users",CFWORKDIR);
SaveItemList(list,VBUFF,"none");

DeleteItemList(list);
Verbose("(Users,root,other) = (%d,%d,%d)\n",THIS[ob_users],THIS[ob_rootprocs],THIS[ob_otherprocs]);
}

/*****************************************************************************/

void GatherCPUData()

{ double q,dq;
  char name[CF_MAXVARSIZE],cpuname[CF_MAXVARSIZE],buf[CF_BUFSIZE];
  int count,userticks=0,niceticks=0,systemticks=0,idle=0,iowait=0,irq=0,softirq=0;
  int total_time = 1;
  FILE *fp;
  enum observables index = ob_spare;
  
if ((fp=fopen("/proc/stat","r")) == NULL)
   {
   Verbose("Didn't find proc data\n");
   return;
   }

Verbose("Reading /proc/stat utilization data -------\n");

count = 0;

while (!feof(fp))
   {
   fgets(buf,CF_BUFSIZE,fp);

   sscanf(buf,"%s%d%d%d%d%d%d%d",&cpuname,&userticks,&niceticks,&systemticks,&idle,&iowait,&irq,&softirq);
   snprintf(name,16,"cpu%d",count);
   
   total_time = (userticks+niceticks+systemticks+idle); 

   q = 100.0 * (double)(total_time - idle);

   if (strncmp(cpuname,name,strlen(name)) == 0)
      {
      Verbose("Found CPU %d\n",count);

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
             Verbose("Error reading proc/stat\n");
             continue;
         }
      }
   else if (strncmp(cpuname,"cpu",3) == 0)
      {
      Verbose("Found aggregate CPU\n",count);
      index = ob_cpuall;
      }
   else 
      {
      Verbose("Found nothing (%s)\n",cpuname);
      index = ob_spare;
      fclose(fp);
      return;
      }

   dq = (q - LASTQ[index])/(double)total_time; /* % Utilization */
   
   THIS[index] = dq;
   LASTQ[index] = q;

   Verbose("Set %s=%d to %.1f after %d 100ths of a second \n",OBS[index][1],index,q,total_time);         
   }

fclose(fp);
}

/*****************************************************************************/

void GatherDiskData()

{ char accesslog[CF_BUFSIZE];
  char errorlog[CF_BUFSIZE];
  char syslog[CF_BUFSIZE];
  char messages[CF_BUFSIZE];
 
Verbose("Gathering disk data\n");
THIS[ob_diskfree] = GetDiskUsage("/",cfpercent);
Verbose("Disk free = %d %%\n",THIS[ob_diskfree]);

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

THIS[ob_webaccess] = GetFileGrowth(accesslog,ob_webaccess);
Verbose("Webaccess = %d %%\n",THIS[ob_webaccess]);
THIS[ob_weberrors] = GetFileGrowth(errorlog,ob_weberrors);
Verbose("Web error = %d %%\n",THIS[ob_weberrors]);
THIS[ob_syslog] = GetFileGrowth(syslog,ob_syslog);
Verbose("Syslog = %d %%\n",THIS[ob_syslog]);
THIS[ob_messages] = GetFileGrowth(messages,ob_messages);
Verbose("Messages = %d %%\n",THIS[ob_messages]);
}

/*****************************************************************************/

void GatherLoadData()

{ double load[4] = {0,0,0,0}, sum = 0.0; 
 int i,n = 1;

Debug("GatherLoadData\n\n");

#ifdef HAVE_GETLOADAVG 
if ((n = getloadavg(load,LOADAVG_5MIN)) == -1)
   {
   THIS[ob_loadavg] = 0.0;
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
 
THIS[ob_loadavg] = (int) (100.0 * sum);
Verbose("100 x Load Average = %d\n",THIS[ob_loadavg]);
}

/*****************************************************************************/

void GatherSocketData()

{ FILE *pp;
  char local[CF_BUFSIZE],remote[CF_BUFSIZE],comm[CF_BUFSIZE];
  struct Item *in[ATTR],*out[ATTR];
  char *sp;
  int i;
  
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
 
if ((pp = cfpopen(comm,"r")) == NULL)
   {
   return;
   }

while (!feof(pp))
   {
   memset(local,0,CF_BUFSIZE);
   memset(remote,0,CF_BUFSIZE);
   
   ReadLine(VBUFF,CF_BUFSIZE,pp);

   if (strstr(VBUFF,"UNIX"))
      {
      break;
      }

   if (!strstr(VBUFF,"."))
      {
      continue;
      }

   /* Different formats here ... ugh.. */

   if (strncmp(VBUFF,"tcp",3) == 0)
      {
      sscanf(VBUFF,"%*s %*s %*s %s %s",local,remote); /* linux-like */
      }
   else
      {
      sscanf(VBUFF,"%s %s",local,remote);             /* solaris-like */
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
         THIS[ECGSOCKS[i].in]++;
         AppendItem(&in[i],VBUFF,"");
         }
      
      for (spend = remote+strlen(remote)-1; (sp >= remote) && isdigit((int)*spend); spend--)
         {
         }
      
      spend++;
      
      if (strcmp(spend,ECGSOCKS[i].portnr) == 0)
         {
         THIS[ECGSOCKS[i].out]++;
         AppendItem(&out[i],VBUFF,"");
         }
      }
   }
 
 cfpclose(pp);
 
/* Now save the state for ShowState() alert function IFF the state is not smaller than the last or
   at least 40 minutes older. This mirrors the persistence of the maxima classes */

 
 for (i = 0; i < ATTR; i++)
    {
    struct stat statbuf;
    time_t now = time(NULL);
    
    Debug("save incoming %s\n",ECGSOCKS[i].name);
    snprintf(VBUFF,CF_MAXVARSIZE,"%s/state/cf_incoming.%s",CFWORKDIR,ECGSOCKS[i].name);
    if (stat(VBUFF,&statbuf) != -1)
       {
       if ((ByteSizeList(in[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
          {
          Verbose("New state %s is smaller, retaining old for 40 mins longer\n",ECGSOCKS[i].name);
          DeleteItemList(in[i]);
          continue;
          }
       }
    
    SetEntropyClasses(ECGSOCKS[i].name,in[i],"in");
    SaveItemList(in[i],VBUFF,"none");
    DeleteItemList(in[i]);
    Debug("Saved in netstat data in %s\n",VBUFF); 
    }
 
 for (i = 0; i < ATTR; i++)
    {
    struct stat statbuf;
    time_t now = time(NULL); 

    Debug("save outgoing %s\n",ECGSOCKS[i].name);
    snprintf(VBUFF,CF_MAXVARSIZE,"%s/state/cf_outgoing.%s",CFWORKDIR,ECGSOCKS[i].name);

    if (stat(VBUFF,&statbuf) != -1)
       {       
       if ((ByteSizeList(out[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
          {
          Verbose("New state %s is smaller, retaining old for 40 mins longer\n",ECGSOCKS[i].name);
          DeleteItemList(out[i]);
          continue;
          }
       }
    
    SetEntropyClasses(ECGSOCKS[i].name,out[i],"out");
    SaveItemList(out[i],VBUFF,"none");
    Debug("Saved out netstat data in %s\n",VBUFF); 
    DeleteItemList(out[i]);
    }
  
 for (i = 0; i < CF_NETATTR; i++)
    {
    struct stat statbuf;
    time_t now = time(NULL); 
    
    Debug("save incoming %s\n",TCPNAMES[i]);
    snprintf(VBUFF,CF_MAXVARSIZE,"%s/state/cf_incoming.%s",CFWORKDIR,TCPNAMES[i]);
    
    if (stat(VBUFF,&statbuf) != -1)
       {       
       if ((ByteSizeList(NETIN_DIST[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
          {
          Verbose("New state %s is smaller, retaining old for 40 mins longer\n",TCPNAMES[i]);
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
    snprintf(VBUFF,CF_MAXVARSIZE,"%s/state/cf_outgoing.%s",CFWORKDIR,TCPNAMES[i]);
    
    if (stat(VBUFF,&statbuf) != -1)
       {       
       if ((ByteSizeList(NETOUT_DIST[i]) < statbuf.st_size) && (now < statbuf.st_mtime+40*60))
          {
          Verbose("New state %s is smaller, retaining old for 40 mins longer\n",TCPNAMES[i]);
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

void GatherSNMPData()

{ char snmpbuffer[CF_BUFSIZE];
 FILE *pp;

/* This is for collecting known counters.  */
 
if (SCLI)
   {
   struct stat statbuf;
   char buffer[CF_MAXVARSIZE];
   sscanf(CF_SCLI_COMM,"%s",buffer);
   
   if (stat(buffer,&statbuf) != -1)
      {
      if ((pp = cfpopen(CF_SCLI_COMM,"r")) == NULL)
         {
         return;
         }

      /* Skip first banner */
      fgets(snmpbuffer,CF_BUFSIZE-1,pp);
      }
   }
}


/*****************************************************************************/

struct Averages *GetCurrentAverages(char *timekey)

{ int err_no;
  DBT key,value;
  DB *dbp;
  static struct Averages entry;
 
if ((err_no = db_create(&dbp,NULL,0)) != 0)
   {
   sprintf(OUTPUT,"Couldn't initialize average database %s\n",AVDB);
   CfLog(cferror,OUTPUT,"db_open");
   return NULL;
   }

#ifdef CF_OLD_DB 
if ((err_no = (dbp->open)(dbp,AVDB,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((err_no = (dbp->open)(dbp,NULL,AVDB,NULL,DB_BTREE,DB_CREATE,0644)) != 0)    
#endif
   {
   sprintf(OUTPUT,"Couldn't open average database %s\n",AVDB);
   CfLog(cferror,OUTPUT,"db_open");
   return NULL;
   }

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
memset(&entry,0,sizeof(entry));
      
key.data = timekey;
key.size = strlen(timekey)+1;

if ((err_no = dbp->get(dbp,NULL,&key,&value,0)) != 0)
   {
   if (err_no != DB_NOTFOUND)
      {
      dbp->err(dbp,err_no,NULL);
      dbp->close(dbp,0);
      return NULL;
      }
   }

AGE++;
WAGE = AGE / CF_WEEK * CF_MEASURE_INTERVAL;

if (value.data != NULL)
   {
   int i;
   memcpy(&entry,value.data,sizeof(entry));

   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      Debug("Previous values (%lf,..) for time index %s\n\n",entry.Q[i].expect,timekey);
      }
   
   dbp->close(dbp,0);
   return &entry;
   }
else
   {
   Debug("No previous value for time index %s\n",timekey);
   dbp->close(dbp,0);
   return &entry;
   }
}

/*****************************************************************************/

void UpdateAverages(char *timekey,struct Averages newvals)

{ int err_no;
  DBT key,value;
  DB *dbp;
 
if ((err_no = db_create(&dbp,NULL,0)) != 0)
   {
   sprintf(OUTPUT,"Couldn't open average database %s\n",AVDB);
   CfLog(cferror,OUTPUT,"db_open");
   return;
   }

#ifdef CF_OLD_DB 
if ((err_no = (dbp->open)(dbp,AVDB,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((err_no = (dbp->open)(dbp,NULL,AVDB,NULL,DB_BTREE,DB_CREATE,0644)) != 0)    
#endif
   {
   sprintf(OUTPUT,"Couldn't open average database %s\n",AVDB);
   CfLog(cferror,OUTPUT,"db_open");
   return;
   }

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = timekey;
key.size = strlen(timekey)+1;

value.data = &newvals;
value.size = sizeof(newvals);
 
if ((err_no = dbp->put(dbp,NULL,&key,&value,0)) != 0)
   {
   dbp->err(dbp,err_no,NULL);
   dbp->close(dbp,0);
   return;
   } 

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));

value.data = &AGE;
value.size = sizeof(double);    
key.data = "DATABASE_AGE";
key.size = strlen("DATABASE_AGE")+1;

if ((err_no = dbp->put(dbp,NULL,&key,&value,0)) != 0)
   {
   dbp->err(dbp,err_no,NULL);
   dbp->close(dbp,0);
   return;
   }
 
dbp->close(dbp,0);
}

/*****************************************************************************/

void UpdateDistributions(char *timekey,struct Averages *av)

{ int position; 
  int day,i,time_to_update = true;
 
/* Take an interval of 4 standard deviations from -2 to +2, divided into CF_GRAINS
   parts. Centre each measurement on CF_GRAINS/2 and scale each measurement by the
   std-deviation for the current time.
 */
if (HISTO)
   {
   time_to_update = (int) (3600.0*rand()/(RAND_MAX+1.0)) > 2400;
   
   day = Day2Number(timekey);
   
   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      position = CF_GRAINS/2 + (int)(0.5+(THIS[i] - av->Q[i].expect)*CF_GRAINS/(4*sqrt((av->Q[i].var))));

      if (0 <= position && position < CF_GRAINS)
         {
         HISTOGRAM[i][day][position]++;
         }
      }
   
   if (time_to_update)
      {
      FILE *fp;
      char filename[CF_BUFSIZE];
      
      snprintf(filename,CF_BUFSIZE,"%s/state/histograms",CFWORKDIR);
      
      if ((fp = fopen(filename,"w")) == NULL)
         {
         CfLog(cferror,"Unable to save histograms","fopen");
         return;
         }
      
      for (position = 0; position < CF_GRAINS; position++)
         {
         fprintf(fp,"%u ",position);
         
         for (i = 0; i < CF_OBSERVABLES; i++)
            {
            for (day = 0; day < 7; day++)
               {
               fprintf(fp,"%u ",HISTOGRAM[i][day][position]);
               }
            }
         fprintf(fp,"\n");
         }
      
      fclose(fp);
      }
   }
}

/*****************************************************************************/

double WAverage(double anew,double aold,double age)

 /* For a couple of weeks, learn eagerly. Otherwise variances will
    be way too large. Then downplay newer data somewhat, and rely on
    experience of a couple of months of data ... */

{ double av;
  double wnew,wold;

if (FORGETRATE > 0.9 || FORGETRATE < 0.1)
   {
   FORGETRATE = 0.7;
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

av = (wnew*anew + wold*aold)/(wnew+wold); 

return av;
}

/*****************************************************************************/

double SetClasses(char * name,double variable,double av_expect,double av_var,double localav_expect,double localav_var,struct Item **classlist,char *timekey)

{ char buffer[CF_BUFSIZE],buffer2[CF_BUFSIZE];
  double dev,delta,sigma,ldelta,lsigma,sig;

Debug("\n SetClasses(%s,X=%f,avX=%f,varX=%f,lavX=%f,lvarX=%f,%s)\n",name,variable,av_expect,av_var,localav_expect,localav_var,timekey);

delta = variable - av_expect;
sigma = sqrt(av_var);
ldelta = variable - localav_expect;
lsigma = sqrt(localav_var);
sig = sqrt(sigma*sigma+lsigma*lsigma); 

Debug(" delta = %f,sigma = %f, lsigma = %f, sig = %f\n",delta,sigma,lsigma,sig);
 
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
       AddPersistentClass(buffer2,CF_PERSISTENCE,cfpreserve); 
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
       AddPersistentClass(buffer2,CF_PERSISTENCE,cfpreserve); 
       }
    
    if (dev > 3.0*sqrt(2.0))
       {
       strcpy(buffer2,buffer);
       strcat(buffer2,"_anomaly");
       AppendItem(classlist,buffer2,"3");
       AddPersistentClass(buffer2,CF_PERSISTENCE,cfpreserve); 
       }

    return sig; 
    }
}

/*****************************************************************************/

void SetVariable(char *name,double value,double average,double stddev,struct Item **classlist)


{ char var[CF_BUFSIZE];

sprintf(var,"value_%s=%d",name,(int)value);
AppendItem(classlist,var,"");

sprintf(var,"average_%s=%1.1f",name,average);
AppendItem(classlist,var,"");

sprintf(var,"stddev_%s=%1.1f",name,stddev);
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
   THIS[i] = 0;
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
   Verbose("Value accepted\n");
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

Verbose("TCP Save %s\n",TCPNAMES[i]);
  
if (list == NULL)
   {
   Verbose("No %s-%s events\n",TCPNAMES[i],inout);
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

Verbose("TCP Save %s\n",filename);
 
if ((fp = fopen(filename,"w")) == NULL)
   {
   Verbose("Unable to write datafile %s\n",filename);
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
  size_t q, dq;

if (stat(filename,&statbuf) == -1)
   {
   return 0;
   }

q = statbuf.st_size;

Verbose("GetFileGrowth(%s) = %d\n",filename,q);

dq = q - LASTQ[index];

if (LASTQ[index] == 0)
   {
   LASTQ[index] = q;
   return dq/10;       /* arbitrary spike mitigation */
   }

LASTQ[index] = q;
return dq;
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

int GetAcpi()

{ DIR *dirh;
  FILE *fp;
  struct dirent *dirp;
  int count = 0;
  char path[CF_BUFSIZE],buf[CF_BUFSIZE],index[4];
  double temp = 0;
  
Debug("ACPI temperature\n");

if ((dirh = opendir("/proc/acpi/thermal_zone")) == NULL)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't open directory %s\n",path);
   CfLog(cfverbose,OUTPUT,"opendir");
   return false;
   }

for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
   {
   if (!SensibleFile(dirp->d_name,path,NULL))
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
            case 0: THIS[ob_temp0] = temp;
                break;
            case 1: THIS[ob_temp1] = temp;
                break;
            case 2: THIS[ob_temp2] = temp;
                break;
            case 3: THIS[ob_temp3] = temp;
                break;
            }

         Debug("Set temp%d to %f\n",count,temp);
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

THIS[ob_temp0] = 0.0;
THIS[ob_temp1] = 0.0;
THIS[ob_temp2] = 0.0;
THIS[ob_temp3] = 0.0;
  
if ((pp = cfpopen("/usr/bin/sensors","r")) == NULL)
   {
   LMSENSORS = false; /* Broken */
   return false;
   }

ReadLine(VBUFF,CF_BUFSIZE,pp); 

while (!feof(pp))
   {
   ReadLine(VBUFF,CF_BUFSIZE,pp);

   if (strstr(VBUFF,"Temp")||strstr(VBUFF,"temp"))
      {
      PrependItem(&list,VBUFF,NULL);
      }
   }

cfpclose(pp);

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
            case 0: THIS[ob_temp0] = temp;
                break;
            case 1: THIS[ob_temp1] = temp;
                break;
            case 2: THIS[ob_temp2] = temp;
                break;
            case 3: THIS[ob_temp3] = temp;
                break;
            }

         Debug("Set temp%d to %f from what looks like cpu temperature\n",count,temp);
         }
      }
   }

if (THIS[ob_temp0] != 0)
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
            case 0: THIS[ob_temp0] = temp;
                break;
            case 1: THIS[ob_temp1] = temp;
                break;
            case 2: THIS[ob_temp2] = temp;
                break;
            case 3: THIS[ob_temp3] = temp;
                break;
            }

         Debug("Set temp%d to %f from what looks like core temperatures\n",count,temp);
         }
      }
   }

if (THIS[ob_temp0] != 0)
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
      THIS[ob_temp0] = temp;
      }

   if (strncmp(ip->name,"M/B Temp:",strlen("M/B Temp:")) == 0  )
      {
      sscanf(ip->name,"%*[^:]: %lf",&temp);
      Debug("Setting temp0 to M/B Temp\n");
      THIS[ob_temp1] = temp;
      }

   if (strncmp(ip->name,"Sys Temp:",strlen("Sys Temp:")) == 0  )
      {
      sscanf(ip->name,"%*[^:]: %lf",&temp);
      Debug("Setting temp0 to Sys Temp\n");
      THIS[ob_temp2] = temp;
      }

   if (strncmp(ip->name,"AUX Temp:",strlen("AUX Temp:")) == 0  )
      {
      sscanf(ip->name,"%*[^:]: %lf",&temp);
      Debug("Setting temp0 to AUX Temp\n");
      THIS[ob_temp3] = temp;
      }
   }

if (THIS[ob_temp0] != 0)
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
            case 0: THIS[ob_temp0] = temp;
                break;
            case 1: THIS[ob_temp1] = temp;
                break;
            case 2: THIS[ob_temp2] = temp;
                break;
            case 3: THIS[ob_temp3] = temp;
                break;
            }

         Debug("Set temp%d to %f\n",count,temp);
         }
      }
   }

/* Give up? */

DeleteItemList(list);

return true;
}







