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
#include <sys/types.h>
#include <dirent.h>
#include "../include/utils.h"
#include "../include/fileMap.h"
#define REPO_PATH "../repository"
#define PATH_SIZE 1024
#define DT_REG 8 //redefinido pues no es reconocido por alguna razon


fileMap map;


long copyFromFile(char* buffer,char* md5,long offset,long bytes){
    FILE* file=lookup(map,md5);
    if(file==NULL)
        return -1;
    fseek(file,offset,SEEK_SET);
    fgets(buffer,bytes,file);
    return 0;
}

long addFile(char* md5){
    return 0;
}

long getFileSize(char* md5){
    FILE* file=lookup(map,md5);
    if(file==NULL)
        return -1;
    long current=ftell(file);
    fseek(file,0,SEEK_END);
    long size=ftell(file);
    fseek(file,current,SEEK_SET);
    return size;
}

long removeFile(char* md5){
    return removeEntry(map,md5);
}


long initializeFileManager(){
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
        if(dirnt->d_type == DT_REG ){
            char md5Buffer[MD5_SIZE+1];
            sprintf(pathname,"%s/%s",REPO_PATH,dirnt->d_name);
            FILE* file=fopen(pathname,"rb");
            calculateMD5(pathname,md5Buffer);
            if(file!=NULL) {
                insert(map, md5Buffer, file);
                printf("%s\n", md5Buffer);
            }
        }
    }
    return 0;
}
