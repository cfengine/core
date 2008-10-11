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
/* File: graph.c                                                             */
/*                                                                           */
/*****************************************************************************/

#define CF_TRIBE_SIZE 30

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef HAVE_LIBGVC
# ifdef HAVE_GRAPHVIZ_GVC_H
#  include <graphviz/gvc.h>
# endif

/*****************************************************************************/

double Acc(double *v1, double *v2);
int Degree(double *m,int dim);
void PrintNeighbours(double *m,int dim,char **names);
void EigenvectorCentrality(double **A,double *v,int dim);
void MatrixOperation(double **A,double *v,int dim);
int Top(double **adj,double *evc,int topic,int dim);
void PlotEntireGraph(struct Topic *map);
void PlotTopicCosmos(int topic,double **adj,char **names,int dim);
void GetTribe(int *tribe,char **names,int *assoc,int topic,double **adj,int dim);

extern char GRAPHDIR[CF_MAXVARSIZE];

/*****************************************************************************/

void VerifyGraph(struct Topic *map)

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  struct Rlist *rp;
  int i,j,topic_count = 0, assoc_count = 0;
  int size,*k,max_k = 0;
  double **adj,*evc;
  char **n;

for (tp = map; tp != NULL; tp=tp->next)
   {
   topic_count++;
   
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      assoc_count++;
      }
   }

adj = (double **)malloc(sizeof(double *)*topic_count);

for (i = 0; i < topic_count; i++)
   {
   adj[i] = (double *)malloc(sizeof(double)*topic_count);
   }

for (i = 0; i < topic_count; i++)
   {
   for (j = 0; j < topic_count; j++)
      {
      adj[i][j] = 0.0;
      }
   }
    
n = (char **)malloc(sizeof(char *)*topic_count);

i = j = 0;

for (tp = map; tp != NULL; tp=tp->next)
   {
   n[i++] = tp->topic_name;
   }

i = j = 0;

for (tp = map; tp != NULL; tp=tp->next)
   {
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      for (rp = ta->associates; rp != NULL; rp=rp->next)
         {
         for (j = 0; j < topic_count; j++)
            {
            if (strcmp((char *)(rp->item),n[j]) == 0)
               {
               Verbose("Adding associate %s to %s\n",n[i],n[j]);
               adj[i][j] = adj[j][i] = 1.0;
               }

            /* Look at hierarchy relationships - these only distort the associations

            if (strcmp((char *)(rp->item),tp->topic_type) == 0)
               {
               adj[i][j] = adj[j][i] = 1.0;
               } */
            }
         }
      }

   for (op = tp->occurrences; op != NULL; op=op->next)
      {
      for (rp = op->represents; rp != NULL; rp=rp->next)
         {
         for (j = 0; j < topic_count; j++)
            {
            if (strcmp((char *)(rp->item),n[j]) == 0)
               {
               Verbose("Adding occurrence topic %s to %s\n",rp->item,tp->topic_name);
               adj[i][j] = adj[j][i] = 1.0;
               }
            }
         }
      }

   i++;
   }

/* Node degree ranking */

k = (int *)malloc(sizeof(int)*topic_count);

for (i = 0; i < topic_count; i++)
   {
   k[i] = Degree(adj[i],topic_count);

   if (k[i] > max_k)
      {
      max_k = k[i];
      }
   
   Verbose("Topic %d - \"%s\" has degree %d\n",i,n[i],k[i]);
   PrintNeighbours(adj[i],topic_count,n);
   }

for (i = max_k; i >= 0; i--)
   {
   for (j = 0; j < topic_count; j++)
      {
      if (k[j] == i)
         {
         Verbose("  k = %d, %s @ %d\n",k[j],n[j],j);
         }
      }
   }


/* Look at centrality */

evc = (double *)malloc(sizeof(double)*topic_count);

EigenvectorCentrality(adj,evc,topic_count);

Verbose("EVC tops:\n");

for (i = 0; i < topic_count; i++)
   {
   if (Top(adj,evc,i,topic_count))
      {
      Verbose("  Topic %d - \"%s\" is an island\n",i,n[i]);      
      }
   }

/* Look at centrality */

for (i = 0; i < topic_count; i++)
   {
   PlotTopicCosmos(i,adj,n,topic_count);
   }

free(adj);
free(k);
}

/*************************************************************************/

int Degree(double *m,int dim)

{ int i, k = 0;

for (i = 0; i < dim; i++)
   {
   k += (int) m[i];
   }

return k;
}

/*************************************************************************/

int Top(double **adj,double *evc,int topic,int dim)

{ int i;
 
for (i = 0; i < dim; i++)
   {
   if (adj[topic,i] && (evc[i] > evc[topic]))
      {
      return false;
      }
   }

return true;
}

/*************************************************************************/

void PrintNeighbours(double *m,int dim,char **names)

{ int i;

for (i = 0; i < dim; i++)
   {
   if (m[i])
      {
      Verbose(" - Sub: %s\n",names[i]);
      }
   }
}

/*************************************************************************/

void EigenvectorCentrality(double **A,double *v,int dim)

{ int i, n;

for (i = 0; i < dim; i++)
   {
   v[i] = 1.0;
   }

for (n = 0; n < 10; n++)
   {
   MatrixOperation(A,v,dim);
   }
}

/*************************************************************************/

void MatrixOperation(double **A,double *v,int dim)

{ int i,j;
  double max = 0;
  double *vp;
  
vp = (double *)malloc(sizeof(double)*dim);

for (i = 0; i < dim; i++)
   {
   for (j = 0; j < dim; j++)
      {
      vp[i] += A[i][j] * v[j];
      }

   if (vp[i] > max)
      {
      max = vp[i];
      }
   }

for (i = 0; i < dim; i++)
   {
   v[i] = vp[i] / max;
   }

free(vp);
}

/*************************************************************************/

void PlotTopicCosmos(int topic,double **adj,char **names,int dim)

{ char filename[CF_BUFSIZE];
  struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  struct Rlist *rp;
  Agraph_t *g;
  Agnode_t **t = NULL;
  Agedge_t **a = NULL;
  GVC_t *gvc;
  int i,j,counter = 0;
  int tribe[CF_TRIBE_SIZE],associate[CF_TRIBE_SIZE];

/* Count the  number of nodes in the solar system, to max
   number based on Dunbar's limit */  

Verbose("Cosmos for %s\n",names[topic]);

if (strlen(GRAPHDIR) > 0)
   {
   snprintf(filename,CF_BUFSIZE,"%s/%s.png",GRAPHDIR,CanonifyName(names[topic]));
   MakeParentDirectory(filename,false);
   }
else
   {
   snprintf(filename,CF_BUFSIZE,"%s.png",CanonifyName(names[topic]));
   }

GetTribe(tribe,names,associate,topic,adj,dim);

gvc = gvContext();
g = agopen("g",AGDIGRAPH);

t = (Agnode_t **)malloc(sizeof(Agnode_t)*(CF_TRIBE_SIZE+2));
a = (Agedge_t **)malloc(sizeof(Agedge_t)*(CF_TRIBE_SIZE+2)*(CF_TRIBE_SIZE+2));

/* Create the nodes - zero is root */

counter = 0;
t[counter] = agnode(g,names[topic]);
agsafeset(t[counter], "style", "filled", "filled");
agsafeset(t[counter], "shape", "circle", "");
agsafeset(t[counter], "fixedsize", "true", "true");
agsafeset(t[counter], "width", "0.75", "0.75");
agsafeset(t[counter], "color", "orange", "");
agsafeset(t[counter], "fontsize", "14", "");
agsafeset(t[counter], "fontweight", "bold", "");
agsafeset(t[counter], "root", "true", "");
counter++;

for (j = 0; tribe[j] >= 0; j++)
   {
   Verbose("Making Node %d for %s (%d)\n",counter,names[tribe[j]],j);
   t[counter] = agnode(g,names[tribe[j]]);   
   agsafeset(t[counter], "style", "filled", "false");
   agsafeset(t[counter], "shape", "circle", "");
   agsafeset(t[counter], "fixedsize", "true", "true");
   agsafeset(t[counter], "width", "0.55", "0.55");
   agsafeset(t[counter], "color", "grey", "");
   agsafeset(t[counter], "fontsize", "12", "");
   counter++;
   }

/* Now make the edges - have to search for the associations in the
   subgraph associate[] is the node that spawned the link to tribe[],
   find their t[] graph nodes and link them  */

Verbose("Made %d nodes\n",counter);

counter = 0;

for (j = 0; tribe[j] >= 0; j++)
   {
   if (associate[j] == topic)
      {
      a[counter] = agedge(g,t[0],t[j]);
      Verbose("Linking ROOT \"%s\" to \"%s\"\n",names[tribe[j]],names[topic]);
      counter++;
      continue;
      }
   
   for (i = 0; tribe[i] >= 0; i++)
      {
      //link t[index] with t[index2 which matches tribe[index2] == assoc[index]] .i.e. t[j] with t[]
      
      if (associate[j] == tribe[i])
         {
         Verbose("(%d) Linking family \"%s\" (%d) to \"%s\"(%d)\n",counter,names[tribe[i]],i,names[tribe[j]],j);
         a[counter] = agedge(g,t[j],t[i]);
         counter++;
         }
      }
   }

Verbose("Made %d edges\n",counter);

agsafeset(g, "truecolor", "true", "");
agsafeset(g, "bgcolor", "lightgrey", "");
agsafeset(g, "overlap", "scale", "");
agsafeset(g, "mode", "major", "");

// circo dot fdp neato nop nop1 nop2 twopi
gvLayout(gvc, g,"neato");
gvRenderFilename(gvc,g,"png",filename);

gvFreeLayout(gvc, g);
agclose(g);
gvFreeContext(gvc);
CfOut(cf_inform,"","Generated topic locale %s\n",filename);
}

/*************************************************************************/

void GetTribe(int *tribe,char **n,int *assoc,int topic,double **adj,int dim)

{ int i,j,counter = 0;

for (i = 0; i < CF_TRIBE_SIZE; i++)
   {
   tribe[i] = -1;
   assoc[i] = -1;
   }
 
for (i = 0; i < dim; i++)
   {
   if (adj[topic][i] > 0)
      {
      Verbose(" -> (%d) %s is a neighbour of topic %s\n",counter,n[i],n[topic]);
      
      tribe[counter] = i;
      assoc[counter] = topic;
      counter++;

      if (counter >= CF_TRIBE_SIZE - 1)
         {
         return;
         }
      }
   }

/* Look recursively at neighbourhoods */

for (i = 0; tribe[i] > 0; i++)
   {
   for (j = 0; j < dim; j++)
      {
      if (j == topic || j == tribe[i])
         {
         continue;
         }

      if (adj[tribe[i]][j] > 0)
         {
         Verbose(" -> (%d) %s is in extended TRIBE of topic %s\n",counter,n[j],n[tribe[i]]);
         //link t[index] with t[index2 which matches tribe[index2] == assoc[index]]
         
         tribe[counter] = j;
         assoc[counter] = tribe[i];
         counter++;

         if (counter >= CF_TRIBE_SIZE - 1)
            {
            return;
            }
         }
      }
   }
}

#endif
