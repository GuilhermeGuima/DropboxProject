#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include "dropboxUtil.h"

// server port
#define PORT 4000

// for INOTIFY sync_dir watcher
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024*(EVENT_SIZE + 16))

/*******************************************************************
					CLIENT MANAGEMENT FUNCTIONS
********************************************************************/

/* Looping function to receive commands from the terminal user */
void selectCommand();

/* Creates the sync_dir for the user if it doesn't exist yet and
	the sync thread that watches for modifications in the folder
	@input user's folder (should be $HOME/sync_dir_<username>)
	@return FAILURE/SUCCESS
*/
int getSyncDir(char *folder);

/* Initializes the INOTIFY watcher */
int initSyncDirWatcher();

/*******************************************************************
					CONNECTION FUNCTIONS
********************************************************************/

/*
	Makes the first connection to the server
	@input connection - server address and socket to use
	@input user - username for the current user
	@return FAILURE/SUCCESS
*/
int firstConnection(char *user, Connection *connection);

/*
	Gets a connection object associated with a port in the server
	@input port - port to be used for this connection
	@return the connection associated with the port
*/
Connection* getConnection(int port);

/*******************************************************************
					DROPBOX FUNCTIONS
********************************************************************/

/*
	Uploads a file to the server
	@input filepath - path for the file to be uploaded
	@input seqNumber - current communication protocol sequence number
	@input connection - connection to the server
	@return FAILURE/SUCCESS
*/
int uploadFile(char *file_path, int *seqNumber, Connection *connection);

/*
	Downloads a file from the server
	@input file - name of the file to be downloaded
	@input connection - connection to the server
	@return FAILURE/SUCCESS
*/
int downloadFile(char *file, Connection *connection);

/*
	Deletes a file from the server
	@input filepath - name of the file to be deleted
	@input seqNumber - current communication protocol sequence number
	@input connection - the connection to the server
*/
int deleteFile(char *file, int *seqNumber, Connection *connection);

/*
	Lists all the files in the server folder for the user
	@input connection - the connection to the server
*/
void listServer(Connection *connection);

/*
	Lists all the files in the client's sync folder
*/
void listClient();

/*
	Function to receive a list of files from the server
	@return string containing the list of files
*/
char *receiveList();

/*
	Download all files in the server folder for the user 
	used after creating createSyncDir()
	@input connection - the connection to the server
	@input seqnum - the current sequence number for the protocol
	@input seqnumReceive - the current sequence number for the receival of packets
*/
void downloadAllFiles(Connection *connection, int *seqnum, int *seqnumReceive);

/*
	Closes the connection on the client side (i.e. sends an EXIT packet to the server)
*/
void closeConnection();

/*******************************************************************
					THREAD FUNCTIONS
********************************************************************/

/*
	Thread to execute the synchronization of the sync_dir folder for the
	user. Uses the INOTIFY interface.
*/
void *sync_thread();

/*
	Thread to listen for broadcast requests from the server. When another
	device for the same user alters the server, this threads listens for 
	files that have to be propagated to the other devices.
*/
void *broadcast_thread();

/*
	Thread to listen for coordinator election results
*/
void *election_thread();

/*
	Thread for command line requests (also the thread that initializes all
	others, ergo the main thread)
*/
void *main_thread(void *arg);