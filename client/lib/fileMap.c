#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/utils.h"
#include "../include/fileMap.h"



typedef struct Entry {
    unsigned char key[MD5_SIZE+1];
    FILE *value;
    struct Entry *next;
} Entry;

typedef struct hashMap {
    Entry **table;
} hashMap;

fileMap createMap() {
    fileMap hashmap = malloc(sizeof(hashMap));
    if(hashmap==NULL){
        return NULL;
    }
    hashmap->table = calloc(1,sizeof(Entry *) * TABLE_SIZE);
    if(hashmap->table==NULL){
        free(hashmap);
        return NULL;
    }
    return hashmap;
}

static unsigned int hash(char *key) {
    unsigned int hashValue = 0;
    for (int i = 0; i < MD5_SIZE; i++) {
        hashValue = (hashValue << 5) + key[i];
    }
    return hashValue % TABLE_SIZE;
}

void insert(fileMap map, char *key, FILE *value) {
    unsigned int index = hash(key);
    Entry *new_entry = malloc(sizeof(Entry));
    memcpy(new_entry->key, key, MD5_SIZE+1);
    new_entry->value = value;
    new_entry->next = map->table[index];
    map->table[index] = new_entry;
}

FILE *lookup(fileMap map, char *key) {
    unsigned int index = hash(key);
    Entry *entry = map->table[index];
    while (entry != NULL) {
        if (memcmp(entry->key, key, MD5_SIZE) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

int removeEntry(fileMap hashmap, char *key) {
    unsigned int index = hash(key);
    Entry *entry = hashmap->table[index];
    Entry *prev = NULL;

    while (entry != NULL) {
        if (memcmp(entry->key, key, MD5_SIZE) == 0) {
            if (prev == NULL) {
                hashmap->table[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            fclose(entry->value);
            free(entry);
            return 1;
        }
        prev = entry;
        entry = entry->next;
    }
    return 0;
}

void freeMap(fileMap map) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry *entry = map->table[i];
        while (entry != NULL) {
            Entry *temp = entry;
            entry = entry->next;
            fclose(temp->value);
            free(temp);
        }
    }
    free(map->table);
    free(map);
}
