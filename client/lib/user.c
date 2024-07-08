#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "../include/user.h"
#include "../include/selector.h"
#include "../include/utils.h"
#include "../include/connectionManager.h"

#define INPUT_SIZE 256

#define LOGIN "PLAIN"
#define FILES "LIST"
#define SEND '\n'

#define LEECH(key) ( (struct Tracker *) (key)->data)

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

size_t sendMessage(struct selector_key * key, char * msg, size_t size) {
    return sendto(LEECH(key)->socket, msg, size, 0, (struct sockaddr *)LEECH(key)->trackerAddr, sizeof(struct sockaddr_in));
}

size_t receiveMessage(struct selector_key * key, char * buff, size_t len) {
    return recvfrom(LEECH(key)->socket, buff, len, 0,
                    (struct sockaddr *)LEECH(key)->trackerAddr,
                    (socklen_t *) sizeof(struct sockaddr_in));
}

void loginHandler(PARAMS) {
    if (argc != 3) {
        printf("Invalid parameter amount\n");
        return;
    }
    size_t requestSize = 256;
    char requestBuff[requestSize];

    //char hash[33];

    snprintf(requestBuff, requestSize - 1, "%s %s:%s%c", LOGIN, argv[1], argv[2], SEND);

    sendMessage(key, requestBuff, requestSize - 1);

    size_t responseSize = 256;
    char responseBuff[responseSize];

    if(receiveMessage(key, responseBuff, responseSize - 1) == 0) {
        printf("Connection unavailable\n");
        return;
    }

    printf("%s", responseBuff);

}

void filesHandler(PARAMS) {
    if(argc != 1) {
        printf("Invalid parameter amount\n");
    }

    size_t requestSize = 256;
    char requestBuff[requestSize];
    snprintf(requestBuff, requestSize - 1, "%s%c", FILES, SEND);

    sendMessage(key, requestBuff, requestSize - 1);

    size_t responseSize = 256;
    char responseBuff[responseSize];

    if(receiveMessage(key, responseBuff, responseSize - 1) == 0) {
        printf("Connection unavailable\n");
        return;
    }

    printf("%s", responseBuff);
}

void downloadHandler(PARAMS) {
    if(argc != 4) {
        printf("Invalid parameter amount\n");
    }

    struct peerMng* p1 = addPeer(key, argv[1], argv[2]);
    struct peerMng* p2 = addPeer(key, argv[1], argv[2]);
    struct peerMng* p3 = addPeer(key, argv[1], argv[2]);

    requestFromPeer(p1, argv[3], 0, 5);
    requestFromPeer(p2, argv[3], 6, 10);
    requestFromPeer(p3, argv[3], 10, 9999);
}
