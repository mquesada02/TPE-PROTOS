#ifndef TPE_PROTOS_USER_H
#define TPE_PROTOS_USER_H

#include "selector.h"

struct Tracker {
    struct sockaddr_in * trackerAddr;
    int socket;
};

void handleInput(struct selector_key *key);
void* handleDownload();

#endif //TPE_PROTOS_USER_H
