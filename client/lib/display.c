#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/selector.h"


void handleInput(struct selector_key *key) {
    char buff[256];

    recv(key->fd, buff, 256, 0);
}