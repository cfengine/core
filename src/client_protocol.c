
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
/* File: files_protocol.c                                                    */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/

int IdentifyAgent(int sd,char *localip,int family)

{ char uname[CF_BUFSIZE], sendbuff[CF_BUFSIZE],dnsname[CF_BUFSIZE];
  struct in_addr *iaddr;
  struct hostent *hp;
  int len,err;
#if defined(HAVE_GETADDRINFO)
  char myaddr[256]; /* Compilation trick for systems that don't know ipv6 */
#else
  struct sockaddr_in myaddr;
#endif
  
memset(sendbuff,0,CF_BUFSIZE);
memset(dnsname,0,CF_BUFSIZE);

if (!SKIPIDENTIFY && (strcmp(VDOMAIN,CF_START_DOMAIN) == 0))
   {
   CfOut(cf_error,"","Undefined domain name");
   return false;
   }

if (!SKIPIDENTIFY)
   {
/* First we need to find out the IP address and DNS name of the socket
   we are sending from. This is not necessarily the same as VFQNAME if
   the machine has a different uname from its IP name (!) This can
   happen on poorly set up machines or on hosts with multiple
   interfaces, with different names on each interface ... */ 
   
   switch (family)
      {
      case AF_INET: len = sizeof(struct sockaddr_in);
          break;
#if defined(HAVE_GETADDRINFO)
      case AF_INET6: len = sizeof(struct sockaddr_in6);
          break;
#endif
      default:
          CfOut(cf_error,"","Software error in IdentifyForVerification");
      }
   
   if (getsockname(sd,(struct sockaddr *)&myaddr,&len) == -1)
      {
      CfOut(cf_error,"getsockname","Couldn't get socket address\n");
      return false;
      }
   
   snprintf(localip,CF_MAX_IP_LEN-1,"%s",sockaddr_ntop((struct sockaddr *)&myaddr)); 
   
   Debug("Identifying this agent as %s i.e. %s, with signature %d\n",localip,VFQNAME,CFSIGNATURE);
   
#if defined(HAVE_GETADDRINFO)
   
   if ((err=getnameinfo((struct sockaddr *)&myaddr,len,dnsname,CF_MAXVARSIZE,NULL,0,0)) != 0)
      {
      CfOut(cf_error,"","Couldn't look up address v6 for %s: %s\n",dnsname,gai_strerror(err));
      return false;
      }
   
#else 
   
   iaddr = &(myaddr.sin_addr); 

   hp = gethostbyaddr((void *)iaddr,sizeof(myaddr.sin_addr),family);
   
   if ((hp == NULL) || (hp->h_name == NULL))
      {
      CfOut(cf_error,"gethostbyaddr","Couldn't lookup IP address\n");
      return false;
      }
   
   strncpy(dnsname,hp->h_name,CF_MAXVARSIZE);
   
   if ((strstr(hp->h_name,".") == 0) && (strlen(VDOMAIN) > 0))
      {
      strcat(dnsname,".");
      strcat(dnsname,VDOMAIN);
      } 
#endif 
   }
else
   {
   strcpy(localip,VIPADDRESS);
   
   if (strlen(VFQNAME) > 0)
      {
      CfOut(cf_verbose,"","skipidentify was promised, so we are trusting and simply announcing the identity as (%s) for this host\n",VFQNAME);
      strcat(dnsname,VFQNAME);
      }
   else
      {
      strcat(dnsname,"skipident");
      }
   }

/* client always identifies as root on windows */
#ifdef MINGW
snprintf(uname, sizeof(uname), "%s", "root");
#else
GetCurrentUserName(uname, sizeof(uname));
#endif

/* Some resolvers will not return FQNAME and missing PTR will give numerical result */

if ((strlen(VDOMAIN) > 0) && !IsIPV6Address(dnsname) && !strchr(dnsname,'.'))
   {
   Debug("Appending domain %s to %s\n",VDOMAIN,dnsname);
   strcat(dnsname,".");
   strncat(dnsname,VDOMAIN,CF_MAXVARSIZE/2);
   }  

if (strncmp(dnsname,localip,strlen(localip)) == 0)
   {
   /* Seems to be a bug in some resolvers that adds garbage, when it just returns the input */
   strcpy(dnsname,localip);
   }

if (strlen(dnsname) == 0)
   {
   strcpy(dnsname,localip);
   }

snprintf(sendbuff,CF_BUFSIZE-1,"CAUTH %s %s %s %d",localip,dnsname,uname,CFSIGNATURE);

Debug("SENT:::%s\n",sendbuff);
 
SendTransaction(sd,sendbuff,0,CF_DONE);
return true;
}

/*********************************************************************/

int AuthenticateAgent(struct cfagent_connection *conn,struct Attributes attr,struct Promise *pp)

{ char sendbuffer[CF_EXPANDSIZE],in[CF_BUFSIZE],*out,*decrypted_cchall;
  BIGNUM *nonce_challenge, *bn = NULL;
  unsigned long err;
  unsigned char digest[EVP_MAX_MD_SIZE];
  int encrypted_len,nonce_len = 0,len,session_size;;
  char dont_implicitly_trust_server, keyname[CF_BUFSIZE], enterprise_field = 'c';
  RSA *server_pubkey = NULL;

if (PUBKEY == NULL || PRIVKEY == NULL) 
   {
   CfOut(cf_error,"","No public/private key pair found\n");
   return false;
   }

enterprise_field = CfEnterpriseOptions();
session_size = CfSessionKeySize(enterprise_field);

/* Generate a random challenge to authenticate the server */
 
nonce_challenge = BN_new();
BN_rand(nonce_challenge,CF_NONCELEN,0,0);

nonce_len = BN_bn2mpi(nonce_challenge,in);
HashString(in,nonce_len,digest,cf_md5);

/* We assume that the server bound to the remote socket is the official one i.e. = root's */

if (BooleanControl("control_agent","hostnamekeys"))
   {
   snprintf(keyname,CF_BUFSIZE,"root-%s",pp->this_server); 
   Debug("AuthenticateAgent(with hostname key %s)\n",keyname);
   }
else
   {
   snprintf(keyname,CF_BUFSIZE,"root-%s",conn->remoteip); 
   Debug("AuthenticateAgent(with IP keyname %s)\n",keyname);
   }

if (server_pubkey = HavePublicKey(keyname))
   {
   dont_implicitly_trust_server = 'y';
   encrypted_len = RSA_size(server_pubkey);
   }
else 
   {
   dont_implicitly_trust_server = 'n';        /* have to trust server, since we can't verify id */
   encrypted_len = nonce_len;
   }

snprintf(sendbuffer,CF_BUFSIZE,"SAUTH %c %d %d %c",dont_implicitly_trust_server,encrypted_len,nonce_len,enterprise_field);
 
if ((out = malloc(encrypted_len)) == NULL)
   {
   FatalError("memory failure");
   }

if (server_pubkey != NULL)
   {
   if (RSA_public_encrypt(nonce_len,in,out,server_pubkey,RSA_PKCS1_PADDING) <= 0)
      {
      err = ERR_get_error();
      cfPS(cf_error,CF_FAIL,"",pp,attr,"Public encryption failed = %s\n",ERR_reason_error_string(err));
      free(out);
      return false;
      }
   
   memcpy(sendbuffer+CF_RSA_PROTO_OFFSET,out,encrypted_len); 
   }
else
   {
   memcpy(sendbuffer+CF_RSA_PROTO_OFFSET,in,nonce_len); 
   }

/* proposition C1 - Send challenge / nonce */
 
SendTransaction(conn->sd,sendbuffer,CF_RSA_PROTO_OFFSET+encrypted_len,CF_DONE);

BN_free(bn);
BN_free(nonce_challenge);
free(out);

if (DEBUG||D2)
   {
   RSA_print_fp(stdout,PUBKEY,0);
   }

/*Send the public key - we don't know if server has it */ 
/* proposition C2 */

memset(sendbuffer,0,CF_EXPANDSIZE); 
len = BN_bn2mpi(PUBKEY->n,sendbuffer); 
SendTransaction(conn->sd,sendbuffer,len,CF_DONE); /* No need to encrypt the public key ... */

/* proposition C3 */ 
memset(sendbuffer,0,CF_EXPANDSIZE);   
len = BN_bn2mpi(PUBKEY->e,sendbuffer); 
SendTransaction(conn->sd,sendbuffer,len,CF_DONE);

/* check reply about public key - server can break connection here */

/* proposition S1 */  
memset(in,0,CF_BUFSIZE);  

if (ReceiveTransaction(conn->sd,in,NULL) == -1)
   {
   cfPS(cf_error,CF_INTERPT,"recv",pp,attr,"Protocol transaction broken off");
   return false;
   }

if (BadProtoReply(in))
   {
   CfOut(cf_error,"",in);
   return false;
   }

/* Get challenge response - should be md5 of challenge */

/* proposition S2 */   
memset(in,0,CF_BUFSIZE);  

if (ReceiveTransaction(conn->sd,in,NULL) == -1)
   {
   cfPS(cf_error,CF_INTERPT,"recv",pp,attr,"Protocol transaction broken off");
   return false;   
   }

if (!HashesMatch(digest,in,cf_md5)) 
   {
   cfPS(cf_error,CF_INTERPT,"",pp,attr,"Challenge response from server %s/%s was incorrect!",pp->this_server,conn->remoteip);
   return false;
   }
else
   {
   if (dont_implicitly_trust_server == 'y')  /* challenge reply was correct */ 
      {
      CfOut(cf_verbose,"",".....................[.h.a.i.l.].................................\n");
      CfOut(cf_verbose,"","Strong authentication of server=%s connection confirmed\n",pp->this_server);
      }
   else
      {
      if (attr.copy.trustkey)
         {
         CfOut(cf_cmdout,""," -> Trusting server identity, promise to accept key from %s=%s",pp->this_server,conn->remoteip);
         }
      else
         {
         CfOut(cf_error,""," !! Not authorized to trust the server=%s's public key (trustkey=false)\n",pp->this_server);
         PromiseRef(cf_verbose,pp);
         return false;
         }
      }
   }

/* Receive counter challenge from server */ 

Debug("Receive counter challenge from server\n");

/* proposition S3 */   
memset(in,0,CF_BUFSIZE);  
encrypted_len = ReceiveTransaction(conn->sd,in,NULL);

if (encrypted_len <= 0)
   {
   CfOut(cf_error,"","Protocol transaction sent illegal cipher length");
   return false;      
   }

if ((decrypted_cchall = malloc(encrypted_len)) == NULL)
   {
   FatalError("memory failure");
   }
 
if (RSA_private_decrypt(encrypted_len,in,decrypted_cchall,PRIVKEY,RSA_PKCS1_PADDING) <= 0)
   {
   err = ERR_get_error();
   cfPS(cf_error,CF_INTERPT,"",pp,attr,"Private decrypt failed = %s, abandoning\n",ERR_reason_error_string(err));
   return false;
   }

/* proposition C4 */   
HashString(decrypted_cchall,nonce_len,digest,cf_md5);
Debug("Replying to counter challenge with md5\n"); 
SendTransaction(conn->sd,digest,16,CF_DONE);
free(decrypted_cchall); 

/* If we don't have the server's public key, it will be sent */


if (server_pubkey == NULL)
   {
   RSA *newkey = RSA_new();

   Debug("Collecting public key from server!\n"); 

   /* proposition S4 - conditional */  
   if ((len = ReceiveTransaction(conn->sd,in,NULL)) <= 0)
      {
      CfOut(cf_error,"","Protocol error in RSA authentation from IP %s\n",pp->this_server);
      return false;
      }
   
   if ((newkey->n = BN_mpi2bn(in,len,NULL)) == NULL)
      {
      err = ERR_get_error();
      cfPS(cf_error,CF_INTERPT,"",pp,attr,"Private decrypt failed = %s\n",ERR_reason_error_string(err));
      RSA_free(newkey);
      return false;
      }

   /* proposition S5 - conditional */  

   if ((len=ReceiveTransaction(conn->sd,in,NULL)) == 0)
      {
      cfPS(cf_inform,CF_INTERPT,"",pp,attr,"Protocol error in RSA authentation from IP %s\n",pp->this_server);
      RSA_free(newkey);
      return false;
      }
   
   if ((newkey->e = BN_mpi2bn(in,len,NULL)) == NULL)
      {
      err = ERR_get_error();
      cfPS(cf_error,CF_INTERPT,"",pp,attr,"Private decrypt failed = %s\n",ERR_reason_error_string(err));
      RSA_free(newkey);
      return false;
      }

   SavePublicKey(keyname,newkey);
   server_pubkey = RSAPublicKey_dup(newkey);
   RSA_free(newkey);
   }
 
/* proposition C5 */

SetSessionKey(conn);

if (conn->session_key == NULL)
   {
   CfOut(cf_error,"","A random session key could not be established");
   return false;
   }

encrypted_len = RSA_size(server_pubkey);

Debug("Encrypt %d bytes of session key into %d RSA bytes\n",session_size,encrypted_len);

if ((out = malloc(encrypted_len)) == NULL)
   {
   FatalError("memory failure");
   }

if (RSA_public_encrypt(session_size,conn->session_key,out,server_pubkey,RSA_PKCS1_PADDING) <= 0)
   {
   err = ERR_get_error();
   cfPS(cf_error,CF_INTERPT,"",pp,attr,"Public encryption failed = %s\n",ERR_reason_error_string(err));
   free(out);
   return false;
   }

SendTransaction(conn->sd,out,encrypted_len,CF_DONE);

if (server_pubkey != NULL)
   {
   RSA_free(server_pubkey);
   }

free(out);
return true; 
}

/*********************************************************************/

void CheckServerVersion(struct cfagent_connection *conn,struct Attributes attr, struct Promise *pp)

{ char sendbuffer[CF_BUFSIZE];
  char recvbuffer[CF_BUFSIZE];
  int tosend;

Debug("CheckRemoteVersion\n");  
snprintf(sendbuffer,CF_BUFSIZE,"VERSION");
tosend = strlen(sendbuffer);

if (SendTransaction(conn->sd,sendbuffer,tosend,CF_DONE) == -1)
   {
   cfPS(cf_inform,CF_INTERPT,"send",pp,attr,"Transmission failed while checking version");
   return;
   }

if (ReceiveTransaction(conn->sd,recvbuffer,NULL) == -1)
   {
   cfPS(cf_inform,CF_INTERPT,"send",pp,attr,"Reply failed while checking version");
   conn->protoversion = 0;
   return;
   }

if (BadProtoReply(recvbuffer))
   {
   conn->protoversion = 0;
   return;
   }
else
   {
   conn->protoversion = 1;
   return;
   }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void SetSessionKey(struct cfagent_connection *conn)

{ BIGNUM *bp;
  int session_size = CfSessionKeySize(conn->encryption_type);

bp = BN_new();

// session_size is in bytes
if(!BN_rand(bp,session_size*8,-1,0))
  {
  FatalError("Can't generate cryptographic key");
  }

//BN_print_fp(stdout,bp);

conn->session_key = (unsigned char *)bp->d;
}

/*********************************************************************/

int BadProtoReply(char *buf)

{
 Debug("Protoreply: (%s)\n",buf);
 return (strncmp(buf,"BAD:",4) == 0);
}

/*********************************************************************/

int OKProtoReply(char *buf)

{
return(strncmp(buf,"OK:",3) == 0);
}

/*********************************************************************/

int FailedProtoReply(char *buf)

{
return(strncmp(buf,CF_FAILEDSTR,strlen(CF_FAILEDSTR)) == 0);
}

