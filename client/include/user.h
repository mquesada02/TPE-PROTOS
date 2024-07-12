#ifndef TPE_PROTOS_USER_H
#define TPE_PROTOS_USER_H

#include "selector.h"

struct Tracker {
    struct sockaddr_in * trackerAddr;
    int socket;

    int leecherSocket;
};

void handleInput(struct selector_key *key);
void* handleDownload();
void cleanUpPeers();

#endif //TPE_PROTOS_USER_H
