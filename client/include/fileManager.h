#ifndef FILEMANAGER_H
#define FILEMANAGER_H


#define PACKET_SIZE 512
#define CHUNKSIZE 1024

long unsigned int getFileSize(char* md5);
long copyFromFile(char* buffer,char* md5,long offset, long unsigned int bytes);
long removeFile(char* md5);
long initializeFileManager();
long addFile(char* md5,char* filename);
void endFileManager();
void cancelDownload();

void initFileBuffer(char* newFilename, long unsigned int size);
int nextChunk();
int retrievedChunk(int chunkNum, char* chunk);

#endif
