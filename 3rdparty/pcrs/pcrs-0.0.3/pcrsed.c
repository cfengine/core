/*********************************************************************
 *
 * File        :  $Source: /cvsroot/ijbswa/current/pcrsed.c,v $
 *
 * Purpose     :  pcrsed is a small demo application for pcrs.
 *                FWIW, it's licensed under the GPL
 *   
 *********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "pcrs.h"

#define BUFFER_SIZE 100000
 
int main(int Argc, char **Argv)
{
   pcrs_job *job;
   char *result;
   int err, linenum=0;
   size_t length;

   char linebuf[BUFFER_SIZE];
   FILE *in;
 
   /*
    * Check usage
    */
   if (Argc < 2 || Argc > 3)
   {
      fprintf(stderr, "Usage: %s s/pattern/substitute/[options]  [file]\n", Argv[0]);
      return 1;
   }
 
   /* 
    * Open input file 
    */
   if (Argc == 3)
   {
      if (NULL == (in = fopen(Argv[2], "r")))
      {
         fprintf(stderr, "%s: Couldn't open %s\n", Argv[2], strerror(errno));
         return(1);
      }
   }
   else /* Argc == 2 */
   {
      in = stdin;
   }

   /*
    * Compile the job
    */
   if (NULL == (job = pcrs_compile_command(Argv[1], &err)))
   {
      fprintf(stderr, "%s Compile error:  %s (%d).\n", Argv[0], pcrs_strerror(err), err);
      return(1);
   }


   /*
    * Execute on every input line
    */
   while (!feof(in) && !ferror(in) && fgets(linebuf, BUFFER_SIZE, in))
   {
      length = strlen(linebuf);
      linenum++;

      if (0 > (err = pcrs_execute(job, linebuf, length, &result, &length)))
      {
         fprintf(stderr, "%s: Exec error:  %s (%d) in input line %d\n", Argv[0], pcrs_strerror(err), err, linenum);
         return(1);
      }
      else
      {
         fwrite(result, 1, length, stdout);
         free(result);
      }
   }

   pcrs_free_job(job);
   return(0);
 
}
