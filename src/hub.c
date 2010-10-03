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
/* File: hub.c                                                               */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);

#ifdef NT
#include <process.h>
#endif

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

int  NO_FORK = false;
int  CONTINUOUS = false;
char MAILTO[CF_BUFSIZE];
char MAILFROM[CF_BUFSIZE];
char EXECCOMMAND[CF_BUFSIZE];
char VMAILSERVER[CF_BUFSIZE];
struct Item *SCHEDULE = NULL;

pid_t MYTWIN = 0;
int MAXLINES = 30;
int SPLAYTIME = 0;
const int INF_LINES = -2;
int NOSPLAY = false;
int NOWINSERVICE = false;
int THREADS = 0;
int CFH_ZENOSS = false;

extern struct BodySyntax CFEX_CONTROLBODY[];

/*******************************************************************/

void StartHub(int argc,char **argv);
int ScheduleRun(void);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The hub is a scheduler and aggregator for the CFDB knowledge\n"
            "repository. It automatically schedules updates from clients\n"
            "that have registered by previous connection.";

 
 struct option OPTIONS[15] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n'},
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f'},
      { "no-lock",no_argument,0,'K'},
      { "no-fork",no_argument,0,'F' },
      { "continuous",no_argument,0,'c' },
      { "logging",no_argument,0,'l' },
      { NULL,0,0,'\0' }
      };

 char *HINTS[15] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Output the version of the software",
      "Specify an alternative input file than the default",
      "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
      "Run as a foreground processes (do not fork)",
      "Continuous update mode of operation",
      "Enable logging of updates to the promise log",
      NULL
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv);
GenericInitialize(argc,argv,"hub");
ThisAgentInit();
KeepPromises();

#ifdef MINGW
CfOut(cf_error,"","This service is not available on Windows.");
#else
StartHub(argc,argv);
#endif

return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  char arg[CF_BUFSIZE];
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

while ((c=getopt_long(argc,argv,"cd:vKf:VhFlM",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          if (optarg && strlen(optarg) < 5)
             {
             snprintf(arg,CF_MAXVARSIZE," -f used but argument \"%s\" incorrect",optarg);
             FatalError(arg);
             }

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'l':
          LOGGING = true;
          break;
          
      case 'd': 
          NewClass("opt_debug");
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
             default:
                 DEBUG = true;
                 break;
             }
          break;
          
      case 'K': IGNORELOCK = true;
          break;
                    
      case 'I': INFORM = true;
          break;
          
      case 'v':
          VERBOSE = true;
          NO_FORK = true;
          break;
	            
      case 'n': DONTDO = true;
          IGNORELOCK = true;
          NewClass("opt_dry_run");
          break;

      case 'c':
          CONTINUOUS = true;
          break;
          
      case 'F':
          NO_FORK = true;
          break;

      case 'V': Version("cf-hub");
          exit(0);
          
      case 'h': Syntax("cf-hub - cfengine's report aggregator",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-hub - cfengine's report aggregator",OPTIONS,HINTS,ID);
          exit(0);
          
      default: Syntax("cf-hub - cfengine's report aggregator",OPTIONS,HINTS,ID);
          exit(1);
          
      }
   }

if (argv[optind] != NULL)
   {
   CfOut(cf_error,"","Unexpected argument with no preceding option: %s\n",argv[optind]);
   }
}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];

umask(077);

if (CONTINUOUS)
   {
   AppendItem(&SCHEDULE,"any",NULL);
   }
else if (SCHEDULE == NULL)
   {
   AppendItem(&SCHEDULE,"Min00",NULL);
   AppendItem(&SCHEDULE,"Min05",NULL);
   AppendItem(&SCHEDULE,"Min10",NULL);
   AppendItem(&SCHEDULE,"Min15",NULL);
   AppendItem(&SCHEDULE,"Min20",NULL);
   AppendItem(&SCHEDULE,"Min25",NULL);   
   AppendItem(&SCHEDULE,"Min30",NULL);
   AppendItem(&SCHEDULE,"Min35",NULL);
   AppendItem(&SCHEDULE,"Min45",NULL);
   AppendItem(&SCHEDULE,"Min50",NULL);
   AppendItem(&SCHEDULE,"Min55",NULL);
   }
}

/*****************************************************************************/

void KeepPromises()

{ struct Constraint *cp;
  char rettype,splay[CF_BUFSIZE];
  void *retval;

for (cp = ControlBodyConstraints(cf_hub); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }
   
   if (GetVariable("control_hub",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in hub control body",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFH_CONTROLBODY[cfh_schedule].lval) == 0)
      {
      struct Rlist *rp;
      Debug("schedule ...\n");
      DeleteItemList(SCHEDULE);
      SCHEDULE = NULL;
      
      for (rp  = (struct Rlist *) retval; rp != NULL; rp = rp->next)
         {
         if (!IsItemIn(SCHEDULE,rp->item))
            {
            AppendItem(&SCHEDULE,rp->item,NULL);
            }
         }
      }

   if (strcmp(cp->lval,CFH_CONTROLBODY[cfh_export_zenoss].lval) == 0)
      {
      CFH_ZENOSS = GetBoolean(retval);
      CfOut(cf_verbose,"","SET export_zenoss = %d\n",CFH_ZENOSS);
      continue;
      }

   }
}

/*****************************************************************************/

#ifndef MINGW

void StartHub(int argc,char **argv)

{
#ifdef HAVE_LIBCFNOVA
Nova_StartHub(argc,argv);
#else
CfOut(cf_error,"","This component is only used in commercial editions of the Cfengine software");
#endif
}

#endif  /* NOT MINGW */

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ScheduleRun()

{ time_t now;
  char timekey[64];
  struct Item *ip;
  
now = time(NULL);

if (EnterpriseExpiry(LIC_DAY,LIC_MONTH,LIC_YEAR,LIC_COMPANY)) 
  {
  CfOut(cf_error,"","Cfengine - autonomous configuration engine. This enterprise license is invalid.\n");
  exit(1);
  }

ThreadLock(cft_system);
DeleteItemList(VHEAP);
VHEAP = NULL;
DeleteItemList(VADDCLASSES);
VADDCLASSES = NULL;
DeleteItemList(IPADDRESSES);
IPADDRESSES = NULL;
DeleteScope("this");
DeleteScope("mon");
DeleteScope("sys");
DeleteScope("match");
CfGetInterfaceInfo(cf_executor);
Get3Environment();
OSClasses();
SetReferenceTime(true);
snprintf(timekey,63,"%s",cf_ctime(&now)); 
AddTimeClass(timekey); 
ThreadUnlock(cft_system);

for (ip = SCHEDULE; ip != NULL; ip = ip->next)
   {
   Debug("Checking schedule %s...\n",ip->name);

   if (IsDefinedClass(ip->name))
      {
      CfOut(cf_verbose,"","Waking up the agent at %s ~ %s \n",timekey,ip->name);
      return true;
      }
   }

return false;
}


/* EOF */


