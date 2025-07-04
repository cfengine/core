/*
  Copyright 2024 Northern.tech AS

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

#ifndef CFENGINE_FILES_OPERATORS_H
#define CFENGINE_FILES_OPERATORS_H

#include <cf3.defs.h>
#include <file_lib.h>

bool MoveObstruction(EvalContext *ctx, char *from, const Attributes *attr, const Promise *pp, PromiseResult *result);

typedef bool (*SaveCallbackFn)(const char *dest_filename, void *param, NewLineMode new_line_mode);
bool SaveAsFile(EvalContext *ctx, SaveCallbackFn callback, void *param, const char *file, const Attributes *a, NewLineMode new_line_mode);
bool SaveItemListAsFile(EvalContext *ctx, Item *liststart, const char *file, const Attributes *a, NewLineMode new_line_mode);

bool CompareToFile(EvalContext *ctx, const Item *liststart, const char *file, const Attributes *a, const Promise *pp, PromiseResult *result);

#endif
