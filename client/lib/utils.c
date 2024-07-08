#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "../include/utils.h"

#define MAX_FILENAME 256

bool calculateMD5(char* filename, char md5Buffer[MD5_SIZE + 1]) {
    FILE *file = NULL;

    char buff[strlen("md5sum ") + MAX_FILENAME];
    memset(buff,0,strlen("md5sum ") + MAX_FILENAME);
    strcat(buff, "md5sum ");
    strcat(buff, filename);

    file = popen(buff,"r");
    md5Buffer[MD5_SIZE] = '\0';
    fgets(md5Buffer,MD5_SIZE+1,file);

    pclose(file);

    return true;
}
