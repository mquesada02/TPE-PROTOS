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
#define MAX_FILE_LENGTH 256
#define IP_LEN 15
#define PORT_LEN 5
#define MAX_USERNAME_SIZE (32+1)

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
    struct PeerMng *peer;
    char ip[IP_LEN + 1];
    char port[PORT_LEN + 1];
    int status;
    size_t currByte;
};

typedef struct CurrentLeechers {
	char ip[IP_LEN + 1];
    char port[PORT_LEN + 1];
	char fileHash[HASH_LEN + 1];
	struct CurrentLeechers * next;
} CurrentLeechers;

#define LOGIN "PLAIN"
#define PORT "SETPORT"
#define FILES "LIST files"
#define SEEDERS "LIST peers"
#define SIZE "SIZE"
#define NAME "NAME"
#define SEND '\n'

#define ATTACHMENT(key) ( (struct Tracker *) (key)->data)

#define PARAMS int argc, char *argv[], struct selector_key *key

#define CLEAN_PEER(i) peers[i].status = DEAD; peers[i].peer->killFlagAck = true; peersFinished++

#define CHECK_THREAD_DEATH(i) if (peers[i].peer->killFlag) { CLEAN_PEER(i); break;}

char inputBuffer[INPUT_SIZE];

struct Peer peers[MAX_PEERS];

struct SeedersList * seedersList = NULL;

CurrentLeechers * leechers = NULL;

struct selector_key clientKey;

int activePeers = 0;
int peersFinished = 0;

bool downloading = false;
bool paused = false;

char fileHash[HASH_LEN + 1];


char user[MAX_USERNAME_SIZE];

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
void seedersHandler(PARAMS);
void leechersHandler(PARAMS);

struct Command commands[] = {
        {.cmd = "login", .handler = loginHandler},
        {.cmd = "files", .handler = filesHandler},
        {.cmd = "download", .handler = downloadHandler},
        {.cmd = "pause", .handler = pauseHandler},
        {.cmd = "resume", .handler = resumeHandler},
        {.cmd = "cancel", .handler = cancelHandler},
        {.cmd = "dstatus", .handler = downloadStatusHandler},
		{.cmd = "seeders", .handler = seedersHandler},
        {.cmd = "leechers", .handler = leechersHandler},
        {NULL, NULL}
};

int requestFileName(struct selector_key *key, char hash[HASH_LEN + 1], char *buff);

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

void freePeers() {
    for(int i=0; i<activePeers; i++) {
        //
    }
}

void* handleDownload() {
    bool connectionLost = false;
    char buff[CHUNKSIZE];
    int val;
    while(true) {
        if (downloading && !paused) {
            if (connectionLost) {
                printf("Lost connection with seeder. Retrying connection...\n");
                cleanUpPeers();
                //client_key can't be accessed unless handleInput is already working
                if (createSeederConnections(&clientKey, fileHash) != 0) {
                    printf("Failed to download file : No available seeders\n");
                    cleanUpPeers();
                    cancelDownload();
                    downloading = false;
                    paused = false;
                } else{
                    downloading = true;
                    paused = false;
                }
                connectionLost = false;
            } else {
                for (int i = 0; i < activePeers; i++) {
                    size_t byte = 0;
                    switch (peers[i].status) {

                        case READ_READY:
                            pthread_mutex_lock(&peers[i].peer->mutex);
                            memset(buff, 0, CHUNKSIZE);
                            if (readFromPeer(peers[i].peer, buff) != -1) {
                                retrievedChunk(peers[i].currByte, buff);
                                peers[i].status = WAITING;
                            }
                            pthread_mutex_unlock(&peers[i].peer->mutex);
                            break;

                        case WAITING:
                            pthread_mutex_lock(&peers[i].peer->mutex);
                            val = nextChunk(&byte);
                            if (val == -2) {
                                for (int j = 0; j < activePeers; j++) {
                                    if (peers[j].status == WAITING) {
                                        peers[j].peer->killFlag = true;
                                        peers[j].peer->killFlagAck = true;
                                        peers[j].status = DEAD;
                                        peersFinished++;
                                        pthread_mutex_unlock(&peers[i].peer->mutex);
                                    }
                                }
                                if (peersFinished == activePeers) {
                                    activePeers = 0;
                                    peersFinished = 0;
                                    downloading = false;
                                    printf("Download finished\n");
                                    break;
                                }
                                break;
                            } else if (val == -3) {
                                pthread_mutex_unlock(&peers[i].peer->mutex);
                                break;
                            }
                            if (requestFromPeer(peers[i].peer, fileHash, byte, byte + CHUNKSIZE) == -1) {
                                peers[i].status = READ_READY;
                            }
                            peers[i].currByte = byte;
                            peers[i].status = BUSY;
                            pthread_mutex_unlock(&peers[i].peer->mutex);
                            break;

                        case BUSY:
                            pthread_mutex_lock(&peers[i].peer->mutex);
                            if (peers[i].peer->killFlag) {
                                peers[i].peer->killFlagAck = true;
                                peers[i].status = DEAD;
                                peersFinished++;
                                pthread_mutex_unlock(&peers[i].peer->mutex);
                                if(retrievedChunk(peers[i].currByte, NULL) == -1) {
                                    cleanUpPeers();
                                    cancelDownload();
                                    downloading = false;
                                    paused = false;
                                }
                                printf("Lost connection with peer %d\n", i);
                                connectionLost = true;
                                break;
                            }
                            if (peers[i].peer->readReady) {
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
    }
    return NULL;
}

void handleInput(struct selector_key *key) {
    clientKey.data = key->data;
    clientKey.fd = key->fd;
    clientKey.s = key->s;
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
            peers[j].peer->killFlagAck = true;
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

    ssize_t bytes;

    if((bytes = sendMessage(key, requestBuff, strlen(requestBuff))) <= 0) {
        printf("Connection unavailable\n");
        return;
    }

    size_t responseSize = 256;
    char responseBuff[responseSize+1];
    if((bytes = receiveMessage(key, responseBuff, responseSize)) <= 0) {
        printf("Connection unavailable\n");
        return;
    }
    responseBuff[bytes] = '\0';

    if(strncmp(responseBuff, "OK", strlen("OK")) != 0) {
        printf("%s", responseBuff);
        return;
    }

    strncpy(user, argv[1], MAX_USERNAME_SIZE - 1);

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

    if(amountVal == 1) {
        return;
    }
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
    char name[MAX_FILE_LENGTH+1] = {0};

    size_t fileSize;

    if(requestFileName(key, fileHash, name) != 0) {
        printf("Failed to download file : Failed to retrieve file name info\n");
        return;
    }

    if(requestFileSize(key, fileHash, &fileSize) != 0) {
        printf("Failed to download file : Failed to retrieve file info\n");
        return;
    }

    initFileBuffer(name, fileSize);

    if(createSeederConnections(key, fileHash) != 0) {
        cancelDownload();
        printf("Failed to download file : No available seeders\n");
        return;
    }

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
        downloading = false;
        paused = false;
        cancelDownload();
        for(int i=0; i<activePeers; i++) {
            if(peers[i].status != DEAD) {
                peers[i].peer->killFlag = true;
                peers[i].peer->killFlagAck = true;
                peers[i].status = DEAD;
                peers[i].peer = NULL;
                activePeers--;
            }
        }
        peersFinished = 0;
        printf("Download cancelled\n");
    } else {
        printf("Nothing to cancel\n");
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

void seedersHandler(PARAMS) {
	if(activePeers == 0) printf("There are currently no seeder connections\n");
	else {
		printf("Current seeder connections (downloading file %s):\n", fileHash);
		for(int i = 0; i<activePeers; i++){
			if(peers[i].status != DEAD){
				printf("- %s:%s\n", peers[i].ip, peers[i].port);
			}
		}
	}
}

void leechersHandler(PARAMS) {
    if(leechers==NULL) printf("There are currently no seeder connections\n");
	else {
		printf("Current leecher connections:\n");
		for(CurrentLeechers * aux = leechers; aux!=NULL; aux = aux->next){
			printf("- %s:%s - file %s\n", aux->ip, aux->port, aux->fileHash);
		}
	}
}

int requestFileName(struct selector_key *key, char hash[HASH_LEN + 1], char *buff) {
    size_t requestSize = 256;
    char requestBuff[requestSize];
    snprintf(requestBuff, requestSize - 1, "%s %s%c", NAME, hash, SEND);

    sendMessage(key, requestBuff, strlen(requestBuff));

    size_t responseSize = 256;
    char responseBuff[responseSize];

    ssize_t bytes = receiveMessage(key, responseBuff, responseSize);
    if(bytes <= 0) {
        printf("Connection to tracker unavailable\n");
        return 1;
    }
    responseBuff[bytes-1] = '\0';
    strcpy(buff, responseBuff);
    return 0;
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

    int amountVal = atoi(amount); // the +1 is for the "No more files found"
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

    printf("%d seeders available\n", seeders);

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
            struct PeerMng *p = addPeer(key, user, hash, peers[activePeers].ip, peers[activePeers].port);
            if (p != NULL) {
                peers[activePeers].peer = p;
                peers[activePeers].status = WAITING;
                activePeers++;
                printf("Added seeder\n");
            }
            availableSeeders--;
        }
    }

    printf("Established connection with %d seeders\n", activePeers);

    if(activePeers > 0) {
        downloading = true;
        paused = false;
        return 0;
    }

    clearSeederList();
    return 1;
}

void addLeecher(char * ip, char * port, char * hash) {

	CurrentLeechers * node = leechers;

	if(leechers == NULL || atoi(leechers->ip) == atoi(ip)){
		leechers = malloc(sizeof(CurrentLeechers));
		strncpy(leechers->ip, ip, IP_LEN);
        leechers->ip[IP_LEN] = '\0';
		strncpy(leechers->port, port, PORT_LEN);
        leechers->port[PORT_LEN] = '\0';
		strncpy(leechers->fileHash, hash, HASH_LEN);
        leechers->fileHash[HASH_LEN] = '\0';
		if(leechers==NULL) leechers->next = NULL;
		else leechers->next = node;

		return;
	}

	while(node->next!=NULL && atoi(node->next->ip) <= atoi(ip)) node = node->next;

	CurrentLeechers * newNode = malloc(sizeof(CurrentLeechers));
	strncpy(newNode->ip, ip, IP_LEN);
	strncpy(newNode->port, port, PORT_LEN);
	strncpy(newNode->fileHash, hash, HASH_LEN);
	newNode->next = node->next;
	node->next = newNode;
}

bool removeLeecher(char * ip, char * port, char * hash) {
	if(leechers==NULL) return false;

	CurrentLeechers * node = leechers;

	if(atoi(node->ip) == atoi(ip) && atoi(node->port) == atoi(port) && atoi(node->fileHash) == atoi(hash)){
		leechers = leechers->next;
		free(node);
		return true;
	}

	while(node->next!=NULL && atoi(node->next->ip) < atoi(ip) && atoi(node->next->port) < atoi(port) && atoi(node->next->fileHash) < atoi(hash))
		node = node->next;

	if(node->next==NULL) return false;

	CurrentLeechers * aux = node->next;
	node->next = aux->next;
	free(aux);
	return true;
}