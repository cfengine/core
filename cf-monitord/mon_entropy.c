/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <cf3.defs.h>
#include <mon.h>
#include <item_lib.h>

/* Globals */

static Item *ENTROPIES = NULL;

/* Implementation */

void MonEntropyClassesInit(void)
{
}

/****************************************************************************/

void MonEntropyClassesReset(void)
{
    ENTROPIES = NULL;           /*? */
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
 *   Divisor is a uncertainty per digit, to normalize it to [0..1] we divide it
 *   by ln(N).
 */

double MonEntropyCalculate(const Item *items)
{
    double S = 0.0;
    double sum = 0.0;
    int numclasses = 0;
    const Item *i;

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
        double q = ((double) i->counter) / sum;

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
    AppendItem(&ENTROPIES, class, "");
}

/****************************************************************************/

void MonEntropyPurgeUnused(char *name)
{
// Don't set setentropy is there is no corresponding class
    DeleteItemMatching(&ENTROPIES, name);
}

/****************************************************************************/

void MonEntropyClassesPublish(FILE *fp)
{
    for (Item *ip = ENTROPIES; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%s\n", ip->name);
    }

    DeleteItemList(ENTROPIES);
}
