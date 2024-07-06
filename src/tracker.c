#include "include/tracker.h"
#include <openssl/md5.h>


#define MAX_REQUESTS 20
#define INITIAL_SELECTOR 1024
#define MD5_SIZE 32
#define MAX_USERNAME_SIZE 32;
#define MAX_STRING_LENGTH 512


typedef struct UserState {
  char* username;
  char* ip;
  char* port;
  UserNode * next;
} UserState;

struct TrackerState {
  UserNode * first;
};

typedef struct UserNode{
    char* username;
    struct userNode* next;
} UserNode;

typedef struct File{
    char* name;
    int size; //bytes
    char MD5[MD5_SIZE];
    UserNode * seeders;
    UserNode * leechers;
} File;

typedef struct FileList{
    File* file;
    struct FileList* next;
} FileList;

static bool done = false;
FILE * users = NULL;

static void sigterm_handler(const int signal) {
    printf("Signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int main(int argc,char ** argv){
    unsigned int port = 15555;

    struct tracker_args args;

    args.socks_port = port;

    parse_args(argc, argv, &args);

    close(0);

    users = fopen("auth/users.csv", "a+");
    int c = fgetc(users);
    if (c == EOF)
      fputs("Username,Password\n",users);
    else
      ungetc(c, users);
    
    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    char portStr[6];
    sprintf(portStr, "%d",args.socks_port);
    int socket = setupUDPServerSocket(portStr,&err_msg);
    if (socket < 0) goto finally;

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

    const struct fd_handler tracker = {
      .handle_read = tracker_handler,
      .handle_write = NULL,
      .handle_close = NULL,
    };

    ss = selector_register(selector,socket,&tracker,OP_READ,NULL);
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
  if (socket >= 0)
    close(socket);
  fclose(users);
  return 0;
}

void handleCmd(char * cmd) {
  if (strcmp(cmd, "PLAIN") == 0) {
    char * user = strtok(NULL, ":");
    char * password = strtok(NULL, "\n");
    if (loginUser(user, password)) {
    }
  }
  
}

void tracker_handler(struct selector_key * key) {
  struct sockaddr_storage client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char buffer[MAX_STRING_LENGTH];
  ssize_t bytesRecv = recvfrom(key->fd, buffer, MAX_STRING_LENGTH, 0, (struct sockaddr *) &client_addr, &client_addr_len);
  if (bytesRecv < 0) return;
  
  handleCmd(strtok(buffer, " "));
  //sendto(key->fd, buffer, bytesRecv, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
}

char * getUser(char* username, char* usersS) {
  int len = strlen(username);
  char usernameComma[len+3];
  usernameComma[0] = '\n';
  strcat(usernameComma, username);
  usernameComma[len+1] = ',';
  usernameComma[len+2] = '\0';
  return strstr(usersS, usernameComma);
}

bool loginUser(char * username, char * password) {
  if (users == NULL) return false;
  fseek(users, 0L, SEEK_END);
  int usersStrLen = ftell(users);
  rewind(users);
  char * usersStr = malloc(usersStrLen*sizeof(char));
  fread(usersStr, sizeof(char), usersStrLen, users);
  char* user = getUser(username, usersStr);
  if (user == NULL) {
    registerUser(username, password);
    return true;
  } else {
    strtok(user, ",");
    char * passwordStr = strtok(NULL, "\n");
    return strcmp(password, passwordStr) == 0;
  }
  return false;
}

void registerUser(char * username, char * password) {
  if (users == NULL) return;
  char buffer[MAX_STRING_LENGTH];
  sprintf(buffer, "%s,%s\n",username,password);
  fputs(buffer, users);
  fflush(users);
}

bool calculateMD5(char* filename, char md5Buffer[MD5_SIZE + 1]) {
    // FILE * stream = NULL;
    // char * fileName = NULL;
    // char outBuffer[BUFFER_MAX_SIZE];
    // int len = 0;
    // char cmd[MAX_FILE_LEN] = "md5sum ";
    // size_t size;
    // while ((len = getline(&fileName, &size, stdin)) != EOF) {

    //     fileName[len-1] = '\0';
        
    //     char buff[BUFFER_MAX_SIZE] = "";
    //     strcat(buff,cmd);
    //     strcat(buff, fileName);
    //     strcat(buff," | cut -b -32");
        
    //     stream = popen(buff,"r");   
    //     md5Buffer[MD5_LEN] = '\0';
    //     fgets(md5Buffer,MD5_LEN+1,stream);
        
    //     pclose(stream);

    //     len = sprintf(outBuffer,"%s - %s - %d\n",fileName,md5Buffer,getpid());
    //     write(STDOUT_FILENO, outBuffer,len);
    //     free(fileName);
    //     fileName = NULL;
    // }
    // free(fileName);
    return true;
}