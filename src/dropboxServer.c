#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

ClientList *client_list;
pthread_mutex_t portBarrier = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clientListMutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
	int sockfd;
	struct sockaddr_in serv_addr;
	int *port_count = malloc(sizeof(int));
	Package *buffer = malloc(sizeof(Package));
	char portMapper[DATA_SEGMENT_SIZE];
	pthread_t th1, th2;
	Client *client;
	int seqnumSend = 0, seqnumReceive = 0;

	initializeClientList();

	DEBUG_PRINT("OPÇÃO DE DEBUG ATIVADA\n");

	strcpy(server_folder, getUserHome());
    strcat(server_folder, "/");
    strcat(server_folder, SERVER_FOLDER_NAME);

    DEBUG_PRINT("SERVER FOLDER: %s\n", server_folder);

    if(mkdir(server_folder, 0777) != 0 && errno != EEXIST){
		fprintf(stderr, "Error while creating server folder.\n");
		return FAILURE;
	} else {
		printf("Creating folder %s\n", server_folder);
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

	*port_count = PORT;

	Connection connection;
	connection.address = &serv_addr;
	connection.socket = sockfd;

	while (TRUE) {
		printf("Aguardando conexões de clientes\n");
		seqnumReceive = 0; seqnumSend = 0;

		receivePackage(&connection, buffer, seqnumReceive);
		seqnumReceive = 1 - seqnumReceive;

		*port_count = *port_count + 1;

		bzero(portMapper, DATA_SEGMENT_SIZE);
		itoa(*port_count, portMapper);
		client = newClient(buffer->data);
		client->addr[0] = *connection.address;

		if (approveClient(client, &client_list, *port_count)) {
			if(buffer->type == SYNC){
				// client sync socket
				pthread_create(&th2, NULL, syncThread, (void*) port_count);
				pthread_mutex_lock(&portBarrier);
			}else if(buffer->type == BROADCAST){
				addClient(client, &client_list);
				*port_count = *port_count - 1;	//no new port is assigned for a broadcast
			}else{
				// new client socket--
				createClientFolder(client->username); 
				pthread_create(&th1, NULL, clientThread, (void*) port_count);
				pthread_mutex_lock(&portBarrier);
			}
		}else{
			DEBUG_PRINT("O CLIENTE JA ESTA USANDO DOIS DISPOSITIVOS\n");
			strcpy(portMapper, ACCESS_ERROR);
			*port_count = *port_count - 1;
		}

		Package *p = newPackage(CMD,client->username,seqnumSend,0,portMapper);
		sendPackage(p,&connection);
		seqnumSend = 1 - seqnumSend;
	}

	close(sockfd);
	return SUCCESS;
}

void broadcast(int operation, char* file, char *username){
	ClientList *current = client_list;

    while(current != NULL){
        if (strcmp(current->client->username, username) == 0) {
            if(current->client->devices[0] != INVALID){
            	sendBroadcastMessage(&current->client->addr[0],  operation, file, username);
            }
            if(current->client->devices[1] != INVALID){
            	sendBroadcastMessage(&current->client->addr[1], operation, file, username);
            }
            return;
		}
        current = current->next;
    }
}

void sendBroadcastMessage(struct sockaddr_in *addr, int operation, char *file, char *username){

	printf("bcast\n");

	int sockfd;
    Connection connection;
    struct sockaddr_in *client_addr = malloc(sizeof(struct sockaddr_in));
    *client_addr = *addr;

    // opens socket to broadcast
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        fprintf(stderr, "ERROR opening socket\n");
    setTimeout(sockfd);

    connection.socket = sockfd;
    connection.address = client_addr;

    // sends file name and operation
	Package *p = newPackage(operation,username,0,0,file);
	printf("Sending packages bcast %d\n", ntohs(connection.address->sin_port));
	sendPackage(p,&connection);

	if(operation == UPLOAD){
		// sends file
		char *file_path = makePath(username, file);
		file_path = makePath(server_folder, file_path);
		sendFile(file_path, &connection, username);
	}

	free(client_addr);
	close(connection.socket);
}

void *clientThread(void *arg) {
	int port = *(int*) arg;
	pthread_mutex_unlock(&portBarrier);

	int sockfd, file_size;
	char *buffer = NULL, *file_path = NULL;
	struct sockaddr_in serv_addr;
	Connection *connection = malloc(sizeof(Connection));
	int seqnum = 0;

	DEBUG_PRINT("Porta %d\n", port);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

	connection->socket = sockfd;
	connection->address = &serv_addr;

	while (TRUE) {
		sleep(2);

		DEBUG_PRINT("ENTROU NO WHILE DA CLIENT THREAD\n");
		DEBUG_PRINT("PORTA DO CLIENTE %d\n", port);

		Package *request = malloc(sizeof(Package));
		receivePackage(connection, request, seqnum);
		seqnum = 1 - seqnum;

		switch(request->type){
			case UPLOAD:
				printf("Processing Upload of file %s for user %s\n",request->data,request->user);
				receiveFile(connection, &buffer, &file_size);
				printf("Received file of size: %d\n",file_size);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				saveFile(buffer, file_size, file_path);
				printf("Client conn: %s:%d\n",inet_ntoa(connection->address->sin_addr), ntohs(connection->address->sin_port));
				broadcast(UPLOAD,request->data, request->user);
				break;
			case DOWNLOAD:
				printf("Processing Download of file %s for user %s\n",request->data,request->user);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				sendFile(file_path, connection, request->user);
				break;
			case DELETE:
				printf("Deleting file %s for user %s\n",request->data,request->user);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				if(remove(file_path) == 0){
					printf("Sucessfully deleted file\n");
					broadcast(DELETE,request->data, request->user);
				}
				break;
			case LISTSERVER:
				printf("Listing files for user %s\n",request->user);
				file_path = makePath(server_folder, request->user);
				sendList(file_path, request->user, connection);
				break;
			case EXIT:
				printf("Processing user %s logout\n", request->user);
				client_list = removeClient(request->user, port);
				break;
			default: printf("Invalid command number %d for client thread\n", request->type);
		}
	}
}

void *syncThread(void *arg) {
	int port = *(int*) arg;
	pthread_mutex_unlock(&portBarrier);

	int sockfd, file_size, nbFiles;
	char *buffer = NULL, *file_path = NULL;
	char nbFilesBuffer[DATA_SEGMENT_SIZE];
	struct sockaddr_in serv_addr;
	Connection *connection = malloc(sizeof(Connection));
	int seqnum = 0, seqnumSend = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

	connection->socket = sockfd;
	connection->address = &serv_addr;

	while (TRUE) {
		sleep(1);

		DEBUG_PRINT("ENTROU NO WHILE DA SYNC THREAD\n");
		DEBUG_PRINT("PORTA DO CLIENTE %d\n", port);

		Package *request = malloc(sizeof(Package));
		receivePackage(connection, request, seqnum);
		seqnum = 1 - seqnum;

		switch(request->type){
			case UPLOAD:
				receiveFile(connection, &buffer, &file_size);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				saveFile(buffer, file_size, file_path);
				broadcast(UPLOAD,request->data, request->user);
				break;
			case DELETE:
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				if(remove(file_path) == 0){
					printf("Sucessfully deleted file\n");
					broadcast(DELETE,request->data, request->user);
				}
				break;
			case DOWNLOAD_ALL:
				nbFiles = countFiles(request->user);
				printf("nv files: %d\n", nbFiles);
				itoa(nbFiles, nbFilesBuffer);
				Package *p = newPackage(CMD, request->user, seqnumSend, 0, nbFilesBuffer);
				seqnumSend = 1 - seqnumSend;
				printf("Client conn: %s:%d\n",inet_ntoa(connection->address->sin_addr), ntohs(connection->address->sin_port));
				sendPackage(p, connection);

				sendAllFiles(request->user, connection, seqnumSend);

				break;
			default: printf("Invalid command number %d for sync thread\n", request->type);
		}
	}
}

void sendAllFiles(char *username, Connection *connection, int seqnum){
    DIR *parent_dir;
    struct dirent *dp;
    struct stat sb;

 	char file_path[MAX_PATH];
    char* dir_path = makePath(server_folder, username);
    Package *p; 

    if((parent_dir = opendir(dir_path)) == NULL){
        printf("Error opening directory %s\n", dir_path);
    }

    while((dp = readdir(parent_dir)) != NULL){
        strcpy(file_path, dir_path);
        strcat(file_path,"/");
        strcat(file_path, dp->d_name);

        if(stat(file_path, &sb) != -1){
            if((sb.st_mode & S_IFMT) == S_IFDIR){
                // skip directories
                continue;
            }else{
            	// sends file name
            	p = newPackage(CMD, username, seqnum, 0, dp->d_name);
                sendPackage(p, connection);
                seqnum = 1 - seqnum;

                // sends file
                sendFile(file_path, connection, username);
                free(p);
                p = NULL;
            }
        }
    }
}

int countFiles(char *username){
	int nb_files = 0;
	DIR *parent_dir;
	struct dirent *dp;

	char *dir_path = makePath(server_folder, username);
	parent_dir = opendir(dir_path);
	while((dp = readdir(parent_dir)) != NULL){
		// if it's a file
		if(dp->d_type == DT_REG){
			nb_files++;
		}
	}
	closedir(parent_dir);

	return nb_files;
}

void sendList(char* file_path, char* username, Connection *connection){
	char *s = listDirectoryContents(file_path);
	char buf[DATA_SEGMENT_SIZE];
	int i;
	Package *p;

	for(i = 0; i < MAX_LIST_SIZE/4; i++){
		memcpy(buf,s,DATA_SEGMENT_SIZE-1);
		buf[DATA_SEGMENT_SIZE-1] = '\0';
		p = newPackage(DATA, username,LIST_START_SEQ+i,0,buf);
		sendPackage(p, connection);
	}
}

Client* newClient(char* username) {
	Client *client = malloc(sizeof(*client));
	strcpy(client->username , username);
	return client;
}

void initializeClientList() {
	client_list = NULL;
}

int approveClient(Client* client, ClientList** client_list, int port) {
    ClientList *current = *client_list;

    pthread_mutex_lock(&clientListMutex);
    while(current != NULL){
        if (strcmp(current->client->username, client->username) == 0) {
        	// both in use
            if(current->client->devices[0] != INVALID && current->client->devices[1] != INVALID){
            	return FALSE;
            } else {
            	//at least one free
            	return TRUE;
            }
        }
        current = current->next;
    }
    pthread_mutex_unlock(&clientListMutex);
  	return TRUE;
}

ClientList* addClient(Client* client, ClientList** client_list) {
    ClientList *current = *client_list;
    ClientList *last = *client_list;
    client->addr->sin_port = htons(CLIENTS_PORT);

    pthread_mutex_lock(&clientListMutex);
    while(current != NULL){
        if (strcmp(current->client->username, client->username) == 0) {
        	// both in use
            if(current->client->devices[0] != INVALID && current->client->devices[1] != INVALID){
            	pthread_mutex_unlock(&clientListMutex);
            	return *client_list;
            } else {
            	DEBUG_PRINT("ADDED NEW DEVICE\n");
            	//at least one is free
            	if(current->client->devices[0] == INVALID){
            		current->client->devices[0] = LOGGED;
            		current->client->addr[0] = client->addr[0];
            	}
            	else{
            		current->client->devices[1] = LOGGED;
            		current->client->addr[1] = client->addr[0];
            	}
            	pthread_mutex_unlock(&clientListMutex);
            	return *client_list;
            }
        }
        last = current;
        current = current->next;
    }

    // user isnt logged in yet
  	ClientList *new_client = malloc(sizeof(ClientList));
  	client->devices[0] = LOGGED;
  	client->devices[1] = INVALID;
  	new_client->client = client;
  	new_client->next = NULL;

  	if(*client_list == NULL)
  		//first element of list
  		*client_list = new_client;
  	else
  		last->next = new_client;

  	DEBUG_PRINT("ADDED NEW CLIENT\n");
  	pthread_mutex_unlock(&clientListMutex);
  	return *client_list;
}

ClientList* removeClient(char* username, int port) {
    ClientList *current = client_list;
    ClientList *prev_client = NULL;

    while(current != NULL) {
        if (strcmp(current->client->username, username) == 0) {
            if(current->client->devices[0] == port){
            	current->client->devices[0] = INVALID;

            	if(current->client->devices[1] == INVALID){
            		if (prev_client != NULL) {
                    	prev_client->next = current->next;
	                } else {
	                    client_list = current->next;
	                }
            	}
            } else if(current->client->devices[1] == port){
            	current->client->devices[1] = INVALID;

            	if(current->client->devices[0] == INVALID){
            		if (prev_client != NULL) {
                    	prev_client->next = current->next;
	                } else {
	                    client_list = current->next;
	                }
            	}
            }
        }
        prev_client = current;
        current = current->next;
    }

    return client_list;
}

void printListClient(ClientList* client_list) {
    int index = 1;
    ClientList *current = client_list;

    while(current != NULL) {
        printf("%d - %s - device1: %d - device2: %d\n", index, current->client->username,
        	current->client->devices[0], current->client->devices[1]);
        current = current->next;
        index++;
    }
}

void createClientFolder (char* name) {
    char client_folder[MAX_PATH];

    strcpy(client_folder, server_folder);
	strcat(client_folder, "/");
	strcat(client_folder, name);
    if(mkdir(client_folder, 0777) != 0 && errno != EEXIST){
		fprintf(stderr, "Error while creating user folder\n");
	} else {
		printf("Creating folder %s\n", client_folder);
	}
}

