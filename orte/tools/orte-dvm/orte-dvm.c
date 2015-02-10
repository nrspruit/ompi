/* -*- C -*-
 *
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2008 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2014 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2009 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2007-2013 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif  /* HAVE_STDLIB_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif  /* HAVE_STRINGS_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */
#include <fcntl.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "opal/mca/event/event.h"
#include "opal/mca/installdirs/installdirs.h"
#include "opal/mca/base/base.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/basename.h"
#include "opal/util/cmd_line.h"
#include "opal/util/opal_environ.h"
#include "opal/util/opal_getcwd.h"
#include "opal/util/show_help.h"
#include "opal/util/fd.h"

#include "opal/version.h"
#include "opal/runtime/opal.h"
#include "opal/util/os_path.h"
#include "opal/util/path.h"
#include "opal/class/opal_pointer_array.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/state/state.h"

#include "orte/runtime/runtime.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/show_help.h"

#include "orte/orted/orted.h"

/*
 * Globals
 */
static bool want_prefix_by_default = (bool) ORTE_WANT_ORTERUN_PREFIX_BY_DEFAULT;

/*
 * Globals
 */
static struct {
    bool help;
    bool version;
    char *report_uri;
    char *basename;
    char *prefix;
} myglobals;

static opal_cmd_line_init_t cmd_line_init[] = {
    /* Various "obvious" options */
    { NULL, 'h', NULL, "help", 0,
      &myglobals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },
    { NULL, 'V', NULL, "version", 0,
      &myglobals.version, OPAL_CMD_LINE_TYPE_BOOL,
      "Print version and exit" },

    { NULL, '\0', "report-uri", "report-uri", 1,
      &myglobals.report_uri, OPAL_CMD_LINE_TYPE_STRING,
      "Printout URI on stdout [-], stderr [+], or a file [anything else]" },
    
    { NULL, '\0', "prefix", "prefix", 1,
      &myglobals.prefix, OPAL_CMD_LINE_TYPE_STRING,
      "Prefix to be used to look for ORTE executables" },

    { "orte_debug_daemons", '\0', "debug-daemons", "debug-daemons", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Debug daemons" },

    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

static void notify_requestor(int sd, short args, void *cbdata);

int main(int argc, char *argv[])
{
    int rc, i, j;
    opal_cmd_line_t cmd_line;
    char *param, *value;
    orte_job_t *jdata=NULL;
    orte_app_context_t *app;
    char *uri;

    /* Setup and parse the command line */
    memset(&myglobals, 0, sizeof(myglobals));
    /* find our basename (the name of the executable) so that we can
       use it in pretty-print error messages */
    myglobals.basename = opal_basename(argv[0]);
    
    opal_cmd_line_create(&cmd_line, cmd_line_init);
    mca_base_cmd_line_setup(&cmd_line);
    if (OPAL_SUCCESS != (rc = opal_cmd_line_parse(&cmd_line, true,
                                                  argc, argv)) ) {
        if (OPAL_ERR_SILENT != rc) {
            fprintf(stderr, "%s: command line error (%s)\n", argv[0],
                    opal_strerror(rc));
        }
        return rc;
    }

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     * NOTE: It is "safe" to call mca_base_cmd_line_process_args() before 
     *  opal_init_util() since mca_base_cmd_line_process_args() does *not*
     *  depend upon opal_init_util() functionality.
     */
    if (OPAL_SUCCESS != mca_base_cmd_line_process_args(&cmd_line, &environ, &environ)) {
        exit(1);
    }

    /* Need to initialize OPAL so that install_dirs are filled in */
    if (OPAL_SUCCESS != opal_init(&argc, &argv)) {
        exit(1);
    }
    
    /* Check for some "global" command line params */
    /* print version if requested.  Do this before check for help so
       that --version --help works as one might expect. */
    if (myglobals.version) {
        char *str;
        char *project_name = NULL;
        if (0 == strcmp(myglobals.basename, "ompi-dvm")) {
            project_name = "Open MPI";
        } else {
            project_name = "OpenRTE";
        }
        str = opal_show_help_string("help-orterun.txt", "orterun:version", 
                                    false,
                                    myglobals.basename, project_name, OPAL_VERSION,
                                    PACKAGE_BUGREPORT);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        exit(0);
    }

    /* Check for help request */
    if (myglobals.help) {
        char *str, *args = NULL;
        char *project_name = NULL;
        if (0 == strcmp(myglobals.basename, "mpirun")) {
            project_name = "Open MPI";
        } else {
            project_name = "OpenRTE";
        }
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-orterun.txt", "orterun:usage", false,
                                    myglobals.basename, project_name, OPAL_VERSION,
                                    myglobals.basename, args,
                                    PACKAGE_BUGREPORT);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);

        /* If someone asks for help, that should be all we do */
        exit(0);
    }

    /* flag that I am the HNP */
    orte_process_info.proc_type = ORTE_PROC_HNP;
    
    /* Setup MCA params */
    orte_register_params();

    /* specify the DVM state machine */
    putenv("OMPI_MCA_state=dvm");
    
    /* Intialize our Open RTE environment */
    if (ORTE_SUCCESS != (rc = orte_init(&argc, &argv, ORTE_PROC_HNP))) {
        /* cannot call ORTE_ERROR_LOG as it could be the errmgr
         * never got loaded!
         */
        return rc;
    }
    /* finalize OPAL. As it was opened again from orte_init->opal_init
     * we continue to have a reference count on it. So we have to finalize it twice...
     */
    opal_finalize();

    /* check for request to report uri */
    uri = orte_rml.get_contact_info();
    if (NULL != myglobals.report_uri) {
        FILE *fp;
        if (0 == strcmp(myglobals.report_uri, "-")) {
            /* if '-', then output to stdout */
            printf("VMURI: %s\n", uri);
        } else if (0 == strcmp(myglobals.report_uri, "+")) {
            /* if '+', output to stderr */
            fprintf(stderr, "VMURI: %s\n", uri);
        } else {
            fp = fopen(myglobals.report_uri, "w");
            if (NULL == fp) {
                orte_show_help("help-orterun.txt", "orterun:write_file", false,
                               myglobals.basename, "pid", myglobals.report_uri);
                exit(0);
            }
            fprintf(fp, "%s\n", uri);
            fclose(fp);
        }
        free(uri);
    } else {
        printf("VMURI: %s\n", uri);
    }

    /* get the daemon job object - was created by ess/hnp component */
    if (NULL == (jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
        orte_show_help("help-orterun.txt", "bad-job-object", true,
                       myglobals.basename);
        exit(0);
    }
    /* also should have created a daemon "app" */
    if (NULL == (app = (orte_app_context_t*)opal_pointer_array_get_item(jdata->apps, 0))) {
        orte_show_help("help-orterun.txt", "bad-app-object", true,
                       myglobals.basename);
        exit(0);
    }

    /* Did the user specify a prefix, or want prefix by default? */
    if (opal_cmd_line_is_taken(&cmd_line, "prefix") || want_prefix_by_default) {
        size_t param_len;
        /* if both the prefix was given and we have a prefix
         * given above, check to see if they match
         */
        if (opal_cmd_line_is_taken(&cmd_line, "prefix") &&
            NULL != myglobals.prefix) {
            /* if they don't match, then that merits a warning */
            param = strdup(opal_cmd_line_get_param(&cmd_line, "prefix", 0, 0));
            /* ensure we strip any trailing '/' */
            if (0 == strcmp(OPAL_PATH_SEP, &(param[strlen(param)-1]))) {
                param[strlen(param)-1] = '\0';
            }
            value = strdup(myglobals.prefix);
            if (0 == strcmp(OPAL_PATH_SEP, &(value[strlen(value)-1]))) {
                value[strlen(value)-1] = '\0';
            }
            if (0 != strcmp(param, value)) {
                orte_show_help("help-orterun.txt", "orterun:app-prefix-conflict",
                               true, myglobals.basename, value, param);
                /* let the global-level prefix take precedence since we
                 * know that one is being used
                 */
                free(param);
                param = strdup(myglobals.prefix);
            }
            free(value);
        } else if (NULL != myglobals.prefix) {
            param = myglobals.prefix;
        } else if (opal_cmd_line_is_taken(&cmd_line, "prefix")){
            /* must be --prefix alone */
            param = strdup(opal_cmd_line_get_param(&cmd_line, "prefix", 0, 0));
        } else {
            /* --enable-orterun-prefix-default was given to orterun */
            param = strdup(opal_install_dirs.prefix);
        }

        if (NULL != param) {
            /* "Parse" the param, aka remove superfluous path_sep. */
            param_len = strlen(param);
            while (0 == strcmp (OPAL_PATH_SEP, &(param[param_len-1]))) {
                param[param_len-1] = '\0';
                param_len--;
                if (0 == param_len) {
                    orte_show_help("help-orterun.txt", "orterun:empty-prefix",
                                   true, myglobals.basename, myglobals.basename);
                    return ORTE_ERR_FATAL;
                }
            }
            orte_set_attribute(&app->attributes, ORTE_APP_PREFIX_DIR, ORTE_ATTR_GLOBAL, param, OPAL_STRING);
            free(param);
        }
    }

    /* Did the user specify a hostfile. Need to check for both 
     * hostfile and machine file. 
     * We can only deal with one hostfile per app context, otherwise give an error.
     */
    if (0 < (j = opal_cmd_line_get_ninsts(&cmd_line, "hostfile"))) {
        if(1 < j) {
            orte_show_help("help-orterun.txt", "orterun:multiple-hostfiles",
                           true, myglobals.basename, NULL);
            return ORTE_ERR_FATAL;
        } else {
            value = opal_cmd_line_get_param(&cmd_line, "hostfile", 0, 0);
            orte_set_attribute(&app->attributes, ORTE_APP_HOSTFILE, ORTE_ATTR_LOCAL, value, OPAL_STRING);
        }
    }
    if (0 < (j = opal_cmd_line_get_ninsts(&cmd_line, "machinefile"))) {
        if(1 < j || orte_get_attribute(&app->attributes, ORTE_APP_HOSTFILE, NULL, OPAL_STRING)) {
            orte_show_help("help-orterun.txt", "orterun:multiple-hostfiles",
                           true, myglobals.basename, NULL);
            return ORTE_ERR_FATAL;
        } else {
            value = opal_cmd_line_get_param(&cmd_line, "machinefile", 0, 0);
            orte_set_attribute(&app->attributes, ORTE_APP_HOSTFILE, ORTE_ATTR_LOCAL, value, OPAL_STRING);
        }
    }
 
    /* Did the user specify any hosts? */
    if (0 < (j = opal_cmd_line_get_ninsts(&cmd_line, "host"))) {
        char **targ=NULL, *tval;
        for (i = 0; i < j; ++i) {
            value = opal_cmd_line_get_param(&cmd_line, "host", i, 0);
            opal_argv_append_nosize(&targ, value);
        }
        tval = opal_argv_join(targ, ',');
        orte_set_attribute(&app->attributes, ORTE_APP_DASH_HOST, ORTE_ATTR_LOCAL, tval, OPAL_STRING);
        opal_argv_free(targ);
        free(tval);
    }
    OBJ_DESTRUCT(&cmd_line);

    /* setup to listen for commands sent specifically to me, even though I would probably
     * be the one sending them! Unfortunately, since I am a participating daemon,
     * there are times I need to send a command to "all daemons", and that means *I* have
     * to receive it too
     */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_DAEMON,
                            ORTE_RML_PERSISTENT, orte_daemon_recv, NULL);

    /* override the notify_completed state so we can send a message
     * back to anyone who submits a job to us telling them the job
     * completed */
    if (ORTE_SUCCESS != (rc = orte_state.set_job_state_callback(ORTE_JOB_STATE_NOTIFY_COMPLETED, notify_requestor))) {
        ORTE_ERROR_LOG(rc);
        ORTE_UPDATE_EXIT_STATUS(rc);
        exit(orte_exit_status);
    }

    /* spawn the DVM - we skip the initial steps as this
     * isn't a user-level application */
    ORTE_ACTIVATE_JOB_STATE(jdata, ORTE_JOB_STATE_ALLOCATE);

    /* loop the event lib until an exit event is detected */
    while (orte_event_base_active) {
        opal_event_loop(orte_event_base, OPAL_EVLOOP_ONCE);
    }

    /* cleanup and leave */
    orte_finalize();

    if (orte_debug_flag) {
        fprintf(stderr, "exiting with status %d\n", orte_exit_status);
    }
    exit(orte_exit_status);
}

static void send_callback(int status, orte_process_name_t *peer,
                          opal_buffer_t* buffer, orte_rml_tag_t tag,
                          void* cbdata)

{
    orte_job_t *jdata = (orte_job_t*)cbdata;
    
    OBJ_RELEASE(buffer);
    /* cleanup the job object */
    opal_pointer_array_set_item(orte_job_data, ORTE_LOCAL_JOBID(jdata->jobid), NULL);
    OBJ_RELEASE(jdata);
}
static void notify_requestor(int sd, short args, void *cbdata)
{
    orte_state_caddy_t *caddy = (orte_state_caddy_t*)cbdata;
    orte_job_t *jdata = caddy->jdata;
    orte_proc_t *pptr;
    int ret;
    opal_buffer_t *reply;
    
    /* notify the requestor */
    reply = OBJ_NEW(opal_buffer_t);
    /* see if there was any problem */
    if (orte_get_attribute(&jdata->attributes, ORTE_JOB_ABORTED_PROC, (void**)&pptr, OPAL_PTR) && NULL != pptr) {
        ret = pptr->exit_code;
    } else {
        ret = 0;
    }
    opal_dss.pack(reply, &ret, 1, OPAL_INT);
    orte_rml.send_buffer_nb(&jdata->originator, reply, ORTE_RML_TAG_TOOL, send_callback, jdata);

    /* we cannot cleanup the job object as we might
     * hit an error during transmission, so clean it
     * up in the send callback */
    OBJ_RELEASE(caddy);
}

