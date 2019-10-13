#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include "dropboxUtil.h"

#define PORT 4000

char server_folder[MAX_PATH];

typedef struct client {
  char username[USER_NAME_SIZE];
  int devices[2]; 	// socket ports for the two client sessions
  struct sockaddr_in addr[2]; // addresses for the two client sessions
  int logged;
} Client;

typedef struct client_list {
  Client* client;
  struct client_list *next;
} ClientList;

void *clientThread(void *arg);
void *syncThread(void *arg);
Client* newClient(char* username);
void initializeClientList();
int approveClient(Client* client, ClientList** client_list, int port);
ClientList* addClient(Client* client, ClientList** client_list);
ClientList* removeClient(char *username, int port);
void printListClient(ClientList* client_list);
void sendList(char *file_path, char* username, Connection *connection);
void sendBroadcastMessage(struct sockaddr_in *addr, int operation, char *file, char *username);
void createClientFolder (char* name);
void sendAllFiles(char *username, Connection *connection, int seqnum);
int countFiles(char *username);
