#include "include/args.h"
#include "include/selector.h"
#include "include/utils.h"
#include "include/connectionManager.h"
#include "include/user.h"
#include "include/fileManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/udp.h>


#define MAX_REQUESTS 20
#define INITIAL_SELECTOR 1024
#define MD5_SIZE 32
#define MAX_USERNAME_SIZE 32;
#define MAX_STRING_LENGTH 512

static bool done = false;

static void sigterm_handler(const int signal) {
    printf("Signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int main(int argc,char ** argv){
    unsigned int port = 15555;

    struct clientArgs args;
    args.conectionLimit = 0; //indica que no tiene limite 
    args.trackerSocksPort = port;

    parse_args(argc, argv, &args);

    openlog("client-application",LOG_PID | LOG_NDELAY ,LOG_LOCAL1);
    syslog(LOG_NOTICE,"Client application started");

    initializeFileManager();

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;


    char trackerPortStr[6];
    sprintf(trackerPortStr, "%d",args.trackerSocksPort);

    char peerPortStr[6];
    sprintf(peerPortStr, "%d",args.leecherSocksPort);
    int leekerSocket = setupLeecherSocket(peerPortStr, &err_msg);
    if (leekerSocket < 0) goto finally;

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            }
    };

    if (selector_init(&conf) != 0) {
        err_msg = "Unable to initialize selector.";
        goto finally;
    }

    selector = selector_new(INITIAL_SELECTOR);
    if (selector == NULL) {
        err_msg = "Unable to create selector";
        goto finally;
    }

    struct Tracker * tracker = NULL;
    tracker = setupTrackerSocket(args.trackerSocksAddr, trackerPortStr, &err_msg);
    if(tracker == NULL) goto finally;

    const struct fd_handler input = {
            .handle_read = handleInput,
            .handle_write = NULL,
            .handle_close = NULL,
    };

    const struct fd_handler leeker = {
            .handle_read = leecherHandler,
            .handle_write = NULL,
            .handle_close = NULL,
    };

    ss = selector_register(selector, STDIN_FILENO, &input, OP_READ, tracker);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Unable to register FD for IPv4/IPv6.";
        goto finally;
    }

    ss = selector_register(selector, leekerSocket, &leeker, OP_READ, NULL);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Unable to register FD for IPv4/IPv6.";
        goto finally;
    }

    while(!done) {
        err_msg = NULL;
        ss = selector_select(selector);
        if (ss != SELECTOR_SUCCESS) {
            err_msg = "Unable to serve for selector.";
            goto finally;
        }
    }
    if (err_msg == NULL) {
        err_msg = "Closing...";
    }

    finally:
    syslog(LOG_NOTICE,"Closing client application");
    closelog();
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg, ss == SELECTOR_IO ? strerror(errno): selector_error(ss));
    } else if(err_msg) {
        perror(err_msg);
    }
    if (selector != NULL)
        selector_destroy(selector);
    selector_close();
    if (tracker != NULL) {
        close(tracker->socket);
        free(tracker);
    }
    if (leekerSocket >= 0)
        close(leekerSocket);
    //TODO cerrar los sockets de los leekers
    return 0;
}

