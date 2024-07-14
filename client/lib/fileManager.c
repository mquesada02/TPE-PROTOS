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
#include <pthread.h>

#include "../include/utils.h"
#include "../include/fileList.h"
#define REPO_PATH "../repository"
#define PATH_SIZE 1024
#define DT_REG 8 //redefinido pues no es reconocido por alguna razon
#define MAX_ATTEMPTS 4

typedef enum State {MISSING, OBTAINING, RETRIEVED} State;

typedef struct StateValue {
    State state;
    int timesAttempted;
} StateValue;


size_t bufferSize = 0;
char* buffer = NULL;
size_t stateMapSize = 0;
StateValue* stateMap = NULL;
size_t chunksRetrieved;
char* filename = NULL;
size_t fileSize = 0;
FILE* newFile;
bool completed;
size_t sections = 0; //tamaño del archivo dividido 5MB
size_t currentSection = 0;
size_t bytesReadPerSection;


size_t copyFromFile(char* buffer,char* md5,size_t offset,size_t bytes,int* statusCode){
    FILE* file=findFile(md5);
    if(file==NULL){
        *statusCode=FILE_ERROR_READING;
        return 0;
    }
    fseek(file,offset,SEEK_SET);
    size_t bytesRead = fread(buffer,1,bytes,file);
    buffer[bytesRead] = 0;
    *statusCode=FILE_SUCCESS;
    return bytesRead;
}

int addFile(char* md5,char* filename){
    FILE* file=findFile(md5);
    if(file!=NULL)
        return 1;
    char pathname[PATH_SIZE];
    sprintf(pathname,"%s/%s",REPO_PATH,filename);

    file=fopen(pathname,"rb");
    if(file==NULL)
        return -1;
    insertFile(md5,file);
    return 0;
}
size_t addBytesRead(size_t bytes){
    bytesReadPerSection+=bytes;
    return bytesReadPerSection;
}
size_t getBytesRead(){
    return bytesReadPerSection;
}

size_t getFileSize(char* md5){
    FILE* file=findFile(md5);
    if(file==NULL)
        return -1;
    size_t current=ftell(file);
    fseek(file,0,SEEK_END);
    size_t size=ftell(file);
    fseek(file,current,SEEK_SET);
    return size;
}

int removeFile(char* md5){
    return removeFile(md5);
}


int initializeFileManager(){
    initializeList();
    struct dirent* dirnt;
    DIR* dir;
    char pathname[PATH_SIZE];
    if((dir=opendir(REPO_PATH))==NULL){
        freeList();
        return -2;
    }
    printf("Indexing files\n");
    while((dirnt=readdir(dir))){
        if(dirnt->d_type == DT_REG ){
            char md5Buffer[MD5_SIZE+1];
            sprintf(pathname,"%s/%s",REPO_PATH,dirnt->d_name);
            FILE* file=fopen(pathname,"rb");
            calculateMD5(pathname,md5Buffer);
            if(file!=NULL) {
                insertFile(md5Buffer, file);
            }
        }
    }
    printf("Done\n");
    closedir(dir);
    return 0;
}

void endFileManager(){
    freeList();
}

char* getFilenamePath(){
    int len = strlen("../repository/") + strlen(filename);
    char* aux = malloc(len+1); // Allocate memory for aux
    if (aux == NULL) {
        perror("Unable to allocate memory for aux");
        return NULL;
    }
    strcpy(aux, "../repository/");
    strcat(aux, filename);
    aux[len] = '\0';

    return aux;
}

//inicializa todas las variables para poder ir juntando los chunks del archivo
void initFileBuffer(char* newFilename, size_t size) {
    if(buffer){
        perror("A file is already being downloaded.");
        return;
    }
    filename = malloc(strlen(newFilename)+strlen("(1)")+1);
    strcpy(filename, newFilename);
    char* aux = getFilenamePath();
    if(access(aux, F_OK) == 0) {
        strcat(filename,"(1)");
        if(access(filename,F_OK)==0){
            remove(filename);
        }
    }
    free(aux);

    fileSize = size;
    bytesReadPerSection=0;
    if(size<=SECTIONSIZE){
        stateMapSize = size % CHUNKSIZE==0 ? size/CHUNKSIZE : (size/CHUNKSIZE)+1;
        bufferSize = stateMapSize*CHUNKSIZE+1;
        buffer = malloc(bufferSize);
    } else{
        bufferSize = SECTIONSIZE+1;
        buffer = malloc(bufferSize);
        stateMapSize = SECTIONSIZE/CHUNKSIZE;
        sections = size % SECTIONSIZE==0 ? size/SECTIONSIZE : (size/SECTIONSIZE)+1;
        currentSection = 0;
    }
    if (buffer == NULL){
        perror("Unable to allocate memory for downloading file");
        return;
    }
    stateMap = malloc(stateMapSize*sizeof(StateValue));
    if (stateMap == NULL){
        perror("Unable to allocate memory for State Map");
        return;
    }
    for(size_t i=0; i<stateMapSize; i++) {
        stateMap[i].state = MISSING;
        stateMap[i].timesAttempted = 0;
    }
    memset(buffer, '\0', bufferSize);
    chunksRetrieved = 0;
    completed = false;
}

void initForNewSection() {

    if(currentSection == sections-1){
        size_t lastSection = fileSize%SECTIONSIZE;
        stateMapSize = lastSection % CHUNKSIZE==0 ? lastSection/CHUNKSIZE : (lastSection/CHUNKSIZE)+1;
        bufferSize = stateMapSize*CHUNKSIZE+1;
        free(stateMap);
        stateMap = malloc(stateMapSize*sizeof(StateValue));
        if (stateMap == NULL){
            perror("Unable to allocate memory for State Map");
            return;
        }
        free(buffer);
        buffer =  malloc(bufferSize);
        if (buffer == NULL){
            perror("Unable to allocate memory for downloading file");
            return;
        }
    }

    for(size_t i=0; i<stateMapSize; i++) {
        stateMap[i].state = MISSING;
        stateMap[i].timesAttempted = 0;
    }
    memset(buffer, '\0', bufferSize);
    chunksRetrieved = 0;
    bytesReadPerSection=0;

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
    sections = 0;
    currentSection = 0;
}

//deberia llamarse con un while nextChunk()!=-2 (o similar)(?
//devuelve el indice del principio del chunk que tiene quue buscar
int nextChunk(size_t *byte) {
    if(completed) {
        return -2;
    }

    if(buffer == NULL) {
        perror("Must initialize File Buffer first");
        return -1;
    }

    size_t i = 0;
    while(i<stateMapSize && stateMap[i].state != MISSING) i++;

    if(i==stateMapSize) {
        return -3;
    }
    stateMap[i].state = OBTAINING;
    *byte = (i*CHUNKSIZE)+(currentSection*SECTIONSIZE);
    return 0;
}

//funcion para que el cliente le pase al file manager el contenido del byte que consiguio
int retrievedChunk(size_t chunkNum, char* chunk) {
    size_t stateMapIndex = ((chunkNum) % SECTIONSIZE)/CHUNKSIZE;
    //no se encontro
    if(chunk == NULL) {
        stateMap[stateMapIndex].timesAttempted++;
        if(stateMap[stateMapIndex].timesAttempted == MAX_ATTEMPTS) {
            cancelDownload();
            perror("could not download file.");
            return -1;
        }
        stateMap[stateMapIndex].state = MISSING;
        return 0;
    }

    memcpy(buffer+(chunkNum % SECTIONSIZE), chunk, CHUNKSIZE);
    stateMap[stateMapIndex].state = RETRIEVED;
    chunksRetrieved++;

    if(chunksRetrieved == stateMapSize) {

        char* aux = getFilenamePath();

        newFile = fopen(aux, "a");
        if (newFile == NULL) {
            perror("Unable to open file");
            free(aux);
            return 1;
        }

        size_t auxSize = bufferSize - 1;

        if(sections == 0 || sections == currentSection + 1) {
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

size_t getCurrentDownloadedFileSize() {
    return fileSize;
}

size_t getCurrentDownloadedBytes() {
    if(completed) {
        return fileSize;
    }
    return (currentSection * SECTIONSIZE) + chunksRetrieved * CHUNKSIZE;
}
