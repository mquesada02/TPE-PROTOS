#ifndef FILEMANAGER_H
#define FILEMANAGER_H
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

typedef enum FileSuccess{FILE_SUCCESS,FILE_MISSING,FILE_ERROR_READING} FileSuccess;

#define CHUNKSIZE (size_t)(32*1024)
#define SECTIONSIZE (size_t)(64*CHUNKSIZE)

size_t getFileSize(char* md5);
size_t copyFromFile(char* buffer,char* md5,size_t offset, size_t bytes,int* statusCode);
int removeFile(char* md5);
int initializeFileManager();
int addFile(char* md5,char* filename);
void endFileManager();
size_t addBytesRead(size_t bytes);
void cancelDownload();
size_t getBytesRead();
void initFileBuffer(char* newFilename, size_t size);
int nextChunk(size_t *byte);
int retrievedChunk(size_t  chunkNum, char* chunk);

#endif
