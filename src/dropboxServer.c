#include "../include/dropboxUtil.h"
#include "../include/dropboxServer.h"

int main(int argc, char *argv[])
{
	int sockfd, n;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	Package *package = malloc(sizeof(*package));

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

	while (TRUE) {
		n = recvfrom(sockfd, package, PACKAGE_SIZE, 0, (struct sockaddr *) &cli_addr, &clilen);
		if (n < 0)
			printf("ERROR on recvfrom");
		printPackage(package);

		n = sendto(sockfd, "Got your message\n", 17, 0,(struct sockaddr *) &cli_addr, sizeof(struct sockaddr));
		if (n < 0)
			printf("ERROR on sendto");
	}

	close(sockfd);

	return 0;
}
