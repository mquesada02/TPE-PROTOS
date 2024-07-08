#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "../include/utils.h"

#define SUCCESS 1
#define ERROR -1

#define MAX_FILENAME 256

#define MAX_ADDR_BUFFER 128
#define MAXPENDING 32

static char addrBuffer[MAX_ADDR_BUFFER];

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

int setupLeekerSocket   (const char *service, const char **errmsg) {
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
        return -1;
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

bool calculateMD5(char* filename, char md5Buffer[MD5_SIZE + 1]) {
    FILE *file = NULL;

    char buff[strlen("md5sum ") + MAX_FILENAME];
    memset(buff,0,strlen("md5sum ") + MAX_FILENAME);
    strcat(buff, "md5sum ");
    strcat(buff, filename);

    file = popen(buff,"r");
    md5Buffer[MD5_SIZE] = '\0';
    fgets(md5Buffer,MD5_SIZE+1,file);

    pclose(file);

    return true;
}
