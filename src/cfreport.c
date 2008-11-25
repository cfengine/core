/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
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
/* File: report.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);
void CheckOpts(int argc,char **argv);
void ThisAgentInit(void);
void KeepReportControlPromises(void);
void ShowLastSeen(void);
void ShowPerformance(void);
void ShowLastSeen(void);
void ShowClasses(void);
void ShowChecksums(void);
void ShowLocks (int active);
void ShowCurrentAudit(void);
char *ChecksumDump(unsigned char digest[EVP_MAX_MD_SIZE+1]);
char *Format(char *s,int width);
int CompareClasses(const void *a, const void *b);
void PrintDB(void);

void ReadAverages(void);
void SummarizeAverages(void);
void WriteGraphFiles(void);
void WriteHistograms(void);
void DiskArrivals(void);
void PeerIntermittency(void);
void GetFQHN(void);
void OpenFiles(void);
void CloseFiles(void);
void MagnifyNow(void);
void OpenMagnifyFiles(void);
void CloseMagnifyFiles(void);
void EraseAverages(void);


/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern struct BodySyntax CFRP_CONTROLBODY[];

int HTML = false;
int GRAPH = false;
char GRAPHDIR[CF_MAXVARSIZE];


int TITLES = false;
int TIMESTAMPS = false;
int HIRES = false;
int SEPARATE = false;
int ERRORBARS = true;
int NOSCALING = true;
int NOWOPT = false;

char FILENAME[CF_BUFSIZE];
unsigned int HISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS];
int SMOOTHHISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS];
char VFQNAME[CF_MAXVARSIZE];
char ERASE[CF_BUFSIZE];
int ERRNO;
time_t NOW;

DB *DBP;
static struct Averages ENTRY,MAX,MIN,DET;
char TIMEKEY[CF_SMALLBUF],FLNAME[CF_BUFSIZE],*sp;
double AGE;
FILE *FPAV=NULL,*FPVAR=NULL, *FPNOW=NULL;
FILE *FPE[CF_OBSERVABLES],*FPQ[CF_OBSERVABLES];
FILE *FPM[CF_OBSERVABLES];



/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
 struct option OPTIONS[12] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f' },
      { "syntax",no_argument,0,'S'},
      { "html",no_argument,0,'H'},
      { "xml",no_argument,0,'X'},
      { "sql",no_argument,0,'s'},
      { "graphs",no_argument,0,'g'},
      { NULL,0,0,'\0' }
      };

/*******************************************************************/

enum databases
   {
   cf_db_lastseen,
   cf_db_locks,
   cf_db_active,
   cf_db_checksum,
   cf_db_performance,
   cf_db_audit,
   cf_db_classes,
   cf_db_regex
   };

enum databases TODO = -1;

#define CF_ACTIVE 1
#define CF_INACTIVE 0

/*******************************************************************/

struct CEnt /* For sorting */
   {
   char name[256];
   char date[32];
   double q;
   double d;
   };

/*******************************************************************/

enum cf_formatindex
   {
   cfb,
   cfe,
   };

enum cf_format
   {
   cfx_entry,
   cfx_event,
   cfx_host,
   cfx_pm,
   cfx_ip,
   cfx_date,
   cfx_q,
   cfx_av,
   cfx_dev,
   cfx_version,
   cfx_ref,
   cfx_filename,
   cfx_index
   };

char *CFRX[][2] =
   {
    "<entry>\n","\n</entry>\n",
    "<event>\n","\n</event>\n",
    "<hostname>\n","\n</hostname>\n",
    "<pm>\n","\n</pm>\n",
    "<ip>\n","\n</ip>\n",
    "<date>\n","\n</date>\n",
    "<q>\n","\n</q>\n",
    "<expect>\n","\n</expect>\n",
    "<sigma>\n","\n</sigma>\n",
    "<version>\n","\n</version>\n",
    "<ref>\n","\n</ref>\n",
    "<filename>\n","\n</filename>\n",
    "<index>\n","\n</index>\n",
    NULL,NULL
   };

char *CFRH[][2] =
   {
    "<tr>","</tr>\n\n",
    "<td>","</td>\n",
    "<td>","</td>\n",
    "<td bgcolor=#add8e6>","</td>\n",
    "<td bgcolor=#e0ffff>","</td>\n",
    "<td bgcolor=#f0f8ff>","</td>\n",
    "<td bgcolor=#fafafa>","</td>\n",
    "<td bgcolor=#ededed>","</td>\n",
    "<td bgcolor=#e0e0e0>","</td>\n",
    "<td bgcolor=#add8e6>","</td>\n",
    "<td bgcolor=#e0ffff>","</td>\n",
    "<td bgcolor=#fafafa><small>","</small></td>\n",
    "<td bgcolor=#fafafa><small>","</small></td>\n",
    NULL,NULL
   };


/*****************************************************************************/

int main(int argc,char *argv[])

{
GenericInitialize(argc,argv,"knowledge");
ThisAgentInit();
KeepReportControlPromises();

PrintDB();

if (strlen(ERASE) > 0)
   {
   EraseAverages();
   exit(0);
   }


ReadAverages(); 
SummarizeAverages();

if (strlen(FLNAME) == 0)
   {
   if (TIMESTAMPS)
      {
      if ((NOW = time((time_t *)NULL)) == -1)
         {
         Verbose("Couldn't read system clock\n");
         }
      sprintf(FLNAME,"cfenvgraphs-%s-%s",CanonifyName(VFQNAME),ctime(&NOW));
      }
   else
      {
      sprintf(FLNAME,"cfenvgraphs-snapshot-%s",CanonifyName(VFQNAME));
      }
   }

Verbose("Creating sub-directory %s\n",FLNAME);

if (mkdir(FLNAME,0755) == -1)
   {
   Verbose("Writing to existing directory\n");
   }
 
if (chdir(FLNAME))
   {
   perror("chdir");
   exit(0);
   }

Verbose("Writing data to sub-directory %s: \n   x,y1,y2,y3...\n ",FLNAME);

if (NOWOPT)
   {
   MagnifyNow();
   }
else
   {
   WriteGraphFiles();
   WriteHistograms();
   DiskArrivals();
   PeerIntermittency();
   }


return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

 
while ((c=getopt_long(argc,argv,"ghHd:vVf:Sst:r:",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'd': 
          AddClassToHeap("opt_debug");
          switch ((optarg==NULL) ? '3' : *optarg)
             {
             case '1':
                 D1 = true;
                 DEBUG = true;
                 break;
             case '2':
                 D2 = true;
                 DEBUG = true;
                 break;
             case '3':
                 D3 = true;
                 DEBUG = true;
                 VERBOSE = true;
                 break;
             case '4':
                 D4 = true;
                 DEBUG = true;
                 break;
             default:
                 DEBUG = true;
                 break;
             }
          break;

      case 'v':
          VERBOSE = true;
          break;
          
      case 'V':
          Version("Knowledge agent");
          exit(0);
          
      case 'h':
          Syntax("Reporting agent",OPTIONS);
          exit(0);

      case 'H':
          HTML = 1;
          break;

      default: Syntax("Reporting agent",OPTIONS);
          exit(1);
          
      }
  }
}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];

strcpy(GRAPHDIR,"");
}

/*****************************************************************************/

void KeepReportControlPromises()

{ struct Constraint *cp;
  char rettype;
  void *retval;

for (cp = ControlBodyConstraints(cf_report); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }

   if (GetVariable("control_reportagent",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in report agent control body",cp->lval);
      continue;
      }
   

   }
}


/********************************************************************/

void PrintDB()

{
 /*
 ShowLastSeen();
 ShowLocks(CF_INACTIVE);
 ShowLocks(CF_ACTIVE);
 ShowChecksums();
 ShowPerformance();
 ShowCurrentAudit();
 ShowClasses();

 */
}

/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

void ShowLastSeen()

{ DBT key,value;
  DB *dbp;
  DBC *dbcp;
  DB_ENV *dbenv = NULL;
  double now = (double)time(NULL),average = 0, var = 0;
  double ticksperhr = (double)CF_TICKS_PER_HOUR;
  char name[CF_BUFSIZE],hostname[CF_BUFSIZE];
  struct QPoint entry;
  int ret;
  
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);

if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   printf("Couldn't open last-seen database %s\n",name);
   perror("db_open");
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   printf("Couldn't open last-seen database %s\n",name);
   perror("db_open");
   dbp->close(dbp,0);
   return;
   }

if (HTML)
   {
   printf("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/menus.css\" /><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/cf_blue.css\"/></head><body><h1>Peers recently seen by %s</h1><p><table class=border cellpadding=5>",VFQNAME);
   }

if (XML)
   {
   printf("<?xml version=\"1.0\"?>\n<output>\n");
   }

/* Acquire a cursor for the database. */

if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   printf("Error reading from last-seen database: ");
   dbp->err(dbp, ret, "DB->cursor");
   return;
   }

 /* Initialize the key/data return pair. */

memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));
memset(&entry, 0, sizeof(entry)); 
 
 /* Walk through the database and print out the key/data pairs. */

while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
   {
   double then;
   time_t fthen;
   char tbuf[CF_BUFSIZE],addr[CF_BUFSIZE];

   memcpy(&then,value.data,sizeof(then));
   strcpy(hostname,(char *)key.data);

   if (value.data != NULL)
      {
      memcpy(&entry,value.data,sizeof(entry));
      then = entry.q;
      average = (double)entry.expect;
      var = (double)entry.var;
      }
   else
      {
      continue;
      }

   if (PURGE == 'y')
      {
      if (now - then > CF_WEEK)
         {
         if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
            {
            CfOut(cferror,"db_store","Cannot delete database entry");
            }
         }

      CfOut(cf_inform,"","Deleting expired entry for %s\n",hostname);
      continue;
      }
   
   fthen = (time_t)then;                            /* format date */
   snprintf(tbuf,CF_BUFSIZE-1,"%s",ctime(&fthen));
   tbuf[strlen(tbuf)-9] = '\0';                     /* Chop off second and year */

   if (strlen(hostname+1) > 15)
      {
      snprintf(addr,15,"...%s",hostname+strlen(hostname)-10); /* ipv6 */
      }
   else
      {
      snprintf(addr,15,"%s",hostname+1);
      }

   if (XML)
      {
      printf("%s",CFRX[cfx_entry][cfb]);
      printf("%s%c%s",CFRX[cfx_pm][cfb],*hostname,CFRX[cfx_pm][cfe]);
      printf("%s%s%s",CFRX[cfx_host][cfb],IPString2Hostname(hostname+1),CFRX[cfx_host][cfe]);
      printf("%s%s%s",CFRX[cfx_ip][cfb],hostname+1,CFRX[cfx_ip][cfe]);
      printf("%s%s%s",CFRX[cfx_date][cfb],tbuf,CFRX[cfx_date][cfe]);
      printf("%s%.2f%s",CFRX[cfx_q][cfb],((double)(now-then))/ticksperhr,CFRX[cfx_q][cfe]);
      printf("%s%.2f%s",CFRX[cfx_av][cfb],average/ticksperhr,CFRX[cfx_av][cfe]);
      printf("%s%.2f%s",CFRX[cfx_dev][cfb],sqrt(var)/ticksperhr,CFRX[cfx_dev][cfe]);
      printf("%s",CFRX[cfx_entry][cfe]);
      }
   else if (HTML)
      {
      printf("%s",CFRH[cfx_entry][cfb]);
      printf("%s%c%s",CFRH[cfx_pm][cfb],*hostname,CFRH[cfx_pm][cfe]);
      printf("%s%s%s",CFRH[cfx_host][cfb],IPString2Hostname(hostname+1),CFRH[cfx_host][cfe]);
      printf("%s%s%s",CFRH[cfx_ip][cfb],hostname+1,CFRH[cfx_ip][cfe]);
      printf("%s Last seen at %s%s",CFRH[cfx_date][cfb],tbuf,CFRH[cfx_date][cfe]);
      printf("%s %.2f hrs ago %s",CFRH[cfx_q][cfb],((double)(now-then))/ticksperhr,CFRH[cfx_q][cfe]);
      printf("%s Av %.2f hrs %s",CFRH[cfx_av][cfb],average/ticksperhr,CFRH[cfx_av][cfe]);
      printf("%s &plusmn; %.2f hrs %s",CFRH[cfx_dev][cfb],sqrt(var)/ticksperhr,CFRH[cfx_dev][cfe]);
      printf("%s",CFRH[cfx_entry][cfe]);
      }
   else
      {
      printf("IP %c %25.25s %15.15s  @ [%s] not seen for (%.2f) hrs, Av %.2f +/- %.2f hrs\n",
             *hostname,
             IPString2Hostname(hostname+1),
             addr,
             tbuf,
             ((double)(now-then))/ticksperhr,
             average/ticksperhr,
             sqrt(var)/ticksperhr);
      }
   }

if (HTML)
   {
   printf("</table>");
   }

if (XML)
   {
   printf("</output>\n");
   }

dbcp->c_close(dbcp);
dbp->close(dbp,0);
}

/*******************************************************************/

void ShowPerformance()

{ DBT key,value;
  DB *dbp;
  DBC *dbcp;
  DB_ENV *dbenv = NULL;
  double now = (double)time(NULL),average = 0, var = 0;
  double ticksperminute = 60.0;
  char name[CF_BUFSIZE],eventname[CF_BUFSIZE];
  struct Event entry;
  int ret;
  
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_PERFORMANCE);

if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   printf("Couldn't open performance database %s\n",name);
   perror("db_open");
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open performance database %s\n",name);
   dbp->close(dbp,0);
   return;
   }

/* Acquire a cursor for the database. */

if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   CfOut(cf_error,"","Error reading from performance database: ");
   dbp->err(dbp, ret, "DB->cursor");
   return;
   }

 /* Initialize the key/data return pair. */
 
memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));
memset(&entry, 0, sizeof(entry)); 

if (HTML)
   {
   printf("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/menus.css\" /><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/cf_blue.css\"/></head><body><h1>Peformance recently measured on %s</h1><p><table class=border cellpadding=5>",VFQNAME);
   printf("<div id=\"performance\">");
   }

if (XML)
   {
   printf("<?xml version=\"1.0\"?>\n<output>\n");
   }

 /* Walk through the database and print out the key/data pairs. */

while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
   {
   double measure;
   time_t then;
   char tbuf[CF_BUFSIZE],addr[CF_BUFSIZE];

   memcpy(&then,value.data,sizeof(then));
   strcpy(eventname,(char *)key.data);

   if (value.data != NULL)
      {
      memcpy(&entry,value.data,sizeof(entry));

      then    = entry.t;
      measure = entry.Q.q/ticksperminute;;
      average = entry.Q.expect/ticksperminute;;
      var     = entry.Q.var;

      snprintf(tbuf,CF_BUFSIZE-1,"%s",ctime(&then));
      tbuf[strlen(tbuf)-9] = '\0';                     /* Chop off second and year */

      if (PURGE == 'y')
         {
         if (now - then > CF_WEEK)
            {
            if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
               {
               CfOut(cf_error,"db_store","Cannot delete from database");
               }
            }
         
         CfOut(cf_inform,"","Deleting expired entry for %s\n",eventname);

         if (measure < 0 || average < 0 || measure > 4*CF_WEEK)
            {
            if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
               {
               CfLog(cferror,"","db_store");
               }
            }
         
         CfOut(cf_inform,"","Deleting entry for %s because it seems to take longer than 4 weeks to complete\n",eventname);

         continue;
         }
      
      if (XML)
         {
         printf("%s",CFRX[cfx_entry][cfb]);
         printf("%s%s%s",CFRX[cfx_event][cfb],eventname,CFRX[cfx_event][cfe]);
         printf("%s%s%s",CFRX[cfx_date][cfb],tbuf,CFRX[cfx_date][cfe]);
         printf("%s%.4lf%s",CFRX[cfx_q][cfb],measure,CFRX[cfx_q][cfe]);
         printf("%s%.4lf%s",CFRX[cfx_av][cfb],average,CFRX[cfx_av][cfe]);
         printf("%s%.4lf%s",CFRX[cfx_dev][cfb],sqrt(var)/ticksperminute,CFRX[cfx_dev][cfe]);
         printf("%s",CFRX[cfx_entry][cfe]);         
         }
      else if (HTML)
         {
         printf("%s",CFRH[cfx_entry][cfb]);
         printf("%s%s%s",CFRH[cfx_event][cfb],eventname,CFRH[cfx_event][cfe]);
         printf("%s last performed at %s%s",CFRH[cfx_date][cfb],tbuf,CFRH[cfx_date][cfe]);
         printf("%s completed in %.4lf mins %s",CFRH[cfx_q][cfb],measure,CFRH[cfx_q][cfe]);
         printf("%s Av %.4lf mins %s",CFRH[cfx_av][cfb],average,CFRH[cfx_av][cfe]);
         printf("%s &plusmn; %.4lf mins %s",CFRH[cfx_dev][cfb],sqrt(var)/ticksperminute,CFRH[cfx_dev][cfe]);
         printf("%s",CFRH[cfx_entry][cfe]);
         }
      else
         {
         printf("(%7.4lf mins @ %s) Av %7.4lf +/- %7.4lf for %s \n",measure,tbuf,average,sqrt(var)/ticksperminute,eventname);
         }
      }
   else
      {
      continue;
      }
   }

if (HTML)
   {
   printf("</table>");
   printf("</div>\n</body></html>\n");
   }

if (XML)
   {
   printf("</output>\n");
   }

dbcp->c_close(dbcp);
dbp->close(dbp,0);
}


/*******************************************************************/

void ShowClasses()

{ DBT key,value;
  DB *dbp;
  DBC *dbcp;
  DB_ENV *dbenv = NULL;
  double now = (double)time(NULL),average = 0, var = 0;
  double ticksperminute = 60.0;
  char name[CF_BUFSIZE],eventname[CF_BUFSIZE];
  struct Event entry;
  struct CEnt array[1024];
  int ret, i;
  
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_CLASSUSAGE);

if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   CfOut(cf_error,"db_open","Couldn't create class database %s\n",name);
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open class database %s\n",name);
   dbp->close(dbp,0);
   return;
   }

/* Acquire a cursor for the database. */

if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   CfOut(cf_error,"","Error reading from class database: ");
   dbp->err(dbp, ret, "DB->cursor");
   return;
   }

 /* Initialize the key/data return pair. */
 
memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));
memset(&entry, 0, sizeof(entry)); 

if (HTML)
   {
   time_t now = time(NULL);
   
   printf("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/menus.css\" /><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/cf_blue.css\"/></head><body><h1>Class probabilities observed on %s at %s</h1><p><table class=border cellpadding=5>",VFQNAME,ctime(&now));
   }

if (XML)
   {
   printf("<?xml version=\"1.0\"?>\n<output>\n");
   }

 /* Walk through the database and print out the key/data pairs. */

for (i = 0; i < 1024; i++)
   {
   array[i].q = -1;
   }

i = 0;

while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
   {
   double measure;
   time_t then;
   char tbuf[CF_BUFSIZE],addr[CF_BUFSIZE];

   memcpy(&then,value.data,sizeof(then));
   strcpy(eventname,(char *)key.data);

   if (value.data != NULL)
      {
      memcpy(&entry,value.data,sizeof(entry));

      then    = entry.t;
      measure = entry.Q.q;
      average = entry.Q.expect;
      var     = entry.Q.var;

      snprintf(tbuf,CF_BUFSIZE-1,"%s",ctime(&then));
      tbuf[strlen(tbuf)-9] = '\0';                     /* Chop off second and year */

      if (PURGE == 'y')
         {
         if (now - then > CF_WEEK*52)
            {
            if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
               {
               CfOut(cf_error,"db_del","Can't delete from database");
               }
            }
         
         CfOut(cf_error,"","Deleting expired entry for %s\n",eventname);
         continue;
         }
      
      if (i++ < 1024)
         {
         strncpy(array[i].date,tbuf,31);
         strncpy(array[i].name,eventname,255);
         array[i].q = average;
         array[i].d = var;
         }
      else
         {
         break;
         }
      }
   }

#ifdef HAVE_QSORT
qsort(array,1024,sizeof(struct CEnt),CompareClasses);
#endif

for (i = 0; array[i].q > 0; i++)
   {
   if (XML)
      {
      printf("%s",CFRX[cfx_entry][cfb]);
      printf("%s%s%s",CFRX[cfx_event][cfb],array[i].name,CFRX[cfx_event][cfe]);
      printf("%s%s%s",CFRX[cfx_date][cfb],array[i].date,CFRX[cfx_date][cfe]);
      printf("%s%.4f%s",CFRX[cfx_av][cfb],array[i].q,CFRX[cfx_av][cfe]);
      printf("%s%.4f%s",CFRX[cfx_dev][cfb],sqrt(array[i].d),CFRX[cfx_dev][cfe]);
      printf("%s",CFRX[cfx_entry][cfe]);         
      }
   else if (HTML)
      {
      printf("%s",CFRH[cfx_entry][cfb]);
      printf("%s%s%s",CFRH[cfx_event][cfb],array[i].name,CFRH[cfx_event][cfe]);
      printf("%s last occured at %s%s",CFRH[cfx_date][cfb],array[i].date,CFRH[cfx_date][cfe]);
      printf("%s Probability %.4f %s",CFRH[cfx_av][cfb],array[i].q,CFRH[cfx_av][cfe]);
      printf("%s &plusmn; %.4f %s",CFRH[cfx_dev][cfb],sqrt(array[i].d),CFRH[cfx_dev][cfe]);
      printf("%s",CFRH[cfx_entry][cfe]);
      }
   else
      {
      printf("Probability %7.4f +/- %7.4f for %s (last oberved @ %s)\n",array[i].q,sqrt(array[i].d),array[i].name,array[i].date);
      }
   }

if (HTML)
   {
   printf("</table>");
   }

if (XML)
   {
   printf("</output>\n");
   }

dbcp->c_close(dbcp);
dbp->close(dbp,0);
}

/*******************************************************************/

void ShowChecksums()

{ DBT key,value;
  DB *dbp;
  DBC *dbcp;
  DB_ENV *dbenv = NULL;
  int ret;
  FILE *pp;
  char checksumdb[CF_BUFSIZE];
  struct stat statbuf;
 
snprintf(checksumdb,CF_BUFSIZE,"%s/%s",CFWORKDIR,CF_CHKDB);
  
if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   CfOut(cf_error,"db_create","Couldn't create checksum database %s\n",checksumdb);
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,checksumdb,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,checksumdb,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open checksum database %s\n",checksumdb);
   dbp->close(dbp,0);
   return;
   }

if (HTML)
   {
   printf("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/menus.css\" /><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/cf_blue.css\"/></head><body><h1>Message Digests sampled on %s</h1><p><table class=border cellpadding=5 width=800>",VFQNAME);
   }

if (XML)
   {
   printf("<?xml version=\"1.0\"?>\n<output>\n");
   }

/* Acquire a cursor for the database. */

 if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
    {
    CfOut(cf_error,"","Error reading from checksum database");
    dbp->err(dbp, ret, "DB->cursor");
    return;
    }

 /* Initialize the key/data return pair. */

 memset(&key,0,sizeof(key));
 memset(&value,0,sizeof(value));
 
 /* Walk through the database and print out the key/data pairs. */

 while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
    {
    char type;
    char strtype[CF_MAXVARSIZE];
    char name[CF_BUFSIZE];
    struct Checksum_Value chk_val;
    unsigned char digest[EVP_MAX_MD_SIZE+1];
    
    memset(digest,0,EVP_MAX_MD_SIZE+1);
    memset(&chk_val,0,sizeof(chk_val));
    
    memcpy(&chk_val,value.data,sizeof(chk_val));
    memcpy(digest,chk_val.mess_digest,EVP_MAX_MD_SIZE+1);

    strncpy(strtype,key.data,CF_MAXDIGESTNAMELEN);
    strncpy(name,(char *)key.data+CF_CHKSUMKEYOFFSET,CF_BUFSIZE-1);

    type = ChecksumType(strtype);

    if (XML)
       {
       printf("%s",CFRX[cfx_entry][cfb]);
       printf("%s%s%s",CFRX[cfx_event][cfb],name,CFRX[cfx_event][cfe]);
       printf("%s%s%s",CFRX[cfx_q][cfb],ChecksumPrint(type,digest),CFRX[cfx_q][cfe]);
       printf("%s",CFRX[cfx_entry][cfe]);
       }
    else if (HTML)
       {
       printf("%s",CFRH[cfx_entry][cfb]);
       printf("%s%s%s",CFRH[cfx_filename][cfb],name,CFRH[cfx_filename][cfe]);
       printf("%s%s%s",CFRH[cfx_q][cfb],ChecksumPrint(type,digest),CFRH[cfx_q][cfe]);
       printf("%s",CFRH[cfx_entry][cfe]);         
       }
    else
       {
       printf("%s = ",name);
       printf("%s\n",ChecksumPrint(type,digest));
       /* attr_digest too here*/
       
       memset(&key,0,sizeof(key));
       memset(&value,0,sizeof(value));       
       }
    }

if (HTML)
   {
   printf("</table>");
   }

if (XML)
   {
   printf("</output>\n");
   }

dbcp->c_close(dbcp);
dbp->close(dbp,0);
}

/*********************************************************************/

void ShowLocks (int active)

{ DBT key,value;
  DB *dbp;
  DBC *dbcp;
  DB_ENV *dbenv = NULL;
  int ret;
  char lockdb[CF_BUFSIZE];
  struct LockData entry;

  
snprintf(lockdb,CF_BUFSIZE,"%s/cfengine_lock_db",CFWORKDIR);
  
if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   printf("Couldn't open checksum database %s\n",lockdb);
   perror("db_open");
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,lockdb,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,lockdb,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open checksum database %s\n",lockdb);
   dbp->close(dbp,0);
   return;
   }

if (HTML)
   {
   printf("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/menus.css\" /><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/cf_blue.css\"/></head><body><h1>Current lock database on %s</h1><p><table class=border cellpadding=5 width=800>",VFQNAME);
   }

if (XML)
   {
   printf("<?xml version=\"1.0\"?>\n<output>\n");
   }

/* Acquire a cursor for the database. */

 if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
    {
    CfOut(cf_error,"","Error reading from checksum database");
    dbp->err(dbp, ret, "DB->cursor");
    return;
    }

 /* Initialize the key/data return pair. */

 memset(&key,0,sizeof(key));
 memset(&value,0,sizeof(value));
 
 /* Walk through the database and print out the key/data pairs. */

 while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
    {
    if (active)
       {
       if (strncmp("lock",(char *)key.data,4) == 0)
          {
          if (XML)
             {
             printf("%s",CFRX[cfx_entry][cfb]);
             printf("%s%s%s",CFRX[cfx_filename][cfb],(char *)key.data,CFRX[cfx_filename][cfe]);
             printf("%s%s%s",CFRX[cfx_date][cfb],ctime(&entry.time),CFRX[cfx_date][cfe]);
             printf("%s",CFRX[cfx_entry][cfe]);         
             }
          else if (HTML)
             {
             printf("%s",CFRH[cfx_entry][cfb]);
             printf("%s%s%s",CFRH[cfx_filename][cfb],(char *)key.data,CFRH[cfx_filename][cfe]);
             printf("%s%s%s",CFRH[cfx_date][cfb],ctime(&entry.time),CFRH[cfx_date][cfe]);
             printf("%s",CFRH[cfx_entry][cfe]);         
             }
          else
             {
             printf("%s = ",(char *)key.data);
             
             if (value.data != NULL)
                {
                memcpy(&entry,value.data,sizeof(entry));
                printf("%s\n",ctime(&entry.time));
                }
             }
          }
       }
    else
       {
       if (strncmp("last",(char *)key.data,4) == 0)
          {
          if (XML)
             {
             printf("%s",CFRX[cfx_entry][cfb]);
             printf("%s%s%s",CFRX[cfx_filename][cfb],(char *)key.data,CFRX[cfx_filename][cfe]);
             printf("%s%s%s",CFRX[cfx_date][cfb],ctime(&entry.time),CFRX[cfx_date][cfe]);
             printf("%s",CFRX[cfx_entry][cfe]);         
             }
          else if (HTML)
             {
             printf("%s",CFRH[cfx_entry][cfb]);
             printf("%s%s%s",CFRH[cfx_filename][cfb],(char *)key.data,CFRH[cfx_filename][cfe]);
             printf("%s%s%s",CFRH[cfx_date][cfb],ctime(&entry.time),CFRH[cfx_date][cfe]);
             printf("%s",CFRH[cfx_entry][cfe]);         
             }
          else
             {
             printf("%s = ",(char *)key.data);
             
             if (value.data != NULL)
                {
                memcpy(&entry,value.data,sizeof(entry));
                printf("%s\n",ctime(&entry.time));
                }
             }
          }
       }
    }

if (HTML)
   {
   printf("</table>");
   }

if (XML)
   {
   printf("</output>\n");
   } 

dbcp->c_close(dbcp);
dbp->close(dbp,0);
}

/*******************************************************************/

void ShowCurrentAudit()

{ char operation[CF_BUFSIZE],name[CF_BUFSIZE];
  struct AuditLog entry;
  DB_ENV *dbenv = NULL;
  DBT key,value;
  DB *dbp;
  DBC *dbcp;
  int ret;

  
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_AUDITDB_FILE);

if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   printf("Couldn't open last-seen database %s\n",name);
   perror("db_open");
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open audit database %s\n",name);
   dbp->close(dbp,0);
   return;
   }

/* Acquire a cursor for the database. */

if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   CfOut(cf_error,"","Error reading from last-seen database: ");
   dbp->err(dbp, ret, "DB->cursor");
   return;
   }

 /* Initialize the key/data return pair. */
 
memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));
memset(&entry, 0, sizeof(entry)); 

if (HTML)
   {
   printf("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/menus.css\" /><link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/cf_blue.css\"/></head><body><h1>Audit log %s</h1><p><table class=border cellpadding=2 cellspacing=2>",VFQNAME);
   /* printf("<th> t-index </th>");*/
   printf("<th> Scan convergence </th>");
   printf("<th> Observed </th>");
   printf("<th> Promise made </th>");
   printf("<th> Promise originates in </th>");
   printf("<th> Promise version </th>");
   printf("<th> line </th>");
   }

if (XML)
   {
   printf("<?xml version=\"1.0\"?>\n<output>\n");
   }

 /* Walk through the database and print out the key/data pairs. */

while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
   {
   strncpy(operation,(char *)key.data,CF_BUFSIZE-1);

   if (value.data != NULL)
      {
      memcpy(&entry,value.data,sizeof(entry));
      
      if (XML)
         {
         printf("%s",CFRX[cfx_entry][cfb]);
         printf("%s %s %s",CFRX[cfx_index][cfb],operation,CFRX[cfx_index][cfe]);
         printf("%s %s, ",CFRX[cfx_event][cfb],entry.operator);
         AuditStatusMessage(entry.status);
         printf("%s",CFRX[cfx_event][cfe]);
         printf("%s %s %s",CFRX[cfx_q][cfb],entry.comment,CFRX[cfx_q][cfe]);
         printf("%s %s %s",CFRX[cfx_date][cfb],entry.date,CFRX[cfx_date][cfe]);
         printf("%s %s %s",CFRX[cfx_av][cfb],entry.filename,CFRX[cfx_av][cfe]);
         printf("%s %s %s",CFRX[cfx_version][cfb],entry.version,CFRX[cfx_version][cfe]);
         printf("%s %d %s",CFRX[cfx_ref][cfb],entry.lineno,CFRX[cfx_ref][cfe]);
         printf("%s",CFRX[cfx_entry][cfe]);
         }
      else if (HTML)
         {
         printf("%s",CFRH[cfx_entry][cfb]);
         /* printf("%s %s %s",CFRH[cfx_index][cfb],operation,CFRH[cfx_index][cfe]);*/
         printf("%s %s, ",CFRH[cfx_event][cfb],Format(entry.operator,40));
         AuditStatusMessage(entry.status);
         printf("%s",CFRH[cfx_event][cfe]);
         printf("%s %s %s",CFRH[cfx_q][cfb],Format(entry.comment,40),CFRH[cfx_q][cfe]);
         printf("%s %s %s",CFRH[cfx_date][cfb],entry.date,CFRH[cfx_date][cfe]);
         printf("%s %s %s",CFRH[cfx_av][cfb],entry.filename,CFRH[cfx_av][cfe]);
         printf("%s %s %s",CFRH[cfx_version][cfb],entry.version,CFRH[cfx_version][cfe]);
         printf("%s %d %s",CFRH[cfx_ref][cfb],entry.lineno,CFRH[cfx_ref][cfe]);
         printf("%s",CFRH[cfx_entry][cfe]);

         if (strstr(entry.comment,"closing"))
            {
            printf("<th></th>");
            printf("<th></th>");
            printf("<th></th>");
            printf("<th></th>");
            printf("<th></th>");
            printf("<th></th>");
            printf("<th></th>");
            }
         }
      else
         {
         printf(". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .\n");
         printf("Converge \'%s\' ",entry.operator);
         
         AuditStatusMessage(entry.status); /* Reminder */

         if (strlen(entry.comment) > 0)
            {
            printf("Comment: %s\n",entry.comment);
            }

         if (strcmp(entry.filename,"Terminal") == 0)
            {
            if (strstr(entry.comment,"closing"))
               {
               printf("\n===============================================================================================\n\n");
               }
            }
         else
            {
            if (strlen(entry.version) == 0)
               {
               printf("Promised in %s (unamed version last edited at %s) at/before line %d\n",entry.filename,entry.date,entry.lineno);
               }
            else
               {
               printf("Promised in %s (version %s last edited at %s) at/before line %d\n",entry.filename,entry.version,entry.date,entry.lineno);
               }
            }
         }
      }
   else
      {
      continue;
      }
   }

if (HTML)
   {
   printf("</table>");
   }

if (XML)
   {
   printf("</output>\n");
   }

dbcp->c_close(dbcp);
dbp->close(dbp,0);
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

char *ChecksumDump(unsigned char digest[EVP_MAX_MD_SIZE+1])

{ unsigned int i;
  static char buffer[EVP_MAX_MD_SIZE*4];
  int len = 1;

for (i = 0; buffer[i] != 0; i++)
   {
   len++;
   }

if (len == 16 || len == 20)
   {
   }
else
   {
   len = 16;
   }

switch(len)
   {
   case 20: sprintf(buffer,"SHA=  ");
       break;
   case 16: sprintf(buffer,"MD5=  ");
       break;
   }
  
for (i = 0; i < len; i++)
   {
   sprintf((char *)(buffer+4+2*i),"%02x", digest[i]);
   }

return buffer; 
}    
/*********************************************************************/

char *Format(char *s,int width)

{ static char buffer[CF_BUFSIZE];
  char *sp;
  int i = 0, count = 0;
  
for (sp = s; *sp != '\0'; sp++)
   {
   buffer[i++] = *sp;
   buffer[i] = '\0';
   count++;

   if ((count > width - 5) && ispunct(*sp))
      {
      strcat(buffer,"<br>");
      i += strlen("<br>");
      count = 0;
      }
   }

return buffer;
}

/*************************************************************/

int CompareClasses(const void *a, const void *b)

{
struct CEnt *da = (struct CEnt *) a;
struct CEnt *db = (struct CEnt *) b;

return (da->q < db->q) - (da->q > db->q);
}


/****************************************************************************/

void ReadAverages()

{ int i;
  DBT key,value;

Verbose("\nLooking for database %s\n",FILENAME);
Verbose("\nFinding MAXimum values...\n\n");
Verbose("N.B. socket values are numbers in CLOSE_WAIT. See documentation.\n"); 
  
if ((ERRNO = db_create(&DBP,NULL,0)) != 0)
   {
   Verbose("Couldn't create average database %s\n",FILENAME);
   exit(1);
   }

#ifdef CF_OLD_DB 
if ((ERRNO = (DBP->open)(DBP,FILENAME,NULL,DB_BTREE,DB_RDONLY,0644)) != 0)
#else
if ((ERRNO = (DBP->open)(DBP,NULL,FILENAME,NULL,DB_BTREE,DB_RDONLY,0644)) != 0)    
#endif
   {
   Verbose("Couldn't open average database %s\n",FILENAME);
   DBP->err(DBP,ERRNO,NULL);
   exit(1);
   }

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   MAX.Q[i].var = MAX.Q[i].expect = MAX.Q[i].q = 0.01;
   MIN.Q[i].var = MIN.Q[i].expect = MIN.Q[i].q = 9999.0;
   FPE[i] = FPQ[i] = NULL;
   }
 
for (NOW = CF_MONDAY_MORNING; NOW < CF_MONDAY_MORNING+CF_WEEK; NOW += CF_MEASURE_INTERVAL)
   {
   memset(&key,0,sizeof(key));       
   memset(&value,0,sizeof(value));
   memset(&ENTRY,0,sizeof(ENTRY));

   strcpy(TIMEKEY,GenTimeKey(NOW));

   key.data = TIMEKEY;
   key.size = strlen(TIMEKEY)+1;
   
   if ((ERRNO = DBP->get(DBP,NULL,&key,&value,0)) != 0)
      {
      if (ERRNO != DB_NOTFOUND)
         {
         DBP->err(DBP,ERRNO,NULL);
         exit(1);
         }
      }
   
   if (value.data != NULL)
      {
      memcpy(&ENTRY,value.data,sizeof(ENTRY));
      
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         if (fabs(ENTRY.Q[i].expect) > MAX.Q[i].expect)
            {
            MAX.Q[i].expect = fabs(ENTRY.Q[i].expect);
            }

         if (fabs(ENTRY.Q[i].q) > MAX.Q[i].q)
            {
            MAX.Q[i].q = fabs(ENTRY.Q[i].q);
            }

         if (fabs(ENTRY.Q[i].expect) < MIN.Q[i].expect)
            {
            MIN.Q[i].expect = fabs(ENTRY.Q[i].expect);
            }
         
         if (fabs(ENTRY.Q[i].q) < MIN.Q[i].q)
            {
            MIN.Q[i].q = fabs(ENTRY.Q[i].q);
            }
         }
      }
   }
 
 DBP->close(DBP,0);
}

/****************************************************************************/

void EraseAverages()

{ int i;
  DBT key,value;
  struct Item *list = NULL;
      
Verbose("\nLooking through current database %s\n",FILENAME);

list = SplitStringAsItemList(ERASE,',');

if ((ERRNO = db_create(&DBP,NULL,0)) != 0)
   {
   Verbose("Couldn't create average database %s\n",FILENAME);
   exit(1);
   }

#ifdef CF_OLD_DB 
if ((ERRNO = (DBP->open)(DBP,FILENAME,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((ERRNO = (DBP->open)(DBP,NULL,FILENAME,NULL,DB_BTREE,DB_CREATE,0644)) != 0)    
#endif
   {
   Verbose("Couldn't open average database %s\n",FILENAME);
   DBP->err(DBP,ERRNO,NULL);
   exit(1);
   }

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   FPE[i] = FPQ[i] = NULL;
   }
 
for (NOW = CF_MONDAY_MORNING; NOW < CF_MONDAY_MORNING+CF_WEEK; NOW += CF_MEASURE_INTERVAL)
   {
   memset(&key,0,sizeof(key));       
   memset(&value,0,sizeof(value));
   memset(&ENTRY,0,sizeof(ENTRY));

   strcpy(TIMEKEY,GenTimeKey(NOW));

   key.data = TIMEKEY;
   key.size = strlen(TIMEKEY)+1;
   
   if ((ERRNO = DBP->get(DBP,NULL,&key,&value,0)) != 0)
      {
      if (ERRNO != DB_NOTFOUND)
         {
         DBP->err(DBP,ERRNO,NULL);
         exit(1);
         }
      }
   
   if (value.data != NULL)
      {
      memcpy(&ENTRY,value.data,sizeof(ENTRY));
      
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         if (IsItemIn(list,OBS[i][0]))
            {
            /* Set history but not most recent to zero */
            ENTRY.Q[i].expect = 0;
            ENTRY.Q[i].var = 0;
            }
         }

      value.data = &ENTRY;
      
      if ((ERRNO = DBP->put(DBP,NULL,&key,&value,0)) != 0)
         {
         DBP->err(DBP,ERRNO,NULL);
         exit(1);
         }
      }
   }
 
DBP->close(DBP,0);
}

/*****************************************************************************/

void SummarizeAverages()

{ int i;
  DBT key,value;

Verbose(" x  yN (Variable content)\n---------------------------------------------------------\n");

 for (i = 0; i < CF_OBSERVABLES; i++)
   {
   Verbose("%2d. MAX <%-10s-in>   = %10f - %10f u %10f\n",i,OBS[i][0],MIN.Q[i].expect,MAX.Q[i].expect,sqrt(MAX.Q[i].var));
   }
 
if ((ERRNO = db_create(&DBP,NULL,0)) != 0)
   {
   Verbose("Couldn't open average database %s\n",FILENAME);
   exit(1);
   }

#ifdef CF_OLD_DB 
if ((ERRNO = (DBP->open)(DBP,FILENAME,NULL,DB_BTREE,DB_RDONLY,0644)) != 0)
#else
if ((ERRNO = (DBP->open)(DBP,NULL,FILENAME,NULL,DB_BTREE,DB_RDONLY,0644)) != 0)
#endif
   {
   Verbose("Couldn't open average database %s\n",FILENAME);
   exit(1);
   }

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = "DATABASE_AGE";
key.size = strlen("DATABASE_AGE")+1;

if ((ERRNO = DBP->get(DBP,NULL,&key,&value,0)) != 0)
   {
   if (ERRNO != DB_NOTFOUND)
      {
      DBP->err(DBP,ERRNO,NULL);
      exit(1);
      }
   }
 
if (value.data != NULL)
   {
   AGE = *(double *)(value.data);
   Verbose("\n\nDATABASE_AGE %.1f (weeks)\n\n",AGE/CF_WEEK*CF_MEASURE_INTERVAL);
   }
}

/*****************************************************************************/

void WriteGraphFiles()

{ int its,i,j,k, count = 0;
  DBT key,value;
  struct stat statbuf;

OpenFiles();

if (TITLES)
   {
   for (i = 0; i < CF_OBSERVABLES; i+=2)
      {
      fprintf(FPAV,"# Column %d: %s\n",i,OBS[i][0]);
      fprintf(FPVAR,"# Column %d: %s\n",i,OBS[i][0]);
      fprintf(FPNOW,"# Column %d: %s\n",i,OBS[i][0]);
      }

   fprintf(FPAV,"##############################################\n");
   fprintf(FPVAR,"##############################################\n");
   fprintf(FPNOW,"##############################################\n");
   }

if (HIRES)
   {
   its = 1;
   }
else
   {
   its = 12;
   }

NOW = CF_MONDAY_MORNING;
memset(&ENTRY,0,sizeof(ENTRY)); 
 
while (NOW < CF_MONDAY_MORNING+CF_WEEK)
   {
   for (j = 0; j < its; j++)
      {
      memset(&key,0,sizeof(key));       
      memset(&value,0,sizeof(value));
      
      strcpy(TIMEKEY,GenTimeKey(NOW));
      
      key.data = TIMEKEY;
      key.size = strlen(TIMEKEY)+1;

      if ((ERRNO = DBP->get(DBP,NULL,&key,&value,0)) != 0)
         {
         if (ERRNO != DB_NOTFOUND)
            {
            DBP->err(DBP,ERRNO,NULL);
            exit(1);
            }
         }

      /* Work out local average over grain size "its" */
      
      if (value.data != NULL)
         {
         memcpy(&DET,value.data,sizeof(DET));
         
         for (i = 0; i < CF_OBSERVABLES; i++)
            {
            ENTRY.Q[i].expect += DET.Q[i].expect/(double)its;
            ENTRY.Q[i].var += DET.Q[i].var/(double)its;
            ENTRY.Q[i].q += DET.Q[i].q/(double)its;
            }         
         
         if (NOSCALING)
            {            
            for (i = 1; i < CF_OBSERVABLES; i++)
               {
               MAX.Q[i].expect = 1;
               MAX.Q[i].q = 1;
               }
            }
         }
      
      NOW += CF_MEASURE_INTERVAL;
      count++;
      }

   /* Output the data in a plethora of files */
   
   fprintf(FPAV,"%d ",count);
   fprintf(FPVAR,"%d ",count);
   fprintf(FPNOW,"%d ",count);

   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      fprintf(FPAV,"%f ",ENTRY.Q[i].expect/MAX.Q[i].expect);
      fprintf(FPVAR,"%f ",ENTRY.Q[i].var/MAX.Q[i].var);
      fprintf(FPNOW,"%f ",ENTRY.Q[i].q/MAX.Q[i].q);
      }                        
   
   fprintf(FPAV,"\n");
   fprintf(FPVAR,"\n");
   fprintf(FPNOW,"\n");
   
   if (SEPARATE)
      {
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         fprintf(FPE[i],"%d %f %f\n",count, ENTRY.Q[i].expect, sqrt(ENTRY.Q[i].var));
         /* Use same scaling for Q so graphs can be merged */
         fprintf(FPQ[i],"%d %f 0.0\n",count, ENTRY.Q[i].q);
         }               
      }
   
   memset(&ENTRY,0,sizeof(ENTRY));
   }

DBP->close(DBP,0);

CloseFiles();
}

/*****************************************************************************/

void MagnifyNow()

{ int its,i,j,k, count = 0;
  DBT key,value;
  time_t now;

OpenMagnifyFiles();

its = 1; /* detailed view */

now = time(NULL);
NOW = now - (time_t)(4 * CF_TICKS_PER_HOUR);
 
while (NOW < now)
   {
   memset(&ENTRY,0,sizeof(ENTRY)); 

   for (j = 0; j < its; j++)
      {
      memset(&key,0,sizeof(key));       
      memset(&value,0,sizeof(value));
      
      strcpy(TIMEKEY,GenTimeKey(NOW));
      
      key.data = TIMEKEY;
      key.size = strlen(TIMEKEY)+1;

      if ((ERRNO = DBP->get(DBP,NULL,&key,&value,0)) != 0)
         {
         if (ERRNO != DB_NOTFOUND)
            {
            DBP->err(DBP,ERRNO,NULL);
            exit(1);
            }
         }

      /* Work out local average over grain size "its" */
      
      if (value.data != NULL)
         {
         memcpy(&DET,value.data,sizeof(DET));
         
         for (i = 0; i < CF_OBSERVABLES; i++)
            {
            ENTRY.Q[i].expect += DET.Q[i].expect/(double)its;
            ENTRY.Q[i].var += DET.Q[i].var/(double)its;
            ENTRY.Q[i].q += DET.Q[i].q/(double)its;
            }         
         
         if (NOSCALING)
            {            
            for (i = 1; i < CF_OBSERVABLES; i++)
               {
               MAX.Q[i].expect = 1;
               MAX.Q[i].q = 1;
               }
            }
         }
      
      NOW += CF_MEASURE_INTERVAL;
      count++;
      }

   /* Output q and E/sig data in a plethora of files */

   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      fprintf(FPM[i],"%d %f %f %f\n",count, ENTRY.Q[i].expect, sqrt(ENTRY.Q[i].var),ENTRY.Q[i].q);
      }               
   }

DBP->close(DBP,0);
CloseMagnifyFiles();
}

/*****************************************************************************/

void WriteHistograms()

{ int i,j,k;
 
/* Finally, look at the histograms */
 
 for (i = 0; i < 7; i++)
    {
    for (j = 0; j < CF_OBSERVABLES; j++)
       {
       for (k = 0; k < CF_GRAINS; k++)
          {
          HISTOGRAM[j][i][k] = 0;
          }
       }
    }
 
 if (SEPARATE)
    {
    int position,day;
    int weekly[CF_OBSERVABLES][CF_GRAINS];
    FILE *fp;
    
    snprintf(FLNAME,CF_BUFSIZE,"%s/state/histograms",CFWORKDIR);
    
    if ((fp = fopen(FLNAME,"r")) == NULL)
       {
       Verbose("Unable to load histogram data\n");
       exit(1);
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
          
          weekly[i][position] = 0;
          }
       }
    
    fclose(fp);
    
    if (!HIRES)
       {
       /* Smooth daily and weekly histograms */
       for (k = 1; k < CF_GRAINS-1; k++)
          {
          for (j = 0; j < CF_OBSERVABLES; j++)
             {
             for (i = 0; i < 7; i++)  
                {
                SMOOTHHISTOGRAM[j][i][k] = ((double)(HISTOGRAM[j][i][k-1] + HISTOGRAM[j][i][k] + HISTOGRAM[j][i][k+1]))/3.0;
                }
             }
          }
       }
    else
       {
       for (k = 1; k < CF_GRAINS-1; k++)
          {
          for (j = 0; j < CF_OBSERVABLES; j++)
             {
             for (i = 0; i < 7; i++)  
                {
                SMOOTHHISTOGRAM[j][i][k] = (double) HISTOGRAM[j][i][k];
                }
             }
          }
       }
    
    
    for (i = 0; i < CF_OBSERVABLES; i++)
       {
       sprintf(FLNAME,"%s.distr",OBS[i][0]); 
       if ((FPQ[i] = fopen(FLNAME,"w")) == NULL)
          {
          perror("fopen");
          exit(1);
          }
       }
    
    /* Plot daily and weekly histograms */
    for (k = 0; k < CF_GRAINS; k++)
       {
       int a;
       
       for (j = 0; j < CF_OBSERVABLES; j++)
          {
          for (i = 0; i < 7; i++)  
             {
             weekly[j][k] += (int) (SMOOTHHISTOGRAM[j][i][k]+0.5);
             }
          }
       
       for (a = 0; a < CF_OBSERVABLES; a++)
          {
          fprintf(FPQ[a],"%d %d\n",k,weekly[a][k]);
          }
       }
    
    for (i = 0; i < CF_OBSERVABLES; i++)
       {
       fclose(FPQ[i]);
       }
    }
}


/*****************************************************************************/

void DiskArrivals(void)

{ DIR *dirh;
  FILE *fp; 
  struct dirent *dirp;
  int count = 0, index = 0, i;
  char filename[CF_BUFSIZE],database[CF_BUFSIZE];
  double val, maxval = 1.0, *array, grain = 0.0;
  time_t now;
  DBT key,value;
  DB *dbp = NULL;
  DB_ENV *dbenv = NULL;


if ((array = (double *)malloc((int)CF_WEEK)) == NULL)
   {
   CfOut(cf_error,"MALLOC","Memory error");
   return;
   }
  
if ((dirh = opendir(CFWORKDIR)) == NULL)
   {
   CfOut(cf_error,"opendir","Can't open directory %s\n",CFWORKDIR);
   return;
   }

Verbose("\n\nLooking for filesystem arrival process data in %s\n",CFWORKDIR); 

for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
   {
   if (strncmp(dirp->d_name,"scan:",5) == 0)
      {
      Verbose("Found %s - generating X,Y plot\n",dirp->d_name);

      snprintf(database,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,dirp->d_name);
      
      if ((ERRNO = db_create(&dbp,dbenv,0)) != 0)
         {
         Verbose("Couldn't open arrivals database %s\n",database);
         return;
         }
      
#ifdef CF_OLD_DB
      if ((ERRNO = (dbp->open)(dbp,database,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
      if ((ERRNO = (dbp->open)(dbp,NULL,database,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
         {
         Verbose("Couldn't open database %s\n",database);
         dbp->close(dbp,0);
         continue;
         }
      
      maxval = 1.0;
      grain = 0.0;
      count = 0.0;
      index = 0;
      
      for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
         {
         memset(&key,0,sizeof(key));       
         memset(&value,0,sizeof(value));
         
         strcpy(TIMEKEY,GenTimeKey(now));
         
         key.data = TIMEKEY;
         key.size = strlen(TIMEKEY)+1;
         
         if ((ERRNO = dbp->get(dbp,NULL,&key,&value,0)) != 0)
            {
            if (ERRNO != DB_NOTFOUND)
               {
               DBP->err(DBP,ERRNO,NULL);
               exit(1);
               }
            }
         
         if (value.data != NULL)
            {
            grain += (double)*(double *)(value.data);
            }
         else
            {
            grain = 0;
            }
         
         if (HIRES)
            {
            if (grain > maxval)
               {
               maxval = grain;
               }
            
            array[index] = grain;
            grain = 0.0;     
            index++;
            }
         else
            {
            if (count % 12 == 0)
               {
               if (grain > maxval)
                  {
                  maxval = grain;
                  }
               array[index] = grain;
               index++;
               grain = 0.0;
               }
            }            
         count++;
         }
      
      dbp->close(dbp,0);
      
      snprintf(filename,CF_BUFSIZE-1,"%s.cfenv",dirp->d_name);
      
      if ((fp = fopen(filename,"w")) == NULL)
         {
         Verbose("Unable to open %s for writing\n",filename);
         perror("fopen");
         return;
         }
      
      Verbose("Data points = %d\n",index);
      
      for (i = 0; i < index; i++)
         {
         if (i > 1 && i < index-1)
            {
            val = (array[i-1]+array[i]+array[i+1])/3.0;  /* Smoothing */
            }
         else
            {
            val = array[i];
            }

         fprintf(fp,"%d %f\n",i,val/maxval*50.0);
         }
      
      fclose(fp);      
      }
   }
 
closedir(dirh);
}

/***************************************************************/

void PeerIntermittency()

{ DBT key,value;
  DB *dbp,*dbpent;
  DBC *dbcp;
  DB_ENV *dbenv = NULL, *dbenv2 = NULL;
  int i,ret;
  FILE *fp1,*fp2;
  char name[CF_BUFSIZE],hostname[CF_BUFSIZE],timekey[CF_MAXVARSIZE];
  char out1[CF_BUFSIZE],out2[CF_BUFSIZE];
  struct QPoint entry;
  struct Item *ip, *hostlist = NULL;
  double entropy,average,var,sum,sum_av;
  time_t now = time(NULL), then, lastseen = CF_WEEK;

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);

average = (double) CF_HOUR;  /* It will take a week for a host to be deemed reliable */
var = 0;

if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   Verbose("Couldn't open last-seen database %s\n",name);
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   Verbose("Couldn't open last-seen database %s\n",name);
   dbp->close(dbp,0);
   return;
   }

if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   Verbose("Error reading from last-seen database\n");
   dbp->err(dbp, ret, "DB->cursor");
   return;
   }

memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));

Verbose("Examining known peers...\n");

while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
   {
   strcpy(hostname,IPString2Hostname((char *)key.data+1));

   if (!IsItemIn(hostlist,hostname))
      {
      /* Check hostname not recorded twice with +/- */
      AppendItem(&hostlist,hostname,NULL);
      Verbose("Examining intermittent host %s\n",hostname);
      }
   }

dbcp->c_close(dbcp);
dbp->close(dbp,0);


/* Now go through each host and recompute entropy */

for (ip = hostlist; ip != NULL; ip=ip->next)
   {
   snprintf(out1,CF_BUFSIZE,"lastseen-%s.q",ip->name);

   Verbose("Opening %s\n",out1);
   
   if ((fp1 = fopen(out1,"w")) == NULL)
      {
      Verbose("Unable to open %s\n",out1);
      continue;
      }

   snprintf(out2,CF_BUFSIZE,"lastseen-%s.E-sigma",hostname);
   if ((fp2 = fopen(out2,"w")) == NULL)
      {
      Verbose("Unable to open %s\n",out1);
      continue;
      }
   
   snprintf(name,CF_BUFSIZE-1,"%s/%s.%s",CFWORKDIR,CF_LASTDB_FILE,ip->name);
   Verbose("Consulting profile %s\n",name);

   if ((errno = db_create(&dbpent,dbenv2,0)) != 0)
      {
      Verbose("Couldn't init reliability profile database %s\n",name);
      return;
      }
   
#ifdef CF_OLD_DB
   if ((errno = (dbpent->open)(dbpent,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
   if ((errno = (dbpent->open)(dbpent,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
      {
      Verbose("Couldn't open last-seen database %s\n",name);
      continue;
      }

   for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
      {
      memset(&key,0,sizeof(key));       
      memset(&value,0,sizeof(value));
      
      strcpy(timekey,GenTimeKey(now));
      
      key.data = timekey;
      key.size = strlen(timekey)+1;

      if ((errno = dbpent->get(dbpent,NULL,&key,&value,0)) != 0)
         {
         if (errno != DB_NOTFOUND)
            {
            dbpent->err(dbp,errno,NULL);
            exit(1);
            }
         }
      
      if (value.data != NULL)
         {
         memcpy(&entry,value.data,sizeof(entry));
         then = (time_t)entry.q;
         lastseen = now - then;
         if (lastseen < 0)
            {
            lastseen = 0; /* Never seen before, so pretend */
            }
         average = (double)entry.expect;
         var = (double)entry.var;

         fprintf(fp2,"%d %lf %lf\n",now,average,sqrt(var));
         }
      else
         {
         /* If we have no data, it means no contact for whatever reason.
            It could be unable to respond unwilling to respond, policy etc.
            Assume for argument that we expect regular responses ... */
         
         lastseen += CF_MEASURE_INTERVAL; /* infer based on no data */
         }

      fprintf(fp1,"%d %d\n",now,lastseen);
      }

   fclose(fp1);
   fclose(fp2);
   dbpent->close(dbpent,0);
   }

DeleteItemList(hostlist);
}


/*********************************************************************/

void OpenFiles()

{ int i;
 
sprintf(FLNAME,"cfenv-average");

if ((FPAV = fopen(FLNAME,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","File %s could not be opened for writing\n",FLNAME);
   exit(1);
   }

sprintf(FLNAME,"cfenv-stddev"); 

if ((FPVAR = fopen(FLNAME,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","File %s could not be opened for writing\n",FLNAME);
   exit(1);
   }

sprintf(FLNAME,"cfenv-now"); 

if ((FPNOW = fopen(FLNAME,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","File %s could not be opened for writing\n",FLNAME);
   exit(1);
   }

/* Now if -s open a file foreach metric! */

if (SEPARATE)
   {
   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      sprintf(FLNAME,"%s.E-sigma",OBS[i][0]);
      
      if ((FPE[i] = fopen(FLNAME,"w")) == NULL)
         {
         CfOut(cf_error,"fopen","File %s could not be opened for writing\n",FLNAME);
         exit(1);
         }
      
      sprintf(FLNAME,"%s.q",OBS[i][0]);
      
      if ((FPQ[i] = fopen(FLNAME,"w")) == NULL)
         {
         CfOut(cf_error,"fopen","File %s could not be opened for writing\n",FLNAME);
         exit(1);
         }

      }
   }
}

/*********************************************************************/

void CloseFiles()

{ int i;
 
fclose(FPAV);
fclose(FPVAR);
fclose(FPNOW); 

if (SEPARATE)
   {
   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      fclose(FPE[i]);
      fclose(FPQ[i]);
      }
   }
}

/*********************************************************************/

void OpenMagnifyFiles()

{ int i;
 
for (i = 0; i < CF_OBSERVABLES; i++)
   {
   sprintf(FLNAME,"%s.mag",OBS[i][0]);
   
   if ((FPM[i] = fopen(FLNAME,"w")) == NULL)
      {
      perror("fopen");
      exit(1);
      }
   }
}

/*********************************************************************/

void CloseMagnifyFiles()

{ int i;

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   fclose(FPM[i]);
   }
}











/* EOF */




