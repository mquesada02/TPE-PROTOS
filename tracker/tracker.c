#include "include/tracker.h"
#include <openssl/md5.h>


#define MAX_REQUESTS 20
#define INITIAL_SELECTOR 1024
#define MD5_SIZE 32
#define MAX_USERNAME_SIZE 32
#define MAX_STRING_LENGTH 512

typedef struct UserNode{
    char username[MAX_USERNAME_SIZE];
    struct UserNode* next;
} UserNode;

typedef struct UserState {
  char username[MAX_USERNAME_SIZE];
  char ip[16];
  char port[6];
  struct UserState * next;
} UserState;

struct TrackerState {
  UserState * first;
};

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

struct TrackerState * state = NULL;
FileList * fileList = NULL;

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

UserState * _insertUser(UserState * node, UserState value) {
  if (node == NULL) {
    UserState * newNode = malloc(sizeof(UserState));
    strcpy(newNode->username, value.username);
    strcpy(newNode->ip, value.ip);
    strcpy(newNode->port, value.port);
    newNode->next = value.next;
    return newNode;
  }
  node->next = _insertUser(node->next, value);
  return node;
}

void insertUser(UserState value) {
  state->first = _insertUser(state->first, value);
}

char * _getUsernameFromIpNPort(UserState * user, char * ip, char * port) {
  if (user == NULL)
    return NULL;
  if (strcmp(user->ip, ip) == 0 && strcmp(user->port, port) == 0)
    return user->username;
  return _getUsernameFromIpNPort(user->next, ip, port);
}

char * getUsernameFromIpNPort(char * ip, char * port) {
  if (state == NULL)
    return NULL;
  return _getUsernameFromIpNPort(state->first, ip, port);
}

UserNode * _insertSeeder(UserNode * node, char * username) {
  int cmp;
  if (node == NULL || (cmp = strcmp(username, node->username)) > 0) {
    // reached the end of the list or passed. Insert new node
    UserNode * newNode = malloc(sizeof(UserNode));
    strcpy(newNode->username, username);
    newNode->next = node;
    return newNode;
  }
  if (cmp < 0) {
    node->next = _insertSeeder(node->next, username);
  }
  return node;
}

UserNode * insertSeeder(UserNode * first, char * ip, char * port) {
  char * username = getUsernameFromIpNPort(ip, port);
  return _insertSeeder(first, username);
}

FileList * _registerFile(FileList * node, char * name, char * bytes, char * hash, char * ip, char * port, int fd, struct sockaddr_storage client_addr) {
  int cmp;
  if (node == NULL || (cmp = strcmp(hash,node->file->MD5)) > 0) {
    // reached the end of the list or passed. Insert new node
    FileList * newNode = malloc(sizeof(FileList));
    newNode->file = malloc(sizeof(File));
    newNode->next = node;
    newNode->file->name = name;
    newNode->file->size = atoi(bytes);
    newNode->file->seeders = NULL;
    newNode->file->leechers = NULL;
    strcpy(newNode->file->MD5, hash);
    newNode->file->seeders = insertSeeder(newNode->file->seeders, ip, port);
    sendto(fd, "You are the first to register this file\n", strlen("You are the first to register this file\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    return newNode;
  }
  if (cmp == 0) {
    // found the same element. Add a seeder
    node->file->seeders = insertSeeder(node->file->seeders, ip, port);
    sendto(fd, "File registed successfully\n", strlen("File registed successfully\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
  }
  if (cmp < 0) {
    // still searching
    node->next = _registerFile(node->next, name, bytes, hash, ip, port, fd, client_addr);
  }
  return node;
}

void registerFile(char * name, char * bytes, char * hash, char * ip, char * port, int fd, struct sockaddr_storage client_addr) {
  fileList = _registerFile(fileList, name, bytes, hash, ip, port, fd, client_addr);
}

void _getIpNPortFromUsername(UserState * userState, char * username, char * ip, char * port) {
  if (userState == NULL )
    return; // not found
  if (strcmp(username, userState->username) == 0) {
    strcpy(ip, userState->ip);
    strcpy(port, userState->port);
    return;
  }
  _getIpNPortFromUsername(userState->next, username, ip, port);
}

void getIpNPortFromUsername(char * username, char * ip, char * port) {
  _getIpNPortFromUsername(state->first, username, ip, port);
}

void sendSeeders(UserNode * seeder, int fd, struct sockaddr_storage client_addr) {
  if (seeder == NULL) {
    return;
  }
  char ip[16] = {0};
  char port[6] = {0};
  getIpNPortFromUsername(seeder->username, ip, port);
  char buffer[MAX_STRING_LENGTH];
  sprintf(buffer, "%s:%s\n",ip,port);
  sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
  sendSeeders(seeder->next, fd, client_addr);
}

void sendPeers(FileList * node, int fd, char * hash, struct sockaddr_storage client_addr) {
  int cmp;
  if (node == NULL || node->file == NULL || (cmp = strcmp(hash, node->file->MD5)) > 0) {
    sendto(fd, "No peers for this file\n", strlen("No peers for this file\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    return;
  }
  if (cmp == 0) {
    sendSeeders(node->file->seeders, fd, client_addr);
    return;
  }
  sendPeers(node->next, fd, hash, client_addr);
}

void handleCmd(char * cmd, char * ipstr, char * portstr, int fd, struct sockaddr_storage client_addr) {
  if (strcmp(cmd, "PLAIN") == 0) { // PLAIN user:password
    char * user = strtok(NULL, ":");
    char * password = strtok(NULL, "\n");
    int loginState = 0;
    if ((loginState = loginUser(user, password))) {
      if (state == NULL)
        state = malloc(sizeof(struct TrackerState));
      UserState node = (UserState) {.next = NULL};
      strcpy(node.username, user);
      strcpy(node.ip, ipstr);
      strcpy(node.port, portstr);
      insertUser(node);
      if (loginState == 1)
        sendto(fd, "Logged in successfully\n", strlen("Logged in successfully\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
      else
        sendto(fd, "Registered successfully\n", strlen("Registered successfully\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    } else {
        sendto(fd, "Incorrect password for user\n", strlen("Incorrect password for user\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    }
  }
  if (strcmp(cmd, "LIST") == 0) { // LIST files
                                  // LIST peers <hash>
    char * arg = strtok(NULL, "\n");
    if (strcmp(arg, "files") == 0) {
      // sendto fd files list
    } else {
      arg = strtok(arg, " ");
      if (strcmp(arg, "peers") == 0) {
        arg = strtok(NULL, "\n");
        // now arg has the hash of the file
        sendPeers(fileList, fd, arg, client_addr);
      }
    }
  }
  if (strcmp(cmd, "REGISTER") == 0) { // REGISTER <name> <bytes> <hash>
    char * name = strtok(NULL, " ");
    char * bytes = strtok(NULL, " ");
    char * hash = strtok(NULL, "\n");
    registerFile(name, bytes, hash, ipstr, portstr, fd, client_addr);
  }
  
}

void tracker_handler(struct selector_key * key) {
  struct sockaddr_storage client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char buffer[MAX_STRING_LENGTH];
  ssize_t bytesRecv = recvfrom(key->fd, buffer, MAX_STRING_LENGTH, 0, (struct sockaddr *) &client_addr, &client_addr_len);
  if (bytesRecv < 0) return;
  char portstr[6] = {0};
  char ipstr[16] = {0};
  getnameinfo((struct sockaddr*)&client_addr, sizeof(struct sockaddr_storage), ipstr, sizeof(ipstr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
  handleCmd(strtok(buffer, " "), ipstr, portstr, key->fd, client_addr);
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

int loginUser(char * username, char * password) {
  if (users == NULL) return false;
  fseek(users, 0L, SEEK_END);
  int usersStrLen = ftell(users);
  rewind(users);
  char * usersStr = malloc(usersStrLen*sizeof(char));
  fread(usersStr, sizeof(char), usersStrLen, users);
  char* user = getUser(username, usersStr);
  if (user == NULL) {
    registerUser(username, password);
    return 2; // true but with a flag
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
 

  return true;
}