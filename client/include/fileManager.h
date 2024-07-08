#ifndef FILEMANAGER_H
#define FILEMANAGER_H


#define PACKET_SIZE 512

int getFileSize(char* md5);
int copyFromFile(char* buffer,char* md5,int offset,int bytes);
int removeFile(char* md5);
int initializeFileManager();
int addFile(char* md5);

#endif
