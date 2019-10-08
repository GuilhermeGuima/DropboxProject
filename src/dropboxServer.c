#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

ClientList *client_list;

int main(int argc, char *argv[]) {
	int sockfd, n;
	int port_count;
	char buffer[256];
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	Package *package = malloc(sizeof(*package));
	pthread_t th1;
	Client *client;

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

	clilen = sizeof(struct sockaddr_in);

	port_count = PORT;

	while (TRUE) {
		printf("Esperando conexões de novos clientes\n");

		n = recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *) &cli_addr, &clilen);
		if (n < 0)
			printf("ERROR on recvfrom");

		port_count++;
		client = newClient(buffer, port_count);
		bzero(buffer, 256);
		itoa(port_count, buffer);
		printf("%s\n", buffer);

		if (approveClient(client, client_list)) {
			pthread_create(&th1, NULL, clientThread, (void*) client);
		} else {
			DEBUG_PRINT("O CLIENTE NÃO FOI APROVADO PARA ACESSAR A LISTA");
			strcpy(buffer, ACCESS_ERROR);
			port_count--;
		}

		n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *) &cli_addr, sizeof(struct sockaddr));
		if (n < 0)
			printf("ERROR on sendto");
	}

	close(sockfd);
	return SUCCESS;
}

void *clientThread(void *arg) {
	Client *client = (Client*) arg;
	int sockfd, n;
	char buffer[256];
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	Package *package = malloc(sizeof(Package));
	Connection *connection = malloc(sizeof(Connection));
	int seqnum = 0;

	DEBUG_PRINT("Porta %d\n", client->port);

	client_list = addClient(client, client_list);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(client->port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

	clilen = sizeof(struct sockaddr_in);

	connection->socket = sockfd;
	connection->address = &serv_addr;

	while (TRUE) {
		sleep(2);

		DEBUG_PRINT("ENTROU NO WHILE DA CLIENT THREAD\n");
		DEBUG_PRINT("PORTA DO CLIENTE %d\n", client->port);

		Package *request = malloc(sizeof(Package));
		receivePackage(connection, request, seqnum);

		switch(request->type){
			case UPLOAD:
				printf("Uploading file %s for user %s\n",request->data,request->user);
				break;
			case DOWNLOAD:
				printf("Sending file %s for user %s\n",request->data,request->user);
				break;
			case DELETE:
				printf("Deleting file %s for user %s\n",request->data,request->user);
				break;
			case LISTSERVER:
				printf("Listing files for user %s\n",request->user);
				break;
			case EXIT:
				printf("Logging out of user %s\n", request->user);
				break;
			default: printf("Invalid command number %d\n", request->type);
		}

		seqnum = 1 - seqnum;
	}
}

Client* newClient(char* username, int port) {
	Client *client = malloc(sizeof(*client));
	client->logged = 0;
	strcpy(client->username , username);
	client->port = port;
	return client;
}

void initializeClientList() {
	client_list = NULL;
}

int approveClient(Client* client, ClientList* client_list) {
    int result = 0;
    ClientList *current = client_list;

    while(current != NULL) {
        if (strcmp(current->client->username, client->username) == 0) {
            result++;
            if (current->client->port == client->port) {
                return FALSE;
            }
        }
        current = current->next;
    }

    if (result >= 2) {
        return FALSE;
    }

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

ClientList* removeClient(Client* client, ClientList* client_list) {
    ClientList *current = client_list;
    ClientList *prev_client = NULL;

    while(current != NULL) {
        if (strcmp(current->client->username, client->username) == 0) {
            if (current->client->port == client->port ) {
                if (prev_client != NULL) {
                    prev_client->next = current->next;
                } else {
                    client_list = current->next;
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
        printf("%d - %s\n", index, current->client->username);
        current = current->next;
        index++;
    }
}

