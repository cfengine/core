/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_REFCOUNT_P_H
#define CFENGINE_REFCOUNT_P_H

/*
 * This file exists as an implementation detail, none of the structs or
 * methods defined here represent any kind of stable API and can be changed
 * at any moment, without warning. Do not include this file in your projects.
 *
 * I mean it, if you include this file your code will break. That is not a
 * threat, just a statement of facts.
 */

struct RefCountNode {
    struct RefCountNode *next;
    struct RefCountNode *previous;
    void *user;
};
typedef struct RefCountNode RefCountNode;

#endif // CFENGINE_REFCOUNT_P_H
