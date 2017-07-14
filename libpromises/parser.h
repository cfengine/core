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

#ifndef CFENGINE_PARSER_H
#define CFENGINE_PARSER_H

#include <policy.h>

#define PARSER_WARNING_DEPRECATED       (1 << 0)
#define PARSER_WARNING_REMOVED          (1 << 1)

#define PARSER_WARNING_ALL              0xfffffff

/**
 * @return warning code, or -1 if not a valid warning
 */
int ParserWarningFromString(const char *warning_str);
const char *ParserWarningToString(unsigned int warning);

/**
 * @brief Parse a CFEngine file to create a Policy DOM
 * @param agent_type Which agent is parsing the file. The parser will ignore elements not pertitent to its type
 * @param path Path of file to parse
 * @param warnings Bitfield of which warnings should be recorded
 * @param warnings_error Bitfield of which warnings should be counted as errors
 * @return
 */
Policy *ParserParseFile(AgentType agent_type, const char *path, unsigned int warnings, unsigned int warnings_error);

#endif
