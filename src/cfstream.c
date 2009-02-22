/*****************************************************************************/
/*                                                                           */
/* File: cfstream.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include <stdarg.h>

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
   snprintf(output,CF_BUFSIZE-1,"(%s: %s)",errstr,strerror(errno));
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
  char *sp,buffer[CF_BUFSIZE],output[CF_BUFSIZE];
  struct Item *ip,*mess = NULL;
  int verbose;

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
   snprintf(output,CF_BUFSIZE-1,"%s: %s",errstr,strerror(errno));
   AppendItem(&mess,output,NULL);
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

       MakeReport(mess,verbose);
       
       if (attr.transaction.log_level == cf_error)
          {   
          MakeLog(mess,level);
          }
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
