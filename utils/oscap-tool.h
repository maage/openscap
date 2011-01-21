/*
 * Copyright 2010 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      Maros Barabas <mbarabas@redhat.com>
 */


/* Standard header files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>

/* openscap common */
#include <oscap.h>
#include <error.h>
#include <text.h>
#include <reporter.h>

#ifdef ENABLE_OVAL
#include <oval_definitions.h>
#endif
#ifdef ENABLE_CVSS
#include <cvss.h>
#endif
#ifdef ENABLE_XCCDF
#include <xccdf.h>
#endif
#ifdef ENABLE_CPE
#include <cpedict.h>
#include <cpeuri.h>
#endif

#define INVALID_DOCUMENT_MSG "oscap was unable to validate the XML document you provided.\n"\
			     "Please ensure that the XML document is valid and well-formed, and try again."

struct oscap_action;
struct oscap_module;

typedef int(*oscap_tool_func)(const struct oscap_action* action);
typedef bool(*oscap_option_func)(int argc, char **argv, struct oscap_action* action);

struct oscap_module {
    const char *name;
    const char *usage;
    const char *usage_extra;
    const char *summary;
    const char *help;
    bool hidden;
    struct oscap_module *parent;
    struct oscap_module **submodules;
    oscap_tool_func func;
    oscap_option_func opt_parser;
    void *user;
};

// standard oscap CLI exit statuses
enum oscap_exitcode {
    OSCAP_OK             =   0, // successful exit
    OSCAP_ERROR          =   1, // an error occured
    OSCAP_FAIL           =   2, // a process (e.g. scan or validation) failed
    OSCAP_ERR_FETCH      =   1, // cold not fetch input file (same as error for now)
    OSCAP_BADARGS        = 100, // bad commandline arguments
    OSCAP_BADMODULE      = 101, // unrecognized module
    OSCAP_UNIMPL_MOD     = 110, // module functionality not implemented
    OSCAP_UNIMPL         = 111, // functionality not implemented
    // end of list
    OSCAP_EXITCODES_END_ = 120  // any code returned shall not be higher than this
};

#ifdef ENABLE_CVSS
struct cvss_metrics {
        cvss_access_vector_t ave;
        cvss_access_complexity_t ace;
        cvss_authentication_t aue;
        cvss_conf_impact_t cie;
        cvss_integ_impact_t iie;
        cvss_avail_impact_t aie;
        cvss_exploitability_t exe;
        cvss_remediation_level_t rle;
        cvss_report_confidence_t rce;
        cvss_collateral_damage_potential_t cde;
        cvss_target_distribution_t tde;
        cvss_conf_req_t cre;
        cvss_integ_req_t ire;
        cvss_avail_req_t are;
        double base;
};
#endif

#ifdef ENABLE_CPE
struct cpe_action {
	char * name;
	char * dict;
};
#endif

struct oscap_action {
        int doctype;
        struct oscap_module *module;
        char *f_xccdf;
        char *f_oval;
        char *f_results;
        char *f_report;
        char *url_xccdf;
        char *url_oval;
        char **urls_oval;
        char *profile;
        char *file_version;
        char *show;
        char *format;
        const char *tmpl;
        char *id;
        char *oval_template;
        int verbosity;
        int hide_profile_info;
#ifdef ENABLE_CVSS
        struct cvss_metrics *cvss_metrics;
#endif
#ifdef ENABLE_CPE
	struct cpe_action * cpe_action;
#endif
	bool force;
	bool oval_results;
};

int app_validate_xml(const struct oscap_action *action);
int app_xslt(const char *infile, const char *xsltfile, const char *outfile, const char **params);

int oscap_module_process(struct oscap_module *module, int argc, char **argv);
bool oscap_module_usage(struct oscap_module *module, FILE *out, const char *err, ...);
int oscap_module_call(struct oscap_action *action);

extern struct oscap_module OSCAP_ROOT_MODULE;
#ifdef ENABLE_XCCDF
extern struct oscap_module OSCAP_XCCDF_MODULE;
#endif
#ifdef ENABLE_CVSS
extern struct oscap_module OSCAP_CVSS_MODULE;
#endif
#ifdef ENABLE_OVAL
extern struct oscap_module OSCAP_OVAL_MODULE;
#endif
#ifdef ENABLE_CPE
extern struct oscap_module OSCAP_CPE_MODULE;
#endif

