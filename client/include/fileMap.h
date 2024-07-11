#ifndef __FILEMAP_H_
#define __FILEMAP_H_
#include <stdio.h>

#define TABLE_SIZE 100

typedef struct hashMap* fileMap;


fileMap createMap();
void insert(fileMap map,char* key,FILE* value);
FILE *lookup(fileMap map, char *key);
void freeMap(fileMap map);
int removeEntry(fileMap hashmap, char *key);

#endif
