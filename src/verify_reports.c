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
/* File: verify_reports.c                                                    */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static void FriendStatus(struct Attributes a,struct Promise *pp);
static void VerifyFriendReliability(struct Attributes a,struct Promise *pp);
static void VerifyFriendConnections(int hours,struct Attributes a,struct Promise *pp);
static void ShowState(char *type,struct Attributes a,struct Promise *pp);
static void PrintFile(struct Attributes a,struct Promise *pp);

/*******************************************************************/
/* Agent reporting                                                 */
/*******************************************************************/

void VerifyReportPromise(struct Promise *pp)

{ struct Attributes a = {{0}};
  struct CfLock thislock;
  struct Rlist *rp;
  char unique_name[CF_EXPANDSIZE];

a = GetReportsAttributes(pp);

if (strcmp(pp->classes,"any") == 0)
   {
   CfOut(cf_verbose,""," --> Reports promises may not be in class \"any\"");
   return;
   }

snprintf(unique_name,CF_EXPANDSIZE-1,"%s_%d",pp->promiser,pp->lineno);
thislock = AcquireLock(unique_name,VUQNAME,CFSTARTTIME,a,pp,false);

if (thislock.lock == NULL)
   {
   return;
   }

PromiseBanner(pp);

cfPS(cf_verbose,CF_CHG,"",pp,a,"Report: %s", pp->promiser);

if (a.report.to_file)
   {
   CfFOut(a.report.to_file,cf_error,"","%s",pp->promiser);
   }
else
   {
   CfOut(cf_reporting,"","R: %s",pp->promiser);
   }

if (a.report.haveprintfile)
   {
   PrintFile(a,pp);
   }

if (a.report.showstate)
   {
   for (rp = a.report.showstate; rp != NULL; rp=rp->next)
      {
      ShowState(rp->item,a,pp);
      }
   }

if (a.report.havelastseen)
   {
   FriendStatus(a,pp);
   }
    
YieldCurrentLock(thislock);
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static void PrintFile(struct Attributes a,struct Promise *pp)

{ FILE *fp;
  char buffer[CF_BUFSIZE];
  int lines = 0;

if (a.report.filename == NULL)
   {
   CfOut(cf_verbose,"","Printfile promise was incomplete, with no filename.\n");
   return;
   }
  
if ((fp = fopen(a.report.filename,"r")) == NULL)
   {
   cfPS(cf_error,CF_INTERPT,"fopen",pp,a," !! Printing of file %s was not possible.\n",a.report.filename);
   return;
   }

while (!feof(fp) && (lines < a.report.numlines))
   {
   buffer[0] = '\0';
   fgets(buffer,CF_BUFSIZE,fp);
   CfOut(cf_error,"","R: %s",buffer);
   lines++;
   }

fclose(fp);
}


/*********************************************************************/

static void ShowState(char *type,struct Attributes a,struct Promise *pp)

{ struct stat statbuf;
  char buffer[CF_BUFSIZE],vbuff[CF_BUFSIZE],assemble[CF_BUFSIZE];
  struct Item *addresses = NULL,*saddresses = NULL,*ip;
  int i = 0, tot=0, min_signal_diversity = 1,conns=1;
  int maxlen = 0,count;
  double *dist = NULL, S = 0.0;
  char *offset = NULL;
  FILE *fp;

Debug("ShowState(%s)\n",type); 

snprintf(buffer,CF_BUFSIZE-1,"%s/state/cf_%s",CFWORKDIR,type);

if (cfstat(buffer,&statbuf) == 0)
   {
   if ((fp = fopen(buffer,"r")) == NULL)
      {
      CfOut(cf_inform,"fopen","Could not open state memory %s\n",buffer);
      return;
      }

   while(!feof(fp))
      {
      char local[CF_BUFSIZE],remote[CF_BUFSIZE];
      buffer[0] = local[0] = remote[0] = '\0';

      memset(vbuff,0,CF_BUFSIZE);
      fgets(buffer,CF_BUFSIZE,fp);

      if (strlen(buffer) > 0)
         {
         CfOut(cf_verbose,"","(%2d) %s",conns,buffer);
         
         if (IsSocketType(type))
            {
            if (strncmp(type,"incoming",8) == 0 || strncmp(type,"outgoing",8) == 0)
               {
               if (strncmp(buffer,"tcp",3) == 0)
                  {
                  sscanf(buffer,"%*s %*s %*s %s %s",local,remote); /* linux-like */
                  }
               else
                  {
                  sscanf(buffer,"%s %s",local,remote);             /* solaris-like */
                  }
               
               strncpy(vbuff,remote,CF_BUFSIZE-1);
               DePort(vbuff);
               }
            }
         else if (IsTCPType(type))
            {
            count = 1;
            sscanf(buffer,"%d %[^\n]",&count,remote);
            AppendItem(&addresses,remote,"");
            SetItemListCounter(addresses,remote,count);
            conns += count;
            continue;
            }
         else      
            {
            /* If we get here this is a process thing */
            if (offset == NULL)
               {
               if ((offset = strstr(buffer,"CMD")))
                  {
                  }
               else if ((offset = strstr(buffer,"COMMAND")))
                  {
                  }
               
               if (offset == NULL)
                  {
                  continue;
                  }
               }
            
            strncpy(vbuff,offset,CF_BUFSIZE-1);
            Chop(vbuff);
            }
         
         if (!IsItemIn(addresses,vbuff))
            {
            conns++;
            AppendItem(&addresses,vbuff,"");
            IncrementItemListCounter(addresses,vbuff);
            }
         else
            {
            conns++;    
            IncrementItemListCounter(addresses,vbuff);
            }
         }
      }
   
   fclose(fp);
   conns--;

   CfOut(cf_error,"","\n");
   CfOut(cf_error,"","R: The peak measured state was q = %d:\n",conns);

   if (IsSocketType(type)||IsTCPType(type))
      {
      if (addresses != NULL)
         {
         cfPS(cf_error,CF_CHG,"",pp,a," {\n");
         }
      
      for (ip = addresses; ip != NULL; ip=ip->next)
         {
         tot+=ip->counter;
         
         buffer[0] = '\0';
         sscanf(ip->name,"%s",buffer);
         
         if (!IsIPV4Address(buffer) && !IsIPV6Address(buffer))
            {
            CfOut(cf_verbose,"","Rejecting address %s\n",ip->name);
            continue;
            }

         CfOut(cf_error,"","R: DNS key: %s = %s (%d/%d)\n",buffer,IPString2Hostname(buffer),ip->counter,conns);
         
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
      for (ip = addresses; ip != NULL; ip=ip->next)
         {
         tot+=ip->counter;
         }
      }

   saddresses = SortItemListCounters(addresses);

   for (ip = saddresses; ip != NULL; ip=ip->next)
      {
      int s;
      
      if (maxlen > 17) /* ipv6 */
         {
         snprintf(assemble,CF_BUFSIZE,"Frequency: %-40s|",ip->name);
         }
      else
         {
         snprintf(assemble,CF_BUFSIZE,"Frequency: %-17s|",ip->name);
         }
      
      for (s = 0; (s < ip->counter) && (s < 50); s++)
         {
         if (s < 48)
            {
            strcat(assemble,"*");
            }
         else
            {
            strcat(assemble,"+");
            }
         }
      
      CfOut(cf_error,"","R: %s \t(%d/%d)\n",assemble,ip->counter,conns);
      }
   
   dist = (double *) malloc((tot+1)*sizeof(double));
   
   if (conns > min_signal_diversity)
      {
      for (i = 0,ip = addresses; ip != NULL; i++,ip=ip->next)
         {
         dist[i] = ((double)(ip->counter))/((double)tot);
         
         S -= dist[i]*log(dist[i]);
         }
      
      CfOut(cf_error,"","R: Variability/entropy of addresses = %.1f %%\n",S/log((double)tot)*100.0);
      CfOut(cf_error,"","R: (Entropy = 0 for single source, 100 for flatly distributed source)\n -\n");
      }
   
   CfOut(cf_error,"","\n");
   CfOut(cf_error,"","R: State of %s peaked at %s\n",type,cf_ctime(&statbuf.st_mtime));
   }
else 
   {
   CfOut(cf_inform,"","R: State parameter %s is not known or recorded\n",type);
   }

DeleteItemList(addresses); 

if (dist)
   {
   free((char *)dist);
   }
}

/*********************************************************************/

static void FriendStatus(struct Attributes a,struct Promise *pp)

{
VerifyFriendConnections(a.report.lastseen,a,pp);
VerifyFriendReliability(a,pp);
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void VerifyFriendConnections(int hours,struct Attributes a,struct Promise *pp)

/* Go through the database of recent connections and check for
   Long Time No See ...*/

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  int ksize,vsize;
  int secs = SECONDS_PER_HOUR*hours, criterion, overdue;
  time_t now = time(NULL),lsea = (time_t)CF_WEEK, tthen, then;
  char name[CF_BUFSIZE],hostname[CF_BUFSIZE],datebuf[CF_MAXVARSIZE];
  char addr[CF_BUFSIZE],type[CF_BUFSIZE],output[CF_BUFSIZE];
  struct QPoint entry;
  double average = 0.0, var = 0.0, ticksperminute = 60.0;
  double ticksperhour = (double)SECONDS_PER_HOUR;

CfOut(cf_verbose,"","CheckFriendConnections(%d)\n",hours);
snprintf(name,CF_BUFSIZE-1,"%s/lastseen/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan friend db");
   return;
   }

 /* Walk through the database and print out the key/data pairs. */

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   memset(&entry, 0, sizeof(entry)); 

   strncpy(hostname,(char *)key,ksize);

   if (value != NULL)
      {
      memcpy(&entry,value,sizeof(entry));
      then = (time_t)entry.q;
      average = (double)entry.expect;
      var = (double)entry.var;
      }
   else
      {
      continue;
      }

   if (then == 0)
      {
      continue; // No data
      }
   
   /* Got data, now get expiry criterion */

   if (secs == 0)
      {
      /* Twice the average delta is significant */
      criterion = (now - then > (int)(average+2.0*sqrt(var)+0.5));
      overdue = now - then - (int)(average);
      }
   else
      {
      criterion = (now - then > secs);
      overdue =  (now - then - secs);
      }

   if (LASTSEENEXPIREAFTER < 0)
      {
      lsea = (time_t)CF_WEEK;
      }
   else
      {
      lsea = LASTSEENEXPIREAFTER;
      }

   if (a.report.friend_pattern)
      {
      if (FullTextMatch(a.report.friend_pattern,IPString2Hostname(hostname+1)))
         {
         CfOut(cf_verbose,"","Not judging friend %s\n",hostname);
         criterion = false;
         lsea = CF_INFINITY;
         }
      }
   
   tthen = (time_t)then;

   snprintf(datebuf,CF_MAXVARSIZE-1,"%s",cf_ctime(&tthen));
   datebuf[strlen(datebuf)-9] = '\0';                     /* Chop off second and year */

   snprintf(addr,15,"%s",hostname+1);

   switch(*hostname)
      {
      case '+':
          snprintf(type,CF_BUFSIZE,"last responded to hails");
          break;
      case'-':
          snprintf(type,CF_BUFSIZE,"last hailed us");
          break;
      }

   snprintf(output,CF_BUFSIZE,"Host %s i.e. %s %s @ [%s] (overdue by %d mins)",
            IPString2Hostname(hostname+1),
            addr,
            type,
            datebuf,
            overdue/(int)ticksperminute);

   if (criterion)
      {
      CfOut(cf_error,"",output);
      }
   else
      {
      CfOut(cf_verbose,"",output);
      }

   snprintf(output,CF_BUFSIZE,"i.e. (%.2f) hrs ago, Av %.2f +/- %.2f hrs\n",
            ((double)(now-then))/ticksperhour,
            average/ticksperhour,
            sqrt(var)/ticksperhour);
   
   if (criterion)
      {
      CfOut(cf_error,"",output);
      }
   else
      {
      CfOut(cf_verbose,"",output);
      }

   if (now - then > lsea)
      {
      CfOut(cf_error,"","Giving up on host %s -- %d hours since last seen",IPString2Hostname(hostname+1),hours);
      DeleteDB(dbp,hostname);
      }
  
   memset(&value,0,sizeof(value));
   memset(&key,0,sizeof(key)); 
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
}

/***************************************************************/

static void VerifyFriendReliability(struct Attributes a,struct Promise *pp)

{ CF_DB *dbp;
  CF_DBC *dbcp;
  int i,ksize,vsize;
  char *key;
  void *value;
  double n[CF_RELIABLE_CLASSES],n_av[CF_RELIABLE_CLASSES],total;
  double p[CF_RELIABLE_CLASSES],p_av[CF_RELIABLE_CLASSES];
  char name[CF_BUFSIZE],hostname[CF_BUFSIZE],timekey[CF_MAXVARSIZE];
  struct QPoint entry;
  struct Item *ip, *hostlist = NULL;
  double average,var,sum,sum_av,expect,actual;
  time_t now = time(NULL), then, lastseen = CF_WEEK;

CfOut(cf_verbose,"","CheckFriendReliability()\n");
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);

average = (double) CF_HOUR;  /* It will take a week for a host to be deemed reliable */
var = 0;

if (!OpenDB(name,&dbp))
   {
   return;
   }

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan last-seen db");
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   strcpy(hostname,IPString2Hostname((char *)key+1));

   if (!IsItemIn(hostlist,hostname))
      {
      /* Check hostname not recorded twice with +/- */
      AppendItem(&hostlist,hostname,NULL);
      CfOut(cf_verbose,""," Measuring reliability of %s\n",hostname);
      }
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);

/* Now go through each host and recompute entropy */

for (ip = hostlist; ip != NULL; ip=ip->next)
   {
   snprintf(name,CF_BUFSIZE-1,"%s/%s.%s",CFWORKDIR,CF_LASTDB_FILE,ip->name);
   MapName(name);

   if (!OpenDB(name,&dbp))
      {
      return;
      }
   
   for (i = 0; i < CF_RELIABLE_CLASSES; i++)
      {
      n[i] = n_av[i] = 0.0;
      }

   total = 0.0;

   for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
      {
      strcpy(timekey,GenTimeKey(now));
      
      if (ReadDB(dbp,timekey,&value,sizeof(entry)))
         {
         memcpy(&entry,value,sizeof(entry));
         then = (time_t)entry.q;
         lastseen = now - then;
         if (lastseen < 0)
            {
            lastseen = 0; /* Never seen before, so pretend */
            }
         average = (double)entry.expect;
         var = (double)entry.var;
         Debug("%s => then = %ld, lastseen = %ld, average=%.2f\n",hostname,then,lastseen,average);
         }
      else
         {
         /* If we have no data, it means no contact for whatever reason.
            It could be unable to respond unwilling to respond, policy etc.
            Assume for argument that we expect regular responses ... */
         
         lastseen += CF_MEASURE_INTERVAL; /* infer based on no data */
         }

      for (i = 0; i < CF_RELIABLE_CLASSES; i++)
         {
         if (lastseen >= i*CF_HOUR && lastseen < (i+1)*CF_HOUR)
            {
            n[i]++;
            }
         
         if (average >= (double)(i*CF_HOUR) && average < (double)((i+1)*CF_HOUR))
            {
            n_av[i]++;
            }
         }
       
      total++;
      }

   sum = sum_av = 0.0;
   
   for (i = 0; i < CF_RELIABLE_CLASSES; i++)
      {
      p[i]    = n[i]/total;
      p_av[i] = n_av[i]/total;
      sum += p[i];
      sum_av += p_av[i];
      }

   Debug("Reliabilities sum to %.2f av %.2f\n\n",sum,sum_av);

   sum = sum_av = 0.0;
   
   for (i = 0; i < CF_RELIABLE_CLASSES; i++)
      {
      if (p[i] == 0.0)
         {
         continue;
         }
      sum -= p[i] * log(p[i]);
      }

   for (i = 0; i < CF_RELIABLE_CLASSES; i++)
      {
      if (p_av[i] == 0.0)
         {
         continue;
         }
      sum_av -= p_av[i] * log(p_av[i]);
      }

   actual = sum/log((double)CF_RELIABLE_CLASSES)*100.0;
   expect = sum_av/log((double)CF_RELIABLE_CLASSES)*100.0;
   
   CfOut(cf_verbose,"","Scaled entropy for %s = %.1f %%\n",ip->name,actual);
   CfOut(cf_verbose,"","Expected entropy for %s = %.1f %%\n\n",ip->name,expect);

   if (actual > expect)
      {
      CfOut(cf_inform,""," !! The reliability of %s deteriorated\n",ip->name);
      }

   if (actual > 50.0)
      {
      CfOut(cf_error,"","FriendStatus reports the intermittency of %s above 50%% (SEUs)\n",ip->name);
      }

   if (expect > actual)
      {
      CfOut(cf_inform,"","The reliability of %s is improved\n",ip->name);
      }

   CloseDB(dbp);
   }

DeleteItemList(hostlist);
}


