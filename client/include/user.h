#ifndef TPE_PROTOS_USER_H
#define TPE_PROTOS_USER_H

#include "selector.h"

struct Tracker {
    struct sockaddr_in * trackerAddr;
    int socket;
    pthread_mutex_t sendingMutex;

    int leecherSocket;
};

void handleInput(struct selector_key *key);
void* handleDownload();
void cleanUpPeers();
void addLeecher(char * ip, char * port, char * hash);
bool removeLeecher(char * ip, char * port, char * hash);

#endif //TPE_PROTOS_USER_H
