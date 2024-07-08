#ifndef FILEMANAGER_H
#define FILEMANAGER_H


#define PACKET_SIZE 512

long getFileSize(char* md5);
long copyFromFile(char* buffer,char* md5,long offset,long bytes);
long removeFile(char* md5);
long initializeFileManager();
long addFile(char* md5);

#endif
