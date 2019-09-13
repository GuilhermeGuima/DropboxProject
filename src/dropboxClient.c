#include "../include/dropboxClient.h"
#include "../include/dropboxUtil.h"

int main(int argc, char *argv[])
{
    int sockfd, n;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *server;
	char buffer[256];

	DEBUG_PRINT("OPÇÃO DE DEBUG ATIVADA\n");

	Package *package = new_package("Carlos", 1, 3000, "Testando Mensagem!");
	print_package(package);

	if (argc < 2) {
		fprintf(stderr, "usage %s hostname\n", argv[0]);
		exit(0);

	}

	server = gethostbyname(argv[1]);
	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	bzero(buffer, 256);

	send_package(sockfd, &serv_addr, package);

	length = sizeof(struct sockaddr_in);
	n = recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *) &from, &length);
	if (n < 0)
		printf("ERROR recvfrom");

	printf("Got an ack: %s\n", buffer);

	close(sockfd);
	return 0;
}
