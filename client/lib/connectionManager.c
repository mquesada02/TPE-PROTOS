#include <string.h>  // memset
#include <unistd.h>  // close
#include <stdlib.h>

#include <sys/socket.h>
#include <stdio.h>
#include <ctype.h>

#include "../include/selector.h"

#define REQUEST_BUFFER_SIZE 256

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

void leekerRead(struct selector_key *key) {

    memset(ATTACHMENT(key)->requestBuffer, 0, REQUEST_BUFFER_SIZE);

    ssize_t bytes = recv(key->fd, ATTACHMENT(key)->requestBuffer, REQUEST_BUFFER_SIZE, 0);

    if(bytes == 0) {
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

    if (size <= 0)
        goto error;

    ATTACHMENT(key)->responseBuffer = malloc(size + 1);

    //getPacket(ATTACHMENT(key)->responseBuffer, hash, byteFrom, size);
    //send(...)

    printf("%s, %d, %d\n", hash, byteFrom, size);

    free(ATTACHMENT(key)->responseBuffer);
    return;

    error:
    //TODO limpiar todo si falla
    printf("Roto todo\n");
}