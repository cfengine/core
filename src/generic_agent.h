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

#ifndef CFENGINE_GENERIC_AGENT_H
#define CFENGINE_GENERIC_AGENT_H

#include "cf3.defs.h"
#include "cf3.extern.h"

typedef struct
   {
   Rlist *bundlesequence;
   bool verify_promises;
   } GenericAgentConfig;

void ThisAgentInit(void);
void KeepPromises(GenericAgentConfig config);

void GenericInitialize(int argc,char **argv,char *agents, GenericAgentConfig config);
void GenericDeInitialize(void);
void InitializeGA(int argc,char **argv);
GenericAgentConfig CheckOpts(int argc,char **argv);
void Syntax(const char *comp, const struct option options[], const char *hints[], const char *id);
void ManPage(const char *component, const struct option options[], const char *hints[], const char *id);
void PrintVersionBanner(const char *component);
int CheckPromises(enum cfagenttype ag);
void ReadPromises(enum cfagenttype ag,char *agents, GenericAgentConfig config);
int NewPromiseProposals(void);
void CompilationReport(char *filename);
void HashVariables(char *name);
void HashControls(void);
void CloseLog(void);
Constraint *ControlBodyConstraints(enum cfagenttype agent);
void SetFacility(const char *retval);
Bundle *GetBundle(char *name,char *agent);
SubType *GetSubTypeForBundle(char *type,Bundle *bp);
void CheckBundleParameters(char *scope,Rlist *args);
void PromiseBanner(Promise *pp);
void BannerBundle(Bundle *bp,Rlist *args);
void BannerSubBundle(Bundle *bp,Rlist *args);
void WritePID(char *filename);
void OpenCompilationReportFiles(const char *fname);
GenericAgentConfig GenericAgentDefaultConfig(enum cfagenttype agent_type);
void CheckLicenses(void);

void SetInputFile(const char *filename);

#endif
