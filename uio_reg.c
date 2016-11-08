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

