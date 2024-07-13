#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/utils.h"
#include "../include/fileList.h"



typedef struct entry {
    char key[MD5_SIZE+1];
    FILE *value;
    struct entry *next;
} entry;

struct entry *head;

void initializeList() {
    head=NULL;
}

int insertFile (char *key, FILE *value) {
    struct entry *current = head;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return 1;
        }
        current = current->next;
    }


    struct entry *newEntry = malloc(sizeof(struct entry));
    if (newEntry == NULL) {
        return -1;
    }
    strncpy(newEntry->key, key,MD5_SIZE+1);
    newEntry->value = value;
    newEntry->next = head;
    head = newEntry;
    return 0;
}

FILE *findFile (char *key) {
    struct entry *current = head;
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

int deleteFile(char *key) {
    struct entry *current = head;
    struct entry *prev = NULL;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (prev == NULL) {
                head = current->next;
            } else {
                prev->next = current->next;
            }
            fclose(current->value);
            free(current);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    return -1;
}

void freeList() {
    struct entry *current = head;
    struct entry *next;

    while (current != NULL) {
        next = current->next;
        fclose(current->value);
        free(current);
        current = next;
    }

    head = NULL;
}
