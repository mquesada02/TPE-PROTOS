#include "../include/utils.h"

#define SUCCESS 1
#define ERROR -1

#define MAX_ADDR_BUFFER 128

static char addrBuffer[MAX_ADDR_BUFFER];

int setupUDPServerSocket(const char *service, const char ** errmsg) {
	// Construct the server address structure
	struct addrinfo addrCriteria;                   // Criteria for address
	memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
	addrCriteria.ai_family = AF_UNSPEC;             // Any address family
	addrCriteria.ai_flags =  AI_PASSIVE;             // Accept on any address/port
	addrCriteria.ai_socktype = SOCK_DGRAM;          // Only datagram socket
	addrCriteria.ai_protocol = IPPROTO_UDP;         // Only UDP socket

	struct addrinfo *servAddr; 			// List of server addresses
	int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
	if (rtnVal != 0)
    sprintf(*errmsg, "getaddrinfo() failed %s", gai_strerror(rtnVal));

	int servSock = -1;
	// Intentamos ponernos a escuchar en alguno de los puertos asociados al servicio, sin especificar una IP en particular
	// Iteramos y hacemos el bind por alguna de ellas, la primera que funcione, ya sea la general para IPv4 (0.0.0.0) o IPv6 (::/0) .
	// Con esta implementación estaremos escuchando o bien en IPv4 o en IPv6, pero no en ambas
	for (struct addrinfo *addr = servAddr; addr != NULL && servSock == -1; addr = addr->ai_next) {
		errno = 0;
		// Create UDP socket
		servSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (servSock < 0) {
      sprintf(*errmsg, "Cant't create socket on %s : %s", printAddressPort(addr, addrBuffer), strerror(errno));
			continue;       // Socket creation failed; try next address
		}

		// Bind to ALL the address
		if (bind(servSock, addr->ai_addr, addr->ai_addrlen) == 0 ) {
			// Print local address of socket
			struct sockaddr_storage localAddr;
			socklen_t addrSize = sizeof(localAddr);
			if (getsockname(servSock, (struct sockaddr *) &localAddr, &addrSize) >= 0) {
				printSocketAddress((struct sockaddr *) &localAddr, addrBuffer);
			}
		} else {
      sprintf(*errmsg, "Cant't bind %s", strerror(errno));
			close(servSock);  // Close and try with the next one
			servSock = -1;
		}
	}

	freeaddrinfo(servAddr);

	return servSock;

}

const char * printFamily(struct addrinfo *aip)
{
	switch (aip->ai_family) {
	case AF_INET:
		return "inet";
	case AF_INET6:
		return "inet6";
	case AF_UNIX:
		return "unix";
	case AF_UNSPEC:
		return "unspecified";
	default:
		return "unknown";
	}
}

const char * printType(struct addrinfo *aip)
{
	switch (aip->ai_socktype) {
	case SOCK_STREAM:
		return "stream";
	case SOCK_DGRAM:
		return "datagram";
	case SOCK_SEQPACKET:
		return "seqpacket";
	case SOCK_RAW:
		return "raw";
	default:
		return "unknown ";
	}
}

const char * printProtocol(struct addrinfo *aip)
{
	switch (aip->ai_protocol) {
	case 0:
		return "default";
	case IPPROTO_TCP:
		return "TCP";
	case IPPROTO_UDP:
		return "UDP";
	case IPPROTO_RAW:
		return "raw";
	default:
		return "unknown ";
	}
}

void printFlags(struct addrinfo *aip)
{
	printf("flags");
	if (aip->ai_flags == 0) {
		printf(" 0");
	} else {
		if (aip->ai_flags & AI_PASSIVE)
			printf(" passive");
		if (aip->ai_flags & AI_CANONNAME)
			printf(" canon");
		if (aip->ai_flags & AI_NUMERICHOST)
			printf(" numhost");
		if (aip->ai_flags & AI_NUMERICSERV)
			printf(" numserv");
		if (aip->ai_flags & AI_V4MAPPED)
			printf(" v4mapped");
		if (aip->ai_flags & AI_ALL)
			printf(" all");
	}
}

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

int sockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2) {
	if (addr1 == NULL || addr2 == NULL)
		return addr1 == addr2;
	else if (addr1->sa_family != addr2->sa_family)
		return 0;
	else if (addr1->sa_family == AF_INET) {
		struct sockaddr_in *ipv4Addr1 = (struct sockaddr_in *) addr1;
		struct sockaddr_in *ipv4Addr2 = (struct sockaddr_in *) addr2;
		return ipv4Addr1->sin_addr.s_addr == ipv4Addr2->sin_addr.s_addr && ipv4Addr1->sin_port == ipv4Addr2->sin_port;
	} else if (addr1->sa_family == AF_INET6) {
		struct sockaddr_in6 *ipv6Addr1 = (struct sockaddr_in6 *) addr1;
		struct sockaddr_in6 *ipv6Addr2 = (struct sockaddr_in6 *) addr2;
		return memcmp(&ipv6Addr1->sin6_addr, &ipv6Addr2->sin6_addr, sizeof(struct in6_addr)) == 0 
			&& ipv6Addr1->sin6_port == ipv6Addr2->sin6_port;
	} else
		return 0;
}