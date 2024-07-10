#ifndef FILEMANAGER_H
#define FILEMANAGER_H


#define CHUNKSIZE (256)
#define SECTIONSIZE (1024*CHUNKSIZE) //5MB, tamaño máximo del buffer de descarga

long unsigned int getFileSize(char* md5);
long copyFromFile(char* buffer,char* md5,long offset, unsigned long bytes);
long removeFile(char* md5);
long initializeFileManager();
long addFile(char* md5,char* filename);
void endFileManager();
long unsigned int addBytesRead(long bytes);
void cancelDownload();
long unsigned int getBytesRead();
void initFileBuffer(char* newFilename, long unsigned int size);
long nextChunk();
int retrievedChunk(unsigned long int chunkNum, char* chunk);

#endif
