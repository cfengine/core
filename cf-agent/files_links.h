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

#ifndef CFENGINE_FILES_LINKS_H
#define CFENGINE_FILES_LINKS_H

#include "cf3.defs.h"

char VerifyLink(char *destination, const char *source, Attributes attr, Promise *pp);
char VerifyAbsoluteLink(char *destination, const char *source, Attributes attr, Promise *pp);
char VerifyRelativeLink(char *destination, const char *source, Attributes attr, Promise *pp);
char VerifyHardLink(char *destination, const char *source, Attributes attr, Promise *pp);
int KillGhostLink(const char *name, Attributes attr, const Promise *pp);
int MakeHardLink(const char *from, const char *to, Attributes attr, const Promise *pp);
int ExpandLinks(char *dest, const char *from, int level);

#endif
