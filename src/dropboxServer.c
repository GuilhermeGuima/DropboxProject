#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

ClientList *client_list;

int main(int argc, char *argv[]) {
	int sockfd;
	struct sockaddr_in serv_addr;
	int *port_count = malloc(sizeof(int));
	Package *buffer = malloc(sizeof(Package));
	char portMapper[DATA_SEGMENT_SIZE];
	pthread_t th1;
	Client *client;
	int seqnumSend = 0, seqnumReceive = 0;

	initializeClientList();

	DEBUG_PRINT("OPÇÃO DE DEBUG ATIVADA\n");

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
		client = newClient(buffer->data);
		bzero(portMapper, DATA_SEGMENT_SIZE);
		itoa(*port_count, portMapper);

		if (approveClient(client, &client_list, *port_count)) {
			pthread_create(&th1, NULL, clientThread, (void*) port_count);
		} else {
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

void *clientThread(void *arg) {
	int port = *(int*) arg;
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
				file_path = makePath(request->user,request->data);
				saveFile(buffer, file_size, file_path);
				break;
			case DOWNLOAD:
				printf("Processing Download of file %s for user %s\n",request->data,request->user);
				file_path = makePath(request->user,request->data);
				printf("File_path_to_download: %s\n", file_path);
				sendFile(file_path, connection, request->user);
				break;
			case DELETE:
				printf("Deleting file %s for user %s\n",request->data,request->user);
				break;
			case LISTSERVER:
				printf("Listing files for user %s\n",request->user);
				break;
			case EXIT:
				printf("Processing user %s\n logout", request->user);
				client_list = removeClient(request->user, port);
				break;
			default: printf("Invalid command number %d\n", request->type);
		}
	}
}

Client* newClient(char* username) {
	Client *client = malloc(sizeof(*client));
	client->logged = TRUE;
	strcpy(client->username , username);
	return client;
}

void initializeClientList() {
	client_list = NULL;
}

int approveClient(Client* client, ClientList** client_list, int port) {
    ClientList *current = *client_list;
    ClientList *last = *client_list;

    while(current != NULL){
        if (strcmp(current->client->username, client->username) == 0) {
        	// both in use
            if(current->client->devices[0] != INVALID && current->client->devices[1] != INVALID){
            	return FALSE;
            } else {
            	DEBUG_PRINT("ADDED NEW DEVICE\n");
            	//at least one is free
            	if(current->client->devices[0] == INVALID) current->client->devices[0] = port;
            	else current->client->devices[1] = port;
            	return TRUE;
            }
        }
        last = current;
        current = current->next;
    }

    // user isnt logged in yet
  	ClientList *new_client = malloc(sizeof(ClientList));
  	client->devices[0] = port;
  	client->devices[1] = INVALID;
  	new_client->client = client;
  	new_client->next = NULL;

  	if(*client_list == NULL)
  		//first element of list
  		*client_list = new_client;
  	else	
  		last->next = new_client;

  	DEBUG_PRINT("ADDED NEW CLIENT\n");
  	return TRUE;
}

ClientList* addClient(Client* client, ClientList* client_list) {
    ClientList *current = client_list;
    ClientList *new_client = malloc(sizeof(ClientList));

    new_client->client = client;
    new_client->next = NULL;

    if (client_list == NULL) {
        client_list = new_client;
        return client_list;
    }

    while(current->next != NULL) {
        current = current->next;
    }

    current->next = new_client;
    return client_list;
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

