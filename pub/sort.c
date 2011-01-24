/*****************************************************************************/
/*                                                                           */
/* File: sort.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "../src/cf3.defs.h"
#include "../src/cf3.extern.h"

/*******************************************************************/

/* The following sort functions are trivial rewrites of merge-sort
 * implementation by Simon Tatham 
 * copyright 2001 Simon Tatham.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


struct Item *SortItemListNames(struct Item *list) /* Alphabetical */

{ struct Item *p, *q, *e, *tail, *oldhead;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
insize = 1;

while (true)
   {
   p = list;
   oldhead = list;                /* only used for circular linkage */
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

/*******************************************************************/

struct Item *SortItemListClasses(struct Item *list) /* Alphabetical */

{ struct Item *p, *q, *e, *tail, *oldhead;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
insize = 1;

while (true)
   {
   p = list;
   oldhead = list;                /* only used for circular linkage */
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
         else if (strcmp(p->classes, q->classes) <= 0)
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

/*******************************************************************/

struct Item *SortItemListCounters(struct Item *list) /* Biggest first */

{ struct Item *p, *q, *e, *tail, *oldhead;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
insize = 1;

while (true)
   {
   p = list;
   oldhead = list;                /* only used for circular linkage */
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
         else if (p->counter - q->counter >= 0)
            {
            /* First element of p is higher (or same);
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

/*******************************************************************/

struct Item *SortItemListTimes(struct Item *list) /* Biggest first */

{ struct Item *p, *q, *e, *tail, *oldhead;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
insize = 1;

while (true)
   {
   p = list;
   oldhead = list;                /* only used for circular linkage */
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
         else if (p->time - q->time >= 0)
            {
            /* First element of p is higher (or same);
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

/*******************************************************************/

struct Rlist *SortRlist(struct Rlist *list, int (*CompareItems)())
/**
 * Sorts an Rlist on list->item. A function CompareItems(i1,i2)
 * must be written for this particular item, which returns
 * true if i1 <= i2, false otherwise.
 **/

{ struct Rlist *p = NULL, *q = NULL, *e = NULL, *tail = NULL, *oldhead = NULL;
  int insize = 0, nmerges = 0, psize = 0, qsize = 0, i = 0;

if (list == NULL)
   { 
   return NULL;
   }
 
insize = 1;

while (true)
   {
   p = list;
   oldhead = list;                /* only used for circular linkage */
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
         else if (CompareItems(p->item,q->item))
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

/*******************************************************************/

struct Rlist *AlphaSortRListNames(struct Rlist *list)

/* Borrowed this algorithm from merge-sort implementation */

{ struct Rlist *p, *q, *e, *tail, *oldhead;
  int insize, nmerges, psize, qsize, i;

if (list == NULL)
   { 
   return NULL;
   }
 
 insize = 1;
 
 while (true)
    {
    p = list;
    oldhead = list;                /* only used for circular linkage */
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
             e = q; q = q->next; qsize--;
             }
          else if (qsize == 0 || !q)
             {
             /* q is empty; e must come from p. */
             e = p; p = p->next; psize--;
             }
          else if (strcmp(p->item, q->item) <= 0)
             {
             /* First element of p is lower (or same);
              * e must come from p. */
             e = p; p = p->next; psize--;
             }
          else
             {
             /* First element of q is lower; e must come from q. */
             e = q; q = q->next; qsize--;
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

