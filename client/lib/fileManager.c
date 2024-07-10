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
int bufferSize = 0;
char* buffer = NULL;
int stateMapSize = 0;
StateValue* stateMap = NULL;
int chunksRetrieved;
char* filename = NULL;
size_t fileSize = 0;
FILE* newFile;
bool completed;
int sections = 0; //tamaño del archivo dividido 5MB
int currentSection = 0;

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
    closedir(dir);
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

    fileSize = size;

    if(size<=SECTIONSIZE){
        stateMapSize = size % CHUNKSIZE==0 ? size/CHUNKSIZE : (size/CHUNKSIZE)+1;
        bufferSize = stateMapSize*CHUNKSIZE+1;
        buffer = malloc(bufferSize);
    } else{
        buffer = malloc(SECTIONSIZE);
        bufferSize = SECTIONSIZE;
        stateMapSize = SECTIONSIZE/CHUNKSIZE;
        sections = size % SECTIONSIZE==0? size/SECTIONSIZE : (size/SECTIONSIZE)+1;
        currentSection = 0;
    }
    if (buffer == NULL){
        perror("Unable to allocate memory for downloading file");
        return;
    }
    stateMap = malloc(stateMapSize*sizeof(StateValue));
    for(int i=0; i<stateMapSize; i++) {
        stateMap[i].state = MISSING;
        stateMap[i].timesAttempted = 0;
    }
    memset(buffer, '\0', bufferSize);
    filename = malloc(strlen(newFilename)+1);
    strcpy(filename, newFilename);
    chunksRetrieved = 0;
    completed = false;
}

void initForNewSection() {

    if(currentSection == sections-1){
        int lastSection = fileSize%SECTIONSIZE;
        stateMapSize = lastSection % CHUNKSIZE==0 ? lastSection/CHUNKSIZE : (lastSection/CHUNKSIZE)+1;
        bufferSize = stateMapSize*CHUNKSIZE+1;
        free(stateMap);
        stateMap = malloc(stateMapSize*sizeof(StateValue));
        free(buffer);
        buffer =  malloc(bufferSize);
    }

    for(int i=0; i<stateMapSize; i++) {
        stateMap[i].state = MISSING;
        stateMap[i].timesAttempted = 0;
    }
    memset(buffer, '\0', bufferSize);
    chunksRetrieved = 0;

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
    if(newFile && filename){
        int len = strlen("../repository/") + strlen(filename);
        char* aux = malloc(len+1); // Allocate memory for aux
        if (aux == NULL) {
            perror("Unable to allocate memory for aux");
            return;
        }
        strcpy(aux, "../repository/");
        strcat(aux, filename);
        aux[len] = '\0';
        remove(aux);
        newFile = NULL;
        free(aux);
    }
    if(filename){
        free(filename);
        filename = NULL;
    }
    sections = 0;
    currentSection = 0;
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
    return i*CHUNKSIZE+currentSection*SECTIONSIZE;
}

//funcion para que el cliente le pase al file manager el contenido del byte que consiguio
int retrievedChunk(unsigned long int chunkNum, char* chunk) {
    int stateMapIndex = ((chunkNum) % SECTIONSIZE)/CHUNKSIZE;
    //no se encontro
    if(chunk == NULL) {
        stateMap[stateMapIndex].timesAttempted++;
        if(stateMap[stateMapIndex].timesAttempted == MAX_ATTEMPTS) {
            cancelDownload();
            perror("could not download file.");
            return -1;
        }
        stateMap[stateMapIndex].state = MISSING;
    }

    if(chunkNum+CHUNKSIZE > fileSize){
        memcpy(&buffer[chunkNum % SECTIONSIZE], chunk, fileSize-chunkNum);
    } else{
        memcpy(&buffer[chunkNum % SECTIONSIZE], chunk, CHUNKSIZE);
    }
    printf("Retrieved chunk %d\n", stateMapIndex);
    stateMap[stateMapIndex].state = RETRIEVED;
    chunksRetrieved++;

    if(chunksRetrieved == stateMapSize) {

        int len = strlen("../repository/") + strlen(filename);
        char *aux = malloc(len + 1); // Allocate memory for aux
        if (aux == NULL) {
            perror("Unable to allocate memory for aux");
            return 1;
        }

        strcpy(aux, "../repository/");
        strcat(aux, filename);

        newFile = fopen(aux, "a");
        if (newFile == NULL) {
            perror("Unable to open file");
            free(aux);
            return 1;
        }

        size_t auxSize = bufferSize;

        if(sections == currentSection + 1) {
            auxSize = fileSize % SECTIONSIZE;
        }
        fwrite(buffer, 1, auxSize, newFile);
        fclose(newFile);
        free(aux);

        if (sections == 0 || ++currentSection == sections) {
            free((void *) buffer);
            buffer = NULL;
            free(stateMap);
            stateMap = NULL;
            free((void *) filename);
            filename = NULL;
            newFile = NULL;
            sections = 0;
            currentSection = 0;

            completed = true;
        } else {
            initForNewSection();
        }

    }

    return 0;
}

