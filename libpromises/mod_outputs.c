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

#include <mod_outputs.h>

#include <syntax.h>

static const ConstraintSyntax CF_OUTPUTS_BODIES[] =
{
    ConstraintSyntaxNewOption("output_level", "verbose,debug,inform", "Output level to observe for the named promise or bundle (meta-promise). Default value: verbose", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("promiser_type", "promise,bundle", "Output level to observe for the named promise or bundle (meta-promise). Default value: promise", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_OUTPUTS_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "outputs", CF_OUTPUTS_BODIES, NULL, SYNTAX_STATUS_REMOVED),
    PromiseTypeSyntaxNewNull()
};
