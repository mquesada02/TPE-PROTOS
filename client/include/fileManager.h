#ifndef FILEMANAGER_H
#define FILEMANAGER_H


#define CHUNKSIZE 1024
#define SECTIONSIZE 5*1024*1024 //5MB, tamaño máximo del buffer de descarga

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
