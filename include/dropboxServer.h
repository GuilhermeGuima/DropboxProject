#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "dropboxUtil.h"

#define PORT 4000

typedef struct file_metadata {
	char name[MAX_FILE_NAME];	// with extension
	char last_modified[MAX_FILE_NAME];	// time as returned by the ctime function
	int size;
} File_Meta;

typedef struct client {
  char username[USER_NAME_SIZE];
  int devices[2]; 	// socket ports for the two client sessions
  int logged;
  File_Meta files[MAX_FILES];
} Client;

typedef struct client_list {
  Client* client;
  struct client_list *next;
} ClientList;

void *clientThread(void *arg);
Client* newClient(char* username);
void initializeClientList();
int approveClient(Client* client, ClientList** client_list, int port);
ClientList* addClient(Client* client, ClientList* client_list);
ClientList* removeClient(char *username, int port);
void printListClient(ClientList* client_list);

