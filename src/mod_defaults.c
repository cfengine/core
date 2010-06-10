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

/*****************************************************************************/
/*                                                                           */
/* File: mod_defaults.c                                                      */
/*                                                                           */
/*****************************************************************************/

#define CF3_MOD_DEFAULTS

#include "cf3.defs.h"
#include "cf3.extern.h"

struct BodyDefault CONTROL_DEFAULT[] =
   {
   {"ignore_missing_bundles","false"},
   {"ignore_missing_inputs","false"},
   {"lastseenexpireafter","One week"},
   {"require_comments","false"},
   {"host_licenses_paid","0"},
   {"syslog_port","514"},
   {"agentfacility","LOG_USER"},
   {"auditing","false"},
   {"binarypaddingchar","space (ASC=32)"},
   {"hashupdates","false"},
   {"checksum_alert_time","10 mins"},
   {"defaultcopytype","ctime or mtime differ"},
   {"dryrun","false"},
   {"editbinaryfilesize","100000"},
   {"editfilesize","10000"},
   {"exclamation","true"},
   {"expireafter","1 min"},
   {"execresult_newlines","false"},
   {"hostnamekeys","false"},
   {"ifelapsed","1"},
   {"inform","false"},
   {"intermittency","false"},
   {"max_children","1 concurrent agent promise"},
   {"maxconnections","30 remote queries"},
   {"mountfilesystems","false"},
   {"nonalphanumfiles","false"},
   {"repchar","_"},
   {"default_repository","in situ"},
   {"secureinput","false"},
   {"sensiblecount","2 files"},
   {"sensiblesize","1000 bytes"},
   {"skipidentify","false"},
   {"syslog","false"},
   {"track_value","false"},
   {"default_timeout","10 seconds"},
   {"verbose","false"},
   {"denybadclocks","true"},
   {"logallconnections","false"},
   {"logencryptedtransfers","false"},
   {"serverfacility","LOG_USER"},
   {"port","5308"},
   {"forgetrate","0.6"},
   {"monitorfacility","LOG_USER"},
   {"histograms","true"},
   {"tcpdump","false"},
   {"force_ipv4","false"},
   {"trustkey","false"},
   {"encrypt","false"},
   {"background_children","false"},
   {"max_children","50 runagents"},
   {"output_to_file","false"},
   {"splaytime","0"},
   {"mailmaxlines","30"},
   {"executorfacility","LOG_USER"},
   {"build_directory","Current working directory"},
   {"generate_manual","false"},
   {"view_projections","false"},
   {"auto_scaling","true"},
   {"error_bars","true"},
   {"reports","none"},
   {"report_output","none"},
   {"time_stamps","false"},
   {NULL,NULL},
   };

/*****************************************************************************/

struct BodyDefault BODY_DEFAULT[] =
   {
   {"ifelapsed","control body value"},
   {"expireafter","control body value"},
   {"background","false"},
   {"report_level","(none)"},
   {"audit","false"},
   {"timer_policy","reset"},
   {"before_after","after"},
   {"first_last","last"},
   {"allow_blank_fields","false"},
   {"extend_fields","false"},
   {"field_operation","none"},
   {"field_separator","none"},
   {"value_separator","none"},
   {"occurrences","all"},
   {"expand_scalars","false"},
   {"insert_type","literal"},
   {"not_matching","false"},
   {"traverse_links","false"},
   {"xdev","false"},
   {"rmdeadlinks","false"},
   {"empty_file_before_editing","false"},
   {"edit_backup","true"},
   {"disable","false"},
   {"link_children","false"},
   {"encrypt","false"},
   {"link_type","symlink"},
   {"when_no_source","nop"},
   {"compare","mtime or ctime differs"},
   {"force_update","false"},
   {"force_ipv4","false"},
   {"preserve","false"},
   {"purge","false"},
   {"trustkey","false"},
   {"typecheck","false"},
   {"verify","false"},
   {"useshell","false"},
   {"preview","false"},
   {"no_output","false"},
   {"module","false"},
   {"package_policy","verify"},
   {"check_foreign","false"},
   {"scan_arrivals","false"},
   {"edit_fstab","false"},
   {"unmount","false"},
   {"database_type","none"},
   {"db_server_type","none"},
   {"ifencrypted","false"},
   {"create","false"},
   {"insert_match_policy","exact_match"},
   {"recognize_join","false"},
   {"include_start_delimiter","false"},
   {"include_end_delimiter","false"},
   {"output_level","verbose"},
   {"promiser_type","promise"},   
   {NULL,NULL},
   };

/*****************************************************************************/

char *GetControlDefault(char *bodypart)

{ int i;

for (i = 0; CONTROL_DEFAULT[i].lval != NULL; i++)
   {
   if (strcmp(CONTROL_DEFAULT[i].lval,bodypart) == 0)
      {
      return CONTROL_DEFAULT[i].rval;
      }   
   }

return NULL;
}

/*****************************************************************************/

char *GetBodyDefault(char *bodypart)

{ int i;

for (i = 0; BODY_DEFAULT[i].lval != NULL; i++)
   {
   if (strcmp(BODY_DEFAULT[i].lval,bodypart) == 0)
      {
      return BODY_DEFAULT[i].rval;
      }   
   }

return NULL;
}

/*****************************************************************************/
/* Now a list of features in editions                                        */
/*****************************************************************************/

// All
// Nova, Constellation and Galaxy
// Constellation and Galaxy
// Galaxy
