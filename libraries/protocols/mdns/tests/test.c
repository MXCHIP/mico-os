/*
 * Test Program for mdns
 * compile with,
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define debug(...)

#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <mdns.h>

#include "mdns_private.h"

void mdns_dname_size_tests(int argc, char **argv)
{
	dname_size_tests();
}

void mdns_dname_cmp_tests(int argc, char **argv)
{
	dname_cmp_tests();
}

void mdns_increment_name_tests(int argc, char **argv)
{
	increment_name_tests();
}

void mdns_txt_to_c_ncpy_tests(int argc, char **argv)
{
	txt_to_c_ncpy_tests();
}

struct cli_command mdns_commands[] = {
	{"mdns-dname-size-tests", "Runs mDNS domain name size tests",
	 mdns_dname_size_tests},
	{"mdns-dname-cmp-tests", "Runs mDNS domain name comparison tests",
	 mdns_dname_cmp_tests},
	{"mdns-increment-name-tests", "Runs mDNS domain name increment tests",
	 mdns_increment_name_tests},
	{"mdns-txt-to-c-ncpy-tests", "Runs mDNS domain name comparison tests",
	 mdns_txt_to_c_ncpy_tests},
};

int mdns_cli_init(void)
{
	if (cli_register_commands
	    (&mdns_commands[0],
	     sizeof(mdns_commands) / sizeof(struct cli_command)))
		return 1;
	return 0;
}
