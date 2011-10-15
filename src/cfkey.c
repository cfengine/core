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
/* File: exec.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int SHOWHOSTS = false;
bool REMOVEKEYS = false;
const char *remove_keys_host;

void ShowLastSeenHosts(void);
static int RemoveKeys(const char *host);
int main (int argc,char *argv[]);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

const char *ID = "The cfengine's generator makes key pairs for remote authentication.\n";
 
const struct option OPTIONS[17] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "output-file",required_argument,0,'f'},
      { "show-hosts",no_argument,0,'s'},
      { "remove-keys",required_argument,0,'r'},
      { NULL,0,0,'\0' }
      };

const char *HINTS[17] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "Output the version of the software",
      "Specify an alternative output file than the default (localhost)",
      "Show lastseen hostnames and IP addresses",
      "Remove keys for specified hostname/IP",
      NULL
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv);

THIS_AGENT_TYPE = cf_keygen;

GenericInitialize(argc,argv,"keygenerator");

if (SHOWHOSTS)
   {
   ShowLastSeenHosts();
   return 0; 	
   }

if (REMOVEKEYS)
   {
   return RemoveKeys(remove_keys_host);
   }

KeepKeyPromises();
return 0;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  int optindex = 0;
  int c;

while ((c=getopt_long(argc,argv,"d:vf:VMsr:",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          snprintf(CFPRIVKEYFILE,CF_BUFSIZE,"%s.priv",optarg);
          snprintf(CFPUBKEYFILE,CF_BUFSIZE,"%s.pub",optarg);
          break;

      case 'd': 
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
                    
      case 'V': PrintVersionBanner("cf-key");
          exit(0);

      case 'v':
          VERBOSE = true;
          break;
      case 's':
          SHOWHOSTS = true;
          break;

      case 'r':
         REMOVEKEYS = true;
         remove_keys_host = optarg;
         break;

      case 'h': Syntax("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(0);

      default: Syntax("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }
}
/*****************************************************************************/

void ShowLastSeenHosts()

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  char name[CF_BUFSIZE],hostname[CF_BUFSIZE],address[CF_MAXVARSIZE];
  struct CfKeyHostSeen entry;
  int ksize,vsize;
  int count = 0;

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan last-seen database");
   CloseDB(dbp);
   return;
   }

 /* Initialize the key/data return pair. */


printf("%9.9s %17.17s %-25.25s %15.15s\n","Direction","IP","Name","Key");

/* Walk through the database and print out the key/data pairs. */

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   if (value != NULL)
      {
      memset(&entry, 0, sizeof(entry));
      memset(hostname, 0, sizeof(hostname));
      memset(address, 0, sizeof(address));
      memcpy(&entry,value,sizeof(entry));
      strncpy(hostname,(char *)key,sizeof(hostname)-1);
      strncpy(address,(char *)entry.address,sizeof(address)-1);
      ++count;  
      }
   else
      {
      continue;
      }

   CfOut(cf_verbose,""," -> Reporting on %s",hostname);
      
   printf("%-9.9s %17.17s %-25.25s %s\n",
             hostname[0] == '+' ? "Outgoing" : "Incoming",
     	     address,	     
	     IPString2Hostname(address),
             hostname+1
          );
   }

printf("Total Entries: %d\n",count);
DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
}

/*****************************************************************************/

static int RemoveKeys(const char *host)
{
RemoveHostFromLastSeen(host,NULL);
int removed_keys = RemovePublicKeys(remove_keys_host);

if (removed_keys < 0)
   {
   CfOut(cf_error, "", "Unable to remove keys for the host %s", remove_keys_host);
   return 255;
   }
else if (removed_keys == 0)
   {
   CfOut(cf_error, "", "No keys for host %s were found", remove_keys_host);
   return 1;
   }
else
   {
   CfOut(cf_inform, "", "Removed %d key(s) for host %s", removed_keys, remove_keys_host);
   return 0;
   }
}
