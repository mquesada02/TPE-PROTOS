#ifndef __FILELIST_H_
#define __FILELIST_H_
#include <stdio.h>



typedef struct Entry* fileList;


void initializeList();
int insertFile (char *key, FILE *value) ;
FILE *findFile (char *key);
void freeList();
int deleteFile(char *key) ;

#endif
