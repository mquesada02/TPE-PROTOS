#include "../include/fileManager.h"
#include <stdio.h>
#include <sys/sendfile.h>
#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../include/fileMap.h"
#define REPO_PATH "../repository"
#define PATH_SIZE 1024
int seederAmount=2;

fileMap map;


int copyFromFile(char* buffer,char* md5,int offset,int bytes){
    FILE* file=lookup(map,md5);
    if(file==NULL)
        return -1;
    fseek(file,offset,SEEK_SET);
    fgets(buffer,bytes,file);
    return 0;
}

int removeFile(char* md5){
    return removeEntry(map,md5);
}


int initializeFileManager(){
    map=createMap();
    if(map==NULL)
        return -1;
    struct dirent* dirnt;
    DIR* dir;
    char pathname[PATH_SIZE];
    if((dir=opendir(REPO_PATH))==NULL){
        freeMap(map);
        return -2;
    }
    while((dirnt=readdir(dir))){
        sprintf(pathname,"%s/%s",REPO_PATH,dirnt->d_name);
        //FILE* file=fopen(pathname,"rb");
        
    }
    return 0;
}
