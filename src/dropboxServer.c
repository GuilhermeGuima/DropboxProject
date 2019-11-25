#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

ClientList *client_list;
sem_t portBarrier;
pthread_t main_thread, bully_thread, replica_thread;
pthread_mutex_t clientListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverListMutex = PTHREAD_MUTEX_INITIALIZER;
ServerList *svList;
int coordinatorId;

char primary_server_ip[16] = "127.0.0.1";

int main(int argc, char *argv[]) {
	ServerList *auxList;
    int id;
    Server *server;
    coordinatorId = 1;
	initializer_static_svlist();
	auxList = svList;


	DEBUG_PRINT2("LISTA ESTATICA DE SERVIDORES \n ---------------------------------------------- \n\n");

	while(auxList != NULL) {
        DEBUG_PRINT2("ID: %d PORTA PADRAO: %d PORTA ELEICAO: %d PORTA \n", auxList->server->id, auxList->server->defaultPort, auxList->server->bullyPort);
        auxList = auxList->next;
	}
	DEBUG_PRINT2("\n");

	if (argc < 2) {
		fprintf(stderr, "Falta de argumentos ./dropboxServer id           (id = 1,2,3,4) \n");
		return 0;
	}

	id = atoi(argv[1]);
	server = getServer(id);

	if(server == NULL) {
        printf("O servidor com ID %d nao existe\n", id);
        return 0;
	}

	while (TRUE) {
        pthread_create(&bully_thread, NULL, bullyThread, (void*) server);

        if (server->id == coordinatorId) {
            pthread_create(&main_thread, NULL, coordinatorFunction, NULL);
        } else {
            pthread_create(&main_thread, NULL, electionFunction, (void*) server);
            pthread_create(&replica_thread, NULL, replicaManagerThread, NULL);
        }
	}

	return 0;
}

void* coordinatorFunction() {         //MAIN DA PT1 DO TRABALHO
	int sockfd;
	struct sockaddr_in serv_addr;
	int *port_count = malloc(sizeof(int));
	Package *buffer = malloc(sizeof(Package));
	char portMapper[DATA_SEGMENT_SIZE];
	pthread_t th1, th2;
	Client *client;
	int seqnumSend = 0, seqnumReceive = 0;

	sem_init(&portBarrier,0,0);
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
		fprintf(stderr, "ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		fprintf(stderr, "ERROR on binding");

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


		if(buffer->type == SYNC){
			// client sync socket
			pthread_create(&th2, NULL, syncThread, (void*) port_count);
			sem_wait(&portBarrier);
		}else if(buffer->type == CMD){
			// new client socket--
			createClientFolder(client->username);
			pthread_create(&th1, NULL, clientThread, (void*) port_count);
			sem_wait(&portBarrier);
		} else{
			if (approveClient(client, &client_list)) {
				if(buffer->type == BROADCAST){
					// TODO: send_new_client_to_replicas()
					addClient(client, &client_list);
					*port_count = *port_count - 1;	//no new port is assigned for a broadcast
				}
			}else{
				DEBUG_PRINT("O CLIENTE JA ESTA USANDO DOIS DISPOSITIVOS\n");
				strcpy(portMapper, ACCESS_ERROR);
				*port_count = *port_count - 1;
			}
		}

		Package *p = newPackage(CMD,client->username,seqnumSend,0,portMapper);
		sendPackage(p,&connection, NOT_LIMITED);
		seqnumSend = 1 - seqnumSend;
	}

	close(sockfd);
	return NULL;
}

void* electionFunction(void* arg) {
    int sockfd, n, status;
    int *res;
    int changedCoordinator = FALSE;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *sv;

	char buffer[256];

	sv = gethostbyname("localhost");
	if (sv == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");
    setTimeout(sockfd);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(coordinatorId);
	serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	printf("RM a postos\n");

	while(TRUE) {

        bzero(buffer, 256);
        strcpy(buffer, "OK");

        n = sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
        if (n < 0)
            printf("ERROR sendto");

        length = sizeof(struct sockaddr_in);


        if(recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *) &from, &length) < 0 ) {
             status = pthread_join(bully_thread, (void **) &res);
             if (status != 0)
                exit(1);
             changedCoordinator = coordinatorId != *res;
             coordinatorId = *res;

             if(changedCoordinator){
             	// TODO: change connections to new server
             }

        } else {
            printf("SYSTEM: %s\n", buffer);
            if(strcmp(buffer, "ACK") == 0) {
                sleep(2);
            }
        }
	}

	printf("SYSTEM: SAINDO DA FUNÇÃO PARA SERVIDOR REPLICADO\n");
	close(sockfd);
	return NULL;
}

void broadcast(int operation, char* file, char *username){
	ClientList *current = client_list;

	pthread_mutex_lock(&clientListMutex);
    while(current != NULL){
        if (strcmp(current->client->username, username) == 0) {
            if(current->client->devices[0] != INVALID){
            	sendBroadcastMessage(&current->client->addr[0],  operation, file, username);
            }
            if(current->client->devices[1] != INVALID){
            	sendBroadcastMessage(&current->client->addr[1], operation, file, username);
            }
            pthread_mutex_unlock(&clientListMutex);
            return;
		}
        current = current->next;
    }
    pthread_mutex_unlock(&clientListMutex);
}

void broadcastUnique(int operation, char* file, char *username, struct sockaddr_in ownAddress){
	ClientList *current = client_list;
	char *first_client_ip, *second_client_ip;

	char* own_ip = strdup(inet_ntoa(ownAddress.sin_addr));

	pthread_mutex_lock(&clientListMutex);
    while(current != NULL){
    	first_client_ip = strdup(inet_ntoa(current->client->addr[0].sin_addr));
    	second_client_ip = strdup(inet_ntoa(current->client->addr[1].sin_addr));   	

        if (strcmp(current->client->username, username) == 0) {
            if(current->client->devices[0] != INVALID && strcmp(first_client_ip, own_ip) != 0){
            	sendBroadcastMessage(&current->client->addr[0],  operation, file, username);
            }
            if(current->client->devices[1] != INVALID && strcmp(second_client_ip, own_ip) != 0){
            	sendBroadcastMessage(&current->client->addr[1], operation, file, username);
            }
            pthread_mutex_unlock(&clientListMutex);
            return;
		}
        current = current->next;
    }
    pthread_mutex_unlock(&clientListMutex);
}

void sendBroadcastMessage(struct sockaddr_in *addr, int operation, char *file, char *username){

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
	sendPackage(p,&connection, NOT_LIMITED);

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
	sem_post(&portBarrier);

	int sockfd, file_size;
	char *buffer = NULL, *file_path = NULL;
	struct sockaddr_in serv_addr;
	Connection *connection = malloc(sizeof(Connection));
	int seqnum = 0;

	DEBUG_PRINT("Porta %d\n", port);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fprintf(stderr, "ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		fprintf(stderr, "ERROR on binding");

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
				//printf("Processing Upload of file %s for user %s\n",request->data,request->user);
				receiveFile(connection, &buffer, &file_size);
				//printf("Received file of size: %d\n",file_size);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				saveFile(buffer, file_size, file_path);

				//TODO: send_to_upload_to_replicas();

				broadcast(UPLOAD,request->data, request->user);
				break;
			case DOWNLOAD:
				//printf("Processing Download of file %s for user %s\n",request->data,request->user);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				sendFile(file_path, connection, request->user);
				break;
			case DELETE:
				//printf("Deleting file %s for user %s\n",request->data,request->user);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				if(remove(file_path) == 0){
					//TODO: send_to_delete_to_replicas();
					broadcast(DELETE,request->data, request->user);
				}
				break;
			case LISTSERVER:
				//printf("Listing files for user %s\n",request->user);
				file_path = makePath(server_folder, request->user);
				sendList(file_path, request->user, connection);
				break;
			case EXIT:
				//printf("Processing user %s logout\n", request->user);
				client_list = removeClient(request->user, &client_list, connection->address);
				break;
			default: fprintf(stderr, "Invalid command number %d for client thread\n", request->type);
		}
	}
}

void *syncThread(void *arg) {
	int port = *(int*) arg;
	sem_post(&portBarrier);

	int sockfd, file_size, nbFiles;
	char *buffer = NULL, *file_path = NULL;
	char nbFilesBuffer[DATA_SEGMENT_SIZE];
	struct sockaddr_in serv_addr;
	Connection *connection = malloc(sizeof(Connection));
	int seqnum = 0, seqnumSend = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fprintf(stderr, "ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		fprintf(stderr, "ERROR on binding");

	connection->socket = sockfd;
	connection->address = &serv_addr;

	while (TRUE) {
		sleep(TIMEOUT);

		DEBUG_PRINT("ENTROU NO WHILE DA SYNC THREAD\n");
		DEBUG_PRINT("PORTA DO CLIENTE %d\n", port);

		Package *request = malloc(sizeof(Package));
		receivePackage(connection, request, seqnum);
		seqnum = 1 - seqnum;

		switch(request->type){
			case UPLOAD:
				receiveFile(connection, &buffer, &file_size);
				if(file_size != 0){
					file_path = makePath(request->user,request->data);
					file_path = makePath(server_folder, file_path);
					saveFile(buffer, file_size, file_path);

					//TODO: send_upload_to_replicas()

					broadcastUnique(UPLOAD, request->data, request->user, *connection->address);
				}
				break;
			case DELETE:
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				if(remove(file_path) == 0){
					//TODO: send_delete_to_replicas()
					broadcastUnique(DELETE,request->data, request->user, *connection->address);
				}
				break;
			case DOWNLOAD_ALL:
				nbFiles = countFiles(request->user);
				itoa(nbFiles, nbFilesBuffer);
				Package *p = newPackage(CMD, request->user, seqnumSend, 0, nbFilesBuffer);
				seqnumSend = 1 - seqnumSend;
				sendPackage(p, connection, NOT_LIMITED);
				sendAllFiles(request->user, connection, seqnumSend);

				break;
			default: fprintf(stderr, "Invalid command number %d for sync thread\n", request->type);
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
        fprintf(stderr, "Error opening directory %s\n", dir_path);
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
                sendPackage(p, connection, NOT_LIMITED);
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

void sendList(char* dir_path, char* username, Connection *connection){
	char *s = listDirectoryContents(dir_path);
	char buf[DATA_SEGMENT_SIZE];
	int i;
	Package *p;

	for(i = 0; i < MAX_LIST_SIZE/DATA_SEGMENT_SIZE; i++){
		memcpy(buf,s+i*(DATA_SEGMENT_SIZE-1),DATA_SEGMENT_SIZE-1);
		buf[DATA_SEGMENT_SIZE-1] = '\0';
		p = newPackage(DATA, username,LIST_START_SEQ+i,0,buf);
		sendPackage(p, connection, NOT_LIMITED);
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

int approveClient(Client* client, ClientList** client_list) {
    ClientList *current = *client_list;

    pthread_mutex_lock(&clientListMutex);
    while(current != NULL){
        if (strcmp(current->client->username, client->username) == 0) {
        	// both in use
            if(current->client->devices[0] != INVALID && current->client->devices[1] != INVALID){
            	pthread_mutex_unlock(&clientListMutex);
            	return FALSE;
            } else {
            	//at least one free
            	pthread_mutex_unlock(&clientListMutex);
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

ClientList* removeClient(char* username, ClientList **client_list, struct sockaddr_in* c_addr) {
    ClientList *current = *client_list;
    ClientList *prev_client = NULL;

    pthread_mutex_lock(&clientListMutex);
    while(current != NULL) {
        if (strcmp(current->client->username, username) == 0) {
        	if(c_addr->sin_addr.s_addr == current->client->addr[0].sin_addr.s_addr){
            	current->client->devices[0] = INVALID;

            	// removes from the list if both devices are now invalid
            	if(current->client->devices[1] == INVALID){
            		if (prev_client != NULL) {
                    	prev_client->next = current->next;
	                } else {
	                    *client_list = current->next;
	                }
            	}
            } else if(c_addr->sin_addr.s_addr == current->client->addr[1].sin_addr.s_addr){
            	current->client->devices[1] = INVALID;

            	// removes from the list if both devices are now invalid
            	if(current->client->devices[0] == INVALID){
            		if (prev_client != NULL) {
                    	prev_client->next = current->next;
	                } else {
	                    *client_list = current->next;
	                }
            	}
            }
        }
        prev_client = current;
        current = current->next;
    }

    pthread_mutex_unlock(&clientListMutex);
    return *client_list;
}

void printListClient(ClientList* client_list) {
    int index = 1;
    ClientList *current = client_list;

    pthread_mutex_lock(&clientListMutex);
    while(current != NULL) {
        printf("%d - %s - device1: %d - device2: %d\n", index, current->client->username,
        	current->client->devices[0], current->client->devices[1]);
        current = current->next;
        index++;
    }
    pthread_mutex_unlock(&clientListMutex);
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


// DAQUI PRA BAIXO REFERENTE A PARTE 2 DO TRABALHO

int setTimeoutElection(int sockfd){
    struct timeval tv;
    tv.tv_sec = TIMEOUT_ELECTION;
    tv.tv_usec = 0;

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0) {
        fprintf(stderr, "Error setting timeout for socket %d.\n", sockfd);
        return FAILURE;
    }

    return SUCCESS;
}

Server* getServer(int id) {
    if (id < 1 || id > 4) {
        return NULL;
    }
    ServerList *current = svList;
    Server* res_server = NULL;

    pthread_mutex_lock(&serverListMutex);
    while(current != NULL){
    	if(current->server->id == id) res_server = current->server;
    	current = current->next;
    }
    pthread_mutex_unlock(&serverListMutex);

    return res_server;
}

void initializer_static_svlist() {
    ServerList *auxList;
    Server *sv1 = malloc(sizeof(Server));
    Server *sv2 = malloc(sizeof(Server));
    Server *sv3 = malloc(sizeof(Server));
    Server *sv4 = malloc(sizeof(Server));
    sv1->id = 1;
    sv1->defaultPort = 5001;
    sv1->bullyPort = 6001;
    strcpy(sv1->ip,"127.0.0.1");
    sv2->id = 2;
    sv2->defaultPort = 5002;
    sv2->bullyPort = 6002;
    strcpy(sv2->ip,"127.0.0.1");
    sv3->id = 3;
    sv3->defaultPort = 5003;
    sv3->bullyPort = 6003;
    strcpy(sv3->ip,"127.0.0.1");
    sv4->id = 4;
    sv4->defaultPort = 5004;
    sv4->bullyPort = 6004;
    strcpy(sv4->ip,"127.0.0.1");

    auxList = malloc(sizeof(ServerList));
    auxList->server = sv4;
    auxList->next = NULL;
    svList = auxList;
    auxList = malloc(sizeof(ServerList));
    auxList->server = sv3;
    auxList->next = NULL;
    svList->next = auxList;
    auxList = malloc(sizeof(ServerList));
    auxList->server = sv2;
    auxList->next = NULL;
    svList->next->next = auxList;
    auxList = malloc(sizeof(ServerList));
    auxList->server = sv1;
    auxList->next = NULL;
    svList->next->next->next = auxList;
}

int getCoordinatorPort() {
    ServerList *auxList = svList;

	while(auxList != NULL) {
        if (coordinatorId == auxList->server->id) {
            return auxList->server->bullyPort;
        }
        auxList = auxList->next;
	}

	return INVALID;
}

void sendCoordinatorMessage(Server* server) {
    int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *sv;
	char data[256];
	bzero(data, 256);
	ServerList *auxList;

	itoa(server->id, data);
	Package *bufferSend = newPackage(COORDINATOR, "bully", 0, 0, data);

	sv = gethostbyname("localhost");
	if (sv == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

	auxList = svList;
	while (auxList != NULL) {
        if (server->id < auxList->server->id) {
            if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                printf("ERROR opening socket");
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(auxList->server->bullyPort);
            serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
            bzero(&(serv_addr.sin_zero), 8);
            sendto(sockfd, bufferSend, PACKAGE_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
            close(sockfd);
        }
        auxList = auxList->next;
	}
}

void *testCoordinator(void *arg) {
    Server* server = (Server*) arg;
    int sockfd;
    int verifyCoordinator = FALSE;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *sv;
	char data[256];
	bzero(data, 256);
	Package *bufferReceive = malloc(PACKAGE_SIZE);
	Package *tester = newPackage(TESTER, "bully", 0, 0, data);
	Package *test_fail = newPackage(TEST_FAIL, "bully", 0, 0, data);
	Package *test_ok = newPackage(TEST_OK, "bully", 0, 0, data);

	sv = gethostbyname("localhost");

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");
    setTimeout(sockfd);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(getCoordinatorPort());
	serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	sendto(sockfd, tester, PACKAGE_SIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	if (recvfrom(sockfd, bufferReceive, PACKAGE_SIZE, 0, (struct sockaddr *) &from, &length) >= 0) {
        if (bufferReceive->type == ACK) {
            verifyCoordinator = TRUE;
        }
	}
	close(sockfd);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                printf("ERROR opening socket");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server->bullyPort);
    serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

	if (verifyCoordinator) {
        sendto(sockfd, test_ok, PACKAGE_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	} else {
        sendto(sockfd, test_fail, PACKAGE_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	}

	close(sockfd);
	pthread_exit(NULL);
}

void *startElection(void *arg) {
    Server* server = (Server*) arg;
    int sockfd, receiveAnswer;
    socklen_t rmlen;
	struct sockaddr_in serv_addr, rm_addr;
	struct hostent *sv;
	char data[256];
	bzero(data, 256);
	ServerList *auxList;

	Package *bufferSendCoordinator = newPackage(SEND_COORDINATOR, "bully", 0, 0, data);
	Package *bufferWaitCoordinator = newPackage(WAITING_NEW_COORDINATOR, "bully", 0, 0, data);
	Package *bufferElection = newPackage(ELECTION, "bully", 0, 0, data);
	Package *bufferReceive = malloc(PACKAGE_SIZE);

	sv = gethostbyname("localhost");
	if (sv == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    rmlen = sizeof(struct sockaddr_in);
	auxList = svList;
	receiveAnswer = FALSE;
	while (auxList != NULL && !receiveAnswer) {
        if (server->id > auxList->server->id) {
            if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                printf("ERROR opening socket");
            setTimeout(sockfd);
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(auxList->server->bullyPort);
            serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
            bzero(&(serv_addr.sin_zero), 8);

            sendto(sockfd, bufferElection, PACKAGE_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
            if (recvfrom(sockfd, bufferReceive, PACKAGE_SIZE, 0, (struct sockaddr *) &rm_addr, &rmlen) >= 0) {
                if (bufferReceive->type == ANSWER) {
                    DEBUG_PRINT2("BULLY - RECEBEU ANSWER\n");
                    receiveAnswer = TRUE;
                }
            }
            close(sockfd);
        }
        auxList = auxList->next;
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                printf("ERROR opening socket");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server->bullyPort);
    serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

	if(receiveAnswer) {
        DEBUG_PRINT2("BULLY - AGUARDANDO MENSAGEM PROCLAMANDO O NOVO COORDENADOR\n");
        sendto(sockfd, bufferWaitCoordinator, PACKAGE_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	} else {
        DEBUG_PRINT2("BULLY - SOU O NOVO COORDENADOR\n");
        sendto(sockfd, bufferSendCoordinator, PACKAGE_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	}

	close(sockfd);
	pthread_exit(NULL);
}

void *bullyThread(void *arg) {
    Server* server = (Server*) arg;
    int sockfd;
    pthread_t th1, th2;
    int electionStatus = WITHOUT_ELECTION;
    int *newCoordinator = (int*) malloc(sizeof(int));
	socklen_t rmlen;
	struct sockaddr_in serv_addr, rm_addr;
	char data[256];
	bzero(data, 256);
	Package *bufferReceive = malloc(PACKAGE_SIZE);
	Package *ack = newPackage(ACK, "bully", 0, 0, data);
	Package *asnwer = newPackage(ANSWER, "bully", 0, 0, data);

	sleep(TIMEOUT_ELECTION); //pra evitar pacotes antigos remanescentes

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server->bullyPort);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

	rmlen = sizeof(struct sockaddr_in);

	DEBUG_PRINT2("BULLY - A THREAD ESTÁ RODANDO NA PORTA %d\n", server->bullyPort);

	if (server->id == coordinatorId) {
        while(TRUE) {
            recvfrom(sockfd, bufferReceive, PACKAGE_SIZE, 0, (struct sockaddr *) &rm_addr, &rmlen);
            if (bufferReceive->type == TESTER) {
                sendto(sockfd, ack, PACKAGE_SIZE, 0,(struct sockaddr *) &rm_addr, sizeof(struct sockaddr));
            }
        }
	} else {
	    setTimeoutElection(sockfd);
        while (TRUE) {
            if(recvfrom(sockfd, bufferReceive, PACKAGE_SIZE, 0, (struct sockaddr *) &rm_addr, &rmlen) < 0) {
                if (electionStatus == WAITING_NEW_COORDINATOR) {
                    DEBUG_PRINT2("BULLY - TIMEOUT NA ESPERA DO COORDENADOR\n"); //VERIFICAR DEPOIS
                } else if (electionStatus == WITHOUT_ELECTION) {
                    pthread_create(&th1, NULL, testCoordinator, (void*) server);
                }
            } else {
                DEBUG_PRINT2("BULLY - NOVO PACOTE\n");
                if(bufferReceive->type == ELECTION) {
                    if (electionStatus == WITHOUT_ELECTION) {
                        sendto(sockfd, asnwer, PACKAGE_SIZE, 0,(struct sockaddr *) &rm_addr, sizeof(struct sockaddr));
                        DEBUG_PRINT2("BULLY - ENVIADO ASNWER E INICIADO PRÓPRIA ELEIÇÃO\n");
                        electionStatus = IN_ELECTION;
                        pthread_create(&th2, NULL, startElection, (void*) server);
                    } else {
                        DEBUG_PRINT2("BULLY - RECEBEU UM ELECTION ATRASADO\n");
                        sendto(sockfd, asnwer, PACKAGE_SIZE, 0,(struct sockaddr *) &rm_addr, sizeof(struct sockaddr)); //manda ack pra não ficar se elegendo
                    }
                }
                if (bufferReceive->type == COORDINATOR) {
                    *newCoordinator = atoi(bufferReceive->data);
                    DEBUG_PRINT2("BULLY - NOVO COORDENADOR ELEITO É %d\n", *newCoordinator);
                    close(sockfd);
                    pthread_exit(newCoordinator);
                }
                if (bufferReceive->type == SEND_COORDINATOR) {
                    DEBUG_PRINT2("BULLY - AVISANDO OUTROS SERVIDORES QUE ME TORNEI COORDENADOR\n");
                    sendCoordinatorMessage(server);
                    *newCoordinator = server->id;
                    close(sockfd);
                    pthread_exit(newCoordinator);
                }
                if (bufferReceive->type == WAITING_NEW_COORDINATOR) {
                    electionStatus = WAITING_NEW_COORDINATOR;
                }
                if (bufferReceive->type == TEST_FAIL) {
                    DEBUG_PRINT2("BULLY - COORDENADOR CAIU, INICIANDO ELEIÇÃO\n");
                    electionStatus = IN_ELECTION;
                    pthread_create(&th2, NULL, startElection, (void*) server);
                }
                if (bufferReceive->type == TEST_OK) {
                    DEBUG_PRINT2("BULLY - COORDENADOR OK\n");
                }
            }
        }
	}
}

// thread to listen for requests coming from the coordinator
// to duplicate all upload and delete operations in the backup servers
void* replicaManagerThread(){
	int sockfd, file_size;
	char *buffer = NULL, *file_path = NULL;
	struct sockaddr_in repl_addr;
	Connection *connectionRM = malloc(sizeof(Connection));
	int seqnumReceive = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fprintf(stderr, "ERROR opening socket");

	repl_addr.sin_family = AF_INET;
	repl_addr.sin_port = htons(REPLICA_PORT);
	repl_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(repl_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &repl_addr, sizeof(struct sockaddr)) < 0)
		fprintf(stderr, "ERROR on binding");

	connectionRM->socket = sockfd;
	connectionRM->address = &repl_addr;

	while (TRUE) {
		Package *request = malloc(sizeof(Package));
		receivePackage(connectionRM, request, seqnumReceive);
		seqnumReceive = 1 - seqnumReceive;

		switch(request->type){
			case UPLOAD:
				receiveFile(connectionRM, &buffer, &file_size);
				if(file_size != 0){
					file_path = makePath(request->user,request->data);
					file_path = makePath(server_folder, file_path);
					saveFile(buffer, file_size, file_path);
				}
				break;
			case DELETE:
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				remove(file_path);
				break;
			case NEW_CLIENT:
				//TODO: create protocol for new client
				break;
			default: fprintf(stderr, "Invalid command number %d for RM thread\n", request->type);
		}
	}

}
