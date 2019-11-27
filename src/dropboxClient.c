#include "../include/dropboxClient.h"

int notifyWatcher, notifyFile;
struct hostent *server;
char folder[MAX_PATH];
Connection *connection;
char user[USER_NAME_SIZE];
int seqnum = 0, seqnumReceive = 0;

int broadcasted;
pthread_mutex_t broadcastMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t bcast_t, sync_t, client_t;

int main(int argc, char *argv[]) {
	
	DEBUG_PRINT("OPÇÃO DE DEBUG ATIVADA\n");

	if (argc < 3) {
		fprintf(stderr, "Falta de argumentos ./dropboxClient user endereço\n");
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
		printf("Bem vindo %s!\n", user);
	} else {
		fprintf(stderr, "O tamanho máximo para um novo usuário é %d. Por favor, digite um nome válido.\n", USER_NAME_SIZE);
		return FAILURE;
	}

	DEBUG_PRINT("User: %s\n", user);
	DEBUG_PRINT("Folder: %s\n", folder);
	
    int *new_client = malloc(sizeof(int));
    *new_client = 1;

    if(pthread_create(&client_t, NULL, main_thread, &new_client)){
    	fprintf(stderr,"Failure creating main client thread.\n"); return FAILURE;
    }

	while(TRUE){};	// main thread shouldn't finish

	close(connection->socket);

	inotify_rm_watch(notifyFile, notifyWatcher); // removes the directory from inotify watcher
	close(notifyFile); //close inotify file

	return SUCCESS;
}

void selectCommand(int new) {
	char command[12 + MAX_PATH];
	char path[MAX_PATH];
	char *valid;
	int server_down = !new;	// to make it transparent to the end-user that a server was down

	seqnum = 0; // resets because new socket has been created on server-side
	
	sleep(1);
	if(!server_down) printf("\n\nComandos disponíveis:\n\nupload <file>\ndownload <file>\ndelete <file>\nlist_server\nlist_client\nexit\n\n");

	do {
		if(!server_down) printf("\nDigite seu comando: ");
		server_down = 0;

		valid = fgets(command, sizeof(command)-1, stdin);
		if(valid != NULL) {
			command[strcspn(command, "\r\n")] = 0;
		}

		DEBUG_PRINT("Comando digitado: %s\n", command);

		if(strncmp(command, CMD_UPLOAD, CMD_UPLOAD_LEN) == 0) {
			DEBUG_PRINT("Detectado comando upload\n");

			strcpy(path, command+CMD_UPLOAD_LEN);
			DEBUG_PRINT("Parâmetro do upload: %s\n", path);

			if(uploadFile(path, &seqnum, connection)) {
				printf("Feito upload do arquivo com sucesso.\n");
			} else {
				printf("Falha ao fazer upload do arquivo \"%s\". Por favor, tente novamente.\n", path);
			}
		} else if(strncmp(command, CMD_DOWNLOAD, CMD_DOWNLOAD_LEN) == 0) {
			DEBUG_PRINT("Detectado comando download\n");

			strcpy(path, command+CMD_DOWNLOAD_LEN);
			DEBUG_PRINT("Parâmetro do download: %s\n", path);

			if(downloadFile(path, connection)) {
				printf("Feito download do arquivo \"%s\" com sucesso.\n", path);
			} else {
				printf("Falha ao fazer download do arquivo \"%s\". Por favor, tente novamente.\n", path);
			}
		} else if(strncmp(command, CMD_DELETE, CMD_DELETE_LEN) == 0) {
			DEBUG_PRINT("Detectado comando delete\n");

			strcpy(path, command+CMD_DELETE_LEN);
			DEBUG_PRINT("Parâmetro do delete: %s\n", path);

			if(deleteFile(path, &seqnum, connection)) {
				printf("Arquivo deletado com sucesso.\n");
			} else {
				printf("Falha ao tentar deletar o arquivo \"%s\". Por favor, tente novamente.\n", path);
			}
		} else if(strncmp(command, CMD_LISTSERVER, CMD_LISTSERVER_LEN) == 0) {
			DEBUG_PRINT("Detectado comando list_server\n");

			listServer(connection);
		} else if(strncmp(command, CMD_LISTCLIENT, CMD_LISTCLIENT_LEN) == 0) {
			DEBUG_PRINT("Detectado comando list_client\n");

			listClient();
		} else if(strncmp(command, CMD_GETSYNCDIR, CMD_GETSYNCDIR_LEN) == 0) {
			DEBUG_PRINT("Detectado comando get_sync_dir\n");

			if(getSyncDir(folder)) {
				printf("Diretório sync_dir sincronizado com sucesso.\n");
			} else {
				printf("Falha ao tentar sincronizar o diretório sync_dir. Por favor, tente novamente.\n");
			}
		} else if(strncmp(command, CMD_EXIT, CMD_EXIT_LEN) == 0) {
			DEBUG_PRINT("Detectado comando exit\n");

			printf("Encerrando aplicação.\n\n");
		} else {
			printf("Comando não reconhecido, insira um comando válido e atente para espaços.\n\n");
			printf("\n\nComandos disponíveis:\n\nupload <file>\ndownload <file>\ndelete <file>\nlist_server\nlist_client\nexit\n\n");
		}
	} while ( strcmp(command, CMD_EXIT) != 0 );

	closeConnection();
}

void closeConnection(){
	Packet *commandPacket = newPacket(EXIT,user,seqnum,0,CMD_EXIT);
	sendPacket(commandPacket, connection, NOT_LIMITED);
}

int firstConnection(char *user, Connection *connection) {
    char buffer[DATA_SEGMENT_SIZE];
    int port;

    bzero(buffer, DATA_SEGMENT_SIZE);
    strcpy(buffer, user);

    Packet *p = newPacket(CMD, user, seqnum, 0, buffer);
    sendPacket(p, connection, NOT_LIMITED);
    seqnum = 1 - seqnum;

    receivePacket(connection, p, seqnumReceive);
    seqnumReceive = 1 - seqnumReceive;

    if (strcmp(p->data, ACCESS_ERROR) == 0) {
        DEBUG_PRINT("O SERVIDOR NÃO APROVOU A CONEXÃO\n");
        return -1;
    }

    port = atoi(p->data);

    DEBUG_PRINT("PORTA RECEBIDA: %d\n", port);

    // continuar
    return port;
}

int uploadFile(char *file_path, int *seqNumber, Connection *connection) {
	char* filename = basename(file_path);
	Packet *commandPacket = newPacket(UPLOAD,user,*seqNumber,0,filename);
	sendPacket(commandPacket, connection, NOT_LIMITED);
	DEBUG_PRINT("seq num: %d\n", *seqNumber);
	*seqNumber = 1 - *seqNumber;

	struct stat buf;
	if (stat(file_path, &buf) == 0){
		sendFile(file_path, connection, user);
	}else{
		DEBUG_PRINT("ARQUIVO %s NAO EXISTE\n", file_path);
		sendFile(file_path, connection, user);
		return FAILURE;
	}

	return SUCCESS;
}

int downloadFile(char *file, Connection *connection) {
	char* filename = basename(file);
	int file_size;
	char *buffer = NULL;

	Packet *commandPacket = newPacket(DOWNLOAD,user,seqnum,0,filename);
	sendPacket(commandPacket, connection, NOT_LIMITED);
	seqnum = 1 - seqnum;

	receiveFile(connection, &buffer, &file_size);
	saveFile(buffer, file_size, filename);

	return SUCCESS;
}

int deleteFile(char *file, int *seqNumber, Connection *connection) {
	char* filename = basename(file);
	Packet *commandPacket = newPacket(DELETE,user,*seqNumber,0,filename);
	sendPacket(commandPacket, connection, NOT_LIMITED);
	*seqNumber = 1 - *seqNumber;

	return SUCCESS;
}

void listServer(Connection *connection) {
	Packet *commandPacket = newPacket(LISTSERVER,user,seqnum,0,CMD_LISTCLIENT);
	sendPacket(commandPacket, connection, NOT_LIMITED);
	seqnum = 1 - seqnum;

	char *s = receiveList();
	printf("%s", s);
}

char *receiveList(){
	char *s = malloc(MAX_LIST_SIZE);
	int i;
	bzero(s,MAX_LIST_SIZE);
	Packet *buffer = malloc(sizeof(Packet));

	for(i = 0; i < MAX_LIST_SIZE/DATA_SEGMENT_SIZE; i++){
		receivePacket(connection, buffer, LIST_START_SEQ+i);
		strcpy(s+i*(DATA_SEGMENT_SIZE-1),buffer->data);
	}

	return s;
}

void listClient() {
	char *s = listDirectoryContents(folder);
	printf("%s", s);
}

int getSyncDir(char *folder) {

	// create sync dir if doesnt exist yet
	if(mkdir(folder, 0777) != 0 && errno != EEXIST){
		fprintf(stderr, "Error while creating sync_dir.\n"); return FAILURE;
	} else {
		printf("Criando diretório %s\n", folder);
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
	int i = 0, seqnumSyn = 0, seqnumReceiveSyn = 0;
	Connection *connectionSync = malloc(sizeof(Connection));
	char buf[DATA_SEGMENT_SIZE], *filename;
    int port;

	connectionSync = getConnection(PORT);

	bzero(buf, DATA_SEGMENT_SIZE);
    strcpy(buf, user);

    Packet *p = newPacket(SYNC, user, seqnumSyn, 0, buf);
    sendPacket(p, connectionSync, NOT_LIMITED);
    seqnumSyn = 1 - seqnumSyn;

    printf("122222\n");
    receivePacket(connectionSync, p, seqnumReceiveSyn);
    seqnumReceiveSyn = 1 - seqnumReceiveSyn;

    if (strcmp(p->data, ACCESS_ERROR) == 0) {
        DEBUG_PRINT("O SERVIDOR NÃO APROVOU A CONEXÃO\n");
    }

    port = atoi(p->data);
    connectionSync = getConnection(port);
    seqnumSyn = 0; seqnumReceiveSyn = 0;

    DEBUG_PRINT("PORTA RECEBIDA SYNC: %d\n", port);

	DEBUG_PRINT("DOWNLOADING ALL FILES IN SERVER'S FOLDER\n");

	downloadAllFiles(connectionSync, &seqnumSyn, &seqnumReceiveSyn);

	// initializing the watcher for directory events 
	if(initSyncDirWatcher() == FAILURE){
		fprintf(stderr,"Failure creating event watcher for sync_dir.\n");
	}

	while(TRUE){
		length  = read(notifyFile, buffer, EVENT_BUF_LEN);
		if(length < 0){
			fprintf(stderr, "Error reading notify event file.\n");
		}
		pthread_mutex_lock(&broadcastMutex);
		if(broadcasted == FALSE){	
			pthread_mutex_unlock(&broadcastMutex);	
			while(i < length){
				struct inotify_event *event = (struct inotify_event *) &buffer[i];
				if(event->len){
					if(event->mask & IN_CREATE || event->mask & IN_MODIFY || event->mask & IN_MOVED_TO){
						if(! (event->mask & IN_ISDIR)){
							filename = makePath(folder,event->name);
							DEBUG_PRINT("upload inotify %s\n", filename);
							uploadFile(filename, &seqnumSyn, connectionSync);
						}
					}
					if(event->mask & IN_DELETE || event->mask & IN_MOVED_FROM){
						if(! (event->mask & IN_ISDIR)){
							filename = makePath(folder,event->name);
							DEBUG_PRINT("delete inotify\n");
							deleteFile(filename, &seqnumSyn, connectionSync);
						}
					}
				}
				i += EVENT_SIZE + event->len;
			}
		}else{
			broadcasted = FALSE;
			pthread_mutex_unlock(&broadcastMutex);
		}

		i = 0;
	}
}

void downloadAllFiles(Connection *connection, int *seqnum, int *seqnumReceive){

	int nbFiles, i, file_size;
	char *buffer, *file_path;

	Packet *p = newPacket(DOWNLOAD_ALL, user, *seqnum, 0, "");
    sendPacket(p, connection, NOT_LIMITED);
    *seqnum = 1 - *seqnum;

    // the response packet from a DOWNLOAD_ALL request has the nb of files as data
    receivePacket(connection, p, *seqnumReceive);
    *seqnumReceive = 1 - *seqnumReceive;

    nbFiles = atoi(p->data);
    for(i = 0; i < nbFiles; i++){
    	receivePacket(connection, p, *seqnumReceive);
    	*seqnumReceive = 1 - *seqnumReceive;

    	receiveFile(connection, &buffer, &file_size);

    	file_path = makePath(folder,p->data);
    	saveFile(buffer, file_size, file_path);
    }

    DEBUG_PRINT("DOWNLOADED %d FILES INTO SYNC DIR\n", nbFiles);
}

void *broadcast_thread(){
	int seqnum = 0, seqnumReceive = 0, sockfd, file_size;
	char *buffer, bufFirst[DATA_SEGMENT_SIZE], *file_path;
    struct sockaddr_in cli_addr;

    Connection *connectionBroad = malloc(sizeof(Connection));
    connectionBroad = getConnection(PORT);

	bzero(bufFirst, DATA_SEGMENT_SIZE);
    strcpy(bufFirst, user);

    // handshake to establish broadcast connection
    Packet *p = newPacket(BROADCAST, user, seqnum, 0, bufFirst);
    sendPacket(p, connectionBroad, NOT_LIMITED);
    seqnum = 1 - seqnum;

    receivePacket(connectionBroad, p, seqnumReceive);
    seqnumReceive = 1 - seqnumReceive;

    // creation of the broadcast client socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fprintf(stderr, "ERROR opening socket");

	cli_addr.sin_family = AF_INET;
	cli_addr.sin_port = htons(CLIENTS_PORT);
	cli_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(cli_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &cli_addr, sizeof(struct sockaddr)) < 0)
		fprintf(stderr, "ERROR on binding");

	connectionBroad->socket = sockfd;
	connectionBroad->address = &cli_addr;

    if (strcmp(p->data, ACCESS_ERROR) == 0) {
        DEBUG_PRINT("O SERVIDOR NÃO APROVOU A CONEXÃO\n");
    }

    seqnumReceive = 0; seqnum = 0;
    Packet *request = malloc(sizeof(Packet));

    while(TRUE){
    	seqnumReceive = 0;
    	//printf("Waiting for packets bcast %d\n", ntohs(connectionBroad->address->sin_port));
		receivePacket(connectionBroad, request, seqnumReceive);
		seqnumReceive = 1 - seqnumReceive;
		pthread_mutex_lock(&broadcastMutex);

		switch(request->type){
			case UPLOAD:
				receiveFile(connectionBroad, &buffer, &file_size);
				file_path = makePath(folder,request->data);
				saveFile(buffer, file_size, file_path);
				break;
			case DELETE:
				file_path = makePath(folder,request->data);
				remove(file_path);
				break;
			default: fprintf(stderr, "Invalid command number %d for broadcast\n", request->type);
		}
		broadcasted = TRUE;
		pthread_mutex_unlock(&broadcastMutex);
    }
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

// FRONT-END CODE (T2)
void *election_thread(){
	int seqnumReceive = 0, sockfd;
    struct sockaddr_in cli_addr;

    Connection *connectionBroad = malloc(sizeof(Connection));

    // creation of the election listening client socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fprintf(stderr, "ERROR opening socket");

	cli_addr.sin_family = AF_INET;
	cli_addr.sin_port = htons(FRONT_END_PORT);
	cli_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(cli_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &cli_addr, sizeof(struct sockaddr)) < 0)
		fprintf(stderr, "ERROR on binding");

	connectionBroad->socket = sockfd;
	connectionBroad->address = &cli_addr;

	seqnumReceive = 0; seqnum = 0;
	Packet *request = malloc(sizeof(Packet));

    while(TRUE){
    	seqnumReceive = 0;

		receivePacket(connectionBroad, request, seqnumReceive);
		seqnumReceive = 1 - seqnumReceive;

		switch(request->type){
			case ANNOUNCE_ELECTION:
				/** Receive packet of new server **/
				server = gethostbyname(inet_ntoa(connectionBroad->address->sin_addr));

				/** Cancel all other running threads for client **/
				pthread_cancel(bcast_t);
				pthread_cancel(sync_t);
				pthread_cancel(client_t);

				/** Restart all threads **/
				int *new_client;
			    *new_client = 0;

			    if(pthread_create(&client_t, NULL, main_thread, &new_client)){
			    	fprintf(stderr,"Failure creating main client thread.\n"); return FAILURE;
			    }

				break;
		}
    }
}

void *main_thread(void *arg){

	int *new_client = (int *) arg;
	int port;

	Connection* firstConn = getConnection(PORT);

	port = firstConnection(user, firstConn);
    connection = getConnection(port);
	
	if (port > 0) {
		getSyncDir(folder);

		// create thread to listen for events and sync
		if(pthread_create(&sync_t, NULL, sync_thread, NULL)){
			fprintf(stderr,"Failure creating sync thread.\n"); return FAILURE;
		}

		if(pthread_create(&bcast_t, NULL, broadcast_thread, NULL)){
			fprintf(stderr,"Failure creating broadcast thread.\n"); return FAILURE;
		}

		selectCommand(*new_client);
	}


	return NULL;
}