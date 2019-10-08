#include "../include/dropboxClient.h"

int notifyWatcher, notifyFile;
struct hostent *server;
char folder[MAX_PATH];
Connection *connection;
char user[USER_NAME_SIZE];
int seqnum = 0;

int main(int argc, char *argv[]) {
	int port;
	Connection *firstConn = malloc(sizeof(Connection));

	DEBUG_PRINT("OPÇÃO DE DEBUG ATIVADA\n");

	if (argc < 3) {
		printf("Falta de argumentos ./dropboxClient user endereço\n");
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
		strcat(folder, "/sync_dir_");
		strcat(folder, user);
	} else {
		printf("O tamanho máximo para o novo usuário é %d\n", USER_NAME_SIZE);
		return FAILURE;
	}

	DEBUG_PRINT("User: %s\n", user);
	DEBUG_PRINT("Folder: %s\n", folder);


	firstConn = getConnection(PORT);

	port = firstConnection(user, firstConn);
	connection = getConnection(port);

	if (port > 0) {

		getSyncDir();

		selectCommand();
	}

	close(connection->socket);

	inotify_rm_watch(notifyFile, notifyWatcher); // removes the directory from inotify watcher
	close(notifyFile); //close inotify file

	return SUCCESS;
}

Connection* getConnection(int port){
	int sockfd;
	Connection *connection = malloc(sizeof(*connection));
	struct sockaddr_in *serv_addr = malloc(sizeof(serv_addr));

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fprintf(stderr, "ERROR opening socket\n");
	setTimeout(sockfd);

	serv_addr->sin_family = AF_INET;
	serv_addr->sin_port = htons(port);
	serv_addr->sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr->sin_zero), 8);

	connection->socket = sockfd;
	connection->address = serv_addr;

	return connection;
}

void selectCommand() {
	char command[12 + MAX_PATH];
	char path[MAX_PATH];
	char *valid;

	sleep(2);
	printf("\n\nComandos disponíveis:\n\nupload <file>\ndownload <file>\ndelete <file>\nlist_server\nlist_client\nexit\n\n");

	do {
		printf("\nDigite seu comando: ");

		valid = fgets(command, sizeof(command)-1, stdin);
		if(valid != NULL) {
			command[strcspn(command, "\r\n")] = 0;
		}

		DEBUG_PRINT("Comando digitado: %s\n", command);

		if(strncmp(command, CMD_UPLOAD, CMD_UPLOAD_LEN) == 0) {
			DEBUG_PRINT("Detectado comando upload\n");

			strcpy(path, command+CMD_UPLOAD_LEN);
			DEBUG_PRINT("Parâmetro do upload: %s\n", path);

			if(uploadFile(path)) {
				printf("Feito upload do arquivo com sucesso.\n");
			} else {
				printf("Falha ao fazer upload do arquivo. Por favor, tente novamente.\n");
			}
		} else if(strncmp(command, CMD_DOWNLOAD, CMD_DOWNLOAD_LEN) == 0) {
			DEBUG_PRINT("Detectado comando download\n");

			strcpy(path, command+CMD_DOWNLOAD_LEN);
			DEBUG_PRINT("Parâmetro do download: %s\n", path);

			if(downloadFile(path)) {
				printf("Feito download do arquivo com sucesso.\n");
			} else {
				printf("Falha ao fazer download do arquivo. Por favor, tente novamente.\n");
			}
		} else if(strncmp(command, CMD_DELETE, CMD_DELETE_LEN) == 0) {
			DEBUG_PRINT("Detectado comando delete\n");

			strcpy(path, command+CMD_DELETE_LEN);
			DEBUG_PRINT("Parâmetro do delete: %s\n", path);

			if(deleteFile(path)) {
				printf("Arquivo deletado com sucesso.\n");
			} else {
				printf("Falha ao tentar deletar o arquivo. Por favor, tente novamente.\n");
			}
		} else if(strncmp(command, CMD_LISTSERVER, CMD_LISTSERVER_LEN) == 0) {
			DEBUG_PRINT("Detectado comando list_server\n");

			printf("%s\n", listServer());
		} else if(strncmp(command, CMD_LISTCLIENT, CMD_LISTCLIENT_LEN) == 0) {
			DEBUG_PRINT("Detectado comando list_client\n");

			printf("%s\n", listClient());
		} else if(strncmp(command, CMD_GETSYNCDIR, CMD_GETSYNCDIR_LEN) == 0) {
			DEBUG_PRINT("Detectado comando get_sync_dir\n");

			if(getSyncDir()) {
				printf("Diretório sync_dir sincronizado com sucesso.\n");
			} else {
				printf("Falha ao tentar sincronizar o diretório sync_dir. Por favor, tente novamente.\n");
			}
		} else {
			printf("Comando não reconhecido, insira um comando válido e atente para espaços.\n\n");
			printf("\n\nComandos disponíveis:\n\nupload <file>\ndownload <file>\ndelete <file>\nlist_server\nlist_client\nexit\n\n");
		}
	} while ( strcmp(command, CMD_EXIT) != 0 );

	closeConnection();
}

void closeConnection(){
	Package *commandPackage = newPackage(EXIT,user,seqnum,0,CMD_EXIT);
	sendPackage(commandPackage, connection);
}

int firstConnection(char *user, Connection *connection) {
    unsigned int length;
    struct sockaddr_in from;
    char buffer[256];
    int port, n;

    strcpy(buffer, user);

    n = sendto(connection->socket, buffer, strlen(buffer), 0, (const struct sockaddr *) connection->address, sizeof(struct sockaddr_in));
    if (n < 0)
        printf("ERROR sendto\n");

    length = sizeof(struct sockaddr_in);

    n = recvfrom(connection->socket, buffer, strlen(buffer), 0, (struct sockaddr *) &from, &length);

    if (n < 0)
        printf("ERROR recvfrom\n");

    if (strcmp(buffer, ACCESS_ERROR) == 0) {
        DEBUG_PRINT("O SERVIDOR NÃO APROVOU A CONEXÃO\n");
        return -1;
    }

    port = atoi(buffer);

    DEBUG_PRINT("PORTA RECEBIDA: %d\n", port);

    // continuar
    return port;
}

int uploadFile(char *file_path) {
	char* filename = basename(file_path);
	Package *commandPackage = newPackage(UPLOAD,user,seqnum,0,filename);
	sendPackage(commandPackage, connection);
	seqnum = 1 - seqnum;
	//sendFile(file_path, connection);

	return SUCCESS;
}

int downloadFile(char *file_path) {
	char* filename = basename(file_path);
	Package *commandPackage = newPackage(DOWNLOAD,user,seqnum,0,filename);
	sendPackage(commandPackage, connection);
	seqnum = 1 - seqnum;
	return FAILURE;
}

int deleteFile(char *file_path) {
	char* filename = basename(file_path);
	Package *commandPackage = newPackage(DELETE,user,seqnum,0,filename);
	sendPackage(commandPackage, connection);
	return FAILURE;
}

const char* listServer() {
	Package *commandPackage = newPackage(LISTSERVER,user,seqnum,0,CMD_LISTCLIENT);
	sendPackage(commandPackage, connection);
	seqnum = 1 - seqnum;
	const char* response = "Função a ser implementada";
	return response;
}

const char* listClient() {
	const char* response = "Função a ser implementada";
	return response;
}

int getSyncDir() {
	pthread_t sync_t;

	// create sync dir if doesnt exist yet
	if(mkdir(folder, 0777) != 0 && errno != EEXIST){
		fprintf(stderr, "Error while creating sync_dir.\n"); return FAILURE;
	} else {
		printf("Creating folder %s\n", folder);
	}

	// initializing the watcher for directory events (not inside thread cause it needs the folder as argument)
	if(initSyncDirWatcher() == FAILURE){
		fprintf(stderr,"Failure creating event watcher for sync_dir.\n");
	}

	// create thread to listen for events and sync
	if(pthread_create(&sync_t, NULL, sync_thread, NULL)){
		fprintf(stderr,"Failure creating sync thread.\n"); return FAILURE;
	}

	return SUCCESS;
}

int initSyncDirWatcher(){

	notifyFile = inotify_init();
	if(notifyFile < 0) return FAILURE;

	notifyWatcher = inotify_add_watch(notifyFile, folder, IN_CREATE | IN_CLOSE_WRITE | IN_DELETE |
		IN_MOVED_FROM | IN_MOVED_TO);

	return SUCCESS;
}

void *sync_thread(){
	char buffer[EVENT_BUF_LEN];
	int length;
	int i = 0;
	//Connection *connection;

	//TODO: create sock to server, download all files at first
	//connection = getConnection();

	while(TRUE){
		length  = read(notifyFile, buffer, EVENT_BUF_LEN);
		if(length < 0){
			fprintf(stderr, "Error reading notify event file.\n");
		}

		while(i < length){
			struct inotify_event *event = (struct inotify_event *) &buffer[i];
			if(event->len){
				//TODO: put uploadFile here
				if(event->mask & IN_CREATE || event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO){
					if(! (event->mask & IN_ISDIR)) printf("File uploaded %s\n", event->name);
				}
				//TODO: deleteFile here
				if(event->mask & IN_DELETE || event->mask & IN_MOVED_FROM){
					if(! (event->mask & IN_ISDIR)) printf("File deleted %s\n", event->name);
				}
			}
			i += EVENT_SIZE + event->len;
		}

		i = 0;
		sleep(10);
	}
}
