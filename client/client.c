#include "include/args.h"
#include "include/selector.h"
#include "include/utils.h"
#include "include/connectionManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

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

    struct tracker_args args;

    args.trackerSocksPort = port;

    parse_args(argc, argv, &args);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    /*
    char trackerPortStr[6];
    sprintf(trackerPortStr, "%d",args.trackerSocksPort);
    int trackerSocket = setupTrackerSocket(trackerPortStr, &err_msg);
    if (trackerSocket < 0) goto finally;
    */

    char peerPortStr[6];
    sprintf(peerPortStr, "%d",args.peerSocksPort);
    int leekerSocket = setupLeekerSocket(peerPortStr, &err_msg);
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

    /*const struct fd_handler tracker = {
            .handle_read = tracker_handler,
            .handle_write = NULL,
            .handle_close = NULL,
    };*/

    const struct fd_handler leeker = {
            .handle_read = leekerHandler,
            .handle_write = NULL,
            .handle_close = NULL,
    };

    /*ss = selector_register(selector, trackerSocket, &tracker, OP_READ, NULL);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Unable to register FD for IPv4/IPv6.";
        goto finally;
    }*/

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
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg, ss == SELECTOR_IO ? strerror(errno): selector_error(ss));
    } else if(err_msg) {
        perror(err_msg);
    }
    if (selector != NULL)
        selector_destroy(selector);
    selector_close();
    /*
    if (trackerSocket >= 0)
        close(trackerSocket);
    */
    if (leekerSocket >= 0)
        close(leekerSocket);
    return 0;
}

