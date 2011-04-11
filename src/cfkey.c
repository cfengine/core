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

void ShowLastSeenHosts(void);
int main (int argc,char *argv[]);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

char *ID = "The cfengine's generator makes key pairs for remote authentication.\n";
 
 struct option OPTIONS[17] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "output-file",required_argument,0,'f'},
      { "show-hosts",no_argument,0,'s'}, 	
      { NULL,0,0,'\0' }
      };

 char *HINTS[17] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "Output the version of the software",
      "Specify an alternative output file than the default (localhost)",
      "Show lastseen hostnames and IP addresses",   	
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

while ((c=getopt_long(argc,argv,"d:vf:VMs",OPTIONS,&optindex)) != EOF)
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
                    
      case 'V': Version("cf-key");
          exit(0);

      case 'v':
          VERBOSE = true;
          break;
      case 's':
          SHOWHOSTS = true;
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

memset(&entry, 0, sizeof(entry));

printf("%9.9s %15.15s %-25.25s %15.15s\n","Direction","IP","Name","Key");
 /* Walk through the database and print out the key/data pairs. */
while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   if (value != NULL)
      {
      memcpy(&entry,value,sizeof(entry));
      strncpy(hostname,(char *)key,ksize);
      strncpy(address,(char *)entry.address,ksize); 
      ++count;  
      }
   else
      {
      continue;
      }
   CfOut(cf_verbose,""," -> Reporting on %s",hostname);
      
   printf("%-9.9s %15.15s %-25.25s %s\n",
             hostname[0] == '+' ? "Incoming" : "Outgoing",
     	     address,	     
	     IPString2Hostname(address),
             hostname+1
          );
   }
printf("Total Entries: %d\n",count);
DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
}

/*eof*/
