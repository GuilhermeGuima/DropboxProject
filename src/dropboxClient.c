#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

int main(int argc, char *argv[]) {
    int sockfd, n;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *server;
	char buffer[256];
	Connection *connection = malloc(sizeof(*connection));

	DEBUG_PRINT("OPÇÃO DE DEBUG ATIVADA\n");

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

	connection->socket = sockfd;
    connection->adress = &serv_addr;

    sendFile("text.txt", connection);

	length = sizeof(struct sockaddr_in);
	n = recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *) &from, &length);
	if (n < 0)
		printf("ERROR recvfrom");

	printf("Got an ack: %s\n", buffer);

	close(sockfd);

	return 0;
}

void sendFile(char *file, Connection *connection) {
    FILE* pFile;
    Package *package;
    int file_size = 0;
    int total_send = 0;
    unsigned short int seq = 0;
    unsigned short int length = 0;
    char data[DATA_SEGMENT_SIZE];

    DEBUG_PRINT("INICIANDO FUNÇÃO \"sendFile\" PARA ENVIO DE ARQUIVOS\n");
    pFile = fopen(file, "rb");
    if(pFile) {
        file_size = getFileSize(file);
        length = floor(file_size/DATA_SEGMENT_SIZE);
        DEBUG_PRINT("TAMANHO DO ARQUIVO: %d\n", file_size);
        for ( total_send = 0 ; total_send < file_size ; total_send = total_send + DATA_SEGMENT_SIZE ) {
            bzero(data, DATA_SEGMENT_SIZE);
            if ( (file_size - total_send) < DATA_SEGMENT_SIZE ) {
                fread(data, sizeof(char), (file_size - total_send), pFile);
            }
            else {
                fread(data, sizeof(char), DATA_SEGMENT_SIZE, pFile);
            }

            package = newPackage("Guilherme", seq, length, data);
            sendPackage(connection->socket, connection->adress, package);
            seq++;
        }
        fclose(pFile);
    }
}
