
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
/* File: cfstream.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include <stdarg.h>

/*****************************************************************************/

void CfFOut(char *filename,enum cfreport level,char *errstr,char *fmt, ...)

{ va_list ap;
  char *sp,buffer[CF_BUFSIZE],output[CF_BUFSIZE];
  struct Item *ip,*mess = NULL;

if ((fmt == NULL) || (strlen(fmt) == 0))
   {
   return;
   }

memset(output,0,CF_BUFSIZE);
va_start(ap,fmt);
vsnprintf(buffer,CF_BUFSIZE-1,fmt,ap);
va_end(ap);
SanitizeBuffer(buffer);
Chop(buffer);
AppendItem(&mess,buffer,NULL);

if ((errstr == NULL) || (strlen(errstr) > 0))
   {
   snprintf(output,CF_BUFSIZE-1," !!! System reports error for %s: \"%s\"",errstr,strerror(errno));
   AppendItem(&mess,output,NULL);
   }

switch(level)
   {
   case cf_inform:
       
       if (INFORM || VERBOSE || DEBUG)
          {
          FileReport(mess,VERBOSE,filename);
          }
       break;
       
   case cf_verbose:
       
       if (VERBOSE || DEBUG)
          {
          FileReport(mess,VERBOSE,filename);
          }       
       break;

   case cf_error:

       FileReport(mess,VERBOSE,filename);
       MakeLog(mess,level);
       break;

   case cf_log:
       
       if (VERBOSE || DEBUG)
          {
          FileReport(mess,VERBOSE,filename);
          }
       MakeLog(mess,cf_verbose);
       break;
       
   default:
       
       FatalError("Report level unknown");
       break;       
   }

DeleteItemList(mess);
}

/*****************************************************************************/

void CfOut(enum cfreport level,char *errstr,char *fmt, ...)

{ va_list ap;
  char *sp,buffer[CF_BUFSIZE],output[CF_BUFSIZE];
  struct Item *ip,*mess = NULL;

if ((fmt == NULL) || (strlen(fmt) == 0))
   {
   return;
   }

memset(output,0,CF_BUFSIZE);
va_start(ap,fmt);
vsnprintf(buffer,CF_BUFSIZE-1,fmt,ap);
va_end(ap);
SanitizeBuffer(buffer);
Chop(buffer);
AppendItem(&mess,buffer,NULL);

if ((errstr == NULL) || (strlen(errstr) > 0))
   {
   snprintf(output,CF_BUFSIZE-1," !!! System error for %s: \"%s\"",errstr,strerror(errno));
   AppendItem(&mess,output,NULL);
   }

switch(level)
   {
   case cf_inform:
       
       if (INFORM || VERBOSE || DEBUG)
          {
          MakeReport(mess,VERBOSE);
          }
       break;
       
   case cf_verbose:
       
       if (VERBOSE || DEBUG)
          {
          MakeReport(mess,VERBOSE);
          }       
       break;

   case cf_error:

       MakeReport(mess,VERBOSE);
       MakeLog(mess,level);
       break;

   case cf_log:
       
       if (VERBOSE || DEBUG)
          {
          MakeReport(mess,VERBOSE);
          }
       MakeLog(mess,cf_verbose);
       break;
       
   default:
       
       FatalError("Report level unknown");
       break;       
   }

DeleteItemList(mess);
}

/*****************************************************************************/

void cfPS(enum cfreport level,char status,char *errstr,struct Promise *pp,struct Attributes attr,char *fmt, ...)

{ va_list ap;
  char rettype,*sp,buffer[CF_BUFSIZE],output[CF_BUFSIZE],*v,handle[CF_MAXVARSIZE];
  struct Item *ip,*mess = NULL;
  int verbose;
  struct Rlist *rp;
  void *retval;
  
if ((fmt == NULL) || (strlen(fmt) == 0))
   {
   return;
   }

va_start(ap,fmt);
vsnprintf(buffer,CF_BUFSIZE-1,fmt,ap);
va_end(ap);

SanitizeBuffer(buffer);
Chop(buffer);
AppendItem(&mess,buffer,NULL);

if ((errstr == NULL) || (strlen(errstr) > 0))
   {
   snprintf(output,CF_BUFSIZE-1," !!! System reports error for %s: \"%s\"",errstr,strerror(errno));
   AppendItem(&mess,output,NULL);
   }

if (level == cf_error)
   {
   if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
      {
      v = (char *)retval;
      }
   else
      {
      v = "not specified";
      }
   
   if ((sp = GetConstraint("handle",pp->conlist,CF_SCALAR)) || (sp = PromiseID(pp)))
      {
      strncpy(handle,sp,CF_MAXVARSIZE-1);
      }
   else
      {
      strcpy(handle,"(unknown)");
      }

   if (INFORM || VERBOSE || DEBUG)
      {
      snprintf(output,CF_BUFSIZE-1,"I: Report relates to a promise with handle \"%s\"",handle);
      AppendItem(&mess,output,NULL);
      }

   if (pp->audit)
      {
      snprintf(output,CF_BUFSIZE-1,"I: Made in version \'%s\' of \'%s\' near line %d",v,pp->audit->filename,pp->lineno);
      }
   else
      {
      snprintf(output,CF_BUFSIZE-1,"I: Promise is made internally by cfengine");
      }

   AppendItem(&mess,output,NULL);

   
   switch (pp->petype)
      {
      case CF_SCALAR:
          
          snprintf(output,CF_BUFSIZE-1,"I: The promise was made to: \'%s\'\n",pp->promisee);
          AppendItem(&mess,output,NULL);
          break;
          
      case CF_LIST:
          
          CfOut(level,"","I: The promise was made to: \n");
          
          for (rp = (struct Rlist *)pp->promisee; rp != NULL; rp=rp->next)
             {
             snprintf(output,CF_BUFSIZE-1,"I:     \'%s\'\n",rp->item);
             AppendItem(&mess,output,NULL);
             }
          break;          
      }
   
   if (pp->ref)
      {
      snprintf(output,CF_BUFSIZE-1,"I: Comment: %s\n",pp->ref);
      AppendItem(&mess,output,NULL);
      }
   }

verbose = (attr.transaction.report_level == cf_verbose) || VERBOSE;

switch(level)
   {
   case cf_inform:
       
       if (INFORM || verbose || DEBUG || attr.transaction.report_level == cf_inform)
          {
          MakeReport(mess,verbose);
          }
       
       if (attr.transaction.log_level == cf_inform)
          {
          MakeLog(mess,level);
          }
       break;
       
   case cf_verbose:
       
       if (verbose || DEBUG)
          {
          MakeReport(mess,verbose);
          }
       
       if (attr.transaction.log_level == cf_verbose)
          {
          MakeLog(mess,level);
          }

       break;

   case cf_error:

       if (attr.report.to_file)
          {
          FileReport(mess,verbose,attr.report.to_file);
          }
       else
          {
          MakeReport(mess,verbose);
          }
       
       if (attr.transaction.log_level == cf_error)
          {   
          MakeLog(mess,level);
          }
       break;

   case cf_log:
       
       MakeLog(mess,level);
       break;
       
   default:
       
       FatalError("Software error: report level unknown: require cf_error, cf_inform, cf_verbose");
       break;       
   }


/* Now complete the exits status classes and auditing */

if (pp != NULL)
   {
   for (ip = mess; ip != NULL; ip = ip->next)
      {
      ClassAuditLog(pp,attr,ip->name,status);
      }
   }

DeleteItemList(mess);
}



/*********************************************************************************/

void CfFile(FILE *fp,char *fmt, ...)

{ va_list ap;
  char *sp,buffer[CF_BUFSIZE];
  int endl = false;

if ((fmt == NULL) || (strlen(fmt) == 0))
   {
   return;
   }

va_start(ap,fmt);
vsnprintf(buffer,CF_BUFSIZE-1,fmt,ap);
va_end(ap);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
   {
   return;
   }
#endif

fprintf(fp,"%s %s",VPREFIX,buffer);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
   {
   /* CfLog(cferror,"pthread_mutex_unlock failed","lock");*/
   }
#endif 
}

/*********************************************************************************/
/* Level                                                                         */
/*********************************************************************************/

void MakeLog(struct Item *mess,enum cfreport level)

{ struct Item *ip;

if (!IsPrivileged() || DONTDO)
   {
   return;
   }
 
/* If we can't mutex it could be dangerous to proceed with threaded file descriptors */

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
   {
   return;
   }
#endif
 
for (ip = mess; ip != NULL; ip = ip->next)
   {
   switch (level)
      {
      case cf_inform:
          syslog(LOG_NOTICE," %s",ip->name);
          break;
          
      case cf_verbose:
          syslog(LOG_INFO," %s",ip->name);
          break;
          
      case cf_error:
          syslog(LOG_ERR," %s",ip->name);
          break;

      default:
          break;
      }
   }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
   {
   /* CfLog(cferror,"pthread_mutex_unlock failed","lock");*/
   }
#endif 

}

/*********************************************************************************/

void MakeReport(struct Item *mess,int prefix)

{ struct Item *ip;

for (ip = mess; ip != NULL; ip = ip->next)
   {
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
   if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
      {
      return;
      }
#endif
   
   if (prefix)
      {
      printf("%s %s\n",VPREFIX,ip->name);
      }
   else
      {
      printf("%s\n",ip->name);
      }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
   if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
      {
      /* CfLog(cferror,"pthread_mutex_unlock failed","lock");*/
      }
#endif    
   }
}

/*********************************************************************************/

void FileReport(struct Item *mess,int prefix,char *filename)

{ struct Item *ip;
  FILE *fp;

if ((fp = fopen(filename,"a")) == NULL)
   {
   CfOut(cf_error,"fopen","Could not open log file %s\n",filename);
   fp = stdout;
   }
  
for (ip = mess; ip != NULL; ip = ip->next)
   {
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
   if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)
      {
      return;
      }
#endif
   
   if (prefix)
      {
      fprintf(fp,"%s %s\n",VPREFIX,ip->name);
      }
   else
      {
      fprintf(fp,"%s\n",ip->name);
      }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
   if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0)
      {
      /* CfLog(cferror,"pthread_mutex_unlock failed","lock");*/
      }
#endif
   }

if (fp != stdout)
   {
   fclose(fp);
   }
}

/*********************************************************************************/

void SanitizeBuffer(char *buffer)

{ char *sp;

 /* Check for %s %m which someone might be able to insert into
   an error message in order to get a syslog buffer overflow...
   bug reported by Pekka Savola */
 
for (sp = buffer; *sp != '\0'; sp++)
   {
   if ((*sp == '%') && (*(sp+1) >= 'a'))
      {
      *sp = '?';
      }
   }
}
