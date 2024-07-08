#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../include/user.h"
#include "../include/selector.h"

#define INPUT_SIZE 256

#define LOGIN "PLAIN"

#define SEND '\n'

#define ATTACHMENT(key) ( (struct Tracker *) (key)->data)

#define PARAMS int argc, char *argv[], struct selector_key *key

char inputBuffer[INPUT_SIZE];

struct Command {
    char *cmd;
    void (*handler)(PARAMS);
};

void filesHandler(PARAMS);
void downloadHandler(PARAMS);
void loginHandler(PARAMS);

struct Command commands[] = {
        {.cmd = "login", .handler = loginHandler},
        {.cmd = "files", .handler = filesHandler},
        {.cmd = "download", .handler = downloadHandler},
        {NULL, NULL}
};

void parseCommand(char *input, struct selector_key *key) {
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
            commands[i].handler(argc, args, key);
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
    parseCommand(inputBuffer, key);
}

void loginHandler(PARAMS) {
    if (argc != 3) {
        printf("Invalid parameter amount\n");
        return;
    }
    size_t requestSize = 256;
    char requestBuff[requestSize];

    requestBuff[requestSize - 1] = '\0';

    snprintf(requestBuff, requestSize - 1, "%s %s:%s%c", LOGIN, argv[1], argv[2], SEND);
    printf("%s", requestBuff);
    sendto(ATTACHMENT(key)->socket, requestBuff, strlen(requestBuff), 0, (struct sockaddr *)ATTACHMENT(key)->trackerAddr, sizeof(struct sockaddr_in));
}

void filesHandler(PARAMS) {
    printf("filesHandler called with %d arguments\n", argc - 1);
    for (int i = 1; i < argc; i++) {
        printf("arg[%d]: %s\n", i - 1, argv[i]);
    }
}

void downloadHandler(PARAMS) {
    printf("downloadHandler called with %d arguments\n", argc - 1);
    for (int i = 1; i < argc; i++) {
        printf("arg[%d]: %s\n", i - 1, argv[i]);
    }
}
