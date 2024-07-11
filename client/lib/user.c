#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>

#include "../include/user.h"
#include "../include/selector.h"
#include "../include/fileManager.h"
#include "../include/utils.h"
#include "../include/connectionManager.h"

#define INPUT_SIZE 256
#define MAX_PEERS 5

enum STATUS {
    READ_READY,
    WAITING,
    BUSY,
    HOLD,
    DEAD
};

struct Peer {
    struct peerMng *peer;
    int status;
    int currByte;
    int holdQuantum;
};

#define LOGIN "PLAIN"
#define FILES "LIST files"
#define SEND '\n'

#define LEECH(key) ( (struct Tracker *) (key)->data)

#define PARAMS int argc, char *argv[], struct selector_key *key

char inputBuffer[INPUT_SIZE];

struct Peer peers[MAX_PEERS];

int activePeers = 0;
int peersFinished = 0;

bool downloading = false;
bool paused = false;

char fileHash[33];

struct Command {
    char *cmd;
    void (*handler)(PARAMS);
};

void filesHandler(PARAMS);
void downloadHandler(PARAMS);
void loginHandler(PARAMS);
void pauseHandler(PARAMS);
void resumeHandler(PARAMS);
void cancelHandler(PARAMS);

struct Command commands[] = {
        {.cmd = "login", .handler = loginHandler},
        {.cmd = "files", .handler = filesHandler},
        {.cmd = "download", .handler = downloadHandler},
        {.cmd = "pause", .handler = pauseHandler},
        {.cmd = "resume", .handler = resumeHandler},
        {.cmd = "cancel", .handler = cancelHandler},
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

void* handleDownload() {
    char buff[CHUNKSIZE];
    while(true) {
        if (downloading && !paused) {
            for (int i = 0; i < activePeers; i++) {
                int byte = 0;
                switch (peers[i].status) {
                    case READ_READY:
                        memset(buff,0,CHUNKSIZE);
                        if(readFromPeer(peers[i].peer, buff) != -1) {
                            retrievedChunk(peers[i].currByte, buff);
                            peers[i].status = WAITING;
                        }
                        break;
                    case WAITING:
                        byte = nextChunk();
                        if (byte == -2) {
                            for (int j = 0; j < activePeers; j++) {
                                if (peers[j].status == WAITING) {
                                    peers[j].peer->killFlag = true;
                                    peers[j].peer = NULL;
                                    peers[j].status = DEAD;
                                    peersFinished++;
                                }
                            }
                            if (peersFinished == activePeers) {
                                activePeers = 0;
                                peersFinished = 0;
                                downloading = false;
                            }
                            break;
                        }

                        else if (byte == -3) {
                            peers[i].holdQuantum = 100;
                            peers[i].status = HOLD;
                            break;
                        }
                        requestFromPeer(peers[i].peer, fileHash, byte, byte + CHUNKSIZE);
                        peers[i].currByte = byte;
                        peers[i].status = BUSY;
                        break;
                    case BUSY:
                        if (peers[i].peer->readReady) {
                            peers[i].status = READ_READY;
                        }
                        break;
                    case HOLD:
                        if(peers[i].holdQuantum-- <= 0) {
                            peers[i].status = WAITING;
                        }
                        break;
                    case DEAD:
                        //TODO handle dead
                        break;
                }
            }
        }
    }
    return NULL;
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

void cleanUpPeers() {
    for (int j = 0; j < activePeers; j++) {
        if (peers[j].status != DEAD) {
            peers[j].peer->killFlag = true;
            peers[j].peer = NULL;
            peers[j].status = DEAD;
            peersFinished++;
        }
    }
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
        return;
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
        return;
    }

    //TODO pedir el tamaño del archivo del tracker, pedir los peers y armarlos
    strncpy(fileHash, argv[3], 32);

    initFileBuffer("esto_es_de_manu.txt", getFileSize(fileHash));


    downloading = true;
    paused = false;

    struct peerMng* p1 = addPeer(key, argv[1], argv[2]);
    struct peerMng* p2 = addPeer(key, argv[1], argv[2]);

    peers[0].peer = p1;
    peers[0].status = WAITING;
    peers[1].peer = p2;
    peers[1].status = WAITING;
    activePeers = 2;
    peersFinished = 0;
    printf("Started Download\n");
}

void pauseHandler(PARAMS) {
    if(downloading && !paused) {
        paused = true;
        printf("Download Paused\n");
        return;
    }
    printf("Nothing to Pause\n");
}
void resumeHandler(PARAMS) {
    if(downloading && paused) {
        paused = false;
        printf("Download Resumed\n");
        return;
    }
    printf("Nothing to Resume\n");
}
void cancelHandler(PARAMS) {
    if(downloading) {
        paused = true;
        cancelDownload();
        for(int i=0; i<activePeers; i++) {
            if(peers[i].status != DEAD) {
                peers[i].peer->killFlag = true;
                peers[i].status = DEAD;
                peers[i].peer = NULL;
            }
        }
        printf("Download Cancelled\n");
    }
}
