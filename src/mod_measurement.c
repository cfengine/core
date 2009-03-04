/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: mod_process.c                                                       */
/*                                                                           */
/*****************************************************************************/

/*

 This file can act as a template for adding functionality to cfengine 3.
 All functionality can be added by extending the main array

 CF_MOD_SUBTYPES[CF3_MODULES]

 and its array dimension, in mod_common, in the manner shown here.
 
*/

#define CF3_MOD_MEASUREMENT

#include "cf3.defs.h"
#include "cf3.extern.h"

 /***********************************************************/
 /* Read this module file backwards, as dependencies have   */
 /* to be defined first - these arrays declare pairs of     */
 /* constraints                                             */
 /*                                                         */
 /* lval => rval                                            */
 /*                                                         */
 /* in the form (lval,type,range)                           */
 /*                                                         */
 /* If the type is cf_body then the range is a pointer      */
 /* to another array of pairs, like in a body "sub-routine" */
 /*                                                         */
 /***********************************************************/

struct BodySyntax CF_MATCHVALUE_BODY[] =
   {
   /* Row models */
   {"select_line_matching",cf_str,CF_ANYSTRING,"Regular expression for matching line location"},
   {"select_line_number",cf_int,CF_VALRANGE,"Read from the n-th line of the output (fixed format)"},
   {"extraction_regex",cf_str,"","Regular expression that should contain a single backreference for extracting a value"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_MEASURE_BODIES[] =
   {
   {"stream_type",cf_opts,"pipe,file","The datatype being collected."},
   {"data_type",cf_opts,"counter,integer,real,string","The datatype being collected."},
   {"history_type",cf_opts,"weekly,none","Whether the data can be seen as a time-series or just an isolated value"},
   {"units",cf_str,"","The engineering dimensions of this value or a note about its intent used in plots"},
   {"match_value",cf_body,CF_MATCHVALUE_BODY,"Criteria for extracting the measurement from a datastream"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_MEASUREMENT_SUBTYPES[] =
  {
  {"monitor","measurements",CF_MEASURE_BODIES},
  {NULL,NULL,NULL},
  };

