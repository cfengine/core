/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
/* File: exec.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

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
      { "output-file",required_argument,0,'o'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[17] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "Output the version of the software",
      "Specify an alternative output file than the default (localhost)",
      NULL
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv);
GenericInitialize(argc,argv,"keygenerator");

ThisAgentInit();
KeepPromises();
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

while ((c=getopt_long(argc,argv,"d:vnKIf:D:N:VxL:hFV1gM",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
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
                    
      case 'V': Version("cf-key");
          exit(0);
          
      case 'h': Syntax("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(0);

      case 'x': SelfDiagnostic();
          exit(0);
          
      default: Syntax("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }
}

/*****************************************************************************/

void ThisAgentInit()

{
}

/*****************************************************************************/

void KeepPromises()

{ unsigned long err;
  RSA *pair;
  FILE *fp;
  struct stat statbuf;
  int fd;
  static char *passphrase = "Cfengine passphrase";
  EVP_CIPHER *cipher = EVP_des_ede3_cbc();
  char vbuff[CF_BUFSIZE];

if (stat(CFPRIVKEYFILE,&statbuf) != -1)
   {
   printf("A key file already exists at %s.\n",CFPRIVKEYFILE);
   return 1;
   }

if (stat(CFPUBKEYFILE,&statbuf) != -1)
   {
   printf("A key file already exists at %s.\n",CFPUBKEYFILE);
   return 1;
   }

printf("Making a key pair for cfengine, please wait, this could take a minute...\n"); 

pair = RSA_generate_key(2048,35,NULL,NULL);

if (pair == NULL)
   {
   err = ERR_get_error();
   printf("Error = %s\n",ERR_reason_error_string(err));
   return 1;
   }

if (VERBOSE)
   {
   RSA_print_fp(stdout,pair,0);
   }
 
fd = open(CFPRIVKEYFILE,O_WRONLY | O_CREAT | O_TRUNC, 0600);

if (fd < 0)
   {
   printf("Ppen %s failed: %s.",CFPRIVKEYFILE,strerror(errno));
   return 1;
   }
 
if ((fp = fdopen(fd, "w")) == NULL )
   {
   printf("fdopen %s failed: %s.",CFPRIVKEYFILE, strerror(errno));
   close(fd);
   return 1;
   }

printf("Writing private key to %s\n",CFPRIVKEYFILE);
 
if (!PEM_write_RSAPrivateKey(fp,pair,cipher,passphrase,strlen(passphrase),NULL,NULL))
    {
    err = ERR_get_error();
    printf("Error = %s\n",ERR_reason_error_string(err));
    return 1;
    }
 
fclose(fp);
 
fd = open(CFPUBKEYFILE,O_WRONLY | O_CREAT | O_TRUNC, 0600);

if (fd < 0)
   {
   printf("open %s failed: %s.",CFPUBKEYFILE,strerror(errno));
   return 1;
   }
 
if ((fp = fdopen(fd, "w")) == NULL )
   {
   printf("fdopen %s failed: %s.",CFPUBKEYFILE, strerror(errno));
   close(fd);
   return 1;
   }

printf("Writing public key to %s\n",CFPUBKEYFILE);
 
if(!PEM_write_RSAPublicKey(fp,pair))
   {
   err = ERR_get_error();
   printf("Error = %s\n",ERR_reason_error_string(err));
   return 1;
   }

fclose(fp);
 
snprintf(vbuff,CF_BUFSIZE,"%s/randseed",CFWORKDIR);
RAND_write_file(vbuff);
chmod(vbuff,0644); 
return 0;
}

