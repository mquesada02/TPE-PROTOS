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
#include <math.h>

#include "../include/utils.h"
#include "../include/fileMap.h"
#define REPO_PATH "../repository"
#define PATH_SIZE 1024
#define DT_REG 8 //redefinido pues no es reconocido por alguna razon
#define MAX_ATTEMPTS 4

typedef enum State {MISSING, OBTAINING, RETRIEVED} State;

typedef struct StateValue {
    State state;
    int timesAttempted;
} StateValue;

fileMap map;
char* buffer = NULL;
int stateMapSize = 0;
StateValue* stateMap = NULL;
int chunksRetrieved;
char* filename = NULL;
size_t fileSize = 0;
bool completed;

long copyFromFile(char* buffer,char* md5,long offset,unsigned long bytes){
    FILE* file=lookup(map,md5);
    if(file==NULL)
        return -1;
    fseek(file,offset,SEEK_SET);
    size_t bytesRead=fread(buffer,1,bytes,file);
    buffer[bytesRead]=0;
    return bytesRead;
}

long addFile(char* md5,char* filename){
    FILE* file=lookup(map,md5);
    if(file!=NULL)
        return 1;
    char pathname[PATH_SIZE];
    sprintf(pathname,"%s/%s",REPO_PATH,filename);

    file=fopen(pathname,"rb");
    if(file==NULL)
        return -1;
    insert(map,md5,file);
    return 0;
}

long unsigned int getFileSize(char* md5){
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

void endFileManager(){
    freeMap(map);
}

//inicializa todas las variables para poder ir juntando los chinks del archivo
void initFileBuffer(char* newFilename, long unsigned int size) {
    if(buffer){
        perror("A file is already being downloaded.");
        return;
    }

    stateMapSize = size % CHUNKSIZE==0 ? size/CHUNKSIZE : (size/CHUNKSIZE)+1;
    long unsigned int bufferSize = stateMapSize*CHUNKSIZE+1;
    fileSize = size;
    buffer = malloc(bufferSize);
    stateMap = malloc(stateMapSize*sizeof(StateValue));
    for(int i=0; i<stateMapSize; i++) {
        stateMap[i].state = MISSING;
        stateMap[i].timesAttempted = 0;
    }
    memset(buffer, '\0', bufferSize);
    filename = malloc(strlen(newFilename));
    strcpy(filename, newFilename);
    chunksRetrieved = 0;
    completed = false;
}

//deberia llamarse con un while nextChunk()!=-2 (o similar)(?
//devuelve el indice del principio del chunk que tiene quue buscar
int nextChunk() {

    if(completed) return -2;

    if(buffer == NULL) {
        perror("Must initialize File Buffer first");
        return -1;
    }

    int i = 0;
    while(i<stateMapSize && stateMap[i].state != MISSING) i++;

    if(i==stateMapSize) return -3;
    stateMap[i].state = OBTAINING;
    return i*CHUNKSIZE;
}

//funcion para que el cliente le pase al file manager el contenido del byte que consiguio
int retrievedChunk(int chunkNum, char* chunk) {
    int stateMapIndex = chunkNum/CHUNKSIZE;
    //no se encontro
    if(chunk == NULL) {
        stateMap[stateMapIndex].timesAttempted++;
        if(stateMap[stateMapIndex].timesAttempted == MAX_ATTEMPTS) {
            perror("could not download file.");
            return -1;
        }
        stateMap[stateMapIndex].state = MISSING;
    }

    memcpy(buffer+chunkNum, chunk, CHUNKSIZE);
    stateMap[stateMapIndex].state = RETRIEVED;
    chunksRetrieved++;

    if(chunksRetrieved == stateMapSize) {

        FILE *newFile;
        int len = strlen("../repository/") + strlen(filename);
        char *aux = (char *)malloc(len); // Allocate memory for aux
        if (aux == NULL) {
            perror("Unable to allocate memory for aux");
            return 1;
        }

        strcpy(aux, "../repository/");
        strcat(aux, filename);

        newFile = fopen(aux, "w+");
        if (newFile == NULL) {
            perror("Unable to open file");
            free(aux);
            return 1;
        }

        fwrite(buffer, 1, fileSize, newFile);
        fclose(newFile);
        free(aux);

        free((void *)buffer);
        buffer = NULL;
        free(stateMap);
        stateMap = NULL;
        free((void *)filename);
        filename = NULL;

        completed = true;

    }
    return 0;
}

void cancelDownload() {
    if(buffer){
        free(buffer);
        buffer = NULL;
    }
    if (stateMap){
        free(stateMap);
        stateMap = NULL;
    }
    if(filename){
        free(filename);
        filename = NULL;
    }
}
