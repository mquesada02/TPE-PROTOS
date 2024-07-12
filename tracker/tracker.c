#include "include/tracker.h"

#include <pthread.h>

#define MAX_REQUESTS 20
#define INITIAL_SELECTOR 1024
#define MD5_SIZE (32+1)
#define MAX_USERNAME_SIZE (32+1)
#define MAX_STRING_LENGTH (512+1)
#define MAX_FILENAME (256+1)
#define INT_LEN 20
#define QUANTUM 20
#define CONF_BUFF_SIZE 32

#define IP_LEN 16
#define PORT_LEN 6


typedef struct UserNode{
    char username[MAX_USERNAME_SIZE];
    struct UserNode* next;
    pthread_t killerTid;
    struct TArg * args;
} UserNode;



typedef struct UserState {
  char username[MAX_USERNAME_SIZE];
  char ip[IP_LEN];
  char port[PORT_LEN];
  char seederPort[PORT_LEN];
  struct UserState * next;
} UserState;

struct TrackerState {
  UserState * first;
};

pthread_mutex_t usersMutex;
pthread_mutex_t filesMutex;
typedef struct File{
    char name[MAX_FILENAME];
    size_t size; //bytes
    char MD5[MD5_SIZE];
    UserNode * seeders;
    size_t seedersSize;
    UserNode * leechers;
} File;

typedef struct TArg {
  File * file;
  UserNode * value;
} TArg;

typedef struct FileList{
    File* file;
    struct FileList* next;
} FileList;

struct TrackerState * state = NULL;
FileList * fileList = NULL;
size_t fileListSize = 0;

static bool done = false;
FILE * users = NULL;

static void sigterm_handler(const int signal) {
    printf("Signal %d, cleaning up and exiting\n",signal);
    done = true;
}

bool lineConfirms(int fd, struct sockaddr_storage client_addr){
	char buff[CONF_BUFF_SIZE];
	socklen_t client_addr_len = sizeof(client_addr);
	recvfrom(fd, buff, CONF_BUFF_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len);
	return (buff[0]=='y' || buff[0]=='Y') && buff[1]=='\n';
}

int main(int argc,char ** argv){
    unsigned int port = 15555;

    struct tracker_args args;

    args.socks_port = port;

    parse_args(argc, argv, &args);

    close(0);

    users = fopen("auth/users.csv", "a+");
    char c;
    if ((c = fgetc(users)) == EOF)
      fputc('\n',users);
    else
      ungetc(c, users);
    
    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    char portStr[6];
    sprintf(portStr, "%d",args.socks_port);
    int socket = setupUDPServerSocket(portStr,&err_msg);
    if (socket < 0) goto no_mutex;

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
    goto no_mutex;
  }

  selector = selector_new(INITIAL_SELECTOR);
  if (selector == NULL) {
    err_msg = "Unable to create selector";
    goto no_mutex;
  }

    const struct fd_handler tracker = {
      .handle_read = tracker_handler,
      .handle_write = NULL,
      .handle_close = NULL,
    };

    ss = selector_register(selector,socket,&tracker,OP_READ,NULL);
  if (ss != SELECTOR_SUCCESS) {
    err_msg = "Unable to register FD for IPv4/IPv6.";
    goto no_mutex;
  }
  if (pthread_mutex_init(&usersMutex, NULL) != 0) {
    perror("Failed to initialize mutex");
    goto no_mutex;
  }
  if (pthread_mutex_init(&filesMutex, NULL) != 0) {
    perror("Failed to initialize mutex");
    goto mutex;
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
  pthread_mutex_destroy(&filesMutex);
  mutex:
  pthread_mutex_destroy(&usersMutex);
  no_mutex:
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
  freeUsers();
  freeFileList();
  return 0;
}

UserState * _insertUser(UserState * node, UserState value) {
  if (node == NULL) {
    UserState * newNode = malloc(sizeof(UserState));
    strncpy(newNode->username, value.username, MAX_USERNAME_SIZE-1);
    newNode->username[MAX_USERNAME_SIZE-1] = '\0';
    strncpy(newNode->ip, value.ip, IP_LEN-1);
    newNode->ip[IP_LEN-1] = '\0';
    strncpy(newNode->port, value.port, PORT_LEN-1);
    newNode->port[PORT_LEN-1] = '\0';
    strncpy(newNode->seederPort, value.port, PORT_LEN-1);
    newNode->seederPort[PORT_LEN-1] = '\0';
    newNode->next = node;
    return newNode;
  }
  node->next = _insertUser(node->next, value);
  return node;
}

void insertUser(UserState value) {
  pthread_mutex_lock(&usersMutex);
  state->first = _insertUser(state->first, value);
  pthread_mutex_unlock(&usersMutex);
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

UserNode * removeSeeder(UserNode * seeder, char * username) {
  if (seeder == NULL)
    return seeder;
  if (strcmp(seeder->username, username) == 0) {
    UserNode * aux = seeder->next;
    free(seeder);
    return aux;
  }
  seeder->next = removeSeeder(seeder->next, username);
  return seeder;
}

FileList * _removeFileSeeder(FileList * node, char * username) {
  if (node == NULL)
    return node;
  node->file->seeders = removeSeeder(node->file->seeders, username);
  node->next = _removeFileSeeder(node->next, username);
  if (node->file->seeders == NULL) {
    FileList * aux = node->next;
    free(node);
    return aux;
  }
  return node;
}

void removeFileSeeder(char * ip, char * port) {
  char * username = getUsernameFromIpNPort(ip, port);
  fileList = _removeFileSeeder(fileList, username);
}

FileList * _removeFile(FileList * file, char * MD5) {
  if (file == NULL) return file;
  if (strcmp(file->file->MD5,MD5) == 0) {
    FileList * aux = file->next;
    if (file->file)
      free(file->file);
    free(file);
    fileListSize--;
    return aux;
  }
  file->next = _removeFile(file->next, MD5);
  return file;
}

void removeFile(char * MD5) {
  fileList = _removeFile(fileList, MD5);
}

void * deleteAfterQuantum(void * arg) {
  TArg * targ = (TArg *) arg;
  sleep(QUANTUM);
  pthread_mutex_lock(&filesMutex);
  targ->file->seeders = removeSeeder(targ->file->seeders, targ->value->username);
  if (targ->file->seeders == NULL) {
    removeFile(targ->file->MD5);
  }
  pthread_mutex_unlock(&filesMutex);
  free(arg);
  return NULL;
}

UserNode * _insertSeeder(File * file, UserNode * node, char * username, bool * inserted) {
  int cmp;
  if (node == NULL || (cmp = strcmp(username, node->username)) > 0) {
    // reached the end of the list or passed. Insert new node
    UserNode * newNode = calloc(1,sizeof(UserNode));
    strncpy(newNode->username, username, MAX_USERNAME_SIZE-1);
    newNode->username[MAX_USERNAME_SIZE-1] = '\0';
    newNode->next = node;
    TArg * arg = malloc(sizeof(TArg));
    newNode->args = arg;
    arg->file = file;
    arg->value = newNode;
    *inserted = true;
    pthread_create(&newNode->killerTid, NULL, deleteAfterQuantum, arg);
    pthread_detach(newNode->killerTid);
    return newNode;
  }
  if (cmp == 0) {
    TArg * arg = malloc(sizeof(TArg));
    arg->file = file;
    arg->value = node;
    pthread_cancel(node->killerTid);
    free(node->args);
    node->args = arg;
    pthread_create(&node->killerTid, NULL, deleteAfterQuantum, arg);
    pthread_detach(node->killerTid);
  }
  if (cmp < 0) {
    node->next = _insertSeeder(file, node->next, username, inserted);
  }
  return node;
}

UserNode * insertSeeder(File * file, UserNode * first, char * ip, char * port, bool * inserted) {
  char * username = getUsernameFromIpNPort(ip, port);
  return _insertSeeder(file, first, username, inserted);
}

FileList * _registerFile(FileList * node, char * name, char * bytes, char * hash, char * ip, char * port, int fd, struct sockaddr_storage client_addr) {
  int cmp;
  if (node == NULL || (cmp = strcmp(hash,node->file->MD5)) > 0) {
    // reached the end of the list or passed. Insert new node
    FileList * newNode = malloc(sizeof(FileList));
    newNode->file = malloc(sizeof(File));
    newNode->next = node;
    strncpy(newNode->file->name, name,MAX_FILENAME-1);
    newNode->file->name[MAX_FILENAME-1] = '\0';
    newNode->file->size = atol(bytes);
    newNode->file->seeders = NULL;
    newNode->file->leechers = NULL;
    strncpy(newNode->file->MD5, hash, MD5_SIZE-1);
    newNode->file->MD5[MD5_SIZE-1] = '\0';
    bool inserted = false;
    newNode->file->seeders = insertSeeder(newNode->file, newNode->file->seeders, ip, port, &inserted);
    if (inserted)
      newNode->file->seedersSize = 1;
    else
      newNode->file->seedersSize = 0;
    fileListSize++;
    //sendto(fd, "You are the first to register this file\n", strlen("You are the first to register this file\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    return newNode;
  }
  if (cmp == 0) {
    // found the same element. Add a seeder
    bool inserted = false;
    node->file->seeders = insertSeeder(node->file, node->file->seeders, ip, port, &inserted);
    if (inserted)
      node->file->seedersSize++;
    //sendto(fd, "File registered successfully\n", strlen("File registered successfully\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
  }
  if (cmp < 0) {
    // still searching
    node->next = _registerFile(node->next, name, bytes, hash, ip, port, fd, client_addr);
  }
  return node;
}

void registerFile(char * name, char * bytes, char * hash, char * ip, char * port, int fd, struct sockaddr_storage client_addr) {
  pthread_mutex_lock(&filesMutex);
  fileList = _registerFile(fileList, name, bytes, hash, ip, port, fd, client_addr);
  pthread_mutex_unlock(&filesMutex);

}

void _getIpNPortFromUsername(UserState * userState, char * username, char * ip, char * port) {
  if (userState == NULL )
    return; // not found
  if (strcmp(username, userState->username) == 0) {
    strncpy(ip, userState->ip, IP_LEN-1);
    ip[IP_LEN-1] = '\0';
    strncpy(port, userState->seederPort, PORT_LEN-1);
    port[PORT_LEN-1] = '\0';
    return;
  }
  _getIpNPortFromUsername(userState->next, username, ip, port);
}

void getIpNPortFromUsername(char * username, char * ip, char * port) {
  _getIpNPortFromUsername(state->first, username, ip, port);
}

char * name(char * md5){
	FileList * fl = fileList;
	while(fl!=NULL && strcmp(fl->file->MD5, md5)!=0) fl=fl->next;
	if(fl==NULL) return NULL;
	return fl->file->name;
}

void sendSeeders(UserNode * seeder, int fd, struct sockaddr_storage client_addr) {
  if (seeder == NULL) {
    return;
  }
  char ip[IP_LEN] = {0};
  char port[PORT_LEN] = {0};
  getIpNPortFromUsername(seeder->username, ip, port);
  char buffer[MAX_STRING_LENGTH];
  sprintf(buffer, "%s:%s\n",ip,port);
  sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
  sendSeeders(seeder->next, fd, client_addr);
}

void sendSeedersSize(size_t size, int fd, struct sockaddr_storage client_addr) {
  char buffer[INT_LEN];
  sprintf(buffer,"%ld\n",size);
  sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
}

void sendPeers(FileList * node, int fd, char * hash, struct sockaddr_storage client_addr) {
  int cmp;
  if (node == NULL || node->file == NULL || (cmp = strcmp(hash, node->file->MD5)) > 0) {
    sendto(fd, "No peers for this file\n", strlen("No peers for this file\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    return;
  }
  if (cmp == 0) {
    sendSeedersSize(node->file->seedersSize, fd, client_addr);
    sendSeeders(node->file->seeders, fd, client_addr);
    return;
  }
  sendPeers(node->next, fd, hash, client_addr);
}

void sendFiles(int fd, FileList* fileList, struct sockaddr_storage client_addr) {
  if(fileList == NULL) {
    sendto(fd, "No more files found\n", strlen("No more files found\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    return;
  }

  char buff[strlen(fileList->file->name) + INT_LEN + MD5_SIZE + 8];
  int len = sprintf(buff, "%s - %lu - %s\n", fileList->file->name, fileList->file->size, fileList->file->MD5);

  sendto(fd, buff, len, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));

  sendFiles(fd, fileList->next, client_addr);
}

bool hasSeeder(UserNode * node, char * username) {
  if (node == NULL)
    return false;
  if (strcmp(node->username, username) == 0)
    return true;
  return hasSeeder(node->next, username);
}

bool _checkIpNPort(FileList * node, char * hash, char * ip, char * port) {
  if (node == NULL || node->file == NULL)
    return false;
  if (strcmp(node->file->MD5,hash) == 0) {
    char * username = getUsernameFromIpNPort(ip, port);
    return hasSeeder(node->file->seeders, username);
  }
  return _checkIpNPort(node->next, hash, ip, port);
}

bool checkIpNPort(char * hash, char * ip, char * port) {
  return _checkIpNPort(fileList, hash, ip, port);
}


UserState * _removeLoggedUser(UserState * node, char * ip, char * port) {
  if (node == NULL)
    return node;
  if (strcmp(node->ip, ip) == 0 && strcmp(node->port, port) == 0) {
    UserState * aux = node->next;
    free(node);
    return aux;
  }
  node->next = _removeLoggedUser(node->next, ip, port);
  return node;
}

void removeLoggedUser(char * ip, char * port) {
  if (state == NULL) return;
  state->first = _removeLoggedUser(state->first, ip, port);
}

bool userFind(UserState * user, char * ip, char * port) {
  if (user == NULL) return false;
  if (strcmp(user->ip, ip) == 0 && strcmp(user->port, port) == 0) {
    return true;
  }
  return userFind(user->next, ip, port);
}

void _freeUsers(UserState * node) {
  if(node == NULL) return;
  _freeUsers(node->next);
  free(node);
}

void freeUsers() {
  if (state == NULL) return;
  _freeUsers(state->first);
}

void freeUserNode(UserNode * node) {
  if(node == NULL) return;
  freeUserNode(node->next);
  free(node);
}

void _freeFileList(FileList * node) {
  if (node == NULL) return;
  if (node->file) {
    freeUserNode(node->file->seeders);
    freeUserNode(node->file->leechers);
    free(node->file);
  }
  _freeFileList(node->next);
  free(node);
}

void freeFileList() {
  _freeFileList(fileList);
}

UserNode * _addLeecher(UserNode * node, char * username) {
  int cmp;
  if (node == NULL || (cmp = strcmp(username, node->username)) > 0) {
    // reached the end of the list or passed. Insert new node
    UserNode * newNode = malloc(sizeof(UserNode));
    strncpy(newNode->username, username, MAX_USERNAME_SIZE-1);
    newNode->username[MAX_USERNAME_SIZE-1] = '\0';
    newNode->next = node;
    return newNode;
  }
  if (cmp < 0) {
    node->next = _addLeecher(node->next, username);
  }
  return node;
}

void findNAddLeecher(FileList * node, char * hash, char * username) {
  if (fileList == NULL || fileList->file == NULL) return;
  if (strcmp(fileList->file->MD5, hash) == 0) {
    fileList->file->leechers = _addLeecher(fileList->file->leechers, username);
  }
}

void addLeecher(char * hash, char * ip, char * port) {
  pthread_mutex_lock(&usersMutex);
  pthread_mutex_lock(&filesMutex);
  char * username = getUsernameFromIpNPort(ip, port);
  findNAddLeecher(fileList, hash, username);
  pthread_mutex_unlock(&filesMutex);
  pthread_mutex_unlock(&usersMutex);
}

bool userIsLoggedIn(char * ip, char * port) {
  if (state == NULL) return false;
  return userFind(state->first, ip, port);
}

void registerUser(char * username, char * password) {
  if (users == NULL) return;
  char buffer[MAX_STRING_LENGTH];
  sprintf(buffer, "%s,%s\n",username,password);
  fputs(buffer, users);
  fflush(users);
}

void sendFileAmount(int fd, struct sockaddr_storage client_addr) {
  char buffer[INT_LEN];
  sprintf(buffer,"%ld\n",fileListSize);
  sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
}

void getNReturnSize(FileList * node, char * MD5, int fd, struct sockaddr_storage client_addr) {
  if (node == NULL || node->file == NULL) {
    sendto(fd, "0\n", strlen("0\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    return;
  }
  if (strcmp(node->file->MD5, MD5) == 0) {
    char buff[INT_LEN];
    sprintf(buff, "%lu\n", node->file->size);
    sendto(fd, buff, strlen(buff), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    return;
  }
  getNReturnSize(node->next, MD5,fd,client_addr);
}

void getSize(char * hash, int fd, struct sockaddr_storage client_addr) {
  getNReturnSize(fileList,hash, fd, client_addr);
}

void _setSeederPort(UserState * user, char * ipstr, char * portstr, char * newPort) {
  if (user == NULL) return;
  if (strcmp(user->ip,ipstr) == 0 && strcmp(user->port,portstr) == 0) {
    strcpy(user->seederPort,newPort);
    return;
  }
  _setSeederPort(user->next, ipstr, portstr, newPort);
}

void setSeederPort(char * ipstr, char * portstr, char * newPort) {
  _setSeederPort(state->first, ipstr, portstr, newPort);
}

void handleCmd(char * cmd, char * ipstr, char * portstr, int fd, struct sockaddr_storage client_addr) {
  if (userIsLoggedIn(ipstr, portstr)) {
    if (strcmp(cmd, "PLAIN") == 0) {
        sendto(fd, "Already logged in\n", strlen("Already logged in\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    } else
    if (strcmp(cmd, "LIST") == 0) { // LIST files
                                  // LIST peers <hash>
      char * arg = strtok(NULL, "\n");
      if (strcmp(arg, "files") == 0) {
        pthread_mutex_lock(&filesMutex);
        sendFileAmount(fd, client_addr);
        sendFiles(fd, fileList, client_addr);
        pthread_mutex_unlock(&filesMutex);
      } else {
        arg = strtok(arg, " ");
        if (strcmp(arg, "peers") == 0) {
          arg = strtok(NULL, "\n");
          // now arg has the hash of the file
          pthread_mutex_lock(&filesMutex);
          pthread_mutex_lock(&usersMutex);
          sendPeers(fileList, fd, arg, client_addr);
          pthread_mutex_unlock(&usersMutex);
          pthread_mutex_unlock(&filesMutex);
        }
      }
    } 
    if (strcmp(cmd, "REGISTER") == 0) { // REGISTER <name> <bytes> <hash>
      char * name = strtok(NULL, " ");
      char * bytes = strtok(NULL, " ");
      char * hash = strtok(NULL, "\n");
	    registerFile(name, bytes, hash, ipstr, portstr, fd, client_addr);
    } else 
    if (strcmp(cmd, "CHECK") == 0) {
      char * ip = strtok(NULL, ":");
      char * port = strtok(NULL, " ");
      char * hash = strtok(NULL, "\n");
      if (ip == NULL || port == NULL || hash == NULL) return;
      if (checkIpNPort(hash, ip, port)) {
        sendto(fd, "User and file are available\n", strlen("User and file are available\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
        addLeecher(hash, ipstr, portstr);
      } else
        sendto(fd, "User or file is unavailable\n", strlen("User or file is unavailable\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    } else
    if (strcmp(cmd, "CHANGEPASSWORD") == 0){ //CHANGEPASSWORD oldPassword newPassword
      char * oldPassword = strtok(NULL, " ");
      char * newPassword = strtok(NULL, "\n");

      if(strcmp(oldPassword, newPassword)==0){
        sendto(fd, "New password cannot be the same as old password\n", strlen("New password cannot be the same as old password\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
      } else {
        fseek(users, 0L, SEEK_END);
        int usersStrLen = ftell(users);
        rewind(users);
        char usersStr[usersStrLen+1];
        size_t size = fread(usersStr, sizeof(char), usersStrLen, users);
        usersStr[size] = '\0';
        char usersCpy[usersStrLen+1];
        strncpy(usersCpy, usersStr, usersStrLen);
        usersCpy[usersStrLen] = '\0';
        char * username = getUsernameFromIpNPort(ipstr, portstr);
        char * user = getUser(username, usersStr);

        strtok(user, ",");
        char * passwordStr = strtok(NULL, "\n");

        if(strcmp(passwordStr, oldPassword) != 0){
          sendto(fd, "Incorrect password\n", strlen("Incorrect password\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
        } else {
          FILE * newCSV = fopen("auth/temp.csv", "a+");
          char * bufferToStoreUsernameAndPasswordTemporarilyToAddToNewCsvFile;
          char bufferToStoreUsernameAndPasswordFromUser[MAX_STRING_LENGTH];
          sprintf(bufferToStoreUsernameAndPasswordFromUser, "%s,%s", username, oldPassword);
          bufferToStoreUsernameAndPasswordTemporarilyToAddToNewCsvFile = strtok(usersCpy, "\n");
          while (bufferToStoreUsernameAndPasswordTemporarilyToAddToNewCsvFile != NULL) {
              if (strcmp(bufferToStoreUsernameAndPasswordTemporarilyToAddToNewCsvFile, bufferToStoreUsernameAndPasswordFromUser) != 0) {
                  fputs(bufferToStoreUsernameAndPasswordTemporarilyToAddToNewCsvFile, newCSV);
                  fputc('\n', newCSV);
              }
              bufferToStoreUsernameAndPasswordTemporarilyToAddToNewCsvFile = strtok(NULL, "\n");
          }
          fflush(newCSV);
          remove("auth/users.csv");
          fclose(users);
          fclose(newCSV);
          rename("auth/temp.csv", "auth/users.csv");
          users = fopen("auth/users.csv", "a+");

          registerUser(username, newPassword);

          sendto(fd, "Password changed successfully\n", strlen("Password changed successfully\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
        }
      }

    } else if (strcmp(cmd, "SIZE") == 0) {
      char * hash = strtok(NULL, "\n");
      getSize(hash, fd, client_addr);
    } else if (strcmp(cmd, "SETPORT") == 0) {
      char * newPort = strtok(NULL, "\n");
      setSeederPort(ipstr, portstr, newPort);
    }
	else if (strcmp(cmd, "NAME") == 0){
      char * hash = strtok(NULL, "\n");
	  char * nameStr = name(hash);
	  if(nameStr == NULL){
		sendto(fd, "No file with hash specified\n", strlen("No file with hash specified\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
		return;
	  }
	  int bufferSize = strlen(nameStr)+1;
      char buffer[bufferSize+1];
	  sprintf(buffer, "%s\n", nameStr);
      sendto(fd, buffer, bufferSize, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
	}
  } else {
    if (strcmp(cmd, "PLAIN") == 0) { // PLAIN user:password
      char * user = strtok(NULL, ":");
      char * password = strtok(NULL, "\n");
      int loginState = loginUser(user, password, fd, client_addr);
      if (loginState > 0 && loginState < 3) {
        if (state == NULL) 
          state = calloc(1,sizeof(struct TrackerState));
        UserState node = (UserState) {.next = NULL };
        strncpy(node.username, user, MAX_USERNAME_SIZE-1);
        node.username[MAX_USERNAME_SIZE-1] = '\0';
        strncpy(node.ip, ipstr, IP_LEN-1);
        node.ip[IP_LEN-1] = '\0';
        strncpy(node.port, portstr, PORT_LEN-1);
        node.port[PORT_LEN-1] = '\0';
        insertUser(node);
        if (loginState == 1)
          sendto(fd, "Logged in successfully\n", strlen("Logged in successfully\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
        else
          sendto(fd, "Registered successfully\n", strlen("Registered successfully\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
      } else if (loginState !=3){
          sendto(fd, "Incorrect password for user\n", strlen("Incorrect password for user\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
      }
    } else if (strcmp(cmd, "REGISTER") != 0){
      sendto(fd, "Authentication failed\n", strlen("Authentication failed\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    }
  }
  
  if (strcmp(strtok(cmd,"\n"), "QUIT") == 0) {
    pthread_mutex_lock(&filesMutex);
    pthread_mutex_lock(&usersMutex);
    removeFileSeeder(ipstr, portstr);
    removeLoggedUser(ipstr, portstr);
    pthread_mutex_unlock(&usersMutex);
    pthread_mutex_unlock(&filesMutex);
  }
}


void tracker_handler(struct selector_key * key) {
  struct sockaddr_storage client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char buffer[MAX_STRING_LENGTH];
  ssize_t bytesRecv = recvfrom(key->fd, buffer, MAX_STRING_LENGTH, 0, (struct sockaddr *) &client_addr, &client_addr_len);
  //if (bytesRecv < 0) return;
  char portstr[6] = {0};
  char ipstr[16] = {0};
  getnameinfo((struct sockaddr*)&client_addr, sizeof(struct sockaddr_storage), ipstr, sizeof(ipstr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
  if (bytesRecv <= 0) {
    removeFileSeeder(ipstr, portstr);
    removeLoggedUser(ipstr, portstr);
    return;
  }
  char * cmd = strtok(buffer, " ");
  if (cmd != NULL)
    handleCmd(cmd, ipstr, portstr, key->fd, client_addr);
}

char * getUser(char* username, char* usersS) {
  int len = strlen(username);
  char usernameComma[len+3];
  memset(usernameComma, '\0', len);
  usernameComma[0] = '\n';
  strcat(usernameComma, username);
  usernameComma[len+1] = ',';
  usernameComma[len+2] = '\0';
  return strstr(usersS, usernameComma);
}

int loginUser(char * username, char * password, int fd, struct sockaddr_storage client_addr) {
  if (users == NULL) return false;
  fseek(users, 0L, SEEK_END);
  int usersStrLen = ftell(users);
  rewind(users);
  char usersStr[usersStrLen+1];
  size_t size = fread(usersStr, sizeof(char), usersStrLen, users);
  usersStr[size] = '\0';
  char* user = getUser(username, usersStr);

  if (user == NULL) {
	int len = strlen("Create user ")+MAX_USERNAME_SIZE+ strlen("? (y/n): ")+1;
	  char msg[len];
	  strcpy(msg, "Create user ");
	  strcat(msg, username);
      strcat(msg, "? (y/n): ");
	  sendto(fd, msg, strlen(msg), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));

      if(lineConfirms(fd, client_addr)){
        registerUser(username, password);
		return 2;
      }
	  sendto(fd, "New user not registered.\n", strlen("New user not registered.\n"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
	  return 3;
  } else {
    strtok(user, ",");
    char * passwordStr = strtok(NULL, "\n");
    return strcmp(password, passwordStr) == 0;
  }
  return false;
}

void changePassword(char * oldPassword, char * newPassword){

}
