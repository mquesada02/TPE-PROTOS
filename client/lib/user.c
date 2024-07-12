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
#define MAX_INT_LEN 12
#define HASH_LEN 32
#define IP_LEN 15
#define PORT_LEN 5

enum STATUS {
    READ_READY,
    WAITING,
    BUSY,
    DEAD
};

struct SeedersList {
    char ip[IP_LEN + 1];
    char port[PORT_LEN + 1];
    struct SeedersList * next;
};

struct Peer {
    struct peerMng *peer;
    char ip[IP_LEN + 1];
    char port[PORT_LEN + 1];
    int status;
    size_t currByte;
};

#define LOGIN "PLAIN"
#define PORT "SETPORT"
#define FILES "LIST files"
#define SEEDERS "LIST peers"
#define SIZE "SIZE"
#define SEND '\n'

#define ATTACHMENT(key) ( (struct Tracker *) (key)->data)

#define PARAMS int argc, char *argv[], struct selector_key *key

#define CLEAN_PEER(i) peers[i].status = DEAD; peersFinished++; pthread_mutex_unlock(&peers[i].peer->mutex); free(peers[i].peer)

#define CHECK_THREAD_DEATH(i) if (peers[i].peer->killFlag) { CLEAN_PEER(i); break;}

char inputBuffer[INPUT_SIZE];

struct Peer peers[MAX_PEERS];

struct SeedersList * seedersList = NULL;

struct selector_key *client_key = NULL;

int activePeers = 0;
int peersFinished = 0;

bool downloading = false;
bool paused = false;
bool cleanup = false;

char fileHash[HASH_LEN + 1];

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
void downloadStatusHandler(PARAMS);

struct Command commands[] = {
        {.cmd = "login", .handler = loginHandler},
        {.cmd = "files", .handler = filesHandler},
        {.cmd = "download", .handler = downloadHandler},
        {.cmd = "pause", .handler = pauseHandler},
        {.cmd = "resume", .handler = resumeHandler},
        {.cmd = "cancel", .handler = cancelHandler},
        {.cmd = "dstatus", .handler = downloadStatusHandler},
        {NULL, NULL}
};

int requestFileSize(struct selector_key *key, char hash[HASH_LEN + 1], size_t *size);

int requestSeeders(struct selector_key *key, char hash[HASH_LEN + 1]);

bool getSeeder(char ip[IP_LEN + 1], char port[PORT_LEN + 1]);

int createSeederConnections(struct selector_key *key, char hash[HASH_LEN + 1]);

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
    int val;
    while(true) {
        if(downloading && !paused) {
            if(peersFinished == activePeers) {
                //client_key can't be accessed unless handleInput is already working
                printf("Connection with seeders lost, attempting to reconnect...\n");
                if(createSeederConnections(client_key, fileHash) != 0) {
                    printf("Failed to download file : No available seeders\n");
                    cancelDownload();
                    downloading = false;
                    paused = false;
                    break;
                }
            }
            for(int i = 0; i < activePeers; i++) {
                size_t byte = 0;
                switch(peers[i].status) {
                    case READ_READY:
                        pthread_mutex_lock(&peers[i].peer->mutex);
                        CHECK_THREAD_DEATH(i)
                        memset(buff,0,CHUNKSIZE);
                        if(readFromPeer(peers[i].peer, buff) != -1) {
                            retrievedChunk(peers[i].currByte, buff);
                            if(cleanup) {
                                CLEAN_PEER(i);
                                if(peersFinished == activePeers) {
                                    activePeers = 0;
                                    peersFinished = 0;
                                    downloading = false;
                                    cleanup = false;
                                    break;
                                }
                            }
                            peers[i].status = WAITING;
                        }
                        pthread_mutex_unlock(&peers[i].peer->mutex);
                        break;
                    case WAITING:
                        pthread_mutex_lock(&peers[i].peer->mutex);
                        CHECK_THREAD_DEATH(i)
                        val = nextChunk(&byte);
                        if(val == -2) {
                            for(int j = 0; j < activePeers; j++) {
                                if(peers[j].status == WAITING) {
                                    peers[j].peer->killFlag = true;
                                    peers[j].status = DEAD;
                                    peersFinished++;
                                    pthread_mutex_unlock(&peers[i].peer->mutex);
                                }
                            }
                            if(peersFinished == activePeers) {
                                activePeers = 0;
                                peersFinished = 0;
                                downloading = false;
                            } else {
                                cleanup = true;
                            }
                            break;
                        }

                        else if(val == -3) {
                            pthread_mutex_unlock(&peers[i].peer->mutex);
                            break;
                        }
                        if(requestFromPeer(peers[i].peer, fileHash, byte, byte + CHUNKSIZE) == -1) {
                            peers[i].status = READ_READY;
                        }
                        peers[i].currByte = byte;
                        peers[i].status = BUSY;
                        pthread_mutex_unlock(&peers[i].peer->mutex);
                        break;
                    case BUSY:
                        pthread_mutex_lock(&peers[i].peer->mutex);
                        CHECK_THREAD_DEATH(i)
                        if(peers[i].peer->killFlag) {
                            peers[i].status = DEAD;
                            peersFinished++;
                            pthread_mutex_unlock(&peers[i].peer->mutex);
                            printf("Lost connection with peer %d\n", i);
                            break;
                        }
                        if(peers[i].peer->readReady) {
                            peers[i].status = READ_READY;
                        }
                        pthread_mutex_unlock(&peers[i].peer->mutex);
                        break;
                    case DEAD:
                        break;
                }
            }
        }
    }
    return NULL;
}

void handleInput(struct selector_key *key) {
    client_key = key; //accessed by handleDownload
    ssize_t bytesRead = read(key->fd, inputBuffer, INPUT_SIZE - 1);
    if (bytesRead <= 0) {
        return;
    }
    inputBuffer[bytesRead] = '\0';
    parseCommand(inputBuffer, key);
}

size_t sendMessage(struct selector_key * key, char * msg, size_t size) {
    return sendto(ATTACHMENT(key)->socket, msg, size, 0, (struct sockaddr *)ATTACHMENT(key)->trackerAddr, sizeof(struct sockaddr_in));
}

ssize_t receiveMessage(struct selector_key * key, char * buff, size_t len) {
    socklen_t plen = sizeof(struct sockaddr_in);
    return recvfrom(ATTACHMENT(key)->socket, buff, len, 0,
                    (struct sockaddr *)ATTACHMENT(key)->trackerAddr,
                    &plen);
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

    //char hash[HASH_LEN + 1];

    snprintf(requestBuff, requestSize - 1, "%s %s:%s%c", LOGIN, argv[1], argv[2], SEND);
    
    sendMessage(key, requestBuff, strlen(requestBuff));

    size_t responseSize = 256;
    char responseBuff[responseSize+1];
    ssize_t bytes;
    if((bytes = receiveMessage(key, responseBuff, responseSize)) == 0) {
        printf("Connection unavailable\n");
        return;
    }
    responseBuff[bytes] = '\0';

    snprintf(requestBuff, requestSize - 1, "%s %d%c", PORT, ATTACHMENT(key)->leecherSocket, SEND);

    sendMessage(key, requestBuff, strlen(requestBuff));

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

    sendMessage(key, requestBuff, strlen(requestBuff));

    size_t responseSize = 256;
    char responseBuff[responseSize+1];
    char amount[MAX_INT_LEN] = {0};
    ssize_t bytes = receiveMessage(key, amount, MAX_INT_LEN);
    if (bytes <= 0) {
        printf("Connection unavailable\n");
        return;
    }
    int amountVal = atoi(amount)+1; // the +1 is for the "No more files found"
    int lineCount = 0;
    printf("Files amount: %d\n", amountVal-1);
    while (lineCount < amountVal) {
        bytes = receiveMessage(key, responseBuff, responseSize);
        if (bytes <= 0) continue;
        responseBuff[bytes] = '\0';
        printf("%s",responseBuff);
        lineCount++;
    }
}

void downloadHandler(PARAMS) {
    if(argc != 2) {
        printf("Invalid parameter amount\n");
        return;
    }

    if(downloading) {
        printf("Unable to download new file, cancel or finish current download\n");
        return;
    }

    strncpy(fileHash, argv[1], HASH_LEN + 1);

    fileHash[HASH_LEN] = '\0';

    size_t fileSize;
    if(requestFileSize(key, fileHash, &fileSize) != 0) {
        printf("Failed to download file : Failed to retrieve file info\n");
        return;
    }

    if(createSeederConnections(key, fileHash) != 0) {
        printf("Failed to download file : No available seeders\n");
        return;
    }

    initFileBuffer(fileHash, fileSize);

    printf("Starting download\n");
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

void downloadStatusHandler(PARAMS) {
    if(!downloading) {
        printf("No active download\n");
        return;
    }
    size_t curr = getCurrentDownloadedBytes();
    size_t total = getCurrentDownloadedFileSize();
    double percentage = ((double)curr / (double)total) * 100.0;
    printf("Progress: %.2f%% || %lu/%lu bytes \n", percentage, curr, total);
}

int requestFileSize(struct selector_key *key, char hash[HASH_LEN + 1], size_t *size) {
    size_t requestSize = 256;
    char requestBuff[requestSize];
    snprintf(requestBuff, requestSize - 1, "%s %s%c", SIZE, hash, SEND);

    sendMessage(key, requestBuff, strlen(requestBuff));

    size_t responseSize = 256;
    char responseBuff[responseSize];

    ssize_t bytes = receiveMessage(key, responseBuff, responseSize);
    if(bytes <= 0) {
        printf("Connection to tracker unavailable\n");
        return 1;
    }
    *size = 0;
    responseBuff[bytes] = '\0';
    *size = atol(responseBuff);
    return 0;
}

int requestSeeders(struct selector_key *key, char hash[HASH_LEN + 1]) {
    size_t requestSize = 256;
    char requestBuff[requestSize];
    snprintf(requestBuff, requestSize - 1, "%s %s%c", SEEDERS, hash, SEND);

    sendMessage(key, requestBuff, strlen(requestBuff));

    size_t responseSize = 256;
    char responseBuff[responseSize+1];
    char amount[MAX_INT_LEN] = {0};
    ssize_t bytes = receiveMessage(key, amount, MAX_INT_LEN);
    if(bytes <= 0) {
        printf("Connection to tracker unavailable\n");
        return 0;
    }

    int amountVal = atoi(amount)+1; // the +1 is for the "No more files found"
    int lineCount = 0;
    int seeders = 0;
    printf("Retrieving seeders\n");
    while(lineCount < amountVal) {
        bytes = receiveMessage(key, responseBuff, responseSize);
        if (bytes <= 0) continue;
        responseBuff[bytes] = '\0';
        struct SeedersList * seeder = malloc(sizeof(struct SeedersList));

        char *token = strtok(responseBuff, ":");
        if (token != NULL) {
            strncpy(seeder->ip, token, IP_LEN);
            seeder->ip[IP_LEN] = '\0';
        }

        printf("IP: %s\n", seeder->ip);

        token = strtok(NULL, "\n");
        if (token != NULL) {
            strncpy(seeder->port, token, PORT_LEN);
            seeder->port[PORT_LEN] = '\0';
        }
        printf("PORT: %s\n", seeder->port);
        seeder->next = seedersList;
        seedersList = seeder;
        seeders++;
        lineCount++;
    }

    printf("Finished processing seeders\n");

    return seeders;
}

bool getSeeder(char ip[IP_LEN + 1], char port[PORT_LEN + 1]) {
    if(seedersList == NULL) {
        return false;
    }
    strncpy(ip, seedersList->ip, IP_LEN + 1);
    strncpy(port, seedersList->port, PORT_LEN + 1);
    struct SeedersList *aux = seedersList;
    seedersList = seedersList->next;
    free(aux);
    return true;
}

void clearSeederList() {
    while(seedersList != NULL) {
        struct SeedersList *aux = seedersList;
        seedersList = seedersList->next;
        free(aux);
    }
}

int createSeederConnections(struct selector_key *key, char hash[HASH_LEN + 1]) {
    int availableSeeders = requestSeeders(key, hash);
    activePeers = 0;
    peersFinished = 0;

    if(availableSeeders == 0) {
        printf("No seeders for the specified file\n");
        return 1;
    }

    while(activePeers < MAX_PEERS && availableSeeders > 0) {
        if(getSeeder(peers[activePeers].ip, peers[activePeers].port)) {
            printf("Added seeder\n");
            struct peerMng *p = addPeer(key, peers[activePeers].ip, peers[activePeers].port);
            if (p != NULL) {
                peers[activePeers].peer = p;
                peers[activePeers].status = WAITING;
                activePeers++;
                availableSeeders--;
            }
        }
    }

    if(activePeers > 0) {
        downloading = true;
        paused = false;
        cleanup = false;
        return 0;
    }

    clearSeederList();
    return 1;
}