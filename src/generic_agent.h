/*
 This file is (C) Cfengine AS. See LICENSE for details.
*/
#ifndef CFENGINE_GENERIC_AGENT_H
#define CFENGINE_GENERIC_AGENT_H

#include "cf3.defs.h"
#include "cf3.extern.h"

struct GenericAgentConfig
   {
   bool verify_promises;
   };

void GenericInitialize(int argc,char **argv,char *agents, struct GenericAgentConfig config);
void GenericDeInitialize(void);
void InitializeGA(int argc,char **argv);
struct GenericAgentConfig CheckOpts(int argc,char **argv);
void Syntax(const char *comp, const struct option options[], const char *hints[], const char *id);
void ManPage(const char *component, const struct option options[], const char *hints[], const char *id);
void PrintVersionBanner(const char *component);
int CheckPromises(enum cfagenttype ag);
void ReadPromises(enum cfagenttype ag,char *agents, bool verify);
int NewPromiseProposals(void);
void CompilationReport(char *filename);
void HashVariables(char *name);
void HashControls(void);
void CloseLog(void);
struct Constraint *ControlBodyConstraints(enum cfagenttype agent);
void SetFacility(const char *retval);
struct Bundle *GetBundle(char *name,char *agent);
struct SubType *GetSubTypeForBundle(char *type,struct Bundle *bp);
void CheckBundleParameters(char *scope,struct Rlist *args);
void PromiseBanner(struct Promise *pp);
void BannerBundle(struct Bundle *bp,struct Rlist *args);
void BannerSubBundle(struct Bundle *bp,struct Rlist *args);
void WritePID(char *filename);
void OpenCompilationReportFiles(const char *fname);
struct GenericAgentConfig GenericAgentDefaultConfig(enum cfagenttype agent_type);


#endif
