#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>

#define PORT 4000

typedef struct client {
  char username[USER_NAME_SIZE];
  int devices[2];
  int logged;
  int port;
} Client;

typedef struct client_list {
  Client* client;
  struct client_list *next;
} ClientList;

void *clientThread(void *arg);
Client* newClient(char* username, int port);
