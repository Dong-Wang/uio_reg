/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Wang Dong (dong.wang.pro@hotmail.com).
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Wang Dong nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h> /*perror*/
#include <stdlib.h>
#include <fcntl.h> /*open*/
#include <sys/mman.h> /*mmap*/
#include <unistd.h> /*close*/
#include <getopt.h> /*getopt_long*/
#include <stdint.h> /*uint32_t*/

#define UIO_NAME "/dev/uio0"
#define BAR0_SIZE 0x20000

static char *cmd_name = NULL; /* store argv[0] */

enum op_type {
	OP_READ=0,
	OP_WRITE
};

/* paramter from command line */
struct input_param {
	enum op_type operate; /* read or write? */
	uint32_t offset; /* register's offset in BAR */
	union {
		uint64_t size; /* for read operation */
		uint64_t value; /* for write operation */
	};
} input_param = {
	.operate = OP_READ,
	.offset = 0x0,
	.size = 1,
};


/* dump the --help */
void dump_help_info() {
	printf("Usage: %s [OPTION]\n", cmd_name);
	printf("A command tool for access UIO device's register.\n");
	printf("OPTION:\n");
	printf("\t-r/--read OFFSET [SIZE]: Read a serial of registers begin at OFFSET. The register count is SIZE.\n");
        printf("\t                         The default value of SIZE is 1.\n");
	printf("\t-w/--write OFFSET VALUE: Write VALUE to register which the offset is OFFSET.\n");
	printf("\t-h/--help              : Dump this help information.\n");
	printf("Author: dong.wang.pro@hotmail.com. Plase send email to me for any suggestions.\n");
}

/* parse and check the parameter */
/* return -1 for stop running (error or help) otherwise return 0 */
int parse_opt(int argc, char *argv[])
{
	#define OPT_STRING "r:w:h"
	struct option long_options[] = {
		{"read", required_argument, NULL, 'r'},
		{"write", required_argument, NULL, 'w'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};

	int ret = 0;
	int option_index = 0;
	int cur_optind = 0;

	int operation_sem = 0;
	char *endptr = NULL;

	while (1) {
		option_index = 0;
		cur_optind = optind ? optind : 1;

		ret = getopt_long(argc, argv, OPT_STRING, long_options, &option_index);
		if (ret == -1) {
			/* all command-line option has parsed */
			if (optind < argc) {
				printf("[ERROR] invalid option of %s.\n", argv[optind]);
				return -1;
			}
			else
				return 0;
		}

		switch (ret) {
		case 'r':
			if (operation_sem == 1) {
				printf("[ERROR] Can't read (-r) and write (-w) a register at same time!\n");
				return -1;
			}

			input_param.operate = OP_READ;
			input_param.offset = strtol(optarg, &endptr, 16);

			/* -r maybe have two arguments, ADDR and SIZE. So I check if the next element start with '-' */
			if (optind != argc && argv[optind][0] != '-') {
				input_param.size = strtoll(argv[optind], &endptr, 0);
				optind ++;
			}
			else
				input_param.size = 1;

			if (input_param.size > 0x100000) {
				/* I don't think it's necessary to read more than 1M registers, agree with me? */
				printf("[ERROR] uio_reg think it is not necessary to read more than 1M registers.Please make the SIZE smaller than 1M.\n");
				return -1;
			}

			operation_sem = 1;
			break;
		case 'w':
			if (operation_sem == 1) {
				printf("[ERROR] Can't read (-r) and write (-w) a register at same time!\n");
				return -1;
			}

			input_param.operate = OP_WRITE;
			input_param.offset = strtol(optarg, &endptr, 16);

			/* -w must have two arguments, optind is point to next element when element have only one argument. */
			if (optind != argc && argv[optind][0] == '-') {
				printf("[ERROR] Write register need two arguments, OFFSET and VALUE.\n");
				return -1;
			}

			input_param.value = strtoll(argv[optind], &endptr, 16);

			operation_sem = 1;
			break;
		case 'h':
			/* just dump the --help */
			dump_help_info();
			return -1;
			break;
		case '?':
			/* getopt_long will print log by itself */
			return -1;
			break;
		default:
			printf("[ERROR] Didn't support this command-line options now: %s\n", argv[cur_optind]);
			return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int uio_fd;
	unsigned int reg_offset = 0;
	unsigned char *pci_bar0 = NULL;
	char *endptr = NULL;

	cmd_name = argv[0];

	if (parse_opt(argc, argv) != 0)
		/* already print error log in subroutine. */
		return -1;

	if (input_param.operate == OP_READ)
		printf("read, offset 0x%08X, length %lu.\n", input_param.offset, input_param.size);
	else
		printf("write, offset 0x%08X, value 0x%016lX.\n", input_param.offset, input_param.value);

	return 0; /* for test */

	reg_offset = strtol(argv[1], &endptr,16);
	reg_offset &= ~0x03;
	/* test */
	printf("reg_offset is 0x%08X\n", reg_offset);

	/* open the uio device which is input from commandline */
	uio_fd = open(UIO_NAME, O_RDWR);
	if ( uio_fd < 0 ) {
		perror("open uio file error:");
		return -1;
	}

	/* map the PCIe BAR0 */
	pci_bar0 = (unsigned char*)mmap(NULL, BAR0_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, uio_fd, 0); /* if want access BAR1, set offset to 4096 */
	if (pci_bar0 == MAP_FAILED) {
		perror("mmap failed:");
		close(uio_fd);
		return -1;
	}

	/* now access register */
	printf("reg[0x%08X]: 0x%08X\n", reg_offset, *(unsigned int*)&pci_bar0[reg_offset]);

	munmap(pci_bar0, BAR0_SIZE);
	close(uio_fd);

	return 0;
}

