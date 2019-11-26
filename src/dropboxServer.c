#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

ClientList *client_list;
sem_t portBarrier;
pthread_t bully_thread, replica_thread, coordinator_thread;
pthread_mutex_t clientListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverListMutex = PTHREAD_MUTEX_INITIALIZER;
ServerList *svList;
Server *thisServer;
int coordinatorId;

int main(int argc, char *argv[]) {
	ServerList *auxList;
    int id;
    coordinatorId = 1;
	initializer_static_svlist();
	auxList = svList;
	Server *server = malloc(sizeof(Server));


	DEBUG_PRINT2("LISTA ESTATICA DE SERVIDORES \n ---------------------------------------------- \n\n");

	while(auxList != NULL) {
        DEBUG_PRINT2("ID: %d ENDEREÇO IP: %s PORTA ELEICAO: %d\n", auxList->server->id, auxList->server->ip, auxList->server->bullyPort);
        auxList = auxList->next;
	}
	DEBUG_PRINT2("\n");

	if (argc < 2) {
		fprintf(stderr, "Falta de argumentos ./dropboxServer id           (id = 1,2,3,4) \n");
		return 0;
	}

	id = atoi(argv[1]);
	server = getServer(id);
	thisServer = server;

	if(thisServer == NULL) {
        printf("O servidor com ID %d nao existe\n", id);
        return 0;
	}

	while (TRUE) {
        pthread_create(&bully_thread, NULL, bullyThread, NULL);

        if (thisServer->id == coordinatorId) {
            pthread_create(&coordinator_thread, NULL, coordinatorThread, NULL);
            coordinatorElectionFunction();
        } else {
        	pthread_create(&replica_thread, NULL, replicaManagerThread, NULL);
            replicaElectionFunction();
        }
	}

	return 0;
}

void *coordinatorThread() { //MAIN DA PT1 DO TRABALHO
	int sockfd;
	struct sockaddr_in serv_addr;
	int *port_count = malloc(sizeof(int));
	Packet *buffer = malloc(sizeof(Packet));
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

		receivePacket(&connection, buffer, seqnumReceive);
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
		} else if(buffer->type == CMD){
			// new client socket--
			createClientFolder(client->username);
			pthread_create(&th1, NULL, clientThread, (void*) port_count);
			sem_wait(&portBarrier);
		} else{
			if (approveClient(client, &client_list)) {
				if(buffer->type == BROADCAST){
					addClient(client, &client_list);
					send_new_client_to_replicas(client->username, &(client->addr[0]));
					*port_count = *port_count - 1;	//no new port is assigned for a broadcast
				}
			}else{
				DEBUG_PRINT("O CLIENTE JA ESTA USANDO DOIS DISPOSITIVOS\n");
				strcpy(portMapper, ACCESS_ERROR);
				*port_count = *port_count - 1;
			}
		}

		Packet *p = newPacket(CMD,client->username,seqnumSend,0,portMapper);
		sendPacket(p,&connection, NOT_LIMITED);
		seqnumSend = 1 - seqnumSend;
	}

	close(sockfd);
	return NULL;
}

void* coordinatorElectionFunction() {
    int sockfd, n;
	socklen_t rmlen;
	struct sockaddr_in serv_addr, rm_addr;
	char buf[256];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(CRASHED_TESTER_PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

	rmlen = sizeof(struct sockaddr_in);

	DEBUG_PRINT2("BULLY - FUNÇÃO PARA TESTAR DISPONIBILIDADE DO COORDENADOR RODANDO\n");

	while (1) {
        bzero(buf, 256);

		n = recvfrom(sockfd, buf, 256, 0, (struct sockaddr *) &rm_addr, &rmlen);
		if (n < 0)
			printf("ERROR on recvfrom");
		printf("SYSTEM: %s\n", buf);

		if(strcmp(buf,"OK") == 0) {

            n = sendto(sockfd, "ACK", 3, 0,(struct sockaddr *) &rm_addr, sizeof(struct sockaddr));
            if (n  < 0)
                printf("ERROR on sendto");
		}
	}
	close(sockfd);
}

void* replicaElectionFunction() {
    int sockfd, n, status;
    int *res;
    int changedCoordinator = FALSE;
    Server *coordinator;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *sv;
	char buffer[256];

	coordinator = getCoordinator();

	sv = gethostbyname(coordinator->ip);
	if (sv == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");
    setTimeout(sockfd);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(CRASHED_TESTER_PORT);
	serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	DEBUG_PRINT2("BULLY - FUNÇÃO PARA TESTAR DISPONIBILIDADE DO COORDENADOR RODANDO\n");

	while(!changedCoordinator) {

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
             coordinatorId = *res;
             changedCoordinator = TRUE;


            /*

         	if(thisServer->id == coordinatorId){
         		// this server was elected the coordinator
         		announce_election_to_clients();
         		// cancel the thread listening to the primary server commands
         		pthread_cancel(replica_thread);
         	}

         	*/

         	if(thisServer->id == coordinatorId){
         		// this server was elected the coordinator
         		//announce_election_to_clients();
         		printf("anuncia para os clientes ser novo coordenador...\n");
         	}

         	// cancel the thread listening to the primary server commands
         	pthread_cancel(replica_thread); //tem que ver que o socket vai ficar pendurado


        } else {
            printf("SYSTEM: %s\n", buffer);
            if(strcmp(buffer, "ACK") == 0) {
                sleep(3);
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
	Packet *p = newPacket(operation,username,0,0,file);
	sendPacket(p,&connection, NOT_LIMITED);

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

		Packet *request = malloc(sizeof(Packet));
		receivePacket(connection, request, seqnum);
		seqnum = 1 - seqnum;

		switch(request->type){
			case UPLOAD:
				//printf("Processing Upload of file %s for user %s\n",request->data,request->user);
				receiveFile(connection, &buffer, &file_size);
				//printf("Received file of size: %d\n",file_size);
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				saveFile(buffer, file_size, file_path);
				send_upload_to_replicas(request->user, file_path);
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
					send_delete_to_replicas(request->user, file_path);
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

		Packet *request = malloc(sizeof(Packet));
		receivePacket(connection, request, seqnum);
		seqnum = 1 - seqnum;

		switch(request->type){
			case UPLOAD:
				receiveFile(connection, &buffer, &file_size);
				if(file_size != 0){
					file_path = makePath(request->user,request->data);
					file_path = makePath(server_folder, file_path);
					saveFile(buffer, file_size, file_path);
					send_upload_to_replicas(request->user, file_path);
					broadcastUnique(UPLOAD, request->data, request->user, *connection->address);
				}
				break;
			case DELETE:
				file_path = makePath(request->user,request->data);
				file_path = makePath(server_folder, file_path);
				if(remove(file_path) == 0){
					send_delete_to_replicas(request->user,file_path);
					broadcastUnique(DELETE,request->data, request->user, *connection->address);
				}
				break;
			case DOWNLOAD_ALL:
				nbFiles = countFiles(request->user);
				itoa(nbFiles, nbFilesBuffer);
				Packet *p = newPacket(CMD, request->user, seqnumSend, 0, nbFilesBuffer);
				seqnumSend = 1 - seqnumSend;
				sendPacket(p, connection, NOT_LIMITED);
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
    Packet *p;

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
            	p = newPacket(CMD, username, seqnum, 0, dp->d_name);
                sendPacket(p, connection, NOT_LIMITED);
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
	Packet *p;

	for(i = 0; i < MAX_LIST_SIZE/DATA_SEGMENT_SIZE; i++){
		memcpy(buf,s+i*(DATA_SEGMENT_SIZE-1),DATA_SEGMENT_SIZE-1);
		buf[DATA_SEGMENT_SIZE-1] = '\0';
		p = newPacket(DATA, username,LIST_START_SEQ+i,0,buf);
		sendPacket(p, connection, NOT_LIMITED);
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
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int i = 0;


    ServerList *auxList;
    Server **sv = malloc(4 * sizeof(Server));
    for (i = 0; i < 4; i++) {
        sv[i] = malloc(sizeof(Server));
    }

    fp = fopen("../resource/SVList.txt", "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    i = 0;

    while ((read = getline(&line, &len, fp)) != -1) {
        sv[i]->id = i + 1;
        chomp(line);
        strcpy(sv[i]->ip, line);
        read = getline(&line, &len, fp);
        sv[i]->bullyPort = atoi(line);
        i++;
    }

    fclose(fp);
    if (line) {
        free(line);
    }

    auxList = malloc(sizeof(ServerList));
    auxList->server = sv[3];
    auxList->next = NULL;
    svList = auxList;
    auxList = malloc(sizeof(ServerList));
    auxList->server = sv[2];
    auxList->next = NULL;
    svList->next = auxList;
    auxList = malloc(sizeof(ServerList));
    auxList->server = sv[1];
    auxList->next = NULL;
    svList->next->next = auxList;
    auxList = malloc(sizeof(ServerList));
    auxList->server = sv[0];
    auxList->next = NULL;
    svList->next->next->next = auxList;
}

Server* getCoordinator() {
    ServerList *auxList = svList;

	while(auxList != NULL) {
        if (coordinatorId == auxList->server->id) {
            return auxList->server;
        }
        auxList = auxList->next;
	}

	return NULL;
}

void sendCoordinatorMessage() {
    int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *sv;
	char data[256];
	bzero(data, 256);
	ServerList *auxList;

	itoa(thisServer->id, data);
	Packet *bufferSend = newPacket(COORDINATOR, "bully", 0, 0, data);

	auxList = svList;
	while (auxList != NULL) {
        if (thisServer->id < auxList->server->id) {

            sv = gethostbyname(auxList->server->ip);
            if (sv == NULL) {
                fprintf(stderr,"ERROR, no such host\n");
                exit(0);
            }
            if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                printf("ERROR opening socket");
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(auxList->server->bullyPort);
            serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
            bzero(&(serv_addr.sin_zero), 8);
            sendto(sockfd, bufferSend, PACKET_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
            close(sockfd);
        }
        auxList = auxList->next;
	}
}

void *testCoordinator() {
    Server* coordinator;
    int sockfd;
    int verifyCoordinator = FALSE;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *sv;
	char data[256];
	bzero(data, 256);
	Packet *bufferReceive = malloc(PACKET_SIZE);
	Packet *tester = newPacket(TESTER, "bully", 0, 0, data);
	Packet *test_fail = newPacket(TEST_FAIL, "bully", 0, 0, data);
	Packet *test_ok = newPacket(TEST_OK, "bully", 0, 0, data);

	coordinator = getCoordinator();

	sv = gethostbyname(coordinator->ip);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");
    setTimeout(sockfd);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(coordinator->bullyPort);
	serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	sendto(sockfd, tester, PACKET_SIZE, 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	if (recvfrom(sockfd, bufferReceive, PACKET_SIZE, 0, (struct sockaddr *) &from, &length) >= 0) {
        if (bufferReceive->type == ACK) {
            verifyCoordinator = TRUE;
        }
	}
	close(sockfd);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                printf("ERROR opening socket");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(thisServer->bullyPort);
    serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

	if (verifyCoordinator) {
        sendto(sockfd, test_ok, PACKET_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	} else {
        sendto(sockfd, test_fail, PACKET_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	}

	close(sockfd);
	pthread_exit(NULL);
}

void *startElection() {
    int sockfd, receiveAnswer;
    socklen_t rmlen;
	struct sockaddr_in serv_addr, rm_addr;
	struct hostent *sv;
	char data[256];
	bzero(data, 256);
	ServerList *auxList;

	Packet *bufferSendCoordinator = newPacket(SEND_COORDINATOR, "bully", 0, 0, data);
	Packet *bufferWaitCoordinator = newPacket(WAITING_NEW_COORDINATOR, "bully", 0, 0, data);
	Packet *bufferElection = newPacket(ELECTION, "bully", 0, 0, data);
	Packet *bufferReceive = malloc(PACKET_SIZE);

    rmlen = sizeof(struct sockaddr_in);
	auxList = svList;
	receiveAnswer = FALSE;
	while (auxList != NULL && !receiveAnswer) {
        if (thisServer->id > auxList->server->id) {

            sv = gethostbyname(auxList->server->ip);
            if (sv == NULL) {
                fprintf(stderr,"ERROR, no such host\n");
                exit(0);
            }
            if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                printf("ERROR opening socket");
            setTimeout(sockfd);
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(auxList->server->bullyPort);
            serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
            bzero(&(serv_addr.sin_zero), 8);

            sendto(sockfd, bufferElection, PACKET_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
            if (recvfrom(sockfd, bufferReceive, PACKET_SIZE, 0, (struct sockaddr *) &rm_addr, &rmlen) >= 0) {
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
    serv_addr.sin_port = htons(thisServer->bullyPort);
    serv_addr.sin_addr = *((struct in_addr *)sv->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

	if(receiveAnswer) {
        DEBUG_PRINT2("BULLY - AGUARDANDO MENSAGEM PROCLAMANDO O NOVO COORDENADOR\n");
        sendto(sockfd, bufferWaitCoordinator, PACKET_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	} else {
        DEBUG_PRINT2("BULLY - SOU O NOVO COORDENADOR\n");
        sendto(sockfd, bufferSendCoordinator, PACKET_SIZE, 0, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	}

	close(sockfd);
	pthread_exit(NULL);
}

void *bullyThread() {
    int sockfd;
    pthread_t th1, th2;
    int electionStatus = WITHOUT_ELECTION;
    int *newCoordinator = (int*) malloc(sizeof(int));
	socklen_t rmlen;
	struct sockaddr_in serv_addr, rm_addr;
	char data[256];
	bzero(data, 256);
	Packet *bufferReceive = malloc(PACKET_SIZE);
	Packet *ack = newPacket(ACK, "bully", 0, 0, data);
	Packet *asnwer = newPacket(ANSWER, "bully", 0, 0, data);

	sleep(TIMEOUT_REESTABLISH_SERVERS);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(thisServer->bullyPort);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

	rmlen = sizeof(struct sockaddr_in);

	DEBUG_PRINT2("BULLY - A THREAD ESTÁ RODANDO NA PORTA %d\n", thisServer->bullyPort);

	if (thisServer->id == coordinatorId) {
        while(TRUE) {
            recvfrom(sockfd, bufferReceive, PACKET_SIZE, 0, (struct sockaddr *) &rm_addr, &rmlen);
            if (bufferReceive->type == TESTER) {
                sendto(sockfd, ack, PACKET_SIZE, 0,(struct sockaddr *) &rm_addr, sizeof(struct sockaddr));
            }
        }
	} else {
	    setTimeoutElection(sockfd);
        while (TRUE) {
            if(recvfrom(sockfd, bufferReceive, PACKET_SIZE, 0, (struct sockaddr *) &rm_addr, &rmlen) < 0) {
                if (electionStatus == WAITING_NEW_COORDINATOR) {
                    DEBUG_PRINT2("BULLY - TIMEOUT NA ESPERA DO COORDENADOR\n"); //VERIFICAR DEPOIS
                } else if (electionStatus == WITHOUT_ELECTION) {
                    pthread_create(&th1, NULL, testCoordinator, NULL);
                }
            } else {
                DEBUG_PRINT2("BULLY - NOVO PACOTE\n");
                if(bufferReceive->type == ELECTION) {
                    if (electionStatus == WITHOUT_ELECTION) {
                        sendto(sockfd, asnwer, PACKET_SIZE, 0,(struct sockaddr *) &rm_addr, sizeof(struct sockaddr));
                        DEBUG_PRINT2("BULLY - ENVIADO ASNWER E INICIADO PRÓPRIA ELEIÇÃO\n");
                        electionStatus = IN_ELECTION;
                        pthread_create(&th2, NULL, startElection, NULL);
                    } else {
                        DEBUG_PRINT2("BULLY - RECEBEU UM ELECTION ATRASADO\n");
                        sendto(sockfd, asnwer, PACKET_SIZE, 0,(struct sockaddr *) &rm_addr, sizeof(struct sockaddr)); //manda ack pra não ficar se elegendo
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
                    sendCoordinatorMessage();
                    *newCoordinator = thisServer->id;
                    close(sockfd);
                    pthread_exit(newCoordinator);
                }
                if (bufferReceive->type == WAITING_NEW_COORDINATOR) {
                    electionStatus = WAITING_NEW_COORDINATOR;
                }
                if (bufferReceive->type == TEST_FAIL) {
                    DEBUG_PRINT2("BULLY - COORDENADOR CAIU, INICIANDO ELEIÇÃO\n");
                    electionStatus = IN_ELECTION;
                    pthread_create(&th2, NULL, startElection, NULL);
                }
                if (bufferReceive->type == TEST_OK) {
                    DEBUG_PRINT2("BULLY - COORDENADOR OK\n");
                }
            }
        }
	}
}

int send_delete_to_replicas(char* user, char* file_path){
	char* filename = basename(file_path);
	int seqNumber = 0, sockfd;
	struct sockaddr_in repl_addr;
	ServerList *current;
	struct hostent *bk_server;

	Connection *connectionDel = malloc(sizeof(Connection));

	/* TODO: This is currently sequential, could be improved by creating n parallel threads
	   each thread uploading the file to one of the backups */
	pthread_mutex_lock(&serverListMutex);
	current = svList;
	while(current != NULL){
		if(current->server->id == coordinatorId) continue;

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
			fprintf(stderr, "ERROR opening socket");

		bk_server = gethostbyname(current->server->ip);
		repl_addr.sin_family = AF_INET;
		repl_addr.sin_port = htons(REPLICA_PORT);
		repl_addr.sin_addr = *((struct in_addr *)bk_server->h_addr);
		bzero(&(repl_addr.sin_zero), 8);

		if (bind(sockfd, (struct sockaddr *) &repl_addr, sizeof(struct sockaddr)) < 0)
			fprintf(stderr, "ERROR on binding");

		connectionDel->socket = sockfd;
		connectionDel->address = &repl_addr;

		Packet *commandPacket = newPacket(DELETE,user,seqNumber,0,filename);
		if(sendPacket(commandPacket, connectionDel, LIMITED) == FAILURE){
			close(sockfd);
			continue;
		}
		seqNumber = 1 - seqNumber;

		close(sockfd);
	}
	pthread_mutex_unlock(&serverListMutex);

	return SUCCESS;
}

int send_upload_to_replicas(char* user, char* file_path){
	char* filename = basename(file_path);
	int seqNumber = 0, sockfd;
	struct sockaddr_in repl_addr;
	ServerList *current;
	struct hostent *bk_server;

	Connection *connectionUp = malloc(sizeof(Connection));

	/* TODO: This is currently sequential, could be improved by creating n parallel threads
	   each thread uploading the file to one of the backups */
	pthread_mutex_lock(&serverListMutex);
	current = svList;
	while(current != NULL){
		if(current->server->id == coordinatorId) continue;

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
			fprintf(stderr, "ERROR opening socket");

		bk_server = gethostbyname(current->server->ip);
		repl_addr.sin_family = AF_INET;
		repl_addr.sin_port = htons(REPLICA_PORT);
		repl_addr.sin_addr = *((struct in_addr *)bk_server->h_addr);
		bzero(&(repl_addr.sin_zero), 8);

		if (bind(sockfd, (struct sockaddr *) &repl_addr, sizeof(struct sockaddr)) < 0)
			fprintf(stderr, "ERROR on binding");

		connectionUp->socket = sockfd;
		connectionUp->address = &repl_addr;

		Packet *commandPacket = newPacket(UPLOAD,user,seqNumber,0,filename);
		if(sendPacket(commandPacket, connectionUp, LIMITED) == FAILURE){
			close(sockfd);
			continue;
		}
		seqNumber = 1 - seqNumber;

		sendFile(file_path, connectionUp, user);

		close(sockfd);
	}
	pthread_mutex_unlock(&serverListMutex);

	return SUCCESS;
}

int send_new_client_to_replicas(char* user, struct sockaddr_in* addr_client){
	int seqNumber = 0, sockfd;
	struct sockaddr_in repl_addr;
	ServerList *current;
	struct hostent *bk_server;
	char data[sizeof(Packet)];

	Connection *connectionCli = malloc(sizeof(Connection));

	/* TODO: This is currently sequential, could be improved by creating n parallel threads
	   each thread uploading the file to one of the backups */
	pthread_mutex_lock(&serverListMutex);
	current = svList;
	while(current != NULL){
		if(current->server->id == coordinatorId) continue;

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
			fprintf(stderr, "ERROR opening socket");

		bk_server = gethostbyname(current->server->ip);
		repl_addr.sin_family = AF_INET;
		repl_addr.sin_port = htons(REPLICA_PORT);
		repl_addr.sin_addr = *((struct in_addr *)bk_server->h_addr);
		bzero(&(repl_addr.sin_zero), 8);

		if (bind(sockfd, (struct sockaddr *) &repl_addr, sizeof(struct sockaddr)) < 0)
			fprintf(stderr, "ERROR on binding");

		connectionCli->socket = sockfd;
		connectionCli->address = &repl_addr;

		bzero(data, sizeof(Packet));
		memcpy(data, addr_client, sizeof(struct sockaddr_in));

		Packet *commandPacket = newPacket(NEW_CLIENT,user,seqNumber,0,data);
		if(sendPacket(commandPacket, connectionCli, LIMITED) == FAILURE){
			close(sockfd);
			continue;
		}
		seqNumber = 1 - seqNumber;

		close(sockfd);
	}
	pthread_mutex_unlock(&serverListMutex);

	return SUCCESS;
}

// thread to listen for requests coming from the coordinator
// to duplicate all upload and delete operations in the backup servers
void* replicaManagerThread(){
	int sockfd, file_size;
	char *buffer = NULL, *file_path = NULL;
	struct sockaddr_in repl_addr;
	Client* client;
	Connection *connectionRM = malloc(sizeof(Connection));
	int seqnumReceive = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fprintf(stderr, "ERROR opening socket");

	repl_addr.sin_family = AF_INET;
	//repl_addr.sin_port = htons(REPLICA_PORT);
	repl_addr.sin_port = htons(thisServer->bullyPort+4187); //TESTANDO LOCAL
	repl_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(repl_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &repl_addr, sizeof(struct sockaddr)) < 0)
		fprintf(stderr, "ERROR on binding8");

	connectionRM->socket = sockfd;
	connectionRM->address = &repl_addr;

	while (TRUE) {
		seqnumReceive = 0;
		Packet *request = malloc(sizeof(Packet));
		receivePacket(connectionRM, request, seqnumReceive);
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
				client = malloc(sizeof(Client));
				strcpy(client->username, request->user);
				memcpy(&client->addr, request->data, sizeof(struct sockaddr_in));
				addClient(client, &client_list);
				break;
			default: fprintf(stderr, "Invalid command number %d for RM thread\n", request->type);
		}
	}
}

void announce_election_to_clients(){
	int seqnum, socket_ann;
	Packet *packet;
	struct sockaddr_in cli_addr;
	Connection *connectionCli = malloc(sizeof(Connection));

	pthread_mutex_lock(&clientListMutex);

	ClientList *current = client_list;

	while(current != NULL){
		for(int i = 0; i < 2; i++){
	 		if(current->client->devices[i] == LOGGED){
	 			seqnum = 0;

	     		if ((socket_ann = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
					printf("ERROR opening socket");

				cli_addr = current->client->addr[i];
				bzero(&(cli_addr.sin_zero), 8);

				connectionCli->socket = socket_ann;
				connectionCli->address = &cli_addr;

				packet = newPacket(ANNOUNCE_ELECTION,current->client->username,seqnum,0,INFO_ELECTION);
				sendPacket(packet, connectionCli, NOT_LIMITED);
				seqnum = 1 - seqnum;

				close(socket_ann);
		    }
		}
	    current = current->next;
	}
	pthread_mutex_lock(&clientListMutex);
}
