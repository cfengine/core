/*****************************************************************************/
/*                                                                           */
/* File: keyring.c                                                           */
/*                                                                           */
/* Created: Sun May 29 13:35:34 2011                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/***************************************************************/

int HostKeyAddressUnknown(char *value)

{
if (strcmp(value,CF_UNKNOWN_IP) == 0)
   {
   return true;
   }

// Is there some other non-ip string left over?

if (!(strchr(value,'.') || strchr(value,':')))
   {
   return false;
   }

return false;
}


/***************************************************************/
/* deprecated code                                             */
/***************************************************************/

#ifdef NOT_DEFINED
// This version is deprecated, but saved for future reference

void LastSaw(char *username,char *ipaddress,unsigned char digest[EVP_MAX_MD_SIZE+1],enum roles role)

{ char databuf[CF_BUFSIZE];
  time_t now = time(NULL);
  int known = false;
  struct Rlist *rp;
  struct CfKeyBinding *kp;

if (strlen(ipaddress) == 0)
   {
   CfOut(cf_inform,"","LastSeen registry for empty IP with role %d",role);
   return;
   }

ThreadLock(cft_output);

switch (role)
   {
   case cf_accept:
       snprintf(databuf,CF_BUFSIZE-1,"-%s",HashPrint(CF_DEFAULT_DIGEST,digest));
       break;
   case cf_connect:
       snprintf(databuf,CF_BUFSIZE-1,"+%s",HashPrint(CF_DEFAULT_DIGEST,digest));
       break;
   }

ThreadUnlock(cft_output);

ThreadLock(cft_server_keyseen);

for (rp = SERVER_KEYSEEN; rp !=  NULL; rp=rp->next)
   {
   kp = (struct CfKeyBinding *) rp->item;

   if (strcmp(kp->name,databuf) == 0)
      {
      known = true;
      kp->timestamp = now;
      CfOut(cf_verbose,""," -> Last saw %s (%s) now",ipaddress,databuf);

      // Refresh address
      
      ThreadLock(cft_system);

      if (kp->address)
         {
         free(kp->address);
         }
      
      kp->address = strdup(ipaddress);
      ThreadUnlock(cft_system);
      ThreadUnlock(cft_server_keyseen);
      return;
      }
   }

CfOut(cf_verbose,""," -> Last saw %s (%s) first time now",ipaddress,databuf);

ThreadLock(cft_system);
kp = (struct CfKeyBinding *)malloc((sizeof(struct CfKeyBinding)));
ThreadUnlock(cft_system);

if (kp == NULL)
   {
   ThreadUnlock(cft_server_keyseen);
   return;
   }

rp = PrependRlist(&SERVER_KEYSEEN,"nothing",CF_SCALAR);

ThreadLock(cft_system);
free(rp->item);
rp->item = kp;

kp->address = strdup(ipaddress);

if ((kp->name = strdup(databuf)) == NULL)
   {
   free(kp);
   ThreadUnlock(cft_system);
   ThreadUnlock(cft_server_keyseen);
   return;
   }

ThreadUnlock(cft_system);

kp->key = HavePublicKey(username,ipaddress,databuf+1);
kp->timestamp = now;

ThreadUnlock(cft_server_keyseen);
}

/***************************************************************/

void UpdateLastSeen() // This function is temporarily or permanently deprecated

{ double lsea = LASTSEENEXPIREAFTER;
  int intermittency = false,qsize,ksize;
  struct CfKeyHostSeen q,newq; 
  double lastseen,delta2;
  void *stored;
  CF_DB *dbp = NULL,*dbpent = NULL;
  CF_DBC *dbcp;
  char name[CF_BUFSIZE],*key;
  struct Rlist *rp;
  struct CfKeyBinding *kp;
  time_t now = time(NULL);
  static time_t then;
  
if (now < then + 300 && then > 0 && then <= now + 300)
   {
   // Rate limiter
   return;
   }

then = now;

CfOut(cf_verbose,""," -> Writing last-seen observations");

ThreadLock(cft_server_keyseen);

if (SERVER_KEYSEEN == NULL)
   {
   ThreadUnlock(cft_server_keyseen);
   CfOut(cf_verbose,""," -> Keyring is empty");
   return;
   }

if (BooleanControl("control_agent",CFA_CONTROLBODY[cfa_intermittency].lval))
   {
   CfOut(cf_inform,""," -> Recording intermittency");
   intermittency = true;
   }

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   ThreadUnlock(cft_server_keyseen);
   return;
   }

/* First scan for hosts that have moved address and purge their records so that
   the database always has a 1:1 relationship between keyhash and IP address    */

if (!NewDBCursor(dbp,&dbcp))
   {
   ThreadUnlock(cft_server_keyseen);
   CfOut(cf_inform,""," !! Unable to scan class db");
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,&stored,&qsize))
   {
   memcpy(&q,stored,sizeof(q));

   lastseen = (double)now - q.Q.q;

   if (lastseen > lsea)
      {
      CfOut(cf_verbose,""," -> Last-seen record for %s expired after %.1lf > %.1lf hours\n",key,lastseen/3600,lsea/3600);
      DeleteDB(dbp,key);
      }

   for (rp = SERVER_KEYSEEN; rp !=  NULL; rp=rp->next)
      {
      kp = (struct CfKeyBinding *) rp->item;
      
      if ((strcmp(q.address,kp->address) == 0) && (strcmp(key+1,kp->name+1) != 0))
         {
         CfOut(cf_verbose,""," ! Deleting %s's address (%s=%d) as this host %s seems to have moved elsewhere (%s=5d)",key,kp->address,strlen(kp->address),kp->name,q.address,strlen(q.address));
         DeleteDB(dbp,key);
         }
      }

   }

DeleteDBCursor(dbp,dbcp);

/* Now perform updates with the latest data */

for (rp = SERVER_KEYSEEN; rp !=  NULL; rp=rp->next)
   {
   kp = (struct CfKeyBinding *) rp->item;

   now = kp->timestamp;
   
   if (intermittency)
      {
      /* Open special file for peer entropy record - INRIA intermittency */
      snprintf(name,CF_BUFSIZE-1,"%s/lastseen/%s.%s",CFWORKDIR,CF_LASTDB_FILE,kp->name);
      MapName(name);
      
      if (!OpenDB(name,&dbpent))
         {
         continue;
         }
      }
   
   if (ReadDB(dbp,kp->name,&q,sizeof(q)))
      {
      lastseen = (double)now - q.Q.q;
      
      if (q.Q.q <= 0)
         {
         lastseen = 300;
         q.Q.expect = 0;
         q.Q.var = 0;
         }
      
      newq.Q.q = (double)now;
      newq.Q.expect = GAverage(lastseen,q.Q.expect,0.4);
      delta2 = (lastseen - q.Q.expect)*(lastseen - q.Q.expect);
      newq.Q.var = GAverage(delta2,q.Q.var,0.4);
      strncpy(newq.address,kp->address,CF_ADDRSIZE-1);
      }
   else
      {
      lastseen = 0.0;
      newq.Q.q = (double)now;
      newq.Q.expect = 0.0;
      newq.Q.var = 0.0;
      strncpy(newq.address,kp->address,CF_ADDRSIZE-1);
      }
   
   if (lastseen > lsea)
      {
      CfOut(cf_verbose,""," -> Last-seen record for %s expired after %.1lf > %.1lf hours\n",kp->name,lastseen/3600,lsea/3600);
      DeleteDB(dbp,kp->name);
      }
   else
      {
      char timebuf[26];
      CfOut(cf_verbose,""," -> Last saw %s (alias %s) at %s (noexpiry %.1lf <= %.1lf)\n",kp->name,kp->address,cf_strtimestamp_local(now,timebuf),lastseen/3600,lsea/3600);

      WriteDB(dbp,kp->name,&newq,sizeof(newq));
      
      if (intermittency)
         {
         WriteDB(dbpent,GenTimeKey(now),&newq,sizeof(newq));
         }
      }
   
   if (intermittency && dbpent)
      {
      CloseDB(dbpent);
      }
   }

ThreadUnlock(cft_server_keyseen);
}

#endif
