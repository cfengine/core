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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#if !HAVE_DECL_SETLINEBUF
void setlinebuf(FILE *stream);
#endif

#ifdef _WIN32
/*
  Line buffered mode doesn't work on Windows, as documented here:
  https://msdn.microsoft.com/en-us/library/86cebhfs.aspx (setvbuf)
  It will fall back to block buffering, so in that case it is better to select
  no buffering.
*/
# define IO_MODE _IONBF
#else
# define IO_MODE _IOLBF
#endif

void setlinebuf(FILE *stream)
{
    setvbuf(stream, (char *) NULL, IO_MODE, 0);
}
