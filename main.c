#include "vmf.h"

#include <stdio.h>
#include <stdlib.h>

int main(void){
	FILE *vmffile = fopen("gm_windowtest.vmf", "rb");
	fseek(vmffile, 0, SEEK_END);
	unsigned int filesize = ftell(vmffile);
	fseek(vmffile, 0, SEEK_SET);

	char *contents = malloc((filesize + 1) * sizeof(char));
	fread(contents, 1, filesize, vmffile);
	contents[filesize] = '\0';

	vmfroot vmf = string2vmf(contents);
	//vmf_cleanupnode(&vmf.root);

	vmfrecursive_iterator info = vmf_reciter_init(&vmf.root);

	vmfnode *curnode = vmf_reciter_next(&info);
	while(curnode != VMF_NULL){
		printf("%s\n", curnode->classname);
		curnode = vmf_reciter_next(&info);
	}

	char *output;
	printf("done!");
	unsigned int outlen = vmf2string(&vmf, &output);

	vmf_free(&vmf);

	FILE *dataout2 = fopen("output.vmf", "wb");
	fwrite(output, 1, outlen, dataout2);

	return 0;
}

