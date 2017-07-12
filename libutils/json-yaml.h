/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_JSON_YAML_H
#define CFENGINE_JSON_YAML_H

#include <json.h>

#ifdef HAVE_LIBYAML
#include <yaml.h>
#endif

/**
  @brief Parse a YAML string to create a JsonElement
  @param data [in Pointer to the string to parse
  @param json_out Resulting JSON object
  @returns See JsonParseError and JsonParseErrorToString
  */
JsonParseError JsonParseYamlString(const char **data, JsonElement **json_out);

/**
 * @brief Convenience function to parse JSON from a YAML file
 * @param path Path to the file
 * @param size_max Maximum size to read in memory
 * @param json_out Resulting JSON object
 * @return See JsonParseError and JsonParseErrorToString
 */
JsonParseError JsonParseYamlFile(const char *path, size_t size_max, JsonElement **json_out);

#endif // CFENGINE_JSON_YAML_H
