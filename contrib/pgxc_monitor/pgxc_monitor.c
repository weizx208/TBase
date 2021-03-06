/*
 * Tencent is pleased to support the open source community by making TBase available.  
 * 
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.  All rights reserved.
 * 
 * TBase is licensed under the BSD 3-Clause License, except for the third-party component listed below. 
 * 
 * A copy of the BSD 3-Clause License is included in this file.
 * 
 * Other dependencies and licenses:
 * 
 * Open Source Software Licensed Under the PostgreSQL License: 
 * --------------------------------------------------------------------
 * 1. Postgres-XL XL9_5_STABLE
 * Portions Copyright (c) 2015-2016, 2ndQuadrant Ltd
 * Portions Copyright (c) 2012-2015, TransLattice, Inc.
 * Portions Copyright (c) 2010-2017, Postgres-XC Development Group
 * Portions Copyright (c) 1996-2015, The PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, The Regents of the University of California
 * 
 * Terms of the PostgreSQL License: 
 * --------------------------------------------------------------------
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written agreement
 * is hereby granted, provided that the above copyright notice and this
 * paragraph and the following two paragraphs appear in all copies.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 * 
 * 
 * Terms of the BSD 3-Clause License:
 * --------------------------------------------------------------------
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of THL A29 Limited nor the names of its contributors may be used to endorse or promote products derived from this software without 
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
 * DAMAGE.
 * 
 */
/*
 * -----------------------------------------------------------------------------
 *
 * pgxc_monitor utility
 *
 *  Monitors if a given node is running or not.
 *
 * Command syntax:
 *
 * pgxc_monitor [ options ]
 *
 * Options are:
 * -Z nodetype        What node type to monitor, gtm or node.
 *                    gtm tests gtm, gtm_standby or gtm_proxy.
 *                    node tests Coordinator or Datanode.
 * -p port            Port number of the monitored node.
 * -h host            Host name or IP address of the monitored node.
 * -n nodename      Specifies pgxc_monitor node name. Default is "pgxc_monitor"
 * -q                Run in quiet mode. Default is quiet mode.
 * -v                Run in verbose mode.
 * -d database        Database name to connect to.
 * -U username        Connect as specified database user.
 * --help            Prints the help message and exits with 0.
 *
 * When monitoring Coordinator or Datanode, -p and -h options can be
 * supplied via .pgpass file. If you use non-default target database name
 * and username, they must also be supplied by .pgpass file.
 * If password is needed, it must also be supplied by .pgpass file.
 *
 * Monitoring Coordinator and Datanode uses system(3) function.  Therefore,
 * you should not use set-userid bit or set-groupid bit. Also, because
 * this uses psql command, psql must be in your PATH.
 *
 * When testing Coordinator/Datanode, you must setup .pgpass file if you
 * need to supply password, as well as non-default database name and username.
 *
 * The username and database name can be specified via command line options
 * too. If password is needed, it must be supplied via .pgpass file though.
 *
 * If invalid parameters are given, error message will be printed even if
 * -q is specified.
 *
 * --------------------------------------------------------------------------
 */


#include "gtm/gtm_client.h"
#include "gtm/libpq-fe.h"

#include <stdlib.h>
#include <getopt.h>
#include <sys/wait.h>

/* Define all the node types */
typedef enum
{
    NONE = 0,
    GTM,    /* GTM or GTM-proxy */
    NODE    /* Coordinator or Datanode */
} nodetype_t;

static char     *progname;

#define Free(x) do{if((x)) free((x)); x = NULL;} while(0)

static void usage(void);
static int do_gtm_ping(char *host, char *node, nodetype_t nodetype, char *nodename, bool verbose);
static int do_node_ping(char *host, char *node, char *username, char *database, bool verbose);

int
main(int ac, char *av[])
{
    int opt;
    nodetype_t    nodetype = NONE;
    char       *port = NULL;
    char       *host = NULL;
    char       *nodename = NULL;
    bool        verbose = false;
    char       *username = NULL;
    char       *database = NULL;

    progname = strdup(av[0]);

    /* Print help if necessary */
    if (ac > 1)
    {
        if (strcmp(av[1], "--help") == 0 || strcmp(av[1], "-?") == 0)
        {
            usage();
            exit(0);
        }
    }

    /* Scan options */
    while ((opt = getopt(ac, av, "Z:U:d:h:n:p:qv")) != -1)
    {
        switch(opt)
        {
            case 'Z':
                if (strcmp(optarg, "gtm") == 0)
                    nodetype = GTM;
                else if (strcmp(optarg, "node") == 0)
                    nodetype = NODE;
                else
                {
                    fprintf(stderr, "%s: invalid -Z option value.\n", progname);
                    exit(3);
                }
                break;
            case 'h':
                Free(host);
                host = strdup(optarg);
                break;
            case 'n':
                Free(nodename);
                nodename = strdup(optarg);
                break;
            case 'p':
                Free(port);
                port = strdup(optarg);
                break;
            case 'q':
                verbose = false;
                break;
            case 'v':
                verbose = true;
                break;
            case 'U':
                username = strdup(optarg);
                break;
            case 'd':
                database = strdup(optarg);
                break;
            default:
                fprintf(stderr, "%s: unknow option %c.\n", progname, opt);
                exit(3);
        }
    }

    /* If no types are defined, well there is nothing to be done */
    if (nodetype == NONE)
    {
        fprintf(stderr, "%s: -Z option is missing, it is mandatory.\n", progname);
        usage();
        exit(3);
    }

    switch(nodetype)
    {
        case GTM:
            exit(do_gtm_ping(host, port, nodetype, nodename, verbose));
        case NODE:
            exit(do_node_ping(host, port, username, database, verbose));
        case NONE:
        default:
            break;
    }

    /* Should not happen */
    fprintf(stderr, "%s: internal error.\n", progname);
    exit(3);
}

/*
 * Ping a given GTM or GTM-proxy
 */
static int
do_gtm_ping(char *host, char* port, nodetype_t nodetype, char *nodename, bool verbose)
{
    char connect_str[256];
    GTM_Conn *conn;

    if (host == NULL)
    {
        fprintf(stderr, "%s: -h is mandatory for -Z gtm or -Z gtm_proxy\n", progname);
        exit(3);
    }
    if (port == NULL)
    {
        fprintf(stderr, "%s: -p is mandatory for -Z gtm or -Z gtm_proxy\n", progname);
        exit(3);
    }
    /* Use 60s as connection timeout */
    sprintf(connect_str, "host=%s port=%s node_name=%s remote_type=%d postmaster=0 connect_timeout=60",
            host, port, nodename ? nodename : "pgxc_monitor", GTM_NODE_COORDINATOR);
    if ((conn = PQconnectGTM(connect_str)) == NULL)
    {
        if (verbose)
            fprintf(stderr, "%s: Could not connect to %s\n", progname, "GTM");
        exit(1);
    }
    GTMPQfinish(conn);
    if (verbose)
        printf("Running\n");
    return 0;
}

/*
 * Ping a given node
 */
static int
do_node_ping(char *host, char *port, char *username, char *database, bool verbose)
{
    int rc;
    int exitStatus;
    char command_line[1024];
    char *quiet_out = " > /dev/null 2> /dev/null";
    char *verbose_out = "";
    char *out = verbose ? verbose_out : quiet_out;

    /* Build psql command launched to node */
    sprintf(command_line, "psql -w -q -c \"select 1 a\"");

    /* Then add options if necessary */
    if (username)
    {
        strcat(command_line, " -U ");
        strcat(command_line, username);
    }

    /* Add database name, default is "postgres" */
    if (database)
    {
        strcat(command_line, " -d ");
        strcat(command_line, database);
    }
    else
        strcat(command_line, " -d postgres ");

    if (host)
    {
        strcat(command_line, " -h ");
        strcat(command_line, host);
    }

    if (port)
    {
        strcat(command_line, " -p ");
        strcat(command_line, port);
    }

    strcat(command_line, out);

    /* Launch the command and output result if necessary */
    rc = system(command_line);
    exitStatus = WEXITSTATUS(rc);
    if (verbose)
    {
        if (exitStatus == 0)
            printf("Running\n");
        else
            printf("Not running\n");
    }

    return exitStatus;
}

/*
 * Show help information
 */
static void
usage(void)
{
    printf("pgxc_monitor -Z nodetype -p port -h host\n\n");
    printf("Options are:\n");
    printf("    -Z nodetype        What node type to monitor, GTM, GTM-Proxy,\n");
    printf("                    Coordinator, or Datanode.\n");
    printf("                    Use \"gtm\" for GTM and GTM-proxy, \"node\" for Coordinator and Datanode.\n");
    printf("    -h host         Host name or IP address of the monitored node.\n");
    printf("                    Mandatory for -Z gtm\n");
    printf("    -n nodename     Nodename of this pgxc_monitor.\n");
    printf("                    Only for -Z gtm. Default is pgxc_monitor\n");
    printf("                    This identifies what is the name of component connecting to GTM.\n");
    printf("    -p port         Port number of the monitored node. Mandatory for -Z gtm\n");
    printf("    -d database     Database name to connect to. Default is \"postgres\".  \n");
    printf("    -U username     Connect as specified database user. \n");
    printf("    -q              Quiet mode.\n");
    printf("    -v              Verbose mode.\n");
    printf("    --help          Prints the help message and exits with 0.\n");
}
