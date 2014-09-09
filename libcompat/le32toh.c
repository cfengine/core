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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <platform.h>

uint32_t le32toh(uint32_t le32uint)
{
#ifdef WORDS_BIGENDIAN
    uint32_t be32uint;
    unsigned char *le_ptr = (unsigned char *)le32uint;
    unsigned char *be_ptr = (unsigned char *)be32uint;
    be_ptr[0] = le_ptr[3];
    be_ptr[1] = le_ptr[2];
    be_ptr[2] = le_ptr[1];
    be_ptr[3] = le_ptr[0];
    return be32uint;
#else
    return le32uint;
#endif
}
