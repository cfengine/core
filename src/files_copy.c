

/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
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
/* File: files_copy.c                                                        */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void *CopyFileSources(char *destination,struct FileAttr attr,struct Promise *pp)

{ char *source = attr.copy.source;
  char *server = pp->this_server;
  char vbuff[CF_BUFSIZE];
  struct stat ssb,dsb;
  struct timespec start,stop;
  double dt = 0;
  int measured_ok = true;
  char eventname[CF_BUFSIZE];

if (pp->conn != NULL && !pp->conn->authenticated)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"No authenticated source %s in files.copyfrom promise\n",source);
   CfLog(cfverbose,OUTPUT,"");
   ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
   free(destination);
   return NULL;
   }
  
if (cf_stat(attr.copy.source,&ssb,attr,pp) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't stat %s in files.copyfrom promise\n",source);
   CfLog(cfverbose,OUTPUT,"");
   ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
   free(destination);
   return NULL;
   }
  
if (server == NULL)
   {
   snprintf(vbuff,CF_BUFSIZE,"%.255s.%.50s_%.50s",source,destination,"localhost");
   }
else
   {
   snprintf(vbuff,CF_BUFSIZE,"%.255s.%.50s_%.50s",source,destination,server);
   }

if (clock_gettime(CLOCK_REALTIME, &start) == -1)
   {
   CfLog(cfverbose,"Clock gettime failure","clock_gettime");
   measured_ok = false;
   }

if (S_ISDIR(ssb.st_mode)) /* could be depth_search */
   {
   strncpy(vbuff,destination,CF_BUFSIZE-4);
   AddSlash(vbuff);
   strcat(vbuff,".");
   }

if (!MakeParentDirectory(vbuff,attr.move_obstructions))
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Can't make directories for %s in files.copyfrom promise\n",vbuff);
   CfLog(cfverbose,OUTPUT,"");
   ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
   ReleaseCurrentLock();
   free(destination);
   return NULL;
   }

if (S_ISDIR(ssb.st_mode)) /* could be depth_search */
   {
   if (attr.copy.purge)
      {
      Verbose(" !! (Destination purging enabled)\n");
      }

   SetSearchDevice(&ssb,pp);
   SourceSearchAndCopy(source,destination,attr.recursion.depth,attr,pp);
   
   if (stat(destination,&dsb) != -1)
      {
      if (attr.copy.check_root)
         {
         VerifyCopiedFileAttributes(source,&dsb,&ssb,attr,pp);
         }
      }
   }
else
   {
   VerifyCopy(source,destination,attr,pp);
   }

if (clock_gettime(CLOCK_REALTIME, &stop) == -1)
   {
   CfLog(cfverbose,"Clock gettime failure","clock_gettime");
   measured_ok = false;
   }

dt = (double)(stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec-start.tv_nsec)/(double)CF_BILLION;
snprintf(eventname,CF_BUFSIZE-1,"Copy(%s:%s > %s)",server,source,destination);

if (measured_ok)
   {
   RecordPerformance(eventname,start.tv_sec,dt);
   }

if (attr.transaction.background)
   {
   ServerDisconnection(pp->conn,attr,pp);
   DeleteAgentConn(pp->conn);
   }

return NULL;
}

/*****************************************************************************/
/* Local low level                                                           */
/*****************************************************************************/

void CheckForFileHoles(struct stat *sstat,struct FileAttr attr,struct Promise *pp)

/* Need a transparent way of getting this into CopyReg() */
/* Use a public member in struct Image                   */

{
#ifndef IRIX
if (sstat->st_size > sstat->st_blocks * DEV_BSIZE)
#else
# ifdef HAVE_ST_BLOCKS
if (sstat->st_size > sstat->st_blocks * DEV_BSIZE)
# else
if (sstat->st_size > ST_NBLOCKS((*sstat)) * DEV_BSIZE)
# endif
#endif
   {
   pp->makeholes = 1;   /* must have a hole to get checksum right */
   }

pp->makeholes = 0;
}

/*********************************************************************/

int CopyRegularFileDisk(char *source,char *new,struct FileAttr attr,struct Promise *pp)

{ int sd, dd, buf_size;
  char *buf, *cp;
  int n_read, *intp;
  long n_read_total = 0;
  int last_write_made_hole = 0;
  
if ((sd = open(source,O_RDONLY|O_BINARY)) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Can't copy %s!\n",source);
   CfLog(cfinform,OUTPUT,"open");
   unlink(new);
   return false;
   }

unlink(new);  /* To avoid link attacks */
 
if ((dd = open(new,O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY, 0600)) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Copy %s:%s possible security violation (race) or permission denied (Not copied)\n",pp->this_server,new);
   CfLog(cfinform,OUTPUT,"open");
   close(sd);
   unlink(new);
   return false;
   }

buf_size = ST_BLKSIZE(dstat);
buf = (char *) malloc(buf_size + sizeof(int));

while (true)
   {
   if ((n_read = read (sd,buf,buf_size)) == -1)
      {
      if (errno == EINTR) 
         {
         continue;
         }

      close(sd);
      close(dd);
      free(buf);
      return false;
      }

   if (n_read == 0)
      {
      break;
      }

   n_read_total += n_read;

   intp = 0;

   if (pp->makeholes)
      {
      buf[n_read] = 1;                    /* Sentinel to stop loop.  */

      /* Find first non-zero *word*, or the word with the sentinel.  */

      intp = (int *) buf;

      while (*intp++ == 0)
         {
         }

      /* Find the first non-zero *byte*, or the sentinel.  */

      cp = (char *) (intp - 1);

      while (*cp++ == 0)
         {
         }

      /* If we found the sentinel, the whole input block was zero,
         and we can make a hole.  */

      if (cp > buf + n_read)
         {
         /* Make a hole.  */
         if (lseek (dd, (off_t) n_read, SEEK_CUR) < 0L)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Copy failed (no space?) while doing %s to %s\n",source,new);
            CfLog(cferror,OUTPUT,"lseek");
            free(buf);
            unlink(new);
            close(dd);
            close(sd);
            return false;
            }
         last_write_made_hole = 1;
         }
      else
         {
         /* Clear to indicate that a normal write is needed. */
         intp = 0;
         }
      }
   
   if (intp == 0)
      {
      if (cf_full_write (dd, buf, n_read) < 0)
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Copy failed (no space?) while doing %s to %s\n",source,new);
         CfLog(cferror,OUTPUT,"");
         close(sd);
         close(dd);
         free(buf);
         unlink(new);
         return false;
         }
      last_write_made_hole = 0;
      }
   }
 
  /* If the file ends with a `hole', something needs to be written at
     the end.  Otherwise the kernel would truncate the file at the end
     of the last write operation.  */

  if (last_write_made_hole)
    {
    /* Write a null character and truncate it again.  */
    
    if (cf_full_write (dd, "", 1) < 0 || ftruncate (dd, n_read_total) < 0)
       {
       CfLog(cferror,"cfengine: full_write or ftruncate error in CopyReg\n","write");
       free(buf);
       unlink(new);
       close(sd);
       close(dd);
       return false;
       }
    }

close(sd);
close(dd);

free(buf);
return true;
}

/*********************************************************************/

int FSWrite(char *new,int dd,char *buf,int towrite,int *last_write_made_hole,int n_read,struct FileAttr attr,struct Promise *pp)

{ int *intp;
  char *cp;
 
 intp = 0;
 
 if (pp->makeholes)
    {
    buf[n_read] = 1;                    /* Sentinel to stop loop.  */
    
    /* Find first non-zero *word*, or the word with the sentinel.  */
    intp = (int *) buf;
    
    while (*intp++ == 0)
       {
       }
    
    /* Find the first non-zero *byte*, or the sentinel.  */
    
    cp = (char *) (intp - 1);
    
    while (*cp++ == 0)
       {
       }
    
    /* If we found the sentinel, the whole input block was zero,
       and we can make a hole.  */
    
    if (cp > buf + n_read)
       {
       /* Make a hole.  */
       if (lseek (dd,(off_t)n_read,SEEK_CUR) < 0L)
          {
          snprintf(OUTPUT,CF_BUFSIZE,"lseek in EmbeddedWrite, dest=%s\n", new);
          CfLog(cferror,OUTPUT,"lseek");
          return false;
          }
       *last_write_made_hole = 1;
       }
    else
       {
       /* Clear to indicate that a normal write is needed. */
       intp = 0;
       }
    }
 
 if (intp == 0)
    {
    if (cf_full_write (dd,buf,towrite) < 0)
       {
       snprintf(OUTPUT,CF_BUFSIZE*2,"Local disk write(%.256s) failed\n",new);
       CfLog(cferror,OUTPUT,"write");
       CONN->error = true;
       return false;
       }

    *last_write_made_hole = 0;
    }

return true;
}
