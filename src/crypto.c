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

/*****************************************************************************/

void KeepKeyPromises()

{ unsigned long err;
  RSA *pair;
  FILE *fp;
  struct stat statbuf;
  int fd;
  static char *passphrase = "Cfengine passphrase";
  const EVP_CIPHER *cipher;
  char vbuff[CF_BUFSIZE];

NewScope("common");
  
cipher = EVP_des_ede3_cbc();

if (cfstat(CFPUBKEYFILE,&statbuf) != -1)
   {
   CfOut(cf_cmdout,"","A key file already exists at %s\n",CFPUBKEYFILE);
   return;
   }

if (cfstat(CFPRIVKEYFILE,&statbuf) != -1)
   {
   CfOut(cf_cmdout,"","A key file already exists at %s\n",CFPRIVKEYFILE);
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
cf_chmod(vbuff,0644); 
}

/*********************************************************************/

void LoadSecretKeys()

{ FILE *fp;
  static char *passphrase = "Cfengine passphrase",name[CF_BUFSIZE],source[CF_BUFSIZE];
  char guard[CF_MAXVARSIZE];
  unsigned char digest[EVP_MAX_MD_SIZE+1];
  unsigned long err;
  struct stat sb;

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

CfOut(cf_verbose,""," -> Loaded private key %s\n",CFPRIVKEYFILE); 

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

CfOut(cf_verbose,""," -> Loaded public key %s\n",CFPUBKEYFILE);  
fclose(fp);

if (BN_num_bits(PUBKEY->e) < 2 || !BN_is_odd(PUBKEY->e))
   {
   FatalError("RSA Exponent too small or not odd");
   }

if(EMPTY(POLICY_SERVER))
  {
  snprintf(name,CF_MAXVARSIZE-1,"%s%cpolicy_server.dat",CFWORKDIR,FILE_SEPARATOR);

  if ((fp = fopen(name,"r")) != NULL)
     {
     fscanf(fp,"%s",POLICY_SERVER);
     fclose(fp);
     }
  }

/* Check that we have our own SHA key form of the key in the IP on the hub */

HashPubKey(PUBKEY,digest,CF_DEFAULT_DIGEST);
snprintf(name,CF_MAXVARSIZE,"%s/ppkeys/%s-%s.pub",CFWORKDIR,"root",HashPrint(CF_DEFAULT_DIGEST,digest));
MapName(name);

snprintf(source,CF_MAXVARSIZE,"%s/ppkeys/localhost.pub",CFWORKDIR);
MapName(source);

// During bootstrap we need the pre-registered IP/hash pair on the hub

snprintf(guard,sizeof(guard),"%s/state/am_policy_hub",CFWORKDIR);
MapName(guard);

// need to use cf_stat

if (stat(name,&sb) == -1 && stat(guard,&sb) != -1)
  // copy localhost.pub to root-HASH.pub on policy server
   {
   LastSaw("root",POLICY_SERVER,digest,cf_connect);
   UpdateLastSeen();

   if (!LinkOrCopy(source,name,false))
      {
      CfOut(cf_error,""," -> Unable to clone server's key file as %s\n",name);
      }
   }

}

/*********************************************************************/

RSA *HavePublicKeyByIP(char *username,char *ipaddress)

{ char hash[CF_MAXVARSIZE];
 
IPString2KeyDigest(ipaddress,hash);

return HavePublicKey(username,ipaddress,hash);
}

/*********************************************************************/

RSA *HavePublicKey(char *username,char *ipaddress,char *digest)

{ char keyname[CF_MAXVARSIZE],newname[CF_BUFSIZE],oldname[CF_BUFSIZE];
  struct stat statbuf; 
  static char *passphrase = "public";
  unsigned long err;
  FILE *fp;
  RSA *newkey = NULL;

snprintf(keyname,CF_MAXVARSIZE,"%s-%s",username,digest);  

Debug("HavePublickey(%s)\n",keyname);
  
snprintf(newname,CF_BUFSIZE,"%s/ppkeys/%s.pub",CFWORKDIR,keyname);   
MapName(newname);

// Check memory cache rlist

if ((newkey = SelectKeyRing(keyname)))
   {
   CfOut(cf_verbose,""," -> Retrieved %s from cache",keyname);
   return newkey;
   }
else if (cfstat(newname,&statbuf) == -1)
   {
   CfOut(cf_verbose,""," -> Did not find new key format %s",newname);
   snprintf(oldname,CF_BUFSIZE,"%s/ppkeys/%s-%s.pub",CFWORKDIR,username,ipaddress);   
   MapName(oldname);

   CfOut(cf_verbose,""," -> Trying old style %s",oldname);
      
   if (cfstat(oldname,&statbuf) == -1)
      {      
      Debug("Did not have old-style key %s\n",oldname);
      return NULL;
      }

   if (strlen(digest) > 0)
      {
      CfOut(cf_inform,""," -> Renaming old key from %s to %s",oldname,newname);

      if(rename(oldname,newname) != 0)
	{
	CfOut(cf_error, "rename", "!! Could not rename from old key format (%s) to new (%s)",oldname,newname);
	}
      }
   else  // we don't know the digest (e.g. because we are a client and
	 // have no lastseen-map and/or root-SHA...pub of the server's key
	 // yet) Just using old file format (root-IP.pub) without renaming for now.
     {
     CfOut(cf_verbose, "", " -> Could not map key file to new format - we have no digest yet (using %s)", oldname);
     snprintf(newname,sizeof(newname),"%s",oldname);
     }
   }

CfOut(cf_verbose,""," -> Going to secondary storage for key");

if ((fp = fopen(newname,"r")) == NULL)
   {
   CfOut(cf_error,"fopen","Couldn't find a public key (%s)",newname);
   return NULL;
   }

if ((newkey = PEM_read_RSAPublicKey(fp,NULL,NULL,passphrase)) == NULL)
   {
   err = ERR_get_error();
   CfOut(cf_error,"PEM_read","Error reading Private Key = %s\n",ERR_reason_error_string(err));
   fclose(fp);
   return NULL;
   }

fclose(fp);

if (BN_num_bits(newkey->e) < 2 || !BN_is_odd(newkey->e))
   {
   FatalError("RSA Exponent too small or not odd");
   }

IdempAddToKeyRing(keyname,ipaddress,newkey);
return newkey;
}

/*********************************************************************/

void SavePublicKey(char *user,char *ipaddress,char *digest,RSA *key)

{ char keyname[CF_MAXVARSIZE],filename[CF_BUFSIZE];
  struct stat statbuf;
  FILE *fp;
  int err;

Debug("SavePublicKey %s\n",ipaddress); 

snprintf(keyname,CF_MAXVARSIZE,"%s-%s",user,digest);

IdempAddToKeyRing(keyname,ipaddress,key);

snprintf(filename,CF_BUFSIZE,"%s/ppkeys/%s.pub",CFWORKDIR,keyname);
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

ThreadLock(cft_system);

if (!PEM_write_RSAPublicKey(fp,key))
   {
   err = ERR_get_error();
   CfOut(cf_error,"PEM_write","Error saving public key %s = %s\n",filename,ERR_reason_error_string(err));
   }

ThreadUnlock(cft_system);
fclose(fp);
}

/*********************************************************************/

void DeletePublicKey(char *user,char *ipaddress,char *digest)

{ char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/ppkeys/%s-%s.pub",CFWORKDIR,user,ipaddress);
MapName(filename);
unlink(filename);

snprintf(filename,CF_BUFSIZE,"%s/ppkeys/%s-%s.pub",CFWORKDIR,user,digest);
MapName(filename);
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

#ifdef HAVE_NOVA
md = EVP_get_digestbyname("sha256");
#else
md = EVP_get_digestbyname("md5");
#endif

EVP_DigestInit(&context,md);

CfOut(cf_verbose,"","...\n");
 
snprintf(buffer,CF_BUFSIZE,"%d%d%25s",(int)CFSTARTTIME,(int)*digest,VFQNAME);

EVP_DigestUpdate(&context,buffer,CF_BUFSIZE);

snprintf(pscomm,CF_BUFSIZE,"%s %s",VPSCOMM[VSYSTEMHARDCLASS],VPSOPTS[VSYSTEMHARDCLASS]);

if ((pp = cf_popen(pscomm,"r")) != NULL)
   {
   CfOut(cf_error,"cf_popen","Couldn't open the process list with command %s\n",pscomm);

   while (!feof(pp))
      {
      CfReadLine(buffer,CF_BUFSIZE,pp);
      EVP_DigestUpdate(&context,buffer,CF_BUFSIZE);
      }
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
EVP_EncryptInit_ex(&ctx,CfengineCipher(type),NULL,key,iv);

if (!EVP_EncryptUpdate(&ctx,out,&cipherlen,in,plainlen))
   {
   EVP_CIPHER_CTX_cleanup(&ctx);
   return -1;
   }
 
if (!EVP_EncryptFinal_ex(&ctx,out+cipherlen,&tmplen))
   {
   EVP_CIPHER_CTX_cleanup(&ctx);
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
EVP_DecryptInit_ex(&ctx,CfengineCipher(type),NULL,key,iv);

if (!EVP_DecryptUpdate(&ctx,out,&plainlen,in,cipherlen))
   {
   CfOut(cf_error, "", "!! Decrypt FAILED");
   EVP_CIPHER_CTX_cleanup(&ctx);
   return -1;
   }
 
if (!EVP_DecryptFinal_ex(&ctx,out+plainlen,&tmplen))
   {
   unsigned long err = ERR_get_error();
   CfOut(cf_error,"","decryption FAILED at final of %d: %s\n",cipherlen,ERR_error_string(err,NULL));
   EVP_CIPHER_CTX_cleanup(&ctx);
   return -1;
   }
 
plainlen += tmplen;

EVP_CIPHER_CTX_cleanup(&ctx);

return plainlen; 
}

/*********************************************************************/

void DebugBinOut(char *buffer,int len,char *comment)

{ unsigned char *sp;
  char buf[CF_BUFSIZE];
  char hexStr[3];  // one byte as hex

if (len >= (sizeof(buf) / 2))  // hex uses two chars per byte
   {
   Debug("Debug binary print is too large (len=%d)", len);
   return;
   }

memset(buf, 0, sizeof(buf));

for (sp = buffer; sp < (unsigned char *)(buffer+len); sp++)
   {
   snprintf(hexStr, sizeof(hexStr), "%2.2x", (int)*sp);
   strcat(buf, hexStr);
   }

CfOut(cf_verbose, "", "BinaryBuffer(%d bytes => %s) -> [%s]",len,comment,buf);
}


/*********************************************************************/

char *KeyPrint(RSA *pubkey)

{ unsigned char digest[EVP_MAX_MD_SIZE+1];
 int i;

for (i = 0; i < EVP_MAX_MD_SIZE+1; i++)
   {
   digest[i] = 0;
   }
 
HashString((char *)pubkey,sizeof(BIGNUM)-4,digest,CF_DEFAULT_DIGEST);
return HashPrint(CF_DEFAULT_DIGEST,digest);
}
