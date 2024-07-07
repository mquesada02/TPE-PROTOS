#ifndef __FILEMAP_H_
#define __FILEMAP_H_
#include <stdio.h>
#define MD5_SIZE 129



typedef struct hashMap* fileMap;


fileMap createMap();
void insert(fileMap map,unsigned char* key,FILE* value);
FILE *lookup(fileMap map, unsigned char *key);
void freeMap(fileMap map);
int removeEntry(fileMap hashmap, unsigned char *key);

#endif // !1
