/*****************************************************************************/
/*                                                                           */
/* File: build_procedures.c                                                  */
/*                                                                           */
/* Created: Fri Dec  4 18:30:52 2009                                         */
/*                                                                           */
/*****************************************************************************/

#include "platform.h"
#include <stdio.h>
#include <string.h>

struct Item
   {
   char   done;
   char  *name;
   char  *classes;
   int    counter;
   struct Item *next;
   };

void PrependItem(struct Item **liststart,char *itemstring,char *classes);
    struct Item *SortItemListNames(struct Item *list);
int IncludeManualFile(FILE *fout,char *file);

    
int main(int argc, char *argv[])

{ FILE *fin,*fout = NULL;
  struct Item *ip,*contents = NULL;
  char buffer[1024],type[1024],control[1024],data[1024],name[1024];

  if (argc != 2)
  {
      fprintf(stderr, "Usage: build-stdlib <cfengine-stdlib.cf>\n");
      return 1;
  }

if ((fin = fopen(argv[1],"r")) == NULL)
   {
   printf("Could not open the %s file\n", argv[1]);
   return 1;
   }

   
while(!feof(fin))
   {
   buffer[0] = '\0';
   control[0] = '\0';
   data[0] = '\0';
   type[0] = '\0';
   fgets(buffer, sizeof(buffer),fin);

   if (strncmp(buffer,"##",2) == 0)
      {
      continue;
      }
   
   sscanf(buffer,"%s %s %[^(\n]",type,data,control);

   if (strncmp(type,"body",4) == 0 || strncmp(type,"bundle",6) == 0)
      {
      if (fout)
         {
         fclose(fout);
         fout = NULL;
         }

      snprintf(name,1024,"%s_%s_%s.tmp",type,data,control);

      if ((fout = fopen(name,"w")) == NULL)
         {
         printf("Could not open output file %s\n",name);
         return 2;
         }

      fprintf(fout,"%s",buffer);
      PrependItem(&contents,buffer,"");
      }
   else if (fout)
      {
      fprintf(fout,"%s",buffer);
      }
   else
      {
      printf("XXXX %s",buffer);
      }
   }

if (fout)
   {
   fclose(fout);
   fout = NULL;
   }

if ((fout = fopen("CfengineStdLibrary.texinfo","w")) == NULL)
   {
   printf("Could not open the CfengineStdLibrary.texinfo file\n");
   return 3;
   }

IncludeManualFile(fout,"preamble.texinfo");

contents = SortItemListNames(contents);

for (ip = contents; ip != NULL; ip=ip->next)
   {
   sscanf(ip->name,"%s %s %[^(\n]",type,data,control);
   fprintf(fout,"@node %s %s %s\n",type,data,control);
   fprintf(fout,"@section %s\n",ip->name);

   fprintf(fout,"@verbatim\n");
   snprintf(name,1024,"%s_%s_%s.tmp",type,data,control);
   IncludeManualFile(fout,name);
   fprintf(fout,"@end verbatim\n\n");
   }

IncludeManualFile(fout,"postamble.texinfo");

fclose(fin);
fclose(fout);
}

/*****************************************************************************/



/*****************************************************************************/

int IncludeManualFile(FILE *fout,char *file)

{ char buffer[1024];
  FILE *fp;

if ((fp = fopen(file,"r")) == NULL)
   {
   printf("Could not read %s\n",file);
   return 1;
   }

while(!feof(fp))
   {
   buffer[0] = '\0';
   fgets(buffer, sizeof(buffer),fp);
   fprintf(fout,"%s",buffer);
   }

fclose(fp);

return 0;
}





/* CODE FROM CFENGINE .....*/

/*********************************************************************/

void PrependItem(struct Item **liststart,char *itemstring,char *classes)

{ struct Item *ip;
  char *sp,*spe = NULL;

if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL)
   {
   return;
   }

if ((sp = malloc(strlen(itemstring)+2)) == NULL)
   {
   return;
   }

if ((classes != NULL) && (spe = malloc(strlen(classes)+2)) == NULL)
   {
   return;
   }

strcpy(sp,itemstring);
ip->name = sp;
ip->next = *liststart;
ip->counter = 0;
*liststart = ip;

if (classes != NULL)
   {
   strcpy(spe,classes);
   ip->classes = spe;
   }
else
   {
   ip->classes = NULL;
   }
}

/*******************************************************************/

/* Borrowed this algorithm from merge-sort implementation */

struct Item *SortItemListNames(struct Item *list) /* Alphabetical */

{ struct Item *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
insize = 1;

while (true)
   {
   p = list;
   list = NULL;
   tail = NULL;
   
   nmerges = 0;  /* count number of merges we do in this pass */
   
   while (p)
      {
      nmerges++;  /* there exists a merge to be done */
      /* step `insize' places along from p */
      q = p;
      psize = 0;
      
      for (i = 0; i < insize; i++)
         {
         psize++;

         q = q->next;

         if (!q)
            {
            break;
            }
         }
      
      /* if q hasn't fallen off end, we have two lists to merge */
      qsize = insize;
      
      /* now we have two lists; merge them */
      while (psize > 0 || (qsize > 0 && q))
         {          
          /* decide whether next element of merge comes from p or q */
         if (psize == 0)
            {
            /* p is empty; e must come from q. */
            e = q;
            q = q->next;
            qsize--;
            }
         else if (qsize == 0 || !q)
            {
            /* q is empty; e must come from p. */
            e = p;
            p = p->next;
            psize--;
            }
         else if (strcmp(p->name, q->name) <= 0)
            {
            /* First element of p is lower (or same);
             * e must come from p. */
            e = p;
            p = p->next;
            psize--;
            }
         else
            {
            /* First element of q is lower; e must come from q. */
            e = q;
            q = q->next;
            qsize--;
            }
         
         /* add the next element to the merged list */
         if (tail)
            {
            tail->next = e;
            }
         else
            {
            list = e;
            }
         
         tail = e;
         }
      
      /* now p has stepped `insize' places along, and q has too */
      p = q;
      }

   tail->next = NULL;
   
   /* If we have done only one merge, we're finished. */
   
   if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
      {
      return list;
      }
   
   /* Otherwise repeat, merging lists twice the size */
   insize *= 2;
   }
}

