#ifndef TRACKER_H
#define TRACKER_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/udp.h>

#include "utils.h"
#include "selector.h"
#include "args.h"

void tracker_handler(struct selector_key * key);

char * getUser(char* username, char* users);

int loginUser(char * username, char * password, int fd, struct sockaddr_storage client_addr);

void registerUser(char * username, char * password);

void freeFileList();

void freeUsers();

#endif