#include <string.h>  // memset
#include <unistd.h>  // close
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../include/selector.h"
#include "../include/user.h"
#include "../include/fileManager.h"
#include "../include/connectionManager.h"
#include "../include/args.h"

#define SUCCESS 1
#define ERROR (-1)

#define CLOSE_MSG "Closing"

#define LEN_DEFINE_SIZE 32

#define TIMEOUT 5
#define LEECHER_TIMEOUT 15

#define MAX_ADDR_BUFFER 128
#define MAXPENDING 32

static char addrBuffer[MAX_ADDR_BUFFER];
int connections = 0;

#define LEECH(key) ( (struct LeecherMng *)(key)->data )
#define PEER(key) ( (struct PeerMng *)(key)->data )
#define LEECH_HANDLER(key) ( (struct LeecherHandlerMng *)(key)->data )

void leecherRead(struct selector_key *key);
void leecherWrite(struct selector_key *key);
static void quit(struct selector_key *key);
void peerRead(struct selector_key *key);
void peerWrite(struct selector_key *key);
void cleanupPeerMng(struct PeerMng *peer);

static const struct fd_handler leechersHandler = {
        .handle_read   = leecherRead,
        .handle_write  = leecherWrite,
        .handle_close  = quit,
        .handle_block  = NULL,
};

static const struct fd_handler peerHandler = {
        .handle_read = peerRead,
        .handle_write = peerWrite,
        .handle_close  = NULL,
        .handle_block  = NULL,
};

char * printAddressPort( const struct addrinfo *aip, char addr[])
{
    char abuf[INET6_ADDRSTRLEN];
    const char *addrAux ;
    if (aip->ai_family == AF_INET) {
        struct sockaddr_in	*sinp;
        sinp = (struct sockaddr_in *)aip->ai_addr;
        addrAux = inet_ntop(AF_INET, &sinp->sin_addr, abuf, INET_ADDRSTRLEN);
        if ( addrAux == NULL )
            addrAux = "unknown";
        strcpy(addr, addrAux);
        if ( sinp->sin_port != 0) {
            sprintf(addr + strlen(addr), ": %d", ntohs(sinp->sin_port));
        }
    } else if ( aip->ai_family ==AF_INET6) {
        struct sockaddr_in6	*sinp;
        sinp = (struct sockaddr_in6 *)aip->ai_addr;
        addrAux = inet_ntop(AF_INET6, &sinp->sin6_addr, abuf, INET6_ADDRSTRLEN);
        if ( addrAux == NULL )
            addrAux = "unknown";
        strcpy(addr, addrAux);
        if ( sinp->sin6_port != 0)
            sprintf(addr + strlen(addr), ": %d", ntohs(sinp->sin6_port));
    } else
        strcpy(addr, "unknown");
    return addr;
}


int printSocketAddress(const struct sockaddr *address, char *addrBuffer) {

    void *numericAddress;

    in_port_t port;

    switch (address->sa_family) {
        case AF_INET:
            numericAddress = &((struct sockaddr_in *) address)->sin_addr;
            port = ntohs(((struct sockaddr_in *) address)->sin_port);
            break;
        case AF_INET6:
            numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
            port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
            break;
        default:
            strcpy(addrBuffer, "[unknown type]");    // Unhandled type
            return 0;
    }
    // Convert binary to printable address
    if (inet_ntop(address->sa_family, numericAddress, addrBuffer, INET6_ADDRSTRLEN) == NULL)
        strcpy(addrBuffer, "[invalid address]");
    else {
        if (port != 0)
            sprintf(addrBuffer + strlen(addrBuffer), ":%u", port);
    }
    return 1;
}

//Codigo obtenido de la cátedra de Protocolos de Comunicación 1C-2024
int setupLeecherSocket(const char *service, const char **errmsg) {
    // Construct the server address structure
    struct addrinfo addrCriteria;                   // Criteria for address match
    memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
    addrCriteria.ai_family = AF_UNSPEC;             // Any address family
    addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
    addrCriteria.ai_socktype = SOCK_STREAM;         // Only stream sockets
    addrCriteria.ai_protocol = IPPROTO_TCP;         // Only TCP protocol

    struct addrinfo *servAddr; 			// List of server addresses
    int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
    if (rtnVal != 0) {
        return ERROR;
    }

    int servSock = -1;
    // Intentamos ponernos a escuchar en alguno de los puertos asociados al servicio, sin especificar una IP en particular
    // Iteramos y hacemos el bind por alguna de ellas, la primera que funcione, ya sea la general para IPv4 (0.0.0.0) o IPv6 (::/0) .
    // Con esta implementación estaremos escuchando o bien en IPv4 o en IPv6, pero no en ambas
    for (struct addrinfo *addr = servAddr; addr != NULL && servSock == -1; addr = addr->ai_next) {
        errno = 0;
        // Create a TCP socket
        servSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (servSock < 0) {
            continue;       // Socket creation failed; try next address
        }

        // Bind to ALL the address and set socket to listen
        if ((bind(servSock, addr->ai_addr, addr->ai_addrlen) == 0) && (listen(servSock, MAXPENDING) == 0)) {
            // Print local address of socket
            struct sockaddr_storage localAddr;
            socklen_t addrSize = sizeof(localAddr);
            if (getsockname(servSock, (struct sockaddr *) &localAddr, &addrSize) >= 0) {
                printSocketAddress((struct sockaddr *) &localAddr, addrBuffer);
            }
        } else {
            close(servSock);  // Close and try with the next one
            servSock = -1;
        }
    }

    freeaddrinfo(servAddr);

    return servSock;
}

void leecherHandler(struct selector_key *key) {

    struct sockaddr_storage leecherAddr;
    socklen_t leecherAddrLen = sizeof(leecherAddr);
    struct LeecherMng * mng = NULL;

    int leecher = -1;
    if(connections < LEECH_HANDLER(key)->args->conectionLimit || LEECH_HANDLER(key)->args->conectionLimit == 0) {
        leecher = accept(key->fd, (struct sockaddr*) &leecherAddr, &leecherAddrLen);
    }
    if(leecher == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(leecher) == -1) {
        goto fail;
    }

    mng = malloc(sizeof(struct LeecherMng));

    if(mng == NULL) {
        goto fail;
    }

    memset(mng, 0, sizeof(*mng));

    mng->registered = false;
    mng->trackerSocket = LEECH_HANDLER(key)->tracker->socket;
    mng->trackerAddr = LEECH_HANDLER(key)->tracker->trackerAddr;
    mng->sendingMutex = LEECH_HANDLER(key)->tracker->sendingMutex;


    if(SELECTOR_SUCCESS != selector_register(key->s, leecher, &leechersHandler, OP_READ, mng)) {
        goto fail;
    }

    connections++;

    return;

    fail:
    if(leecher != -1) {
        close(leecher);
    }
    if(mng != NULL) {
        free(mng);
    }
}

static void quit(struct selector_key *key) {
    removeLeecher(LEECH(key)->ip, LEECH(key)->port, LEECH(key)->hash);
    connections--;
    close(key->fd);
    free(LEECH(key));
}


void leecherRead(struct selector_key *key) {

    memset(LEECH(key)->requestBuffer, 0, REQUEST_BUFFER_SIZE);

    ssize_t bytes = recv(key->fd, LEECH(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0);

    if(bytes == 0) {
        selector_unregister_fd(key->s, key->fd);
        return;
    }

    //HASH:BYTE_FROM:BYTE_TO
    char tempByteFrom[256];
    char tempByteTo[256];
    char user[MAX_USER_LEN];

    // Split the received string
    char *token = strtok(LEECH(key)->requestBuffer, ":");
    if (token == NULL)
        goto error;
    strncpy(LEECH(key)->hash, token, 32);

    token = strtok(NULL, ":");
    if (token == NULL)
        goto error;
    strcpy(tempByteFrom, token);

    token = strtok(NULL, ":");
    if (token == NULL)
        goto error;
    strcpy(tempByteTo, token);

    token = strtok(NULL, ":");
    if (token == NULL)
        goto error;
    strcpy(user, token);

    if (strtok(NULL, ":") != NULL)
        goto error;

    if(!LEECH(key)->registered) {

        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);

        if (getpeername(key->fd, (struct sockaddr*)&addr, &addr_len) == -1) {
            printf("WTF\n");
            goto auth_fail;
        }

        inet_ntop(AF_INET, &addr.sin_addr, LEECH(key)->ip, 16);

        snprintf(LEECH(key)->port, 6, "%d", ntohs(addr.sin_port));


        snprintf(LEECH(key)->requestBuffer, REQUEST_BUFFER_SIZE, "%s %s:%s:%s:%s\n", "CHECK", user, LEECH(key)->ip, LEECH(key)->port, LEECH(key)->hash);

        pthread_mutex_lock(&LEECH(key)->sendingMutex);
        if (sendto(LEECH(key)->trackerSocket, LEECH(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0, (struct sockaddr *)LEECH(key)->trackerAddr, sizeof(struct sockaddr_in)) <= 0) {
            perror("failed");
            pthread_mutex_unlock(&LEECH(key)->sendingMutex);
            goto auth_fail;
        }

        socklen_t plen = sizeof(struct sockaddr_in);
        if (recvfrom(LEECH(key)->trackerSocket, LEECH(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0, (struct sockaddr *)LEECH(key)->trackerAddr, &plen) <= 0) {
            perror("failed");
            pthread_mutex_unlock(&LEECH(key)->sendingMutex);
            goto auth_fail;
        }
        pthread_mutex_unlock(&LEECH(key)->sendingMutex);

        /*if (strncmp(LEECH(key)->requestBuffer, "OK", 2) != 0) {
            goto auth_fail;
        }*/


        LEECH(key)->registered = true;
        addLeecher(LEECH(key)->ip, LEECH(key)->port, LEECH(key)->hash);
    }

    size_t byteFrom = atol(tempByteFrom);
    size_t byteTo = atol(tempByteTo);

    if (byteTo < byteFrom)
        goto error;

    size_t size = byteTo - byteFrom;

    if (size > CHUNKSIZE) {
        size = CHUNKSIZE;
    }

    LEECH(key)->size = size;
    LEECH(key)->byteFrom = byteFrom;

    selector_set_interest(key->s, key->fd, OP_WRITE);

    return;

    auth_fail:
    error:
    // no need to inform the client that a leecher lost connection/sent a bad request
    selector_unregister_fd(key->s, key->fd);
}

void leecherWrite(struct selector_key *key) {
    int statusCode;
    size_t bytesRead = copyFromFile(LEECH(key)->responseBuffer, LEECH(key)->hash, LEECH(key)->byteFrom, LEECH(key)->size, &statusCode);
    if(statusCode!=FILE_SUCCESS){
        perror("Error reading file");
        goto error;
    }
    char buff[LEN_DEFINE_SIZE];

    snprintf(buff, LEN_DEFINE_SIZE, "%lu", bytesRead);

    size_t aux = send(key->fd, buff, LEN_DEFINE_SIZE, 0);

    if(aux <= 0) {
        perror("Failed to send response");
        goto error;
    }

    size_t totalBytesSent = 0;
    while (totalBytesSent < bytesRead) {
        size_t sent_bytes = send(key->fd, LEECH(key)->responseBuffer + totalBytesSent, bytesRead - totalBytesSent, 0);
        if (sent_bytes <= 0) {
            perror("Failed to send response");
            goto error;
        }
        totalBytesSent += sent_bytes;
    }
    if (totalBytesSent != bytesRead) {
        perror("Failed to send all data");
        goto error;
    }

    selector_set_interest(key->s, key->fd, OP_READ);

    return;

    error:
    // no need to inform the client that a leecher lost connection/sent a bad request
    selector_unregister_fd(key->s, key->fd);
}

struct Tracker * setupTrackerSocket(const char *ip, const char *port, const char **errmsg) {

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        *errmsg = "Failed to create socket";
        return NULL;
    }

    int port_i = atoi(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) == -1) {
        close(sock);
        *errmsg = "Invalid IP address";
        return NULL;
    }

    serv_addr.sin_port = htons(port_i);
    serv_addr.sin_family = AF_INET;

    struct Tracker *tracker = malloc(sizeof(struct Tracker));
    if (tracker == NULL) {
        close(sock);
        *errmsg = "Memory allocation failed";
        return NULL;
    }

    tracker->trackerAddr = malloc(sizeof(struct sockaddr_in));
    if (tracker->trackerAddr == NULL) {
        free(tracker);
        close(sock);
        *errmsg = "Memory allocation failed";
        return NULL;
    }

    memcpy(tracker->trackerAddr, &serv_addr, sizeof(struct sockaddr_in));
    tracker->socket = sock;

    return tracker;
}

struct PeerMng *initializePeerMng() {
    struct PeerMng *peer = malloc(sizeof(struct PeerMng));
    if (peer == NULL) {
        perror("Failed to allocate memory for PeerMng");
        return NULL;
    }

    peer->writeReady = false;
    peer->readReady = false;
    memset(peer->responseBuffer,0,sizeof(peer->responseBuffer));
    peer->killFlag = false;
    peer->killFlagAck = false;

    if (pthread_mutex_init(&peer->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(peer);
        return NULL;
    }

    return peer;
}

void cleanupPeerMng(struct PeerMng *peer) {
    if (peer != NULL) {
        pthread_mutex_destroy(&peer->mutex);
        free(peer);
    }
}

int setupPeerSocket(const char *ip, const char *port) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1) {
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    int port_i = atoi(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) == -1) {
        close(sock);
        return -1;
    }
    serv_addr.sin_port = htons(port_i);
    serv_addr.sin_family = AF_INET;

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        close(sock);
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        close(sock);
        return -1;
    }


    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        close(sock);
        return -1;
    }

    return sock;

}

struct PeerMng * addPeer(struct selector_key *key, char *user, char *hash, char *ip, char *port) {
    int socket;
    struct PeerMng * mng = NULL;

    socket = setupPeerSocket(ip, port);

    if(socket == -1){
        perror("Failed to connect to peer");
        goto fail;
    }

    mng = initializePeerMng();

    if(mng == NULL)
        goto fail;

    strncpy(mng->user, user, MAX_USER_LEN);

    if(SELECTOR_SUCCESS != selector_register(key->s, socket, &peerHandler, OP_WRITE, mng)) {
        goto fail;
    }

    return mng;

    fail:
    if(socket >= 0) {
        close(socket);
    }
    if(mng != NULL) {
        cleanupPeerMng(mng);
    }
    return NULL;
}

void peerRead(struct selector_key *key) {
    pthread_mutex_lock(&PEER(key)->mutex);

    if(PEER(key)->killFlag) {
        if(PEER(key)->killFlagAck) {
            selector_unregister_fd(key->s, key->fd);
            close(key->fd);
            free(PEER(key));
            return;
        }
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    char buff[LEN_DEFINE_SIZE];

    size_t aux = recv(key->fd, buff, LEN_DEFINE_SIZE, 0);

    if(aux <= 0) {
        perror("Failed to connect (recv) to seeder");
        PEER(key)->killFlag = true;
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    size_t totalBytesIncoming = atoi(buff);

    size_t totalBytesReceived = 0;
    while (totalBytesReceived < totalBytesIncoming) {
        size_t bytes = recv(key->fd, PEER(key)->responseBuffer + totalBytesReceived, totalBytesIncoming - totalBytesReceived, 0);
        if (bytes > 0) {
            totalBytesReceived += bytes;
        } else {
            PEER(key)->killFlag = true;
            perror("Failed to connect (recv) to seeder2");
            break;
        }
    }
    PEER(key)->readReady = true;
    addBytesRead(totalBytesReceived);

    selector_set_interest(key->s, key->fd, OP_WRITE);
    pthread_mutex_unlock(&PEER(key)->mutex);
}

void peerWrite(struct selector_key *key) {
    pthread_mutex_lock(&PEER(key)->mutex);

    if(PEER(key)->killFlag) {
        if(PEER(key)->killFlagAck) {
            selector_unregister_fd(key->s, key->fd);
            close(key->fd);
            free(PEER(key));
            return;
        }
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    if (!PEER(key)->writeReady || PEER(key)->readReady) {
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    ssize_t bytes = send(key->fd, PEER(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0);

    if (bytes <= 0) {
        PEER(key)->killFlag = true;
        perror("Failed to connect (send) to seeder");
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    memset(PEER(key)->requestBuffer, '\0', REQUEST_BUFFER_SIZE);

    PEER(key)->writeReady = false;

    pthread_mutex_unlock(&PEER(key)->mutex);
    selector_set_interest(key->s, key->fd, OP_READ);
}

int requestFromPeer(struct PeerMng * peer, char *hash, size_t byteFrom, size_t byteTo) {
    if (byteTo <= byteFrom + 1) {
        return -1;
    }

    if(peer->readReady) {
        return -1;
    }

    snprintf(peer->requestBuffer, REQUEST_BUFFER_SIZE, "%s:%lu:%lu:%s", hash, byteFrom, byteTo, peer->user);
    memset(peer->responseBuffer, 0, sizeof(peer->responseBuffer));

    peer->writeReady = true;
    return 0;
}

int readFromPeer(struct PeerMng * peer, char buff[CHUNKSIZE]) {
    if(peer->readReady) {
        peer->readReady = false;
        memcpy(buff, peer->responseBuffer, CHUNKSIZE);
        return 0;
    }
    return -1;
}
