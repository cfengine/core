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

#ifndef CFENGINE_SORT_H
#define CFENGINE_SORT_H

/*
 * Sorting is destructive, use the returned pointer to refer to sorted list.
 */

Item *SortItemListNames(Item *list); /* Alphabetical */
Item *SortItemListClasses(Item *list); /* Alphabetical */
Item *SortItemListCounters(Item *list); /* Reverse sort */
Item *SortItemListTimes(Item *list); /* Reverse sort */

Rlist *SortRlist(Rlist *list, int (*CompareItems) ());
Rlist *AlphaSortRListNames(Rlist *list);
Rlist *IntSortRListNames(Rlist *list);
Rlist *RealSortRListNames(Rlist *list);
Rlist *IPSortRListNames(Rlist *list);
Rlist *MACSortRListNames(Rlist *list);

bool GenericItemLess(const char *mode, void *lhs, void *rhs);

bool GenericStringItemLess(const char *sort_type, const char *lhs, const char *rhs);

#endif
