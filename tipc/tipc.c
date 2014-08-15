/*
 * Copyright (c) 2014, Ericsson AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <getopt.h>
#include <sys/queue.h>
#include <unistd.h>
#include <libgen.h>

#include "tipc.h"
#include "netlink.h"
#include "cmdl.h"

int help_flag;
char *tool_version = "1.0";
struct cmd_head cmd_list = TAILQ_HEAD_INITIALIZER(cmd_list);

static void about(char *cmd)
{
	fprintf(stderr,
		"Transparent Inter-Process Communication Protocol\n"
		"Usage: %s [OPTIONS] COMMAND [ARGS]\n"
		"\n"
		"Options:\n"
		" -h, --help \t\tPrint help for command\n"
		"\n"
		"Commands:\n", cmd
	);
	usage_list(&cmd_list);
}


int main(int argc, char *argv[])
{
	int c;
	int res;
	struct arg_struct arg;

	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	if (strcmp(basename(argv[0]), "tipc-config") == 0) {
		printf("tipc-config is deprecated, use \"tipc\" instead\n");
		return EXIT_FAILURE;
	}

	/*
	 * We have two places where we handle options, here in
	 * main and in leafs (commands).
	 */
	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "vh",
				long_options, &option_index);

		/* End of options */
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			/*
			 * We want the help for the last command, so we flag
			 * here in order to print later.
			 */
			help_flag = 1;
			break;
		default:
			/* Invalid option, error msg is printed by getopts */
			exit(EXIT_FAILURE);
		}
	}

	/* Main help is threated specially */
	if ((argc - optind == 0)) {
		about(argv[0]);
		return EXIT_FAILURE;
	}

	/*
	 * We save pointers to the command line arguments in the cmd struct
	 * so that we don't have to pass them manually. It also becomes easier
	 * for leafs to pass arguments down to callbacks. This however means we
	 * assume that argv and argc never changes after calling run_cmdline.
	 */
	arg.argc = argc;
	arg.argv = argv;
	arg.loc = optind;

	res = run_cmdline(&cmd_list, &arg);

	if (res != 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
