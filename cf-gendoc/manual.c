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

#include "manual.h"

#include "cf3.defs.h"

#include "vars.h"
#include "writer.h"
#include "mod_measurement.h"
#include "mod_exec.h"
#include "mod_access.h"
#include "item_lib.h"
#include "sort.h"
#include "scope.h"
#include "files_interfaces.h"
#include "assoc.h"
#include "cfstream.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

extern char BUILD_DIR[CF_BUFSIZE];

static void TexinfoHeader(FILE *fout);
static void TexinfoFooter(FILE *fout);
static void TexinfoBodyParts(const char *source_dir, FILE *fout, const BodySyntax *bs, const char *context);
static void TexinfoSubBodyParts(const char *source_dir, FILE *fout, BodySyntax *bs);
static void TexinfoShowRange(FILE *fout, char *s, DataType type);
static void IncludeManualFile(const char *source_dir, FILE *fout, char *filename);
static void TexinfoPromiseTypesFor(const char *source_dir, FILE *fout, const SubTypeSyntax *st);
static void TexinfoSpecialFunction(const char *source_dir, FILE *fout, FnCallType fn);
static void TexinfoVariables(const char *source_dir, FILE *fout, char *scope);
static char *TexInfoEscape(char *s);
static void PrintPattern(FILE *fout, const char *pattern);

/*****************************************************************************/

void TexinfoManual(const char *source_dir, const char *output_file)
{
    char filename[CF_BUFSIZE];
    const SubTypeSyntax *st;
    Item *done = NULL;
    FILE *fout;
    int i;

    if ((fout = fopen(output_file, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Unable to open %s for writing\n", filename);
        return;
    }

    TexinfoHeader(fout);

/* General background */

    fprintf(fout, "@c *****************************************************\n");
    fprintf(fout, "@c * CHAPTER \n");
    fprintf(fout, "@c *****************************************************\n");

    fprintf(fout, "@node Getting started\n@chapter CFEngine %s -- Getting started\n\n", Version());
    IncludeManualFile(source_dir, fout, "reference_basics.texinfo");

/* Control promises */

    fprintf(fout, "@c *****************************************************\n");
    fprintf(fout, "@c * CHAPTER \n");
    fprintf(fout, "@c *****************************************************\n");

    fprintf(fout, "@node Control Promises\n@chapter Control promises\n\n");
    IncludeManualFile(source_dir, fout, "reference_control_intro.texinfo");

    fprintf(fout, "@menu\n");
    for (i = 0; CF_ALL_BODIES[i].bundle_type != NULL; ++i)
    {
        fprintf(fout, "* control %s::\n", CF_ALL_BODIES[i].bundle_type);
    }
    fprintf(fout, "@end menu\n");

    for (i = 0; CF_ALL_BODIES[i].bundle_type != NULL; i++)
    {
        fprintf(fout, "@node control %s\n@section @code{%s} control promises\n\n", CF_ALL_BODIES[i].bundle_type,
                CF_ALL_BODIES[i].bundle_type);
        snprintf(filename, CF_BUFSIZE - 1, "control/%s_example.texinfo", CF_ALL_BODIES[i].bundle_type);
        IncludeManualFile(source_dir, fout, filename);
        snprintf(filename, CF_BUFSIZE - 1, "control/%s_notes.texinfo", CF_ALL_BODIES[i].bundle_type);
        IncludeManualFile(source_dir, fout, filename);

        TexinfoBodyParts(source_dir, fout, CF_ALL_BODIES[i].bs, CF_ALL_BODIES[i].bundle_type);
    }

/* Components */

    for (i = 0; i < CF3_MODULES; i++)
    {
        st = (CF_ALL_SUBTYPES[i]);

        if ((st == CF_COMMON_SUBTYPES) || (st == CF_EXEC_SUBTYPES) || (st == CF_REMACCESS_SUBTYPES)
            || (st == CF_MEASUREMENT_SUBTYPES))

        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Dealing with chapter / bundle type %s\n", st->bundle_type);
            fprintf(fout, "@c *****************************************************\n");
            fprintf(fout, "@c * CHAPTER \n");
            fprintf(fout, "@c *****************************************************\n");

            if (strcmp(st->bundle_type, "*") == 0)
            {
                fprintf(fout, "@node Bundles for common\n@chapter Bundles of @code{common}\n\n");
            }
            else
            {
                fprintf(fout, "@node Bundles for %s\n@chapter Bundles of @code{%s}\n\n", st->bundle_type, st->bundle_type);
            }
        }

        if (!IsItemIn(done, st->bundle_type)) /* Avoid multiple reading if several modules */
        {
            char bundle_filename[CF_BUFSIZE];
            if (strcmp(st->bundle_type, "*") == 0)
            {
                strcpy(bundle_filename, "common");
            }
            else
            {
                strlcpy(bundle_filename, st->bundle_type, CF_BUFSIZE);
            }
            PrependItem(&done, st->bundle_type, NULL);
            snprintf(filename, CF_BUFSIZE - 1, "bundletypes/%s_example.texinfo", bundle_filename);
            IncludeManualFile(source_dir, fout, filename);
            snprintf(filename, CF_BUFSIZE - 1, "bundletypes/%s_notes.texinfo", bundle_filename);
            IncludeManualFile(source_dir, fout, filename);

            fprintf(fout, "@menu\n");
            for (int k = 0; k < CF3_MODULES; ++k)
            {
                for (int j = 0; CF_ALL_SUBTYPES[k][j].bundle_type != NULL; ++j)
                {
                    const char *constraint_type_name;
                    if (strcmp(CF_ALL_SUBTYPES[k][j].subtype, "*") == 0)
                    {
                        constraint_type_name = "Miscellaneous";
                    }
                    else
                    {
                        constraint_type_name = CF_ALL_SUBTYPES[k][j].subtype;
                    }

                    const char *bundle_type_name;
                    if (strcmp(CF_ALL_SUBTYPES[k][j].bundle_type, "*") == 0)
                    {
                        bundle_type_name = "common";
                    }
                    else
                    {
                        bundle_type_name = CF_ALL_SUBTYPES[k][j].bundle_type;
                    }

                    fprintf(fout, "* %s in %s promises: %s in %s promises\n", CF_ALL_SUBTYPES[k][j].subtype,
                            bundle_type_name,
                            constraint_type_name,
                            bundle_type_name);
                }
            }
            fprintf(fout, "@end menu\n");
        }

        TexinfoPromiseTypesFor(source_dir, fout, st);
    }

/* Special functions */

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Dealing with chapter / bundle type - special functions\n");
    fprintf(fout, "@c *****************************************************\n");
    fprintf(fout, "@c * CHAPTER \n");
    fprintf(fout, "@c *****************************************************\n");

    fprintf(fout, "@node Special functions\n@chapter Special functions\n\n");

    fprintf(fout, "@menu\n");
    fprintf(fout, "* Introduction to functions::\n");

    for (i = 0; CF_FNCALL_TYPES[i].name != NULL; ++i)
    {
        fprintf(fout, "* Function %s::\n", CF_FNCALL_TYPES[i].name);
    }

    fprintf(fout, "@end menu\n");

    fprintf(fout, "@node Introduction to functions\n@section Introduction to functions\n\n");

    IncludeManualFile(source_dir, fout, "functions_intro.texinfo");

    for (i = 0; CF_FNCALL_TYPES[i].name != NULL; i++)
    {
        fprintf(fout, "@node Function %s\n@section Function %s \n\n", CF_FNCALL_TYPES[i].name, CF_FNCALL_TYPES[i].name);
        TexinfoSpecialFunction(source_dir, fout, CF_FNCALL_TYPES[i]);
    }

/* Special variables */

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Dealing with chapter / bundle type - special variables\n");
    fprintf(fout, "@c *****************************************************\n");
    fprintf(fout, "@c * CHAPTER \n");
    fprintf(fout, "@c *****************************************************\n");

    fprintf(fout, "@node Special Variables\n@chapter Special Variables\n\n");

    static const char *scopes[] =
    {
        "const",
        "edit",
        "match",
        "mon",
        "sys",
        "this",
        NULL,
    };

    fprintf(fout, "@menu\n");
    for (const char **s = scopes; *s != NULL; ++s)
    {
        fprintf(fout, "* Variable context %s::\n", *s);
    }
    fprintf(fout, "@end menu\n");

// scopes const and sys

    NewScope("edit");
    NewScalar("edit", "filename", "x", DATA_TYPE_STRING);

    NewScope("match");
    NewScalar("match", "0", "x", DATA_TYPE_STRING);

    for (const char **s = scopes; *s != NULL; ++s)
    {
        TexinfoVariables(source_dir, fout, (char *) *s);
    }

// Log files

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Dealing with chapter / bundle type - Logs and records\n");
    fprintf(fout, "@c *****************************************************\n");
    fprintf(fout, "@c * CHAPTER \n");
    fprintf(fout, "@c *****************************************************\n");

    fprintf(fout, "@node Logs and records\n@chapter Logs and records\n\n");
    IncludeManualFile(source_dir, fout, "reference_logs.texinfo");

    TexinfoFooter(fout);

    fclose(fout);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static void TexinfoHeader(FILE *fout)
{
    fprintf(fout,
            "\\input texinfo-altfont\n"
            "\\input texinfo-logo\n"
            "\\input texinfo\n"
            "@selectaltfont{cmbright}\n"
            "@setlogo{CFEngineFrontPage}\n"
            "@c *********************************************************************\n"
            "@c\n"
            "@c  This is an AUTO_GENERATED TEXINFO file. Do not submit patches against it.\n"
            "@c  Refer to the the component .texinfo files instead when patching docs.\n"
            "@c\n"
            "@c ***********************************************************************\n"
            "@c %%** start of header\n"
            "@setfilename cf3-Reference.info\n"
            "@settitle CFEngine reference manual\n"
            "@setchapternewpage odd\n"
            "@c %%** end of header\n"
            "@titlepage\n"
            "@title CFEngine Reference Manual\n" "@subtitle Auto generated, self-healing knowledge\n" "@subtitle %s\n"
#ifdef HAVE_NOVA
            "@subtitle %s\n"
#endif
            "@author cfengine.com\n"
            "@c @smallbook\n"
            "@fonttextsize 10\n"
            "@page\n"
            "@vskip 0pt plus 1filll\n"
            "@cartouche\n"
            "Under no circumstances shall CFEngine AS be liable for errors or omissions\n"
            "in this document. All efforts have been made to ensure the correctness of\n"
            "the information contained herein.\n"
            "@end cartouche\n"
            "Copyright @copyright{} 2008,2010 to the year of issue CFEngine AS\n"
            "@end titlepage\n"
            "@c *************************** File begins here ************************\n"
            "@ifinfo\n"
            "@dircategory CFEngine Training\n"
            "@direntry\n"
            "* cfengine Reference:\n"
            "                        CFEngine is a language based framework\n"
            "                        designed for configuring and maintaining\n"
            "                        Unix-like operating systems attached\n"
            "                        to a TCP/IP network.\n"
            "@end direntry\n"
            "@end ifinfo\n"
            "@ifnottex\n"
            "@node Top\n"
            "@top CFEngine-AutoReference\n"
            "@end ifnottex\n"
            "@menu\n"
            "* Getting started::\n"
            "* A simple crash course::\n"
            "* How to run CFEngine 3 examples::\n"
            "* A complete configuration::\n"
            "* Control Promises::\n"
            "* Bundles for common::\n"
            "* Bundles for agent::\n"
            "* Bundles for server::\n"
            "* Bundles for knowledge::\n"
            "* Bundles for monitor::\n"
            "* Special functions::\n"
            "* Special Variables::\n"
            "* Logs and records::\n"
            "@end menu\n"
            "@ifhtml\n"
            "@html\n"
            "<a href=\"#Contents\"><h1>COMPLETE TABLE OF CONTENTS</h1></a>\n"
            "<h2>Summary of contents</h2>\n"
            "@end html\n" "@end ifhtml\n" "@iftex\n" "@contents\n" "@end iftex\n", NameVersion()
#ifdef HAVE_NOVA
            , Nova_NameVersion()
#endif
        );
}

/*****************************************************************************/

static void TexinfoFooter(FILE *fout)
{
    fprintf(fout,
            "@c =========================================================================\n"
            "@c @node Index\n"
            "@c @unnumbered Concept Index\n"
            "@c @printindex cp\n"
            "@c =========================================================================\n"
            "@ifhtml\n"
            "@html\n"
            "<a name=\"Contents\">\n"
            "@contents\n"
            "@end html\n"
            "@end ifhtml\n"
            "@c  The file is structured like a programming language. Each chapter\n"
            "@c  starts with a chapter comment.\n"
            "@c\n"
            "@c  Menus list the subsections so that an online info-reader can parse\n"
            "@c  the file hierarchically.\n"
            "@ifhtml\n"
            "@html\n"
            "<script type=\"text/javascript\">\n"
            "var gaJsHost = ((\"https:\" == document.location.protocol) ? \"https://ssl.\" : \"http://www.\");\n"
            "document.write(unescape(\"%%3Cscript src='\" + gaJsHost + \"google-analytics.com/ga.js' type='text/javascript'%%3E%%3C/script%%3E\"));\n"
            "</script>\n"
            "<script type=\"text/javascript\">\n"
            "var pageTracker = _gat._getTracker(\"UA-2576171-2\");\n"
            "pageTracker._initData();\n"
            "pageTracker._trackPageview();\n" "</script>\n" "@end html\n" "@end ifhtml\n" "@bye\n");
}

/*****************************************************************************/

static void TexinfoPromiseTypesFor(const char *source_dir, FILE *fout, const SubTypeSyntax *st)
{
    int j;
    char filename[CF_BUFSIZE];

/* Each array element is SubtypeSyntax representing an agent-promise assoc */

    for (j = 0; st[j].bundle_type != NULL; j++)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " - Dealing with promise type %s\n", st[j].subtype);

        if ((strcmp("*", st[j].subtype) == 0) && (strcmp("*", st[j].bundle_type) == 0))
        {
            fprintf(fout, "\n\n@node Miscellaneous in common promises\n@section @code{%s} promises\n\n", 
                    st[j].subtype);
            snprintf(filename, CF_BUFSIZE - 1, "promise_common_intro.texinfo");
        }
        else if ((strcmp("*", st[j].subtype) == 0) && ((strcmp("edit_line", st[j].bundle_type) == 0) || (strcmp("edit_xml", st[j].bundle_type) == 0)))
        {
            fprintf(fout, "\n\n@node Miscellaneous in %s promises\n@section Miscelleneous in @code{%s} promises\n\n", st[j].bundle_type, st[j].bundle_type);
            snprintf(filename, CF_BUFSIZE - 1, "promises/%s_intro.texinfo", st[j].bundle_type);

        }
        else
        {
            if (strcmp("*", st[j].bundle_type) == 0)
            {
                fprintf(fout, "\n\n@node %s in common promises\n@section @code{%s} promises in @samp{%s}\n\n", st[j].subtype,
                    st[j].subtype, st[j].bundle_type);            
            }
            else
            {
                fprintf(fout, "\n\n@node %s in %s promises\n@section @code{%s} promises in @samp{%s}\n\n", st[j].subtype,
                    st[j].bundle_type, st[j].subtype, st[j].bundle_type);
            }

            char subtype_filename[CF_BUFSIZE];
            if (strcmp("*", st[j].subtype))
            {
                strcpy(subtype_filename, "common");
            }
            else
            {
                strlcpy(subtype_filename, st[j].subtype, CF_BUFSIZE);
            }

            snprintf(filename, CF_BUFSIZE - 1, "promises/%s_intro.texinfo", subtype_filename);
            IncludeManualFile(source_dir, fout, filename);
            snprintf(filename, CF_BUFSIZE - 1, "promises/%s_example.texinfo", subtype_filename);
            IncludeManualFile(source_dir, fout, filename);
            snprintf(filename, CF_BUFSIZE - 1, "promises/%s_notes.texinfo", subtype_filename);
        }
        IncludeManualFile(source_dir, fout, filename);
        TexinfoBodyParts(source_dir, fout, st[j].bs, st[j].subtype);
    }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static void TexinfoBodyParts(const char *source_dir, FILE *fout, const BodySyntax *bs, const char *context)
{
    int i;
    char filename[CF_BUFSIZE];

    if (bs == NULL)
    {
        return;
    }

    if (bs[0].lval != NULL)
    {
        fprintf(fout, "@menu\n");

        for (i = 0; bs[i].lval != NULL; ++i)
        {
            fprintf(fout, "* %s in %s::\n", bs[i].lval, context);
        }

        fprintf(fout, "@end menu\n");
    }

    for (i = 0; bs[i].lval != NULL; i++)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " - -  Dealing with body type %s\n", bs[i].lval);

        if (bs[i].range == (void *) CF_BUNDLE)
        {
            fprintf(fout, "\n\n@node %s in %s\n@subsection @code{%s}\n\n@b{Type}: %s (Separate Bundle) \n", bs[i].lval,
                    context, bs[i].lval, CF_DATATYPES[bs[i].dtype]);
        }
        else if (bs[i].dtype == DATA_TYPE_BODY)
        {
            fprintf(fout, "\n\n@node %s in %s\n@subsection @code{%s} (body template)\n@noindent @b{Type}: %s\n\n",
                    bs[i].lval, context, bs[i].lval, CF_DATATYPES[bs[i].dtype]);
            TexinfoSubBodyParts(source_dir, fout, (BodySyntax *) bs[i].range);
        }
        else
        {
            const char *res = bs[i].default_value;

            fprintf(fout, "\n\n@node %s in %s\n@subsection @code{%s}\n@noindent @b{Type}: %s\n\n", bs[i].lval, context,
                    bs[i].lval, CF_DATATYPES[bs[i].dtype]);
            TexinfoShowRange(fout, (char *) bs[i].range, bs[i].dtype);

            if (res)
            {
                fprintf(fout, "@noindent @b{Default value:} %s\n", res);
            }

            fprintf(fout, "\n@noindent @b{Synopsis}: %s\n\n", bs[i].description);
            fprintf(fout, "\n@noindent @b{Example}:@*\n");
            snprintf(filename, CF_BUFSIZE - 1, "bodyparts/%s_example.texinfo", bs[i].lval);
            IncludeManualFile(source_dir, fout, filename);
            fprintf(fout, "\n@noindent @b{Notes}:@*\n");
            snprintf(filename, CF_BUFSIZE - 1, "bodyparts/%s_notes.texinfo", bs[i].lval);
            IncludeManualFile(source_dir, fout, filename);
        }
    }
}

/*******************************************************************/

static void TexinfoVariables(const char *source_dir, FILE *fout, char *scope)
{
    char filename[CF_BUFSIZE], varname[CF_BUFSIZE];
    Rlist *rp, *list = NULL;
    int i;

    char *extra_mon[] = { "listening_udp4_ports", "listening_tcp4_ports", "listening_udp6_ports", "listening_tcp6_ports", NULL };
    
    HashToList(GetScope(scope), &list);
    list = AlphaSortRListNames(list);

    fprintf(fout, "\n\n@node Variable context %s\n@section Variable context @code{%s}\n\n", scope, scope);
    snprintf(filename, CF_BUFSIZE - 1, "varcontexts/%s_intro.texinfo", scope);
    IncludeManualFile(source_dir, fout, filename);

    fprintf(fout, "@menu\n");

    if (strcmp(scope, "mon") != 0)
    {
        for (rp = list; rp != NULL; rp = rp->next)
        {
            fprintf(fout, "* Variable %s.%s::\n", scope, (char *) rp->item);
        }
    }
    else
    {
        for (i = 0; extra_mon[i] != NULL; i++)        
        {
            fprintf(fout, "* Variable %s.%s::\n", "mon", extra_mon[i]);
        }

        for (i = 0; i < CF_OBSERVABLES; ++i)
        {
            if (strcmp(OBS[i][0], "spare") == 0)
            {
                break;
            }

            fprintf(fout, "* Variable mon.value_%s::\n", OBS[i][0]);
            fprintf(fout, "* Variable mon.av_%s::\n", OBS[i][0]);
            fprintf(fout, "* Variable mon.dev_%s::\n", OBS[i][0]);
        }
    }

    fprintf(fout, "@end menu\n");

    if (strcmp(scope, "mon") != 0)
    {
        for (rp = list; rp != NULL; rp = rp->next)
        {
            fprintf(fout, "@node Variable %s.%s\n@subsection Variable %s.%s \n\n", scope, (char *) rp->item, scope,
                    (char *) rp->item);
            snprintf(filename, CF_BUFSIZE - 1, "vars/%s_%s.texinfo", scope, (char *) rp->item);
            IncludeManualFile(source_dir, fout, filename);
        }
    }
    else
    {
        for (i = 0; extra_mon[i] != NULL; i++)        
        {
            fprintf(fout, "\n@node Variable %s.%s\n@subsection Variable %s.%s \n\n", "mon", extra_mon[i], "mon", extra_mon[i]);
            fprintf(fout, "List variable containing an observational measure collected every 2.5 minutes from cf-monitord, description: port numbers that were observed to be set up to receive connections on the host concerned");

        }

    
        for (i = 0; i < CF_OBSERVABLES; i++)
        {
            if (strcmp(OBS[i][0], "spare") == 0)
            {
                break;
            }

            snprintf(varname, CF_MAXVARSIZE, "value_%s", OBS[i][0]);
            fprintf(fout, "\n@node Variable %s.%s\n@subsection Variable %s.%s \n\n", scope, varname, scope, varname);
            fprintf(fout, "Observational measure collected every 2.5 minutes from cf-monitord, description: @var{%s}.",
                    OBS[i][1]);

            snprintf(varname, CF_MAXVARSIZE, "av_%s", OBS[i][0]);
            fprintf(fout, "\n@node Variable %s.%s\n@subsection Variable %s.%s \n\n", scope, varname, scope, varname);
            fprintf(fout, "Observational measure collected every 2.5 minutes from cf-monitord, description: @var{%s}.",
                    OBS[i][1]);

            snprintf(varname, CF_MAXVARSIZE, "dev_%s", OBS[i][0]);
            fprintf(fout, "\n@node Variable %s.%s\n@subsection Variable %s.%s \n\n", scope, varname, scope, varname);
            fprintf(fout, "Observational measure collected every 2.5 minutes from cf-monitord, description: @var{%s}.",
                    OBS[i][1]);
        }
    }

    RlistDestroy(list);
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static void TexinfoShowRange(FILE *fout, char *s, DataType type)
{
    Rlist *list = NULL, *rp;

    if (strlen(s) == 0)
    {
        fprintf(fout, "@noindent @b{Allowed input range}: (arbitrary string)\n\n");
        return;
    }

    if ((type == DATA_TYPE_OPTION) || (type == DATA_TYPE_OPTION_LIST))
    {
        list = RlistFromSplitString(s, ',');
        fprintf(fout, "@noindent @b{Allowed input range}: @*\n@example");

        for (rp = list; rp != NULL; rp = rp->next)
        {
            fprintf(fout, "\n          @code{%s}", (char *) rp->item);
        }

        fprintf(fout, "\n@end example\n");
        RlistDestroy(list);
    }
    else
    {
        fprintf(fout, "@noindent @b{Allowed input range}: @code{%s}\n\n", TexInfoEscape(s));
    }
}

/*****************************************************************************/

static void TexinfoSubBodyParts(const char *source_dir, FILE *fout, BodySyntax *bs)
{
    int i;
    char filename[CF_BUFSIZE];

    if (bs == NULL)
    {
        return;
    }

    fprintf(fout, "@table @samp\n");

    for (i = 0; bs[i].lval != NULL; i++)
    {
        if (bs[i].range == (void *) CF_BUNDLE)
        {
            fprintf(fout, "@item @code{%s}\n@b{Type}: %s\n (Separate Bundle) \n\n", bs[i].lval,
                    CF_DATATYPES[bs[i].dtype]);
        }
        else if (bs[i].dtype == DATA_TYPE_BODY)
        {
            fprintf(fout, "@item @code{%s}\n@b{Type}: %s\n\n", bs[i].lval, CF_DATATYPES[bs[i].dtype]);
            TexinfoSubBodyParts(source_dir, fout, (BodySyntax *) bs[i].range);
        }
        else
        {
            const char *res = bs[i].default_value;

            fprintf(fout, "@item @code{%s}\n@b{Type}: %s\n\n", bs[i].lval, CF_DATATYPES[bs[i].dtype]);
            TexinfoShowRange(fout, (char *) bs[i].range, bs[i].dtype);
            fprintf(fout, "\n@noindent @b{Synopsis}: %s\n\n", bs[i].description);

            if (res)
            {
                fprintf(fout, "\n@noindent @b{Default value:} %s\n", res);
            }

            fprintf(fout, "\n@b{Example}:@*\n");
            snprintf(filename, CF_BUFSIZE - 1, "bodyparts/%s_example.texinfo", bs[i].lval);
            IncludeManualFile(source_dir, fout, filename);
            fprintf(fout, "\n@b{Notes}:@*\n");
            snprintf(filename, CF_BUFSIZE - 1, "bodyparts/%s_notes.texinfo", bs[i].lval);
            IncludeManualFile(source_dir, fout, filename);
        }
    }

    fprintf(fout, "@end table\n");
}

/*****************************************************************************/

static bool GenerateStub(const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "fopen", "Could not write to manual source %s\n", filename);
        return false;
    }

#ifdef HAVE_NOVA
    fprintf(fp, "\n@i{History}: Was introduced in %s, Enterprise %s (%d)\n\n", Version(), Nova_Version(), BUILD_YEAR);
#else
    fprintf(fp, "\n@i{History}: Was introduced in %s (%d)\n\n", Version(), BUILD_YEAR);
#endif
    fprintf(fp, "\n@verbatim\n\nFill me in (%s)\n\"\"\n@end verbatim\n", filename);
    fclose(fp);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Created %s template\n", filename);
    return true;
}

/*****************************************************************************/

char *ReadTexinfoFileF(const char *source_dir, const char *fmt, ...)
{
    Writer *filenamew = StringWriter();

    struct stat sb;
    char *buffer = NULL;
    FILE *fp = NULL;
    off_t file_size;

    va_list ap;

    va_start(ap, fmt);
    WriterWriteF(filenamew, "%s/", source_dir);
    WriterWriteVF(filenamew, fmt, ap);
    va_end(ap);

    char *filename = StringWriterClose(filenamew);

    if (cfstat(filename, &sb) == -1)
    {
        if (!GenerateStub(filename))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Unable to write down stub for missing texinfo file");
            free(filename);
            return NULL;
        }
    }

    if ((fp = fopen(filename, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "fopen", "Could not read manual source %s\n", filename);
        free(filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftello(fp);
    fseek(fp, 0, SEEK_SET);

    buffer = (char *) xcalloc(file_size + 1, sizeof(char));
    buffer[file_size] = '\0';
    int cnt = fread(buffer, sizeof(char), file_size, fp);

    if ((ferror(fp)) || (cnt != file_size))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "fread", "Could not read manual source %s\n", filename);
        free(buffer);
        fclose(fp);
        free(filename);
        return NULL;
    }

    fclose(fp);
    free(filename);

    return buffer;
}

/*****************************************************************************/

static void IncludeManualFile(const char *source_dir, FILE *fout, char *file)
{
    char *contents = ReadTexinfoFileF(source_dir, "%s", file);

    if (contents)
    {
        fprintf(fout, "@*\n%s\n", contents);
    }
}

/*****************************************************************************/

static void TexinfoSpecialFunction(const char *source_dir, FILE *fout, FnCallType fn)
{
    char filename[CF_BUFSIZE];
    const FnCallArg *args = fn.args;
    int i;

    fprintf(fout, "\n@noindent @b{Synopsis}: %s(", fn.name);

    for (i = 0; args[i].pattern != NULL; i++)
    {
        fprintf(fout, "arg%d", i + 1);

        if (args[i + 1].pattern != NULL)
        {
            fprintf(fout, ",");
        }
    }
    if (fn.varargs)
    {
        if (i != 0)
        {
            fprintf(fout, ",");
        }
        fprintf(fout, "...");
    }

    fprintf(fout, ") returns type @b{%s}\n\n@*\n", CF_DATATYPES[fn.dtype]);

    for (i = 0; args[i].pattern != NULL; i++)
    {
        fprintf(fout, "@noindent @code{  } @i{arg%d} : %s, @i{in the range} ", i + 1, args[i].description);
        PrintPattern(fout, args[i].pattern);
        fprintf(fout, "\n@*\n");
    }

    fprintf(fout, "\n@noindent %s\n\n", fn.description);
    fprintf(fout, "\n@noindent @b{Example}:@*\n");

    snprintf(filename, CF_BUFSIZE - 1, "functions/%s_example.texinfo", fn.name);
    IncludeManualFile(source_dir, fout, filename);

    fprintf(fout, "\n@noindent @b{Notes}:@*\n");
    snprintf(filename, CF_BUFSIZE - 1, "functions/%s_notes.texinfo", fn.name);
    IncludeManualFile(source_dir, fout, filename);
}

/*****************************************************************************/

static void PrintPattern(FILE *fout, const char *pattern)
{
    const char *sp;

    for (sp = pattern; *sp != '\0'; sp++)
    {
        switch (*sp)
        {
        case '@':
        case '{':
        case '}':
            fputc((int) '@', fout);
        default:
            fputc((int) *sp, fout);
        }
    }
}

/*****************************************************************************/

char *TexInfoEscape(char *s)
{
    char *spf, *spt;
    static char buffer[CF_BUFSIZE];

    memset(buffer, 0, CF_BUFSIZE);

    for (spf = s, spt = buffer; *spf != '\0'; spf++)
    {
        switch (*spf)
        {
        case '{':
        case '}':
        case '@':
            *spt++ = '@';
            break;
        }

        *spt++ = *spf;
    }

    return buffer;
}
