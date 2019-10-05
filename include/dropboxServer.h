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
  int logged;
  int port;
} Client;

typedef struct client_list {
  Client* client;
  struct client_list *next;
} ClientList;

void *clientThread(void *arg);
Client* newClient(char* username, int port);
void initializeClientList();
int approveClient(Client* client, ClientList* client_list);
ClientList* addClient(Client* client, ClientList* client_list);
ClientList* removeClient(Client* client, ClientList* client_list);
void printListClient(ClientList* client_list);
