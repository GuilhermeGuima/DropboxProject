#include "../include/dropboxUtil.h"
#include "../include/dropboxClient.h"

int main(int argc, char *argv[]) {
    int sockfd, n;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *server;
	char buffer[256];
	char folder[256];
	char user[USER_NAME_SIZE];
	Connection *connection = malloc(sizeof(*connection));

	DEBUG_PRINT("OPÇÃO DE DEBUG ATIVADA\n");

	if (argc < 3) {
        printf("Falta de argumentos ./dropboxClient user endereço");
        return FAILURE;
    }

	server = gethostbyname(argv[2]);
	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        return FAILURE;
    }

    if (strlen(argv[1]) <= USER_NAME_SIZE) {
        strcpy(user, argv[1]);
        strcpy(folder, getUserHome());
        strcat(folder, "/dropbox_dir_");
        strcat(folder, user);
    } else {
        printf("O tamanho máximo para o novo usuário é %d", USER_NAME_SIZE);
        return FAILURE;
    }

    DEBUG_PRINT("User: %s\n", user);
    DEBUG_PRINT("Folder: %s\n", folder);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	connection->socket = sockfd;
    connection->adress = &serv_addr;

	length = sizeof(struct sockaddr_in);

	// continuar

	close(sockfd);

	return SUCCESS;
}

int firstConnection(char *user, char *folder, Connection *connection) {
    Package *package;
    char data[DATA_SEGMENT_SIZE];
    bzero(data, DATA_SEGMENT_SIZE);
    data[0] = CMD_CONNECT;
    package = newPackage(CMD, user, 0, 0, data);
    sendPackage(package, connection);
    // continuar
    return SUCCESS;
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

            package = newPackage(CMD, "username", seq, length, data);
            sendPackage(package, connection);
            seq++;
        }
        fclose(pFile);
    }
}
