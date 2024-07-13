#ifndef TPE_PROTOS_CONNECTIONMANAGER_H
#define TPE_PROTOS_CONNECTIONMANAGER_H

#include <bits/pthreadtypes.h>

#include "fileManager.h"
#include "selector.h"
#include "args.h"

#define REQUEST_BUFFER_SIZE 256
#define MAX_USER_LEN 32

struct PeerMng {
    bool killFlag;
    bool killFlagAck;

    bool writeReady;
    char requestBuffer[REQUEST_BUFFER_SIZE];

    bool readReady;
    char responseBuffer[CHUNKSIZE + 1];

    char user[MAX_USER_LEN + 1];
    pthread_mutex_t mutex;

};

struct LeecherHandlerMng {
    struct clientArgs *args;

    struct Tracker *tracker;
};

struct LeecherMng {

    bool registered;

    char requestBuffer[REQUEST_BUFFER_SIZE];

    char responseBuffer[CHUNKSIZE + 1];

    struct sockaddr_in * trackerAddr;
    int trackerSocket;
    pthread_mutex_t sendingMutex;

    char port[6];
    char ip[16];
    char hash[33];
    size_t byteFrom;
    size_t size;

};

int setupLeecherSocket(const char *service, const char **errmsg);

struct Tracker * setupTrackerSocket(const char *ip, const char *port, const char **errmsg);

int setupPeerSocket(const char *ip, const char *port);

void leecherHandler(struct selector_key *key);

struct PeerMng * addPeer(struct selector_key *key, char *user, char *hash, char *ip, char *port);

int requestFromPeer(struct PeerMng * peer, char *hash, size_t byteFrom, size_t byteTo);

int readFromPeer(struct PeerMng * peer, char buff[CHUNKSIZE]);

#endif //TPE_PROTOS_CONNECTIONMANAGER_H
