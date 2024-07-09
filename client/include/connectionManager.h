#ifndef TPE_PROTOS_CONNECTIONMANAGER_H
#define TPE_PROTOS_CONNECTIONMANAGER_H

#include <bits/pthreadtypes.h>
#include "selector.h"

#define REQUEST_BUFFER_SIZE 256

struct peerMng {

    bool killFlag;

    bool writeReady;
    char requestBuffer[REQUEST_BUFFER_SIZE];

    bool readReady;
    char * responseBuffer;
    size_t responseBufferSize;
    pthread_mutex_t mutex;

};

int setupLeecherSocket(const char *service, const char **errmsg);

struct Tracker * setupTrackerSocket(const char *ip, const char *port, const char **errmsg);

int setupPeerSocket(const char *ip, const char *port);

void leecherHandler(struct selector_key *key);

struct peerMng * addPeer(struct selector_key *key, char *ip, char *port);

void requestFromPeer(struct peerMng * peer, char *hash, size_t byteFrom, size_t byteTo);

#endif //TPE_PROTOS_CONNECTIONMANAGER_H
