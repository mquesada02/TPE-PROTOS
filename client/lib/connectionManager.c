#include <string.h>  // memset
#include <unistd.h>  // close
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/selector.h"
#include "../include/user.h"
#include "../include/fileManager.h"

#define REQUEST_BUFFER_SIZE 256
#define SUCCESS 1
#define ERROR (-1)

#define CLOSE_MSG "Closing"

#define MAX_ADDR_BUFFER 128
#define MAXPENDING 32

static char addrBuffer[MAX_ADDR_BUFFER];

#define ATTACHMENT(key) ( (struct leekerMng *)(key)->data)

void leekerRead(struct selector_key *key);
void leekerWrite(struct selector_key *key);
void leekerClose(struct selector_key *key);
void leekerBlock(struct selector_key *key);

struct leekerMng {

    char requestBuffer[REQUEST_BUFFER_SIZE];

    char * responseBuffer;

};

static const struct fd_handler leekersHandler = {
        .handle_read   = leekerRead,
        .handle_write  = NULL,
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

int setupLeekerSocket(const char *service, const char **errmsg) {
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

void leekerHandler(struct selector_key *key) {

    struct sockaddr_storage leekerAddr;
    socklen_t leekerAddrLen = sizeof(leekerAddr);
    struct leekerMng * mng = NULL;

    const int leeker = accept(key->fd, (struct sockaddr*) &leekerAddr, &leekerAddrLen);
    if(leeker == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(leeker) == -1) {
        goto fail;
    }

    mng = malloc(sizeof(struct leekerMng));

    if(mng == NULL) {
        goto fail;
    }

    memset(mng, 0, sizeof(*mng));


    if(SELECTOR_SUCCESS != selector_register(key->s, leeker, &leekersHandler, OP_READ, mng)) {
        goto fail;
    }

    return;
    fail:
    if(leeker != -1) {
        close(leeker);
    }
}

static void quit(struct selector_key *key) {
    send(key->fd, CLOSE_MSG, strlen(CLOSE_MSG), 0);
    close(key->fd);
    selector_unregister_fd(key->s, key->fd);
}


void leekerRead(struct selector_key *key) {

    memset(ATTACHMENT(key)->requestBuffer, 0, REQUEST_BUFFER_SIZE);

    ssize_t bytes = recv(key->fd, ATTACHMENT(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0);

    if(bytes == 0) {
        quit(key);
        return;
    }

    //HASH:BYTE_FROM:BYTE_TO
    char hash[256];
    char tempByteFrom[256];
    char tempByteTo[256];

    // Split the received string
    char *token = strtok(ATTACHMENT(key)->requestBuffer, ":");
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

    int byteFrom = atoi(tempByteFrom);
    int byteTo = atoi(tempByteTo);

    int size = byteTo - byteFrom;

    if (size <= 1)
        goto error;

    ATTACHMENT(key)->responseBuffer = malloc(size + 1);

    copyFromFile(ATTACHMENT(key)->responseBuffer, hash, byteFrom, size);

    ssize_t sent_bytes = send(key->fd, ATTACHMENT(key)->responseBuffer, size, 0);
    if (sent_bytes <= 0) {
        goto error;
    }

    printf("%s\n", ATTACHMENT(key)->responseBuffer);

    free(ATTACHMENT(key)->responseBuffer);
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