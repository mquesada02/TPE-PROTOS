#ifndef FILEMANAGER_H
#define FILEMANAGER_H


#define PACKET_SIZE 512
#define CHUNKSIZE 1024

int copyFromFile(char* buffer,char* md5,int offset,int bytes);
int removeFile(char* md5);
int initializeFileManager();
int addFile(char* md5);

void initFileBuffer(char* newFilename, int size);
int nextChunk();
int retrievedChunk(int chunkNum, char* chunk);

#endif
