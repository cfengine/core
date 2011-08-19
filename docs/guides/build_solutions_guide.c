/*****************************************************************************/
/*                                                                           */
/* File: build_solutions_guide.c                                             */
/*                                                                           */
/* Created: Fri Aug 19 11:06:29 2011                                         */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define CF_SCALAR 's'

#define false 0
#define true  1
#define CF_BUFSIZE 2048
#define CF_MAXVARSIZE 1024
#define cf_error 1

/*****************************************************************************/

main(int argc, char **argv)

{
Manual();
}

/*****************************************************************************/

int Manual()

{ FILE *fin,*fout;
  char line[2048];

/* Start with the main doc file */
 
if ((fin = fopen("cf3-solutions.in","r")) == NULL)
   {
   printf("Can't open cf3-solutions.in \n");
   perror("fopen");
   exit(1);
   }

if ((fout = fopen("cf3-solutions.texinfo","w")) == NULL)
   {
   printf("Couldn't write output\n");
   exit(0);
   }


while (!feof(fin))
   {
   line[0] = '\0';
   fgets(line,2048,fin);

   if (strncmp(line,"#CFEexample:",strlen("#CFEexample:")) == 0)
      {
      FILE *tmp;
      char filename[1024];

      // Chop
      line[strlen(line)-1] = '\0';
      
      snprintf(filename,1023,"../../examples/%s",line+strlen("#CFEexample:"));
      
      if ((tmp = fopen(filename,"r")) == NULL)
         {
         printf("File missing (%s)\n",filename);
         exit(0);
         }

      printf("Including %s\n",filename);

      while(!feof(tmp))
         {
         fgets(line,2048,tmp);
         fprintf(fout,"%s",line);
         }
      
      fclose(tmp);
      }
   else
      {
      fprintf(fout,"%s",line);
      }
   }

fclose(fin);
}

