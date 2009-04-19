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
KeepPromises();
return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void KeepPromises()

{ unsigned long err;
  RSA *pair;
  FILE *fp;
  struct stat statbuf;
  int fd;
  static char *passphrase = "Cfengine passphrase";
  EVP_CIPHER *cipher;
  char vbuff[CF_BUFSIZE];

NewScope("common");
  
cipher = EVP_des_ede3_cbc();

if (stat(CFPRIVKEYFILE,&statbuf) != -1)
   {
   CfOut(cf_error,"","A key file already exists at %s.\n",CFPRIVKEYFILE);
   return;
   }

if (stat(CFPUBKEYFILE,&statbuf) != -1)
   {
   CfOut(cf_error,"","A key file already exists at %s.\n",CFPUBKEYFILE);
   return;
   }

printf("Making a key pair for cfengine, please wait, this could take a minute...\n"); 

pair = RSA_generate_key(2048,35,NULL,NULL);

if (pair == NULL)
   {
   err = ERR_get_error();
   CfOut(cf_error,"","Unable to generate key: %s\n",ERR_reason_error_string(err));
   return;
   }

if (DEBUG)
   {
   RSA_print_fp(stdout,pair,0);
   }
 
fd = open(CFPRIVKEYFILE,O_WRONLY | O_CREAT | O_TRUNC, 0600);

if (fd < 0)
   {
   CfOut(cf_error,"open","Open %s failed: %s.",CFPRIVKEYFILE,strerror(errno));
   return;
   }
 
if ((fp = fdopen(fd, "w")) == NULL )
   {
   CfOut(cf_error,"fdopen","Couldn't open private key %s.",CFPRIVKEYFILE);
   close(fd);
   return;
   }

CfOut(cf_verbose,"","Writing private key to %s\n",CFPRIVKEYFILE);
 
if (!PEM_write_RSAPrivateKey(fp,pair,cipher,passphrase,strlen(passphrase),NULL,NULL))
   {
   err = ERR_get_error();
   CfOut(cf_error,"","Couldn't write private key: %s\n",ERR_reason_error_string(err));
   return;
   }

fclose(fp);
 
fd = open(CFPUBKEYFILE,O_WRONLY | O_CREAT | O_TRUNC, 0600);

if (fd < 0)
   {
   CfOut(cf_error,"open","Unable to open public key %s.",CFPUBKEYFILE);
   return;
   }
 
if ((fp = fdopen(fd, "w")) == NULL )
   {
   CfOut(cf_error,"fdopen","Open %s failed.",CFPUBKEYFILE);
   close(fd);
   return;
   }

CfOut(cf_verbose,"","Writing public key to %s\n",CFPUBKEYFILE);
 
if (!PEM_write_RSAPublicKey(fp,pair))
   {
   err = ERR_get_error();
   CfOut(cf_error,"","Unable to write public key: %s\n",ERR_reason_error_string(err));
   return;
   }

fclose(fp);
 
snprintf(vbuff,CF_BUFSIZE,"%s/randseed",CFWORKDIR);
RAND_write_file(vbuff);
chmod(vbuff,0644); 
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

while ((c=getopt_long(argc,argv,"d:vf:VM",OPTIONS,&optindex)) != EOF)
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
          
      case 'h': Syntax("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(0);

      case 'M': ManPage("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(0);

      default: Syntax("cf-key - cfengine's key generator",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }
}

