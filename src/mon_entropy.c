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

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "monitoring.h"

/* Globals */

static struct Item *PREVIOUS_STATE = NULL;
static struct Item *ENTROPIES = NULL;

static char ENVFILE_NEW[CF_BUFSIZE];
static char ENVFILE[CF_BUFSIZE];

/* Implementation */

void MonEntropyClassesInit(void)
{
snprintf(ENVFILE_NEW,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENVNEW_FILE);
MapName(ENVFILE_NEW);

snprintf(ENVFILE,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENV_FILE);
MapName(ENVFILE);
}

/****************************************************************************/

void MonEntropyClassesReset(void)
{
ENTROPIES = NULL; /*?*/
}

/****************************************************************************/

/*
 * This function calculates entropy of items distribution.
 *
 * Let's define:
 *    N = length of items list
 *  q_i = items[i]->counter
 *  p_i = q_i / sum_{i=1..N}(q_i)
 *    -- normalized values, or probability of i-th class amongst all
 *
 *  Entropy is
 *         ---
 *       - >         q_i * ln(q_i)
 *         --- i=0..N
 *   E = -------------------------
 *                 ln(N)
 *
 *   Divisor is a uncertainity per digit, to normalize it to [0..1] we divide it
 *   by ln(N).
 */
double MonEntropyCalculate(const struct Item *items)
{
double S = 0.0;
double sum = 0.0;
int numclasses = 0;
const struct Item *i;

for (i = items; i; i = i->next)
   {
   sum += i->counter;
   numclasses++;
   }

if (numclasses < 2)
   {
   return 0.0;
   }

for (i = items; i; i = i->next)
   {
   double q = ((double)i->counter) / sum;
   S -= q * log(q);
   }

return S / log(numclasses);
}

/****************************************************************************/

void MonEntropyClassesSet(const char *service, const char *direction, double entropy)
{
char class[CF_MAXVARSIZE];

const char *class_type = "medium";

if (entropy > 0.9)
   {
   class_type = "high";
   }
if (entropy < 0.2)
   {
   class_type = "low";
   }

snprintf(class, CF_MAXVARSIZE, "entropy_%s_%s_%s", service, direction, class_type);
AppendItem(&ENTROPIES,class,"");
}

/****************************************************************************/

void MonEntropyClassesPublish(struct Item *classlist)
{
FILE *fp;
struct Item *ip;

unlink(ENVFILE_NEW);

if ((fp = fopen(ENVFILE_NEW,"a")) == NULL)
   {
   DeleteItemList(PREVIOUS_STATE);
   PREVIOUS_STATE = classlist;
   return;
   }

for (ip = classlist; ip != NULL; ip=ip->next)
   {
   fprintf(fp,"%s\n",ip->name);
   }

DeleteItemList(PREVIOUS_STATE);
PREVIOUS_STATE = classlist;

for (ip = ENTROPIES; ip != NULL; ip=ip->next)
   {
   fprintf(fp,"%s\n",ip->name);
   }

DeleteItemList(ENTROPIES);
fclose(fp);

cf_rename(ENVFILE_NEW,ENVFILE);
}
