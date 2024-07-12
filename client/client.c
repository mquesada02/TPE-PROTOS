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
#include <pthread.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <sys/stat.h>

#define REPO_PATH "../repository"
#define PATH_SIZE 1024
#define DT_REG 8 //redefinido pues no es reconocido por alguna razon

#define INT_LENGTH 10

#define QUANTUM 5

pthread_mutex_t sendingMutex;

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

void* registerFiles(void *vkey) {
    struct Tracker * tracker = (struct Tracker *) vkey;
    while(!done) {
        struct dirent* dirnt;
        DIR* dir;
        char pathname[PATH_SIZE];
        if((dir=opendir(REPO_PATH))==NULL){
            return NULL;
        }
        while((dirnt=readdir(dir))){
            if(dirnt->d_type == DT_REG ){
                char md5Buffer[MD5_SIZE+1];
                sprintf(pathname,"%s/%s",REPO_PATH,dirnt->d_name);
                calculateMD5(pathname,md5Buffer);
                int length = MD5_SIZE+strlen("REGISTER")+strlen(dirnt->d_name)+INT_LENGTH+10;
                char buffer[length];
                struct stat st;
                stat(pathname, &st);
                int size = st.st_size;
                length = sprintf(buffer, "REGISTER %s %d %s\n", dirnt->d_name, size, md5Buffer);
                pthread_mutex_lock(&sendingMutex);
                if (sendto(tracker->socket, buffer, length, 0, (struct sockaddr*)tracker->trackerAddr, sizeof(struct sockaddr_in)) <= 0)
                    printf("Error\n");
                pthread_mutex_unlock(&sendingMutex);
                //recvfrom(tracker->socket, buffer, MAX_STRING_LENGTH, 0, (struct sockaddr *)tracker->trackerAddr, (socklen_t *) sizeof(struct sockaddr_in));
            }
        }
        closedir(dir);
        sleep(QUANTUM);
    }
    return NULL;
}

int main(int argc,char ** argv){
    unsigned int port = 15555;

    struct clientArgs args;
    args.conectionLimit = 0; //indica que no tiene limite
    args.trackerSocksPort = port;

    parse_args(argc, argv, &args);

    openlog("client-application",LOG_PID | LOG_NDELAY ,LOG_LOCAL1);
    syslog(LOG_NOTICE,"Client application started");


    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    struct stat st;

    if(stat("../downloads",&st)!=0){
       if(mkdir("../downloads",0777)!=0){
         err_msg="error creating downloads directory";
         goto no_mutex;
       }
    }

    if(stat("../repository",&st)!=0){
        if(mkdir("../repository",0777)!=0){
         err_msg="error creating repository directory";
         goto no_mutex;
       }
    }


    if(initializeFileManager()<0){
        err_msg="Failed to initialize File Manager";
        goto finally;
    }


    char trackerPortStr[6];
    sprintf(trackerPortStr, "%d",args.trackerSocksPort);

    char peerPortStr[6];
    sprintf(peerPortStr, "%d",args.leecherSocksPort);
    int leekerSocket = setupLeecherSocket(peerPortStr, &err_msg);
    if (leekerSocket < 0) goto no_mutex;

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            }
    };

    if (pthread_mutex_init(&sendingMutex, NULL) != 0) {
        err_msg="Failed to initialize mutex";
        goto no_mutex;
    }

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

    tracker->leecherSocket = args.leecherSocksPort;
    ss = selector_register(selector, STDIN_FILENO, &input, OP_READ, tracker);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Unable to register FD for IPv4/IPv6.";
        goto finally;
    }

    ss = selector_register(selector, leekerSocket, &leeker, OP_READ, &args);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Unable to register FD for IPv4/IPv6.";
        goto finally;
    }

    pthread_t tid;

    pthread_create(&tid, NULL, handleDownload, NULL);
    pthread_detach(tid);

    pthread_t updateTID;

    pthread_create(&updateTID, NULL, registerFiles, (void*) tracker);
    pthread_detach(updateTID);

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
    pthread_mutex_lock(&sendingMutex);
    //pthread_cancel(updateTID);
    if (tracker->socket >= 0) {
        sendto(tracker->socket, "QUIT\n", strlen("QUIT\n"),0,(struct sockaddr*)tracker->trackerAddr, sizeof(struct sockaddr_in));
    }
    pthread_mutex_unlock(&sendingMutex);
    pthread_mutex_destroy(&sendingMutex);
    no_mutex:
    cancelDownload();
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
    cleanUpPeers();
    if (tracker != NULL) {
        close(tracker->socket);
        free(tracker->trackerAddr);
        free(tracker);
    }
    if (leekerSocket >= 0)
        close(leekerSocket);

    return 0;
}

