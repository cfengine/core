/*
   Copyright 2018 Northern.tech AS

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

#ifndef CFENGINE_FILES_EDIT_H
#define CFENGINE_FILES_EDIT_H

#include <cf3.defs.h>
#include <file_lib.h>

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/xpathInternals.h>
#endif



#define CF_EDIT_IFELAPSED 1     /* NOTE: If doing copy template then edit working copy,
                                   the edit ifelapsed must not be higher than
                                   the copy ifelapsed. This will make the working
                                   copy equal to the copied template file - not the
                                   copied + edited file. */

typedef struct
{
    char *filename;
    Item *file_start;
    int num_edits;
    int num_rewrites;
#ifdef HAVE_LIBXML2
    xmlDocPtr xmldoc;
#endif
    NewLineMode new_line_mode;
} EditContext;

// filename must not be freed until FinishEditContext.
EditContext *NewEditContext(char *filename, Attributes a);
void FinishEditContext(EvalContext *ctx, EditContext *ec,
                       Attributes a, const Promise *pp,
                       PromiseResult *result);

#ifdef HAVE_LIBXML2
int LoadFileAsXmlDoc(xmlDocPtr *doc, const char *file, EditDefaults ed);
bool SaveXmlDocAsFile(xmlDocPtr doc, const char *file,
                      Attributes a, NewLineMode new_line_mode);
#endif

#endif
