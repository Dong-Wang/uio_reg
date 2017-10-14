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
#include <sys/types.h> /*opendir*/
#include <dirent.h> /*opendir*/
#include <unistd.h> /*close*/
#include <getopt.h> /*getopt_long*/
#include <stdint.h> /*uint32_t*/
#include <string.h> /*strlen*/

#define UIO_NAME "/dev/uio0"
#define BAR0_SIZE 0x20000

#define MAX_PATH_STR_WIDTH 256
#define MAX_UIO_NAME_STR_WIDTH 8
#define MAX_BAR_NAME_STR_WIDTH 8
#define MAX_BAR_SIZE_STR_WIDTH 32

#define BAR_OFFSET_SHIFT 12	// BAR0 offset is 0; BAR1 offset is 4096; BAR2 offset is 8192; etc.... Equal to page size.
#define MAX_BAR_NUM 6		// maximum BAR number is 6

#define perrorf(...) \
{ \
	char tmp_string[256] = {0}; \
	sprintf(tmp_string, __VA_ARGS__); \
	perror(tmp_string); \
}

static char *cmd_name = NULL; /* store argv[0] */

enum op_type {
	OP_NONE = 0,
	OP_READ,
	OP_WRITE
};

#define UIO_NAME_WIDTH	8
#define BDF_WIDTH	16
/* paramter from command line, no need to align to cache line */
struct input_param {
	enum op_type operate; /* read or write */
	char bdf[BDF_WIDTH]; /* SSSS:BB:DD.F totally 13 byte -- include '\0', align to 16 byte */
	uint32_t bar_num; /* which bar this register belong to, bar0/bar1/bar2... */
	uint32_t offset; /* register's offset in BAR */
	union {
		uint32_t count; /* how many registers to be read */
		uint32_t value; /* for write operation */
	};
} input_param = {
	.operate = OP_NONE,
	.bdf = {0},
	.bar_num = 0,
	.offset = 0x0,
	.count = 0,
};


/* dump the --help */
void dump_help_info() {
	printf("Usage: %s [OPTION]\n", cmd_name);
	printf("A command tool for access UIO device's register.\n");
	printf("NOTE: Only support 32bit register now. That means 64bit register will be displayed as two 32bit register.\n");
	printf("OPTION:\n");
	printf("\t-s/--bdf STRING        : The BDF of PCIe device which you want to access.\n");
	printf("\t                         e.g. \"0000:01:00.0\" or \"01:00.0\"\n");
	printf("\t-b/--bar INDEX         : The index of Base Address Register.\n");
	printf("\t-r/--read OFFSET [COUNT]: Read COUNT registers begin at OFFSET.\n");
        printf("\t                         The default value of SIZE is 1.\n");
	printf("\t-w/--write OFFSET VALUE: Write VALUE to register which the offset is OFFSET.\n");
	printf("\t-h/--help              : Dump this help information.\n");
	printf("Author: dong.wang.pro@hotmail.com. Plase send email to me for any suggestions.\n");
}

/* parse and check the parameter */
/* return -1 for stop running (error or help) otherwise return 0 */
int parse_opt(int argc, char *argv[])
{
	#define OPT_STRING "r:w:b:n:s:h"
	struct option long_options[] = {
		{"read", required_argument, NULL, 'r'},
		{"write", required_argument, NULL, 'w'},
		{"bar", required_argument, NULL, 'b'},
		{"bdf", required_argument, NULL, 's'},
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

			if (argc == 1) {
				/* must have options */
				dump_help_info();
				return -1;
			}
			else {
				/* final check */
				if (/*input_param.name[0] == '\0' && */input_param.bdf[0] == '\0') {
					printf("[ERROR] Should input BDF number by -s.\n");
					return -1;
				}

				if (input_param.operate == OP_NONE) {
					printf("[ERROR] Didn't specify read or write register.\n");
					return -1;
				}
				return 0;
			}
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
				input_param.count = strtol(argv[optind], &endptr, 0);
				optind++;
			}
			else
				input_param.count = 1;

			if (input_param.count > 0x100000) {
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
			if (optind == argc || argv[optind][0] == '-') {
				printf("[ERROR] Write register need two arguments, OFFSET and VALUE.\n");
				return -1;
			}

			input_param.value = strtol(argv[optind], &endptr, 16);
			optind++;

			operation_sem = 1;
			break;
		case 's':
			if (optarg == NULL || optarg[0] == '-') {
				printf("[ERROR] Need argument with %s.\n", argv[cur_optind]);
				return -1;
			}

			if (strlen(optarg) >= BDF_WIDTH) {
				printf("[ERROR] BDF is too large, make sure you put the correct BDF: %s\n", optarg);
				return -1;
			}

			strcpy(input_param.bdf, optarg);
			break;
		case 'b':
			/* bar number */
			if (optarg == NULL || optarg[0] == '-') {
				printf("[ERROR] Need argument with %s.\n", argv[cur_optind]);
				return -1;
			}

			input_param.bar_num = strtol(optarg, &endptr, 0);

			if (input_param.bar_num >= 64) {
				printf("[ERROR] BAR number is too large, make it less than 64. If you really have a BAR number large than 63, please modify uio_reg source code.\n");
				return -1;
			}
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

#define COLUME_GAP	"        " /* 8 */
#define COLUME_OFFSET	"    OFFSET" /* 10 */
#define COLUME_VALUE	"             VALUE" /* 18 */
/* print 32bit registers */
void read_reg_32(void *pci_bar, uint32_t offset, uint32_t count)
{
	uint32_t i;
	uint32_t *reg_addr = (uint32_t*)((char*)pci_bar+offset); /* the address of frist register */

	printf( COLUME_OFFSET COLUME_GAP COLUME_VALUE "\n" );

	for (i=0; i<count; i++) {
		printf("0x%08X"COLUME_GAP"        0x%08X\n", offset+i*4, reg_addr[i]);
	}
}

/* print 64bit registers */
void read_reg_64(void *pci_bar, uint32_t offset, uint32_t count)
{
	uint32_t i;
	uint64_t *reg_addr = (uint64_t*)((char*)pci_bar+offset);

	printf( COLUME_OFFSET COLUME_GAP COLUME_VALUE "\n" );

	for (i=0; i<count; i++) {
		printf("0x%08X"COLUME_GAP"0x%016lX\n", offset+i*8, reg_addr[i]);
	}
}


/* wirte value to 32bit register */
void write_reg_32(void *pci_bar, uint32_t offset, uint32_t value)
{
	uint32_t *reg_addr = (uint32_t*)((char*)pci_bar+offset);

	*reg_addr = value;
}


/*
 * Get the full path of /sys/bus/pci/device/xxxxxx
 * IN param bdf is the BDF name like "0000:01:00.0"
 * OUT param sys_bdf_dir should be larger than 256 bytes.
 */
#define SYS_DEVICE_DIR "/sys/bus/pci/devices"
#define DEFAULT_PCIE_SLOT "0000:"
int get_sys_bdf_dir(char *bdf, char *sys_bdf_dir)
{
	char *tmp_bdf = bdf;
	int colon_num = 0;
	int dot_num = 0;

	if (bdf == NULL && sys_bdf_dir == NULL) {
		printf("[ERROR] Parameter for %s can't be NULL.\n", __FUNCTION__); // only be used for debug
		return -1;
	}

	/* check if bdf is valid, should have 2x':' and 1x'.' */
	while ((tmp_bdf = strchr(tmp_bdf, ':')) != NULL) {
		tmp_bdf++;
		colon_num++;
	}

	tmp_bdf = bdf;
	while ((tmp_bdf = strchr(tmp_bdf, '.')) != NULL) {
		tmp_bdf++;
		dot_num++;
	}

	if (dot_num != 1) {
		printf("[ERROR] Invalid BDF: %s\n", bdf);
		return -1;
	}

	switch (colon_num) {
	case 2:
		sprintf(sys_bdf_dir, SYS_DEVICE_DIR"/%s", bdf);
		break;
	case 1:
		sprintf(sys_bdf_dir, SYS_DEVICE_DIR"/"DEFAULT_PCIE_SLOT"%s", bdf);
		break;
	default:
		printf("[ERROR] Invalid BDF: %s\n", bdf);
		return -1;
	}

	return 0;
}

/*
 * Get the uio device name which will be used in /dev/ .
 * OUT param uio_name is the name like uioX.
 * return value: 0 for success, -1 for error.
 */
int get_uio_name(char *sys_bdf_dir, char *uio_name)
{
	char sys_uio_dir[MAX_PATH_STR_WIDTH] = {0};
	DIR *DIR_sys_uio = NULL;
	struct dirent *entry_sys_uio = NULL;
	int find_uio = 0;

	if (uio_name == NULL) {
		printf("[ERROR] OUT param uio_name in %s can't be NULL.\n", __FUNCTION__); // only be used for debug
		return -1;
	}

	sprintf(sys_uio_dir, "%s/uio", sys_bdf_dir);
	DIR_sys_uio  = opendir(sys_uio_dir);
	if (DIR_sys_uio  == NULL) {
		perrorf("[ERROR] Open %s:", sys_uio_dir);
		return -1;
	}

	/* Not sure if there are more uioX in uio direcotry. Suppose there is one. */
	while ((entry_sys_uio = readdir(DIR_sys_uio)) != NULL) {
		if (strncmp(entry_sys_uio->d_name, "uio", 3) == 0) {
			strcpy(uio_name, entry_sys_uio->d_name);
			find_uio = 1;
			break;
		}

	}
	closedir(DIR_sys_uio);

	if (find_uio == 0) {
		printf("[ERROR] Can't find \"uioX\" in %s.", sys_uio_dir);
		return -1;
	}

	return 0;
}


/*
 * Compare "name" to bar_num in maps direcotry of uio/uioX/maps/mapY, then read "size" when "name" equal to bar_num.
 */
int get_bar_size(char *sys_bdf_dir, char *uio_name, uint32_t bar_num, uint32_t *bar_size)
{
	int i = 0;
	char sys_mapY_name_file[MAX_PATH_STR_WIDTH] = {0};
	char sys_mapY_size_file[MAX_PATH_STR_WIDTH] = {0};
	int fd_name = -1;
	int fd_size = -1;
	char target_bar_name[MAX_BAR_NAME_STR_WIDTH] = {0};
	char str_bar_name[MAX_BAR_NAME_STR_WIDTH] = {0};
	char str_bar_size[MAX_BAR_SIZE_STR_WIDTH] = {0};
	char *endptr = NULL;

	if (sys_bdf_dir == NULL && uio_name == NULL && bar_size == NULL) {
		printf("[ERROR] Invalid parameter for %s.\n", __FUNCTION__); // only be used for debug
		return -1;
	}

	sprintf(target_bar_name, "BAR%d", bar_num);

	/* check all "mapY" directory, max BAR number is 6 */
	for (i=0; i<MAX_BAR_NUM; i++) {
		sprintf(sys_mapY_name_file, "%s/uio/%s/maps/map%d/name", sys_bdf_dir, uio_name, i);
		sprintf(sys_mapY_size_file, "%s/uio/%s/maps/map%d/size", sys_bdf_dir, uio_name, i);

		fd_name = open(sys_mapY_name_file, O_RDONLY);
		if (fd_name == -1)
			break;
		if (read(fd_name, str_bar_name, MAX_BAR_NAME_STR_WIDTH) == -1) {
			perrorf("[ERROR] Read %s:", sys_mapY_name_file);
			close(fd_name);
			return -1;
		}
		close(fd_name);

		if (strncmp(str_bar_name, target_bar_name, strlen(target_bar_name)) != 0) {
			continue;
		}

		fd_size = open(sys_mapY_size_file, O_RDONLY);
		if (fd_size == -1) {
			perrorf("[ERROR] Open %s:", sys_mapY_size_file);
			return -1;
		}
		if (read(fd_size, str_bar_size, MAX_BAR_SIZE_STR_WIDTH) == -1) {
			perrorf("[ERROR] Read %s:", sys_mapY_size_file);
			close(fd_size);
			return -1;
		}
		close(fd_size);

		*bar_size = strtol(str_bar_size, &endptr, 16);
		return 0;
	}

	printf("Didn't find BAR%d in %s/uio/%s/maps/mapY.\n", bar_num, sys_bdf_dir, uio_name);
	return -1;
}

int main(int argc, char *argv[])
{
	int uio_fd;
	unsigned char *pci_bar = NULL;
	char sys_bdf_dir[MAX_PATH_STR_WIDTH] = {0};
	char uio_name[MAX_UIO_NAME_STR_WIDTH] = {0};
	char dev_uio_file[64]= {0};

	off_t bar_offset = 0; // be used for mmap of BAR
	uint32_t bar_size = 0; // read from /sys/bus/pci/devices/0000:00:19.0/uio/uio0/maps/map0/size, mmap() will use this data

	cmd_name = argv[0];

	if (parse_opt(argc, argv) != 0) {
		/* already print error log in subroutine. */
		return -1;
	}

	if (input_param.operate == OP_READ)
		printf("read, offset 0x%08X, length %u.\n", input_param.offset, input_param.count);
	else
		printf("write, offset 0x%08X, value 0x%08X.\n", input_param.offset, input_param.value);

	printf("bar number: %d, bdf: %s\n", input_param.bar_num, input_param.bdf);

	/* priority: bdf > name > default */
	if (input_param.bdf[0] != '\0') {
		if (get_sys_bdf_dir(input_param.bdf, sys_bdf_dir) != 0)
			return -1; // error log has been printed
		if (get_uio_name(sys_bdf_dir, uio_name) != 0)
			return -1; // error log has been printed
	}
	else {
		/* default value */
		strcpy(uio_name, UIO_NAME);
	}
	sprintf(dev_uio_file, "/dev/%s", uio_name);

	if (get_bar_size(sys_bdf_dir, uio_name, input_param.bar_num, &bar_size) != 0) {
		return -1;
	}

	/* open the uio device which is input from commandline */
	uio_fd = open(dev_uio_file, O_RDWR);
	if ( uio_fd < 0 ) {
		perrorf("open uio file error:");
		return -1;
	}

	/* map the PCIe BAR0, if you want access BAR1, set offset to 4096 */
	bar_offset = input_param.bar_num << BAR_OFFSET_SHIFT;
	pci_bar = (unsigned char*)mmap(NULL, bar_size, PROT_READ | PROT_WRITE, MAP_SHARED, uio_fd, bar_offset);
	if (pci_bar == MAP_FAILED) {
		perrorf("mmap failed:");
		close(uio_fd);
		return -1;
	}

	/* now access register */
	if (input_param.operate == OP_READ) {
		read_reg_32(pci_bar, input_param.offset, input_param.count);
	} else if (input_param.operate == OP_WRITE) {
		write_reg_32(pci_bar, input_param.offset, input_param.value);
	} else {
		printf("Unkown operate type, must be -r for read or -w for write.\n");
	}

	munmap(pci_bar, bar_size);
	close(uio_fd);

	return 0;
}

