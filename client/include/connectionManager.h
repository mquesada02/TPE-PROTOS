#ifndef TPE_PROTOS_CONNECTIONMANAGER_H
#define TPE_PROTOS_CONNECTIONMANAGER_H

#include <bits/pthreadtypes.h>

#include "fileManager.h"
#include "selector.h"

#define REQUEST_BUFFER_SIZE 256

struct peerMng {

    bool killFlag;
    bool killFlagAck;

    bool writeReady;
    char requestBuffer[REQUEST_BUFFER_SIZE];

    bool readReady;
    char responseBuffer[CHUNKSIZE + 1];

    pthread_mutex_t mutex;

};

int setupLeecherSocket(const char *service, const char **errmsg);

struct Tracker * setupTrackerSocket(const char *ip, const char *port, const char **errmsg);

int setupPeerSocket(const char *ip, const char *port);

void leecherHandler(struct selector_key *key);

struct peerMng * addPeer(struct selector_key *key, char *ip, char *port);

int requestFromPeer(struct peerMng * peer, char *hash, size_t byteFrom, size_t byteTo);

int readFromPeer(struct peerMng * peer, char buff[CHUNKSIZE]);

#endif //TPE_PROTOS_CONNECTIONMANAGER_H
