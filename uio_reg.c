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

#define UIO_NAME "/dev/uio0"
#define BAR0_SIZE 0x20000

int main(int argc, char *argv[])
{
	int uio_fd;
	unsigned int reg_offset = 0;
	unsigned char *pci_bar0 = NULL;
	char *endptr = NULL;

	if (argc < 2) {
		printf("error with input, should be: uio_reg REG_OFFSET, and REG_OFFSET must be hexadecimal\n");
		return -1;
	}

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

