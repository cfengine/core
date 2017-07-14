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

#ifndef CFENGINE_ADDR_LIB_H
#define CFENGINE_ADDR_LIB_H

#include <cf3.defs.h>

bool IsLoopbackAddress(const char *address);
int FuzzySetMatch(const char *s1, const char *s2);
bool FuzzyHostParse(const char *arg2);
int FuzzyHostMatch(const char *arg0, const char *arg1, const char *basename);
bool FuzzyMatchParse(const char *item);
bool IsInterfaceAddress(const Item *ip_addresses, const char *adr);
void ParseHostPort(char *s, char **hostname, char **port);

#endif
