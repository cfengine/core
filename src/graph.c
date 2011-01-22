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
/* File: graph.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifndef HAVE_LIBCFNOVA
#define CF_TRIBE_SIZE 30
#endif

/*****************************************************************************/

void VerifyGraph(struct Rlist *assoc_views,char *view)

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  struct Rlist *rp;
  int i,j,topic_count = 0;
  int size,*k,max_k = 0;
  double **adj,*evc;
  char **n;

if (view)
   {
   CfOut(cf_verbose,""," -> Graph view %s\n",view);
   }

/* Just count up the topics */

topic_count = CF_NODES;

/* Allocate an array we can pass to a subroutine */

topic_count++;

adj = (double **)malloc(sizeof(double *)*topic_count);
evc = (double *)malloc(sizeof(double)*topic_count);
   
if (adj == NULL)
   {
   FatalError("memory allocation in graphs");
   }

for (i = 0; i < topic_count; i++)
   {
   adj[i] = (double *)malloc(sizeof(double)*topic_count);
   }

/* And a vector to contain the names */
    
n = (char **)malloc(sizeof(char *)*topic_count);

for (i = 0; i < topic_count; i++)
   {
   for (j = 0; j < topic_count; j++)
      {
      adj[i][j] = 0.0;
      }

   n[i] = NULL;
   evc[i] = 0;
   }


/* Construct the adjacency matrix for the map */

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   for (tp = TOPICHASH[i]; tp != NULL; tp=tp->next)
      {
      for (ta = tp->associations; ta != NULL; ta=ta->next)
         {
         /* Semantic projection if selected a view... */
         
         if (assoc_views && !KeyInRlist(assoc_views,ta->fwd_name))
            {
            continue;
            }
         
         /* ...else all associations in play */
         
         for (rp = ta->associates; rp != NULL; rp=rp->next)
            {
            int count = 0;
            int to_id = GetTopicPid(rp->item);
            int from_id = tp->id;
            
            if (to_id > 0 && from_id > 0)
               {
               adj[from_id][to_id] = adj[to_id][from_id] = 1.0;
               }
            }
         }
      }
   }

/* Node degree ranking - global ranking is too non-specific as the data are highly clustered or very sparse */

EigenvectorCentrality(adj,evc,topic_count);

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   for (tp = TOPICHASH[i]; tp != NULL; tp=tp->next)
      {
      n[tp->id] = strdup(ClassifiedTopic(tp->topic_name,tp->topic_context));
      CfOut(cf_verbose,""," -> Populating %d = %s (%s)\n",tp->id,tp->topic_name,tp->topic_context);
      tp->evc = evc[tp->id];
      }
   }

#if defined HAVE_LIBCFNOVA && defined HAVE_LIBGD
// Superior/Lazy approach avoids this
#else
for (i = 0; i < topic_count; i++)
   {
   PlotTopicCosmos(i,adj,n,topic_count,view);
   }
#endif

/* Clean up */

for (i = 0; i < topic_count; i++)
   {
   if (n[i])
      {
      free(n[i]);
      }

   if (adj[i])
      {
      free(adj[i]);
      }
   }

free(evc);
free(adj);
free(n);
}

/*************************************************************************/

void PlotTopicCosmos(int topic,double **adj,char **names,int dim,char *view)

{
#ifdef HAVE_LIBGVC
# ifdef HAVE_GRAPHVIZ_GVC_H
#  include <graphviz/gvc.h>
# endif
  
  char filename[CF_BUFSIZE];
  struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  struct Rlist *nodelist = NULL;
  Agraph_t *g;
  Agnode_t *t[CF_TRIBE_SIZE];
  Agedge_t *a[CF_TRIBE_SIZE];
  GVC_t *gvc;
  int i,j,counter = 0,nearest_neighbours = 0, cadj[CF_TRIBE_SIZE][CF_TRIBE_SIZE];
  int tribe[CF_TRIBE_SIZE],associate[CF_TRIBE_SIZE],tribe_size = 0;
  char ltopic[CF_MAXVARSIZE],ltype[CF_MAXVARSIZE];
  char filenode[CF_MAXVARSIZE];
  struct stat sb;

/* Count the  number of nodes in the solar system, to max
   number based on Dunbar's limit */  

CfOut(cf_verbose,"","Cosmos for %s\n",names[topic]);

if (view)
   {
   snprintf(filenode,CF_MAXVARSIZE-1,"%s_%s.png",view,CanonifyName(names[topic]));
   }
else
   {
   snprintf(filenode,CF_MAXVARSIZE-1,"%s.png",CanonifyName(names[topic]));
   }

if (strlen(GRAPHDIR) > 0)
   {
   snprintf(filename,CF_BUFSIZE,"%s/%s",GRAPHDIR,filenode);
   MakeParentDirectory(filename,false);
   }
else
   {
   strcpy(filename,filenode);
   }

if (cfstat(filename,&sb) != -1)
   {
   CfOut(cf_inform,"","Graph \"%s\" already exists, delete to refresh\n",filename);
   return;
   }

GetTribe(tribe,names,associate,topic,adj,dim);

/* Tech specific part */

gvc = gvContext();
g = agopen("g",AGDIGRAPH);

for (i = 0; i < CF_TRIBE_SIZE; i++)
   {
   for (j = 0; j < CF_TRIBE_SIZE; j++)
      {
      cadj[i][j] = 0;
      }      
   }

/* Create the nodes - zero is root topic */

t[0] = agnode(g,names[topic]);   
agsafeset(t[0], "style", "filled", "filled");
agsafeset(t[0], "shape", "circle", "");
agsafeset(t[0], "fixedsize", "true", "true");
agsafeset(t[0], "width", "1.15", "1.15");
agsafeset(t[0], "color", "orange", "");
agsafeset(t[0], "fontsize", "14", "");
agsafeset(t[0], "fontweight", "bold", "");
agsafeset(t[0], "root", "true", "");

for (j = 1; tribe[j] >= 0; j++)
   {
   CfOut(cf_verbose,"","Making Node %d for %s (%d)\n",counter,names[tribe[j]],j);
   DeClassifyTopic(names[tribe[j]],ltopic,ltype);

   if (!KeyInRlist(nodelist,ltopic))
      {
      IdempPrependRScalar(&nodelist,ltopic,CF_SCALAR);
      CfOut(cf_verbose,"","Multiple nodes with same name will overlap on picture -- to fix later\n");
      }

   t[j] = agnode(g,ltopic);
   
   agsafeset(t[j], "style", "filled", "false");
   agsafeset(t[j], "shape", "circle", "");
   agsafeset(t[j], "fixedsize", "true", "true");
   agsafeset(t[j], "width", "0.75", "0.75");
   agsafeset(t[j], "fontsize", "12", "");
   
   if (associate[j] == topic)
      {
      agsafeset(t[j], "color", "bisque2", "");
      }
   else
      {
      agsafeset(t[j], "color", "burlywood3", "");
      }
   }

counter = 0;

/* Now make the edges - have to search for the associations in the
   subgraph associate[] is the node that spawned the link to tribe[],
   find their t[] graph nodes and link them  */

CfOut(cf_verbose,"","Made %d nodes\n",counter);

for (i = 1; associate[i] == topic; i++)
   {
   /* The node for tribe member tribe[j] is t[j] */
   CfOut(cf_verbose,"","Link: %s to %s\n",names[tribe[i]],names[topic]);
   a[counter] = agedge(g,t[0],t[i]);
   agsafeset(a[counter], "color", "red", "");
   counter++;
   }

nearest_neighbours = counter;

// If the tribe of the each nearest neighbour

for (i = nearest_neighbours; tribe[i] >= 0; i++)
   {
   for (j = 0; tribe[j] >= 0; j++)
      {
      CfOut(cf_verbose,"","Link: %s to %s\n",names[tribe[j]],names[tribe[i]]);

      if (associate[i] == tribe[j])
         {
         if (cadj[i][j] < 3)
            {
            a[counter] = agedge(g,t[j],t[i]);
            agsafeset(a[counter], "color", "brown", "");
            counter++;
            cadj[i][j]++;
            cadj[j][i]++;
            tribe_size++;
            }
         }
      }
   }

CfOut(cf_verbose,"","Made %d edges\n",counter);

agsafeset(g, "truecolor", "true", "");
agsafeset(g, "bgcolor", "lightgrey", "");
agsafeset(g, "overlap", "scale", "");
agsafeset(g, "mode", "major", "");

// circo dot fdp neato nop nop1 nop2 twopi
gvLayout(gvc, g,"neato");

if (tribe_size > 1)
   {
   gvRenderFilename(gvc,g,"png",filename);
   }
else
   {
   CfOut(cf_error,"","Warning: Tribe size of %d is too small to make sensible graph\n",tribe_size);
   }

gvFreeLayout(gvc, g);
agclose(g);
gvFreeContext(gvc);
CfOut(cf_inform,"","Generated topic locale %s\n",filename);
DeleteRlist(nodelist);

#endif
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

int IsTop(double **adj,double *evc,int topic,int dim)

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
      CfOut(cf_verbose,""," - Sub: %s\n",names[i]);
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

void GetTribe(int *tribe,char **n,int *neigh,int topic,double **adj,int dim)

{ int counter = 0,possible_neighbour,i,j;
  int nearest_neighbour_boundary;

for (i = 0; i < CF_TRIBE_SIZE; i++)
   {
   tribe[i] = -1;
   neigh[i] = -1;
   }

/* Tribe[i] are the nearest neighbours of i, then neighbours of those too
   Make tribe[0] == topic so it is in its own tribe .. !*/

tribe[0] = topic;
neigh[0] = topic;

counter = 1;

CfOut(cf_verbose,"","Tribe for topic %d = %s\n",topic,n[topic]);

for (possible_neighbour = 0; possible_neighbour < dim; possible_neighbour++)
   {
   if (possible_neighbour == topic)
      {
      continue;
      }
   
   if (adj[topic][possible_neighbour] > 0 || adj[possible_neighbour][topic] > 0)
      {
      CfOut(cf_verbose,""," -> %d (%s) is a nearest neighbour of topic (%s)\n",counter,n[possible_neighbour],n[topic]);
      
      tribe[counter] = possible_neighbour;
      neigh[counter] = topic;
      counter++;

      if (counter >= CF_TRIBE_SIZE - 1)
         {
         return;
         }
      }
   }

/* Look recursively at 2nd order neighbourhoods */

nearest_neighbour_boundary = counter;

for (j = 0; j < nearest_neighbour_boundary; j++)
   {
   for (possible_neighbour = 1; possible_neighbour < dim; possible_neighbour++)
      {
      if (possible_neighbour == topic)
         {
         continue;
         }

      if (AlreadyInTribe(possible_neighbour,tribe))
         {
         continue;
         }

      if (adj[tribe[j]][possible_neighbour] > 0)
         {
         CfOut(cf_verbose,""," -> (%d) %s is in extended TRIBE of topic %s\n",counter,n[possible_neighbour],n[tribe[j]]);
         //link t[index] with t[index2 which matches tribe[index2] == neigh[index]]
         
         tribe[counter] = possible_neighbour;
         neigh[counter] = tribe[j];
         counter++;

         if (counter >= CF_TRIBE_SIZE - 1)
            {
            return;
            }
         }
      }
   }

/* Look recursively at 3nd order neighbourhoods for bridge connections*/

for (j = nearest_neighbour_boundary; j < counter; j++)
   {
   for (possible_neighbour = 0; possible_neighbour < dim; possible_neighbour++)
      {
      if (possible_neighbour == topic)
         {
         continue;
         }

      if (possible_neighbour == tribe[j])
         {
         continue;
         }

      if (AlreadyInTribe(possible_neighbour,tribe))
         {
         continue;
         }

      if (adj[tribe[j]][possible_neighbour] > 0)
         {
         CfOut(cf_verbose,""," -> (%d) %s is in 3extended TRIBE of topic %s\n",counter,n[possible_neighbour],n[tribe[j]]);
         
         tribe[counter] = possible_neighbour;
         neigh[counter] = tribe[j];

         counter++;

         if (counter >= CF_TRIBE_SIZE - 1)
            {
            return;
            }
         }
      }
   }
}

/*************************************************************************/

int AlreadyInTribe(int node, int *tribe)

{ int i;

for (i = 0; tribe[i] > 0; i++)
    {
    if (tribe[i] == node)
       {
       return true;
       }
    }

return false;
}

