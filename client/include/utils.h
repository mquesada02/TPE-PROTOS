#ifndef TPE_PROTOS_UTILS_H
#define TPE_PROTOS_UTILS_H

#include <stdbool.h>
#define MD5_SIZE 32

int setupLeekerSocket(const char *service, const char **errmsg);
bool calculateMD5(char* filename, char md5Buffer[MD5_SIZE + 1]);

#endif //TPE_PROTOS_UTILS_H
