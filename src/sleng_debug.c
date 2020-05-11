#include <stdio.h>

#include "sleng_debug.h"



/**********************************************************************
 * function:print info in format like Ultra Edit
 * input:	buf to print,
 * 			length to print,
 * 			prestr before info,
 * 			endstr after info
 * output:	void
 **********************************************************************/
void print_in_hex(void *buf, int len, char *pre, char *end) {
	int i, j, k, row=(len>>4);
	if (buf == NULL) {
		printf("params invalid, buf=%p", buf);
		return;
	}
	if (pre) printf("%s:\n", pre);
	for (i=0, k=0; k<row; ++k) {
		printf("\t[0%02d0] ", k);
		for (j=0; j<8; ++j, ++i) printf("%02hhx ", *((unsigned char *)buf+i));
		printf("  ");
		for (j=8; j<16; ++j, ++i) printf("%02hhx ", *((unsigned char *)buf+i));
		printf("\n");
	}
	if (len&0xf) {
		printf("\t[0%02d0] ", k);
		for (k=0; k<(len&0xf); ++k, ++i) {
			if (k==8) printf("  ");
			printf("%02hhx ", *((unsigned char *)buf+i));
		}
		printf("\n");
	}
	if (end) printf("%s", end);
	printf("\n");
}
