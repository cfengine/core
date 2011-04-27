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

/*******************************************************************/
/*                                                                 */
/*  HEADER for cfServerd - Not generically includable!             */
/*                                                                 */
/*******************************************************************/

#ifndef CFENGINE_CF3_SERVER_H
#define CFENGINE_CF3_SERVER_H

#ifndef HAVE_SERV_H
#define HAVE_SERV_H 1
#define queuesize 50
#define connection 1
#define RFC931_PORT 113
#define CF_BUFEXT 128

/**********************************************************************/

struct cfd_connection
   {
   int  id_verified;
   int  rsa_auth;
   int  synchronized;
   int  maproot;
   int  trust;
   int  sd_reply;
   unsigned char *session_key;
   unsigned char digest[EVP_MAX_MD_SIZE+1];
   char hostname[CF_MAXVARSIZE];
   char username[CF_MAXVARSIZE];
   #ifdef MINGW
   char sid[CF_MAXSIDSIZE];  /* we avoid dynamically allocated buffers due to potential memory leaks */
   #else
   uid_t uid;
   #endif
   char encryption_type;
   char ipaddr[CF_MAX_IP_LEN];
   char output[CF_BUFSIZE*2];   /* Threadsafe output channel */
   };

struct cfd_get_arg
   {
   struct cfd_connection *connect;
   int encrypt;
   int buf_size;
   char *replybuff;
   char *replyfile;
   };

/*******************************************************************/
/* PARSER                                                          */
/*******************************************************************/

char CFRUNCOMMAND[CF_BUFSIZE];
time_t CFDSTARTTIME;

#ifdef RE_DUP_MAX
# undef RE_DUP_MAX
#endif

/*******************************************************************
 * Sunos4.1.4 need these prototypes
 *******************************************************************/

#if defined(SUN4)
extern char *realpath(/* char *path; char resolved_path[MAXPATHLEN] */);
#endif

#endif

#endif
