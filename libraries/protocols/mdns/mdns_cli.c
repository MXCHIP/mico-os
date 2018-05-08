/**
 ******************************************************************************
 * @file    mdns_cli.c
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   mDNS command called by cli to display mdns states
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mdns.h"
#include "mdns_private.h"

extern mdns_responder_stats mr_stats;
extern int cli_register_commands(const struct cli_command *commands, int num_commands);

static void mdns_stat(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);

struct cli_command  mdns_cli_cmds[] = 
{
    {"mdns-status", "print mdns daemon status", mdns_stat}
};

void mdns_cli_init(void)
{
	/* Register an user command to cli interface */
    cli_register_commands(mdns_cli_cmds, sizeof(mdns_cli_cmds)/sizeof(struct cli_command));
}


static void mdns_stat(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	printf("mDNS Stats\r\n");
	printf("=== Rx Stats\r\n");
	printf("Total \t= %10d\r\n", mr_stats.total_rx);
	printf("Queries = %10d \tAnswers \t= %10d\r\n", \
		mr_stats.rx_queries, mr_stats.rx_answers);
	printf("Errors  = %10d \tKnown Answers\t= %10d\r\n", \
		mr_stats.rx_errors, mr_stats.rx_known_ans);
	printf("====== Conflicts \r\nHost \t= %10u "
		"\tService \t= %10u\r\n", \
		mr_stats.rx_hn_conflicts,
		mr_stats.rx_sn_conflicts);

	printf("=== Tx Stats\r\n");
	printf("Total \t= %10d\r\n", mr_stats.total_tx);
	printf("Probes \t= %10d \tAnnouncements \t= %10d " \
		"\tClosing probes \t= %10d\r\n", \
		mr_stats.tx_probes, mr_stats.tx_announce, \
		mr_stats.tx_bye);
	printf("Response= %10d \tReannounce \t= %10d\r\n", \
		mr_stats.tx_response, mr_stats.tx_reannounce);
	printf("====== Errors \r\nIPv4 \t= %10d "
		"\tIPv6 \t\t= %10d\r\n", \
		mr_stats.tx_ipv4_err, mr_stats.tx_ipv6_err);
	printf("====== Current stats \r\nProbes \t= %10u "
		"\tAnnouncements \t= %10u\tRx events \t= %10u\r\n", \
		mr_stats.tx_probes_curr, mr_stats.tx_announce_curr, \
		mr_stats.probe_rx_events_curr);
	printf("====== Interface stats\r\n");
	printf("UP \t= %10u \tDOWN \t= %10u \tREANNOUNCE \t= %10u\r\n", \
			mr_stats.iface_up, mr_stats.iface_down,
			mr_stats.iface_reannounce);
}

