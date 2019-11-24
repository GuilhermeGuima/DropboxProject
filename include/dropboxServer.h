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

// server port
#define PORT 4000

// server main folder (should be $HOME/DropboxUsers/)
char server_folder[MAX_PATH];

// data struct to hold clients information
typedef struct client {
  char username[USER_NAME_SIZE];
  int devices[2]; 	// the two devices the user can be using at the time (INVALID or LOGGED)
  struct sockaddr_in addr[2]; // addresses for the two client sessions
} Client;

// list of the clients currently connected to the server
typedef struct client_list {
  Client* client;
  struct client_list *next;
} ClientList;

// data struct to hold servers information

typedef struct server {
  int id;
  int defaultPort;
  int bullyPort;
  char ip[16];
} Server;

// list of the statics servers
typedef struct server_list {
  Server* server;
  struct server_list *next;
} ServerList;

/*******************************************************************
					CLIENT MANAGEMENT FUNCTIONS
********************************************************************/

/* Creates a new Client
	@input user's username
	@return new Client with this username
*/
Client* newClient(char* username);

/*  Initializes the server's client list */
void initializeClientList();

/*  Checks whether or not the client is approved for connection
	@input client - the client trying to connect
	@input client_list - a pointer to the server's client list
	@return TRUE/FALSE
*/
int approveClient(Client* client, ClientList** client_list);

/*  Adds a client to the server's client list. If client already exists,
	adds new device with client->addr[0] as the address
	@input client - the client to be added
	@input client_list - a pointer to the server's client list
	@return updated client list
*/
ClientList* addClient(Client* client, ClientList** client_list);

/*  Removes a client from the server's client list
	@input client - the client to be removed
	@input client_list - a pointer to the server's client list
	@input c_addr - client to be removed address
	@return updated client list
*/
ClientList* removeClient(char *username, ClientList** client_list, struct sockaddr_in* c_addr);

/*  Prints server's client list
	@input client_list - the server's client list
*/
void printListClient(ClientList* client_list);

/*******************************************************************
					DROPBOX FUNCTIONS
********************************************************************/

/*  Sends list of user's sync files in the server to the client
	@input dir_path - user's sync dir
	@input username - client's username
	@input connection - the connection to the client
*/
void sendList(char *dir_path, char* username, Connection *connection);

void broadcast(int operation, char* file, char *username);
void broadcastUnique(int operation, char* file, char *username, struct sockaddr_in ownAddress);

/*  Sends message to propagate uploaded file to other client devices
	@input addr - client address
	@input operation - operation to be executed [UPLOAD/DELETE]
	@input file - the name of the file involved in the operation
	@input username - the client's username
*/
void sendBroadcastMessage(struct sockaddr_in *addr, int operation, char *file, char *username);

/*  Creates client's  sync folder in the server if it doesn't already exists
	shold be $HOME/DropboxUsers/<username>
	@input name - client's username
*/
void createClientFolder (char* name);

/*  Sends all files in the client's folder
	@input username - the client's username
	@input connection - the current connection
	@input seqnum - the current sequence number for this connection
*/
void sendAllFiles(char *username, Connection *connection, int seqnum);

/*  Counts all files in the client's folder
	@input username - the client's username
	@return how many files there are in the client's folder
*/
int countFiles(char *username);

/*******************************************************************
					THREAD FUNCTIONS
********************************************************************/

/*  Thread to listen for client's requests
	@input arg - port number assigned to this client's thread
*/
void *clientThread(void *arg);

/*  Thread to listen for INOTIFY synchronization request s
	@input arg - port number assigned to this client's sync thread
*/
void *syncThread(void *arg);




// DEIXAR BONITO DEPOIS NATY

void *coordinatorFunction();
void *replicaFunction(void* arg);
int setTimeoutElection(int sockfd);
Server* getServer(int id);
void initializer_static_svlist();
int getCoordinatorPort();
void sendCoordinatorMessage(Server* server);
void *testCoordinator(void *arg);
void *startElection(void *arg);
void *bullyThread(void *arg);
