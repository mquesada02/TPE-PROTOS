#ifndef FILEMANAGER_H
#define FILEMANAGER_H


#define PACKET_SIZE 512
#define CHUNKSIZE 1024

long getFileSize(char* md5);
long copyFromFile(char* buffer,char* md5,long offset,unsigned long bytes);
long removeFile(char* md5);
long initializeFileManager();
long addFile(char* md5,char* filename);
void endFileManager();

void initFileBuffer(char* newFilename, int size);
int nextChunk();
int retrievedChunk(int chunkNum, char* chunk);

#endif
