#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "utils.h"

void err(const char *fmt,...){
	fprintf(stderr,"error: ");
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
	}

void *safeAlloc(size_t nBytes){
	void *p=malloc(nBytes);
	if(!p){
		err("not enough memory");
	}
	return p;
	}

char *loadFile(const char *fileName){
	FILE *fis=fopen(fileName,"rb");
	if(!fis){
		err("unable to open %s",fileName);
	}
	fseek(fis,0,SEEK_END);
	size_t n=(size_t)ftell(fis);
	fseek(fis,0,SEEK_SET);
	char *buf=(char*)safeAlloc((size_t)n+1);
	size_t nRead=fread(buf,sizeof(char),(size_t)n,fis);
	fclose(fis);
	if(n!=nRead){
	err("cannot read all the content of %s",fileName);
	}
	buf[n]='\0';
	return buf;
}

void compareFiles(const char *file1, const char *file2) {
    FILE *f1 = fopen(file1, "r");
    FILE *f2 = fopen(file2, "r");

    if(!f1){
		fclose(f1);
		err("unable to open %s",f1);
	}

	if(!f2){
		fclose(f2);
		err("unable to open %s",f2);
	}

    char line1[1024], line2[1024];
    int lineNum = 1;
    int differences = 0;

    while (fgets(line1, sizeof(line1), f1) && fgets(line2, sizeof(line2), f2)) {
        if (strcmp(line1, line2) != 0) {
            printf("Difference at line %d:\nFile1: %sFile2: %s\n", lineNum, line1, line2);
            differences++;
        }
        lineNum++;
    }

	while (fgets(line1, sizeof(line1), f1)) {
        printf("Extra line in %s at line %d: %s", file1, lineNum, line1);
        differences++;
        lineNum++;
    }
    while (fgets(line2, sizeof(line2), f2)) {
        printf("Extra line in %s at line %d: %s", file2, lineNum, line2);
        differences++;
        lineNum++;
    }

    if (differences == 0) {
        printf("Files are identical!\n");
    } else {
        printf("Total differences: %d\n", differences);
    }

    fclose(f1);
    fclose(f2);
}
