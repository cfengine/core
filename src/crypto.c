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
/* File: crypto.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/**********************************************************************/

void RandomSeed()

{ static unsigned char digest[EVP_MAX_MD_SIZE+1];
  struct stat statbuf;
  char vbuff[CF_BUFSIZE];
  
/* Use the system database as the entropy source for random numbers */

Debug("RandomSeed() work directory is %s\n",CFWORKDIR);

snprintf(vbuff,CF_BUFSIZE,"%s%crandseed",CFWORKDIR,FILE_SEPARATOR);

 if (cfstat(vbuff,&statbuf) == -1)
    {
    snprintf(AVDB,CF_MAXVARSIZE-1,"%s%cstate%c%s",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,CF_AVDB_FILE);
    }
 else
    {
    strncpy(AVDB,vbuff,CF_MAXVARSIZE-1);
    }

CfOut(cf_verbose,"","Looking for a source of entropy in %s\n",AVDB);

if (!RAND_load_file(AVDB,-1))
   {
   CfOut(cf_verbose,"","Could not read sufficient randomness from %s\n",AVDB);
   }

while (!RAND_status())
   {
   MD5Random(digest);
   RAND_seed((void *)digest,16);
   }
}

/*********************************************************************/

void LoadSecretKeys()

{ FILE *fp;
  static char *passphrase = "Cfengine passphrase";
  unsigned long err;
  
if ((fp = fopen(CFPRIVKEYFILE,"r")) == NULL)
   {
   CfOut(cf_inform,"fopen","Couldn't find a private key (%s) - use cf-key to get one",CFPRIVKEYFILE);
   return;
   }
 
if ((PRIVKEY = PEM_read_RSAPrivateKey(fp,(RSA **)NULL,NULL,passphrase)) == NULL)
   {
   err = ERR_get_error();
   CfOut(cf_error,"PEM_read","Error reading Private Key = %s\n",ERR_reason_error_string(err));
   PRIVKEY = NULL;
   fclose(fp);
   return;
   }

fclose(fp);

CfOut(cf_verbose,"","Loaded %s\n",CFPRIVKEYFILE); 

if ((fp = fopen(CFPUBKEYFILE,"r")) == NULL)
   {
   CfOut(cf_error,"fopen","Couldn't find a public key (%s) - use cf-key to get one",CFPUBKEYFILE);
   return;
   }
 
if ((PUBKEY = PEM_read_RSAPublicKey(fp,NULL,NULL,passphrase)) == NULL)
   {
   err = ERR_get_error();
   CfOut(cf_error,"PEM_read","Error reading Private Key = %s\n",ERR_reason_error_string(err));
   PUBKEY = NULL;
   fclose(fp);
   return;
   }

CfOut(cf_verbose,"","Loaded %s\n",CFPUBKEYFILE);  
fclose(fp);

if (BN_num_bits(PUBKEY->e) < 2 || !BN_is_odd(PUBKEY->e))
   {
   FatalError("RSA Exponent too small or not odd");
   }

}

/*********************************************************************/

RSA *HavePublicKey(char *name)

{ char filename[CF_BUFSIZE],*sp;
  struct stat statbuf; 
  static char *passphrase = "public";
  unsigned long err;
  FILE *fp;
  RSA *newkey = NULL;

Debug("HavePublickey(%s)\n",name);
  
if (!IsPrivileged())
   {
   if ((sp = getenv("HOME")) == NULL)
      {
      FatalError("You do not have a HOME variable pointing to your home directory");
      }  

   snprintf(filename,CF_BUFSIZE,"%s/.cfagent/ppkeys/%s.pub",sp,name);
   }
else
   {
   snprintf(filename,CF_BUFSIZE,"%s/ppkeys/%s.pub",CFWORKDIR,name);
   }
   
MapName(filename);
 
if (cfstat(filename,&statbuf) == -1)
   {
   Debug("Did not have key %s\n",name);
   return NULL;
   }
else
   {
   if ((fp = fopen(filename,"r")) == NULL)
      {
      CfOut(cf_error,"fopen","Couldn't find a public key (%s) - use cf-key to get one",filename);
      return NULL;
      }
   
   if ((newkey = PEM_read_RSAPublicKey(fp,NULL,NULL,passphrase)) == NULL)
      {
      err = ERR_get_error();
      CfOut(cf_error,"PEM_read","Error reading Private Key = %s\n",ERR_reason_error_string(err));
      fclose(fp);
      return NULL;
      }
   
   CfOut(cf_verbose,"","Loaded %s\n",filename);  
   fclose(fp);
   
   if (BN_num_bits(newkey->e) < 2 || !BN_is_odd(newkey->e))
      {
      FatalError("RSA Exponent too small or not odd");
      }

   return newkey;
   }
}

/*********************************************************************/

void SavePublicKey(char *name,RSA *key)

{ char filename[CF_BUFSIZE],*sp;
  struct stat statbuf;
  FILE *fp;
  int err;

Debug("SavePublicKey %s\n",name); 

snprintf(filename,CF_BUFSIZE,"%s/ppkeys/%s.pub",CFWORKDIR,name);
MapName(filename);

if (cfstat(filename,&statbuf) != -1)
   {
   return;
   }
 
CfOut(cf_verbose,"","Saving public key %s\n",filename); 
  
if ((fp = fopen(filename, "w")) == NULL )
   {
   CfOut(cf_error,"fopen","Unable to write a public key %s",filename);
   return;
   }

if (!PEM_write_RSAPublicKey(fp,key))
   {
   err = ERR_get_error();
   CfOut(cf_error,"PEM_write","Error saving public key %s = %s\n",filename,ERR_reason_error_string(err));
   }
 
fclose(fp);
}

/*********************************************************************/

void DeletePublicKey(name)

char *name;

{ char filename[CF_BUFSIZE],*sp;

snprintf(filename,CF_BUFSIZE,"%s/ppkeys/%s.pub",CFWORKDIR,name);
unlink(filename);
}

/*********************************************************************/

void MD5Random(unsigned char digest[EVP_MAX_MD_SIZE+1])

   /* Make a decent random number by crunching some system states & garbage through
      MD5. We can use this as a seed for pseudo random generator */

{ unsigned char buffer[CF_BUFSIZE];
  char pscomm[CF_BUFSIZE];
  char uninitbuffer[100];
  int md_len;
  const EVP_MD *md;
  EVP_MD_CTX context;
  FILE *pp;
 
CfOut(cf_verbose,"","Looking for a random number seed...\n");

md = EVP_get_digestbyname("md5");
EVP_DigestInit(&context,md);

CfOut(cf_verbose,"","...\n");
 
snprintf(buffer,CF_BUFSIZE,"%d%d%25s",(int)CFSTARTTIME,(int)*digest,VFQNAME);

EVP_DigestUpdate(&context,buffer,CF_BUFSIZE);

snprintf(pscomm,CF_BUFSIZE,"%s %s",VPSCOMM[VSYSTEMHARDCLASS],VPSOPTS[VSYSTEMHARDCLASS]);

if ((pp = cf_popen(pscomm,"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","Couldn't open the process list with command %s\n",pscomm);
   return;
   }

while (!feof(pp))
   {
   CfReadLine(buffer,CF_BUFSIZE,pp);
   EVP_DigestUpdate(&context,buffer,CF_BUFSIZE);
   }

uninitbuffer[99] = '\0';
snprintf(buffer,CF_BUFSIZE-1,"%ld %s",time(NULL),uninitbuffer);
EVP_DigestUpdate(&context,buffer,CF_BUFSIZE);

cf_pclose(pp);

EVP_DigestFinal(&context,digest,&md_len);
}

/*********************************************************************/

int EncryptString(char type,char *in,char *out,unsigned char *key,int plainlen)

{ int cipherlen = 0, tmplen;
  unsigned char iv[32] = {1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8};
  EVP_CIPHER_CTX ctx;
 
EVP_CIPHER_CTX_init(&ctx);
EVP_EncryptInit(&ctx,CfengineCipher(type),key,iv);

if (!EVP_EncryptUpdate(&ctx,out,&cipherlen,in,plainlen))
   {
   return -1;
   }
 
if (!EVP_EncryptFinal(&ctx,out+cipherlen,&tmplen))
   {
   return -1;
   }
 
cipherlen += tmplen;
EVP_CIPHER_CTX_cleanup(&ctx);
return cipherlen; 
}

/*********************************************************************/

int DecryptString(char type,char *in,char *out,unsigned char *key,int cipherlen)

{ int plainlen = 0, tmplen;
  unsigned char iv[32] = {1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8};
  EVP_CIPHER_CTX ctx;

EVP_CIPHER_CTX_init(&ctx);
EVP_DecryptInit(&ctx,CfengineCipher(type),key,iv);

if (!EVP_DecryptUpdate(&ctx,out,&plainlen,in,cipherlen))
   {
   return -1;
   }
 
if (!EVP_DecryptFinal(&ctx,out+plainlen,&tmplen))
   {
   return -1;
   }
 
plainlen += tmplen;

EVP_CIPHER_CTX_cleanup(&ctx);
return plainlen; 
}

/*********************************************************************/

void DebugBinOut(char *buffer,int len)

{ char *sp;
  int check = 0;

Debug("BinaryBuffer(%d)[",len);
 
for (sp = buffer; (sp < buffer+len); sp++)
   {
   check++;
   Debug("%x",*sp);
   }
 
Debug("] = %d\n",check); 
}


/*********************************************************************/

char *KeyPrint(RSA *pubkey)

{ unsigned char digest[EVP_MAX_MD_SIZE+1];
 int i;

for (i = 0; i < EVP_MAX_MD_SIZE+1; i++)
   {
   digest[i] = 0;
   }
 
HashString((char *)pubkey,sizeof(BIGNUM)-4,digest,cf_md5);
return HashPrint(cf_md5,digest);
}
