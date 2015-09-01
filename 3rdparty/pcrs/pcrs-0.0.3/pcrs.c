const char pcrs_rcs[] = "$Id: pcrs.c,v 1.19.2.3 2003/12/04 12:32:45 oes Exp $";
 
/*********************************************************************
 *
 * File        :  $Source: /cvsroot/ijbswa/current/Attic/pcrs.c,v $
 *
 * Purpose     :  pcrs is a supplement to the pcre library by Philip Hazel
 *                <ph10@cam.ac.uk> and adds Perl-style substitution. That
 *                is, it mimics Perl's 's' operator. See pcrs(3) for details.
 *
 *
 * Copyright   :  Written and Copyright (C) 2000, 2001 by Andreas S. Oesterhelt
 *                <andreas@oesterhelt.org>
 *
 *                This program is free software; you can redistribute it
 *                and/or modify it under the terms of the GNU Lesser
 *                General Public License (LGPL), version 2.1, which  should
 *                be included in this distribution (see LICENSE.txt), with
 *                the exception that the permission to replace that license
 *                with the GNU General Public License (GPL) given in section
 *                3 is restricted to version 2 of the GPL.
 *
 *                This program is distributed in the hope that it will
 *                be useful, but WITHOUT ANY WARRANTY; without even the
 *                implied warranty of MERCHANTABILITY or FITNESS FOR A
 *                PARTICULAR PURPOSE.  See the license for more details.
 *
 *                The GNU Lesser General Public License should be included
 *                with this file.  If not, you can view it at
 *                http://www.gnu.org/licenses/lgpl.html
 *                or write to the Free Software Foundation, Inc., 59
 *                Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Revisions   :
 *    $Log: pcrs.c,v $
 *    Revision 1.19.2.3  2003/12/04 12:32:45  oes
 *    Append a trailing nullbyte to result to facilitate string processing
 *
 *    Revision 1.19.2.2  2002/10/08 16:22:28  oes
 *    Bugfix: Need to check validity of backreferences explicitly,
 *    because when max_matches are reached and matches is expanded,
 *    realloc() does not zero the memory. Fixes Bug # 606227
 *
 *    Revision 1.19.2.1  2002/08/10 11:23:40  oes
 *    [Irrelevant for stand-alone pcrs]
 *
 *    Revision 1.19  2002/03/08 14:47:48  oes
 *    Cosmetics
 *
 *    Revision 1.18  2002/03/08 14:17:14  oes
 *    Fixing -Wconversion warnings
 *
 *    Revision 1.17  2002/03/08 13:45:48  oes
 *    Hiding internal functions
 *
 *    Revision 1.16  2001/11/30 21:32:14  jongfoster
 *    Fixing signed/unsigned comparison (Andreas please check this!)
 *    One tab->space
 *
 *    Revision 1.15  2001/09/20 16:11:06  steudten
 *
 *    Add casting for some string functions.
 *
 *    Revision 1.14  2001/09/09 21:41:57  oes
 *    Fixing yet another silly bug
 *
 *    Revision 1.13  2001/09/06 14:05:59  oes
 *    Fixed silly bug
 *
 *    Revision 1.12  2001/08/18 11:35:00  oes
 *    - Introduced pcrs_strerror()
 *    - made some NULL arguments non-fatal
 *    - added support for \n \r \e \b \t \f \a \0 in substitute
 *    - made quoting adhere to standard rules
 *    - added warning for bad backrefs
 *    - added pcrs_execute_list()
 *    - fixed comments
 *    - bugfix & cosmetics
 *
 *    Revision 1.11  2001/08/15 15:32:03  oes
 *     - Added support for Perl's special variables $+, $' and $`
 *     - Improved the substitute parser
 *     - Replaced the hard limit for the maximum number of matches
 *       by dynamic reallocation
 *
 *    Revision 1.10  2001/08/05 13:13:11  jongfoster
 *    Making parameters "const" where possible.
 *
 *    Revision 1.9  2001/07/18 17:27:00  oes
 *    Changed interface; Cosmetics
 *
 *    Revision 1.8  2001/06/29 21:45:41  oes
 *    Indentation, CRLF->LF, Tab-> Space
 *
 *    Revision 1.7  2001/06/29 13:33:04  oes
 *    - Cleaned up, renamed and reordered functions,
 *      improved comments
 *    - Removed my_strsep
 *    - Replaced globalflag with a general flags int
 *      that holds PCRS_GLOBAL, PCRS_SUCCESS, and PCRS_TRIVIAL
 *    - Introduced trivial option that will prevent pcrs
 *      from honouring backreferences in the substitute,
 *      which is useful for large substitutes that are
 *      red in from somewhere and saves the pain of escaping
 *      the backrefs
 *    - Introduced convenience function pcrs_free_joblist()
 *    - Split pcrs_make_job() into pcrs_compile(), which still
 *      takes a complete s/// comand as argument and parses it,
 *      and a new function pcrs_make_job, which takes the
 *      three separate components. This should make for a
 *      much friendlier frontend.
 *    - Removed create_pcrs_job() which was useless
 *    - Fixed a bug in pcrs_execute
 *    - Success flag is now handled by pcrs instead of user
 *
 *    Revision 1.6  2001/06/03 19:12:45  oes
 *    added FIXME
 *
 *    Revision 1.5  2001/05/29 09:50:24  jongfoster
 *    (Fixed one int -> size_t)
 *
 *    Revision 1.4  2001/05/25 14:12:40  oes
 *    Fixed bug: Empty substitutes now detected
 *
 *    Revision 1.3  2001/05/25 11:03:55  oes
 *    Added sanity check for NULL jobs to pcrs_exec_substitution
 *
 *    Revision 1.2  2001/05/22 18:46:04  oes
 *
 *      Added support for PCRE_UNGREEDY behaviour to pcrs,
 *      which is selected by the (nonstandard and therefore
 *      capital) letter 'U' in the option string.
 *      It causes the quantifiers to be ungreedy by default.
 *      Appending a ? turns back to greedy (!).
 *
 *    Revision 1.1.1.1  2001/05/15 13:59:02  oes
 *    Initial import of version 2.9.3 source tree
 *
 *
 *********************************************************************/


#include <string.h>
#include <ctype.h>

#include "pcrs.h"

const char pcrs_h_rcs[] = PCRS_H_VERSION;

/*
 * Internal prototypes
 */

static int              pcrs_parse_perl_options(const char *optstring, int *flags);
static pcrs_substitute *pcrs_compile_replacement(const char *replacement, int trivialflag,
                        int capturecount, int *errptr);

/*********************************************************************
 *
 * Function    :  pcrs_strerror
 *
 * Description :  Return a string describing a given error code.
 *             
 * Parameters  :
 *          1  :  error = the error code
 *
 * Returns     :  char * to the descriptive string
 *
 *********************************************************************/
const char *pcrs_strerror(const int error)
{
   if (error < 0)
   {
      switch (error)
      {
         /* Passed-through PCRE error: */
         case PCRE_ERROR_NOMEMORY:     return "(pcre:) No memory";

         /* Shouldn't happen unless PCRE or PCRS bug, or user messed with compiled job: */
         case PCRE_ERROR_NULL:         return "(pcre:) NULL code or subject or ovector";
         case PCRE_ERROR_BADOPTION:    return "(pcre:) Unrecognized option bit";
         case PCRE_ERROR_BADMAGIC:     return "(pcre:) Bad magic number in code";
         case PCRE_ERROR_UNKNOWN_NODE: return "(pcre:) Bad node in pattern";

         /* Can't happen / not passed: */
         case PCRE_ERROR_NOSUBSTRING:  return "(pcre:) Fire in power supply"; 
         case PCRE_ERROR_NOMATCH:      return "(pcre:) Water in power supply";

         /* PCRS errors: */
         case PCRS_ERR_NOMEM:          return "(pcrs:) No memory";
         case PCRS_ERR_CMDSYNTAX:      return "(pcrs:) Syntax error while parsing command";
         case PCRS_ERR_STUDY:          return "(pcrs:) PCRE error while studying the pattern";
         case PCRS_ERR_BADJOB:         return "(pcrs:) Bad job - NULL job, pattern or substitute";
         case PCRS_WARN_BADREF:        return "(pcrs:) Backreference out of range";

         /* What's that? */
         default:  return "Unknown error";
      }
   }
   /* error >= 0: No error */
   return "(pcrs:) Everything's just fine. Thanks for asking.";

}


/*********************************************************************
 *
 * Function    :  pcrs_parse_perl_options
 *
 * Description :  This function parses a string containing the options to
 *                Perl's s/// operator. It returns an integer that is the
 *                pcre equivalent of the symbolic optstring.
 *                Since pcre doesn't know about Perl's 'g' (global) or pcrs',
 *                'T' (trivial) options but pcrs needs them, the corresponding
 *                flags are set if 'g'or 'T' is encountered.
 *                Note: The 'T' and 'U' options do not conform to Perl.
 *             
 * Parameters  :
 *          1  :  optstring = string with options in perl syntax
 *          2  :  flags = see description
 *
 * Returns     :  option integer suitable for pcre 
 *
 *********************************************************************/
static int pcrs_parse_perl_options(const char *optstring, int *flags)
{
   size_t i;
   int rc = 0;
   *flags = 0;

   if (NULL == optstring) return 0;

   for (i = 0; i < strlen(optstring); i++)
   {
      switch(optstring[i])
      {
         case 'e': break; /* ToDo ;-) */
         case 'g': *flags |= PCRS_GLOBAL; break;
         case 'i': rc |= PCRE_CASELESS; break;
         case 'm': rc |= PCRE_MULTILINE; break;
         case 'o': break;
         case 's': rc |= PCRE_DOTALL; break;
         case 'x': rc |= PCRE_EXTENDED; break;
         case 'U': rc |= PCRE_UNGREEDY; break;
         case 'T': *flags |= PCRS_TRIVIAL; break;
         default: break;
      }
   }
   return rc;

}


/*********************************************************************
 *
 * Function    :  pcrs_compile_replacement
 *
 * Description :  This function takes a Perl-style replacement (2nd argument
 *                to the s/// operator and returns a compiled pcrs_substitute,
 *                or NULL if memory allocation for the substitute structure
 *                fails.
 *
 * Parameters  :
 *          1  :  replacement = replacement part of s/// operator
 *                              in perl syntax
 *          2  :  trivialflag = Flag that causes backreferences to be
 *                              ignored.
 *          3  :  capturecount = Number of capturing subpatterns in
 *                               the pattern. Needed for $+ handling.
 *          4  :  errptr = pointer to an integer in which error
 *                         conditions can be returned.
 *
 * Returns     :  pcrs_substitute data structure, or NULL if an
 *                error is encountered. In that case, *errptr has
 *                the reason.
 *
 *********************************************************************/
static pcrs_substitute *pcrs_compile_replacement(const char *replacement, int trivialflag, int capturecount, int *errptr)
{
   int i, k, l, quoted;
   size_t length;
   char *text;
   pcrs_substitute *r;

   i = k = l = quoted = 0;

   /*
    * Sanity check
    */
   if (NULL == replacement)
   {
      replacement = "";
   }

   /*
    * Get memory or fail
    */
   if (NULL == (r = (pcrs_substitute *)malloc(sizeof(pcrs_substitute))))
   {
      *errptr = PCRS_ERR_NOMEM;
      return NULL;
   }
   memset(r, '\0', sizeof(pcrs_substitute));

   length = strlen(replacement);

   if (NULL == (text = (char *)malloc(length + 1)))
   {
      free(r);
      *errptr = PCRS_ERR_NOMEM;
      return NULL;
   }
   memset(text, '\0', length + 1);
   

   /*
    * In trivial mode, just copy the substitute text
    */
   if (trivialflag)
   {
      text = strncpy(text, replacement, length + 1);
      k = length;
   }

   /*
    * Else, parse, cut out and record all backreferences
    */
   else
   {
      while (i < (int)length)
      {
         /* Quoting */
         if (replacement[i] == '\\')
         {
            if (quoted)
            {
               text[k++] = replacement[i++];
               quoted = 0;
            }
            else
            {
               if (replacement[i+1] && strchr("tnrfae0", replacement[i+1]))
               {
                  switch (replacement[++i])
                  {
                  case 't':
                     text[k++] = '\t';
                     break;
                  case 'n':
                     text[k++] = '\n';
                     break;
                  case 'r':
                     text[k++] = '\r';
                     break;
                  case 'f':
                     text[k++] = '\f';
                     break;
                  case 'a':
                     text[k++] = 7;
                     break;
                  case 'e':
                     text[k++] = 27;
                     break;
                  case '0':
                     text[k++] = '\0';
                     break;
                  }
                  i++;
               }
               else
               {
                  quoted = 1;
                  i++;
               }
            }
            continue;
         }

         /* Backreferences */
         if (replacement[i] == '$' && !quoted && i < (int)(length - 1))
         {
            char *symbol, symbols[] = "'`+&";
            r->block_length[l] = k - r->block_offset[l];

            /* Numerical backreferences */
            if (isdigit((int)replacement[i + 1]))
            {
               while (i < (int)length && isdigit((int)replacement[++i]))
               {
                  r->backref[l] = r->backref[l] * 10 + replacement[i] - 48;
               }
               if (r->backref[l] > capturecount)
               {
                  *errptr = PCRS_WARN_BADREF;
               }
            }

            /* Symbolic backreferences: */
            else if (NULL != (symbol = strchr(symbols, replacement[i + 1])))
            {
               
               if (symbol - symbols == 2) /* $+ */
               {
                  r->backref[l] = capturecount;
               }
               else if (symbol - symbols == 3) /* $& */
               {
                  r->backref[l] = 0;
               }
               else /* $' or $` */
               {
                  r->backref[l] = PCRS_MAX_SUBMATCHES + 1 - (symbol - symbols);
               }
               i += 2;
            }

            /* Invalid backref -> plain '$' */
            else
            {
               goto plainchar;
            }

            /* Valid and in range? -> record */
            if (r->backref[l] < PCRS_MAX_SUBMATCHES + 2)
            {
               r->backref_count[r->backref[l]] += 1;
               r->block_offset[++l] = k;
            }
            else
            {
               *errptr = PCRS_WARN_BADREF;
            }   
            continue;
         }
         
plainchar:
         /* Plain chars are copied */
         text[k++] = replacement[i++];
         quoted = 0;
      }
   } /* -END- if (!trivialflag) */

   /*
    * Finish & return
    */
   r->text = text;
   r->backrefs = l;
   r->block_length[l] = k - r->block_offset[l];

   return r;

}


/*********************************************************************
 *
 * Function    :  pcrs_free_job
 *
 * Description :  Frees the memory used by a pcrs_job struct and its
 *                dependant structures.
 *
 * Parameters  :
 *          1  :  job = pointer to the pcrs_job structure to be freed
 *
 * Returns     :  a pointer to the next job, if there was any, or
 *                NULL otherwise. 
 *
 *********************************************************************/
pcrs_job *pcrs_free_job(pcrs_job *job)
{
   pcrs_job *next;

   if (job == NULL)
   {
      return NULL;
   }
   else
   {
      next = job->next;
      if (job->pattern != NULL) free(job->pattern);
      if (job->hints != NULL) free(job->hints);
      if (job->substitute != NULL)
      {
         if (job->substitute->text != NULL) free(job->substitute->text);
         free(job->substitute);
      }
      free(job);
   }
   return next;

}


/*********************************************************************
 *
 * Function    :  pcrs_free_joblist
 *
 * Description :  Iterates through a chained list of pcrs_job's and
 *                frees them using pcrs_free_job.
 *
 * Parameters  :
 *          1  :  joblist = pointer to the first pcrs_job structure to
 *                be freed
 *
 * Returns     :  N/A
 *
 *********************************************************************/
void pcrs_free_joblist(pcrs_job *joblist)
{
   while ( NULL != (joblist = pcrs_free_job(joblist)) ) {};

   return;

}


/*********************************************************************
 *
 * Function    :  pcrs_compile_command
 *
 * Description :  Parses a string with a Perl-style s/// command, 
 *                calls pcrs_compile, and returns a corresponding
 *                pcrs_job, or NULL if parsing or compiling the job
 *                fails.
 *
 * Parameters  :
 *          1  :  command = string with perl-style s/// command
 *          2  :  errptr = pointer to an integer in which error
 *                         conditions can be returned.
 *
 * Returns     :  a corresponding pcrs_job data structure, or NULL
 *                if an error was encountered. In that case, *errptr
 *                has the reason.
 *
 *********************************************************************/
pcrs_job *pcrs_compile_command(const char *command, int *errptr)
{
   int i, k, l, quoted = FALSE;
   size_t limit;
   char delimiter;
   char *tokens[4];   
   pcrs_job *newjob;
   
   i = k = l = 0;
   
   /*
    * Tokenize the perl command
    */
   limit = strlen(command);
   if (limit < 4)
   {
      *errptr = PCRS_ERR_CMDSYNTAX;
      return NULL;
   }
   else
   {
      delimiter = command[1];
   }

   tokens[l] = (char *) malloc(limit + 1);

   for (i = 0; i <= (int)limit; i++)
   {
      
      if (command[i] == delimiter && !quoted)
      {
         if (l == 3)
         {
            l = -1;
            break;
         }
         tokens[0][k++] = '\0';
         tokens[++l] = tokens[0] + k;
         continue;
      }
      
      else if (command[i] == '\\' && !quoted)
      {
         quoted = TRUE;
         if (command[i+1] == delimiter) continue;
      }
      else
      {
         quoted = FALSE;
      }
      tokens[0][k++] = command[i];
   }

   /*
    * Syntax error ?
    */
   if (l != 3)
   {
      *errptr = PCRS_ERR_CMDSYNTAX;
      free(tokens[0]);
      return NULL;
   }
   
   newjob = pcrs_compile(tokens[1], tokens[2], tokens[3], errptr);
   free(tokens[0]);
   return newjob;
   
}


/*********************************************************************
 *
 * Function    :  pcrs_compile
 *
 * Description :  Takes the three arguments to a perl s/// command
 *                and compiles a pcrs_job structure from them.
 *
 * Parameters  :
 *          1  :  pattern = string with perl-style pattern
 *          2  :  substitute = string with perl-style substitute
 *          3  :  options = string with perl-style options
 *          4  :  errptr = pointer to an integer in which error
 *                         conditions can be returned.
 *
 * Returns     :  a corresponding pcrs_job data structure, or NULL
 *                if an error was encountered. In that case, *errptr
 *                has the reason.
 *
 *********************************************************************/
pcrs_job *pcrs_compile(const char *pattern, const char *substitute, const char *options, int *errptr)
{
   pcrs_job *newjob;
   int flags;
   int capturecount;
   const char *error;

   *errptr = 0;

   /* 
    * Handle NULL arguments
    */
   if (pattern == NULL) pattern = "";
   if (substitute == NULL) substitute = "";


   /* 
    * Get and init memory
    */
   if (NULL == (newjob = (pcrs_job *)malloc(sizeof(pcrs_job))))
   {
      *errptr = PCRS_ERR_NOMEM;
      return NULL;
   }
   memset(newjob, '\0', sizeof(pcrs_job));


   /*
    * Evaluate the options
    */
   newjob->options = pcrs_parse_perl_options(options, &flags);
   newjob->flags = flags;


   /*
    * Compile the pattern
    */
   newjob->pattern = pcre_compile(pattern, newjob->options, &error, errptr, NULL);
   if (newjob->pattern == NULL)
   {
      pcrs_free_job(newjob);
      return NULL;
   }


   /*
    * Generate hints. This has little overhead, since the
    * hints will be NULL for a boring pattern anyway.
    */
   newjob->hints = pcre_study(newjob->pattern, 0, &error);
   if (error != NULL)
   {
      *errptr = PCRS_ERR_STUDY;
      pcrs_free_job(newjob);
      return NULL;
   }
 

   /* 
    * Determine the number of capturing subpatterns. 
    * This is needed for handling $+ in the substitute.
    */
   if (0 > (*errptr = pcre_fullinfo(newjob->pattern, newjob->hints, PCRE_INFO_CAPTURECOUNT, &capturecount)))
   {
      pcrs_free_job(newjob);
      return NULL;
   }
 

   /*
    * Compile the substitute
    */
   if (NULL == (newjob->substitute = pcrs_compile_replacement(substitute, newjob->flags & PCRS_TRIVIAL, capturecount, errptr)))
   {
      pcrs_free_job(newjob);
      return NULL;
   }
 
   return newjob;

}


/*********************************************************************
 *
 * Function    :  pcrs_execute_list
 *
 * Description :  This is a multiple job wrapper for pcrs_execute().
 *                Apply the regular substitutions defined by the jobs in
 *                the joblist to the subject.
 *                The subject itself is left untouched, memory for the result
 *                is malloc()ed and it is the caller's responsibility to free
 *                the result when it's no longer needed. 
 *
 *                Note: For convenient string handling, a null byte is
 *                      appended to the result. It does not count towards the
 *                      result_length, though.
 *
 *
 * Parameters  :
 *          1  :  joblist = the chained list of pcrs_jobs to be executed
 *          2  :  subject = the subject string
 *          3  :  subject_length = the subject's length 
 *          4  :  result = char** for returning  the result 
 *          5  :  result_length = size_t* for returning the result's length
 *
 * Returns     :  On success, the number of substitutions that were made.
 *                 May be > 1 if job->flags contained PCRS_GLOBAL
 *                On failiure, the (negative) pcre error code describing the
 *                 failiure, which may be translated to text using pcrs_strerror().
 *
 *********************************************************************/
int pcrs_execute_list(pcrs_job *joblist, char *subject, size_t subject_length, char **result, size_t *result_length)
{
   pcrs_job *job;
   char *old, *new;
   int hits, total_hits;
 
   old = subject;
   *result_length = subject_length;
   hits = total_hits = 0;

   for (job = joblist; job != NULL; job = job->next)
   {
      hits = pcrs_execute(job, old, *result_length, &new, result_length);

      if (old != subject) free(old);

      if (hits < 0)
      {
         return(hits);
      }
      else
      {
         total_hits += hits;
         old = new;
      }
   }

   *result = new;
   return(total_hits);

}


/*********************************************************************
 *
 * Function    :  pcrs_execute
 *
 * Description :  Apply the regular substitution defined by the job to the
 *                subject.
 *                The subject itself is left untouched, memory for the result
 *                is malloc()ed and it is the caller's responsibility to free
 *                the result when it's no longer needed.
 *
 *                Note: For convenient string handling, a null byte is
 *                      appended to the result. It does not count towards the
 *                      result_length, though.
 *
 * Parameters  :
 *          1  :  job = the pcrs_job to be executed
 *          2  :  subject = the subject (== original) string
 *          3  :  subject_length = the subject's length 
 *          4  :  result = char** for returning  the result 
 *          5  :  result_length = size_t* for returning the result's length
 *
 * Returns     :  On success, the number of substitutions that were made.
 *                 May be > 1 if job->flags contained PCRS_GLOBAL
 *                On failiure, the (negative) pcre error code describing the
 *                 failiure, which may be translated to text using pcrs_strerror().
 *
 *********************************************************************/
int pcrs_execute(pcrs_job *job, char *subject, size_t subject_length, char **result, size_t *result_length)
{
   int offsets[3 * PCRS_MAX_SUBMATCHES],
       offset,
       i, k,
       matches_found,
       submatches,
       max_matches = PCRS_MAX_MATCH_INIT;
   size_t newsize;
   pcrs_match *matches, *dummy;
   char *result_offset;

   offset = i = k = 0;

   /* 
    * Sanity check & memory allocation
    */
   if (job == NULL || job->pattern == NULL || job->substitute == NULL)
   {
      *result = NULL;
      return(PCRS_ERR_BADJOB);
   }

   if (NULL == (matches = (pcrs_match *)malloc(max_matches * sizeof(pcrs_match))))
   {
      *result = NULL;
      return(PCRS_ERR_NOMEM);
   }
   memset(matches, '\0', max_matches * sizeof(pcrs_match));


   /*
    * Find the pattern and calculate the space
    * requirements for the result
    */
   newsize = subject_length;

   while ((submatches = pcre_exec(job->pattern, job->hints, subject, (int)subject_length, offset, 0, offsets, 3 * PCRS_MAX_SUBMATCHES)) > 0)
   {
      job->flags |= PCRS_SUCCESS;
      matches[i].submatches = submatches;

      for (k = 0; k < submatches; k++)
      {
         matches[i].submatch_offset[k] = offsets[2 * k];

         /* Note: Non-found optional submatches have length -1-(-1)==0 */
         matches[i].submatch_length[k] = offsets[2 * k + 1] - offsets[2 * k]; 

         /* reserve mem for each submatch as often as it is ref'd */
         newsize += matches[i].submatch_length[k] * job->substitute->backref_count[k];
      }
      /* plus replacement text size minus match text size */
      newsize += strlen(job->substitute->text) - matches[i].submatch_length[0]; 

      /* chunk before match */
      matches[i].submatch_offset[PCRS_MAX_SUBMATCHES] = 0;
      matches[i].submatch_length[PCRS_MAX_SUBMATCHES] = offsets[0];
      newsize += offsets[0] * job->substitute->backref_count[PCRS_MAX_SUBMATCHES];

      /* chunk after match */
      matches[i].submatch_offset[PCRS_MAX_SUBMATCHES + 1] = offsets[1];
      matches[i].submatch_length[PCRS_MAX_SUBMATCHES + 1] = subject_length - offsets[1] - 1;
      newsize += (subject_length - offsets[1]) * job->substitute->backref_count[PCRS_MAX_SUBMATCHES + 1];

      /* Storage for matches exhausted? -> Extend! */
      if (++i >= max_matches)
      {
         max_matches = (int)(max_matches * PCRS_MAX_MATCH_GROW);
         if (NULL == (dummy = (pcrs_match *)realloc(matches, max_matches * sizeof(pcrs_match))))
         {
            free(matches);
            *result = NULL;
            return(PCRS_ERR_NOMEM);
         }
         matches = dummy;
      }

      /* Non-global search or limit reached? */
      if (!(job->flags & PCRS_GLOBAL)) break;

      /* Don't loop on empty matches */
      if (offsets[1] == offset)
         if ((size_t)offset < subject_length)
            offset++;
         else
            break;
      /* Go find the next one */
      else
         offset = offsets[1];
   }
   /* Pass pcre error through if (bad) failiure */
   if (submatches < PCRE_ERROR_NOMATCH)
   {
      free(matches);
      return submatches;   
   }
   matches_found = i;


   /* 
    * Get memory for the result (must be freed by caller!)
    * and append terminating null byte.
    */
   if ((*result = (char *)malloc(newsize + 1)) == NULL)
   {
      free(matches);
      return PCRS_ERR_NOMEM;
   }
   else
   {
      (*result)[newsize] = '\0';
   }


   /* 
    * Replace
    */
   offset = 0;
   result_offset = *result;

   for (i = 0; i < matches_found; i++)
   {
      /* copy the chunk preceding the match */
      memcpy(result_offset, subject + offset, (size_t)matches[i].submatch_offset[0] - offset); 
      result_offset += matches[i].submatch_offset[0] - offset;

      /* For every segment of the substitute.. */
      for (k = 0; k <= job->substitute->backrefs; k++)
      {
         /* ...copy its text.. */
         memcpy(result_offset, job->substitute->text + job->substitute->block_offset[k], job->substitute->block_length[k]);
         result_offset += job->substitute->block_length[k];

         /* ..plus, if it's not the last chunk, i.e.: There *is* a backref.. */
         if (k != job->substitute->backrefs
             /* ..in legal range.. */
             && job->substitute->backref[k] < PCRS_MAX_SUBMATCHES + 2
             /* ..and referencing a real submatch.. */
             && job->substitute->backref[k] < matches[i].submatches
             /* ..that is nonempty.. */
             && matches[i].submatch_length[job->substitute->backref[k]] > 0)
         {
            /* ..copy the submatch that is ref'd. */
            memcpy(
               result_offset,
               subject + matches[i].submatch_offset[job->substitute->backref[k]],
               matches[i].submatch_length[job->substitute->backref[k]]
            );
            result_offset += matches[i].submatch_length[job->substitute->backref[k]];
         }
      }
      offset =  matches[i].submatch_offset[0] + matches[i].submatch_length[0];
   }

   /* Copy the rest. */
   memcpy(result_offset, subject + offset, subject_length - offset);

   *result_length = newsize;
   free(matches);
   return matches_found;

}


/*
  Local Variables:
  tab-width: 3
  end:
*/
