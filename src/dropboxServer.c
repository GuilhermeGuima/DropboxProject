#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

int main(int argc, char *argv[]) {
	int sockfd, n;
	int port_count;
	char buffer[256];
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	Package *package = malloc(sizeof(*package));
	Client *client;
	pthread_t th1;

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


		n = recvfrom(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *) &cli_addr, &clilen);
		if (n < 0)
			printf("ERROR on recvfrom");

        port_count++;
        client = newClient(buffer, port_count);
		bzero(buffer, 256);
		itoa(port_count, buffer);
		printf("%s\n", buffer);

		n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *) &cli_addr, sizeof(struct sockaddr));
		if (n < 0)
			printf("ERROR on sendto");

        pthread_create(&th1, NULL, clientThread, (void*) client);

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
	Package *package = malloc(sizeof(*package));

	DEBUG_PRINT("Porta %d\n", client->port);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(client->port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		printf("ERROR on binding");

    clilen = sizeof(struct sockaddr_in);

    while (1) {

        sleep(2);

        DEBUG_PRINT("ENTROU NO WHILE DA CLIENT THREAD\n");
        DEBUG_PRINT("PORTA DO CLIENTE %d\n", client->port);

	}
}

Client* newClient(char* username, int port) {

    Client *client = malloc(sizeof(*client));
	client->devices[0] = 1;
	client->devices[1] = 0;
	client->logged = 1;
	strcpy(client->username , username);
    client->port = port;

    return client;
}



