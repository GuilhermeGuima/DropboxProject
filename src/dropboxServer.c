#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 4000

void clientsThread(void* arg) {

    char buffer[256];
    int n;
    int newsockfd = *(int*)arg;

    bzero(buffer, 256);

    /* read from the socket */
    n = read(newsockfd, buffer, 256);
    if (n < 0)
        printf("ERROR reading from socket");
    printf("A mensagem do cliente %d eh: %s\n", newsockfd, buffer);
    /* write in the socket */
    n = write(newsockfd,"I got your message", 18);
    if (n < 0)
        printf("ERROR writing to socket");
}

int main(int argc, char *argv[]) {
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	pthread_t thread_id;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		printf("ERROR on binding");

	listen(sockfd, 5);

	while(1) {

        printf("\n\n--- SERVIDOR NO AR ESPERANDO CONEXÕES ---\n\n");

        clilen = sizeof(struct sockaddr_in);
        if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1)
            printf("ERROR on accept");

        printf("O CLIENTE %d ESTA CONECTADO \n", newsockfd);

        if (pthread_create(&thread_id, NULL, clientsThread, (void*) &newsockfd) < 0) {
            printf("Error on create thread\n");
        }
    }

	close(newsockfd);
	close(sockfd);
	return 0;
}
