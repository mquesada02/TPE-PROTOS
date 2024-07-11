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

#define SUCCESS 1
#define ERROR (-1)

#define CLOSE_MSG "Closing"

#define MAX_ADDR_BUFFER 128
#define MAXPENDING 32

static char addrBuffer[MAX_ADDR_BUFFER];

#define LEECH(key) ( (struct leecherMng *)(key)->data)
#define PEER(key) ( (struct peerMng *)(key)->data)

void leecherRead(struct selector_key *key);
//void leecherWrite(struct selector_key *key);
void peerRead(struct selector_key *key);
void peerWrite(struct selector_key *key);
void cleanupPeerMng(struct peerMng *peer);

struct leecherMng {

    char requestBuffer[REQUEST_BUFFER_SIZE];

    char * responseBuffer;

};

static const struct fd_handler leechersHandler = {
        .handle_read   = leecherRead,
        .handle_write  = NULL,
        .handle_close  = NULL,
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

    struct sockaddr_storage leekerAddr;
    socklen_t leekerAddrLen = sizeof(leekerAddr);
    struct leecherMng * mng = NULL;

    const int leeker = accept(key->fd, (struct sockaddr*) &leekerAddr, &leekerAddrLen);
    if(leeker == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(leeker) == -1) {
        goto fail;
    }

    mng = malloc(sizeof(struct leecherMng));

    if(mng == NULL) {
        goto fail;
    }

    memset(mng, 0, sizeof(*mng));


    if(SELECTOR_SUCCESS != selector_register(key->s, leeker, &leechersHandler, OP_READ, mng)) {
        goto fail;
    }

    return;

    fail:
    if(leeker != -1) {
        close(leeker);
    }
    if(mng != NULL) {
        free(mng);
    }
}

static void quit(struct selector_key *key) {
    send(key->fd, CLOSE_MSG, strlen(CLOSE_MSG), 0);
    free(LEECH(key));
    close(key->fd);
    selector_unregister_fd(key->s, key->fd);
}


void leecherRead(struct selector_key *key) {

    memset(LEECH(key)->requestBuffer, 0, REQUEST_BUFFER_SIZE);

    ssize_t bytes = recv(key->fd, LEECH(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0);

    if(bytes == 0) {
        quit(key);
        return;
    }

    //HASH:BYTE_FROM:BYTE_TO
    char hash[256];
    char tempByteFrom[256];
    char tempByteTo[256];

    // Split the received string
    char *token = strtok(LEECH(key)->requestBuffer, ":");
    if (token == NULL)
        goto error;
    strcpy(hash, token);

    token = strtok(NULL, ":");
    if (token == NULL)
        goto error;
    strcpy(tempByteFrom, token);

    token = strtok(NULL, ":");
    if (token == NULL)
        goto error;
    strcpy(tempByteTo, token);

    if (strtok(NULL, ":") != NULL)
        goto error;

    // Validate and convert byteFrom and byteTo
    //if (!isValidNumber(tempByteFrom) || !isValidNumber(tempByteTo))
    //    goto error;

    size_t byteFrom = atol(tempByteFrom);
    size_t byteTo = atol(tempByteTo);

    if (byteTo< byteFrom)
        goto error;
    size_t size = byteTo - byteFrom;


    LEECH(key)->responseBuffer = malloc(size + 1);
    int statusCode;
    size_t bytesRead=copyFromFile(LEECH(key)->responseBuffer, hash, byteFrom, size,&statusCode);
    if(statusCode!=FILE_SUCCESS){
        perror("Error reading file");
        goto error;
    }

    ssize_t sent_bytes = send(key->fd, LEECH(key)->responseBuffer, bytesRead, 0);
    if (sent_bytes <= 0) {
        perror("Failed to send response\n");
        goto error;
    }

    free(LEECH(key)->responseBuffer);
    return;

    error:
    quit(key);
    printf("Roto todo\n");
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

struct peerMng *initializePeerMng() {
    struct peerMng *peer = malloc(sizeof(struct peerMng));
    if (peer == NULL) {
        perror("Failed to allocate memory for peerMng");
        return NULL;
    }

    peer->writeReady = false;
    peer->readReady = false;
    memset(peer->responseBuffer,0,sizeof(peer->responseBuffer));
    peer->killFlag = false;

    if (pthread_mutex_init(&peer->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(peer);
        return NULL;
    }

    return peer;
}

void cleanupPeerMng(struct peerMng *peer) {
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

    return sock;

}

struct peerMng * addPeer(struct selector_key *key, char *ip, char *port) {
    int socket = 0;
    socket = setupPeerSocket(ip, port);

    struct peerMng * mng = NULL;

    mng = initializePeerMng();

    if(mng == NULL)
        goto fail;

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
        selector_unregister_fd(key->s, key->fd);
        free(PEER(key));
        return;
    }

    ssize_t bytes = recv(key->fd, PEER(key)->responseBuffer, CHUNKSIZE, 0);

    if (bytes > 0) {
        PEER(key)->readReady = true;
        addBytesRead(bytes);
    } else {
        PEER(key)->killFlag = true;
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    pthread_mutex_unlock(&PEER(key)->mutex);
    selector_set_interest(key->s, key->fd, OP_WRITE);
}

void peerWrite(struct selector_key *key) {
    pthread_mutex_lock(&PEER(key)->mutex);

    if(PEER(key)->killFlag) {
        selector_unregister_fd(key->s, key->fd);
        free(PEER(key));
        return;
    }

    if (!PEER(key)->writeReady) {
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    ssize_t bytes = send(key->fd, PEER(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0);

    if (bytes <= 0) {
        PEER(key)->killFlag = true;
        pthread_mutex_unlock(&PEER(key)->mutex);
        return;
    }

    memset(PEER(key)->requestBuffer, '\0', REQUEST_BUFFER_SIZE);

    PEER(key)->writeReady = false;

    pthread_mutex_unlock(&PEER(key)->mutex);
    selector_set_interest(key->s, key->fd, OP_READ);
}

int requestFromPeer(struct peerMng * peer, char *hash, size_t byteFrom, size_t byteTo) {
    if (byteTo <= byteFrom + 1) {
        return -1;
    }
    pthread_mutex_lock(&peer->mutex);

    if(peer->readReady) {
        pthread_mutex_unlock(&peer->mutex);
        return -1;
    }

    snprintf(peer->requestBuffer, REQUEST_BUFFER_SIZE, "%s:%lu:%lu", hash, byteFrom, byteTo);
    memset(peer->responseBuffer, 0, sizeof(peer->responseBuffer));

    peer->writeReady = true;
    pthread_mutex_unlock(&peer->mutex);
    return 0;
}

int readFromPeer(struct peerMng * peer, char buff[CHUNKSIZE]) {
    pthread_mutex_lock(&peer->mutex);
    if(peer->readReady) {
        peer->readReady = false;
        memcpy(buff, peer->responseBuffer, CHUNKSIZE);
        pthread_mutex_unlock(&peer->mutex);
        return 0;
    }
    pthread_mutex_unlock(&peer->mutex);
    return -1;
}
