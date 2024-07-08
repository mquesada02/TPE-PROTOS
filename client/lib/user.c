#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../include/selector.h"

#define INPUT_SIZE 256

char inputBuffer[INPUT_SIZE];

struct Command {
    char *cmd;
    void (*handler)(int argc, char *argv[]);
};

void filesHandler(int argc, char *argv[]);
void downloadHandler(int argc, char *argv[]);

struct Command commands[] = {
        {.cmd = "files", .handler = filesHandler},
        {.cmd = "download", .handler = downloadHandler},
        {NULL, NULL}
};

void parseCommand(char *input) {
    char *args[INPUT_SIZE / 2];
    int argc = 0;
    char *token = strtok(input, " \n");

    if (token == NULL) {
        return;
    }
    char *command = token;

    while (token != NULL) {
        args[argc++] = token;
        token = strtok(NULL, " \n");
    }

    for (int i = 0; commands[i].cmd != NULL; i++) {
        if (strcmp(command, commands[i].cmd) == 0) {
            commands[i].handler(argc, args);
            return;
        }
    }

    printf("Unknown command: %s\n", command);
}

void handleInput(struct selector_key *key) {
    ssize_t bytesRead = read(key->fd, inputBuffer, INPUT_SIZE - 1);
    if (bytesRead <= 0) {
        return;
    }

    inputBuffer[bytesRead] = '\0';
    parseCommand(inputBuffer);
}

void filesHandler(int argc, char *argv[]) {
    printf("filesHandler called with %d arguments\n", argc - 1);
    for (int i = 1; i < argc; i++) {
        printf("arg[%d]: %s\n", i - 1, argv[i]);
    }
}

void downloadHandler(int argc, char *argv[]) {
    printf("downloadHandler called with %d arguments\n", argc - 1);
    for (int i = 1; i < argc; i++) {
        printf("arg[%d]: %s\n", i - 1, argv[i]);
    }
}
