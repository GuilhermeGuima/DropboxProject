#include "../include/dropboxUtil.h"

void printPackage(Package *package) {
    DEBUG_PRINT("PRINTANDO PACOTE\n");
    printf("%hu\n", package->type);
    printf("%s\n", package->user);
    printf("%hu\n", package->seq);
    printf("%hu\n", package->length);
    printf("%s\n", package->data);
}

int setTimeout(int sockfd){
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0){
        fprintf(stderr, "Error setting timeout for socket %d.\n", sockfd);
        return FAILURE;
    }

    return SUCCESS;
}

Package* newPackage(unsigned short int type, char* user, unsigned short int seq, unsigned short int length, char *data) {

    DEBUG_PRINT("CRIANDO NOVO PACOTE\n");
    if (strlen(user) > USER_NAME_SIZE) {
        DEBUG_PRINT("FALHA AO CRIAR NOVO PACOTE, NOME DO USUARIO MUITO GRANDE\n");
        return NULL;
    }

    if (strlen(data) > DATA_SEGMENT_SIZE) {
        DEBUG_PRINT("FALHA AO CRIAR NOVO PACOTE, AREA DE DADOS MUITO GRANDE\n");
        return NULL;
    }

    Package *package = malloc(sizeof(*package));
    strcpy (package->user, user);
    package->type = type;
    package->seq = seq;
    package->length = length;
    strcpy(package->data, data);
    return package;

}

int sendPackage(Package *package, Connection *connection){
    int n;
    int notACKed = TRUE;
    unsigned int length;
    struct sockaddr_in from;
    Package *ackBuffer = malloc(PACKAGE_SIZE);
    
    do{
        DEBUG_PRINT("ENVIANDO PACOTE TIPO %d SEQ %d\n", package->type, package->seq);
        n = sendto(connection->socket, package, PACKAGE_SIZE, 0, (const struct sockaddr *) connection->address, sizeof(struct sockaddr_in));

        if (n < 0) {
            DEBUG_PRINT("ERRO AO ENVIAR PACOTE\n");
    		return FAILURE;
    	}

        length = sizeof(struct sockaddr_in);
        if(recvfrom(connection->socket, ackBuffer, PACKAGE_SIZE, 0, (struct sockaddr *) &from, &length) < 0 ){
            DEBUG_PRINT("TIMEOUT NO ENVIO DO PACOTE\n");
        } else {
            if(ackBuffer->type == ACK && ackBuffer->seq == package->seq){
                notACKed = FALSE;
            }else{
                DEBUG_PRINT ("RECEBEU ACK %d FORA DE SEQUENCIA\n", ackBuffer->seq);
            }
        }

    } while(notACKed);

    DEBUG_PRINT("RECEBEU ACK %d DO PACOTE\n", ackBuffer->seq);
    free(ackBuffer);
    return SUCCESS;
}

int receivePackage(Connection *connection, Package *buffer, int expectedSeq){
    unsigned int length, n;
    struct sockaddr_in *from = malloc(sizeof(struct sockaddr_in));
    Package *package = malloc(PACKAGE_SIZE);

    length = sizeof(struct sockaddr_in);
    
    // blocking call
    while(1){
        DEBUG_PRINT("RECEBENDO PACOTE %d\n", expectedSeq);
        if(recvfrom(connection->socket, buffer, PACKAGE_SIZE, 0, (struct sockaddr *) from, &length) < 0){
            fprintf(stderr, "ERRO NO RECEBIMENTO DE PACOTE");
        }else{
            if(buffer->seq == expectedSeq){
                DEBUG_PRINT("MANDANDO PACOTE DE ACK %d\n",expectedSeq);
                // send ACK package
                package->type = ACK;
                package->seq = expectedSeq;
                n = sendto(connection->socket, package, PACKAGE_SIZE, 0, (const struct sockaddr *) from, sizeof(struct sockaddr_in));

                if (n < 0) {
                    DEBUG_PRINT("ERRO AO ENVIAR PACOTE DE ACK\n");
                    return FAILURE;
                }

                break;  // ACK has been sent
            } else {
                // discard duplicate and resend ACK
                DEBUG_PRINT("RE-ENVIANDO PACOTE DE ACK %d, SEQ RECEBIDA %d\n", expectedSeq, buffer->seq);
                package->type = ACK;
                package->seq = expectedSeq;
                n = sendto(connection->socket, package, PACKAGE_SIZE, 0, (const struct sockaddr *) &from, sizeof(struct sockaddr_in));
            }
        }
    }

    connection->address = from;

    free(package);
    return SUCCESS;
}

void sendFile(char *file, Connection *connection, char* username) {
    FILE* pFile;
    Package *package;
    int file_size = 0;
    int total_send = 0;
    unsigned short int seq = SEQUENCE_SHIFT; // start at two to avoid clash with comands sequence
    unsigned short int length = 0;
    char data[DATA_SEGMENT_SIZE+1];

    DEBUG_PRINT("INICIANDO FUNÇÃO \"sendFile\" PARA ENVIO DE ARQUIVOS\n");
    pFile = fopen(file, "rb");
    if(pFile) {
        file_size = getFileSize(file);

        length = floor(file_size/DATA_SEGMENT_SIZE);
        for ( total_send = 0 ; total_send < file_size ; total_send = total_send + DATA_SEGMENT_SIZE ) {
            bzero(data, DATA_SEGMENT_SIZE+1);
            if ( (file_size - total_send) < DATA_SEGMENT_SIZE ) {
                fread(data, sizeof(char), (file_size - total_send), pFile);
            }
            else {
                fread(data, sizeof(char), DATA_SEGMENT_SIZE, pFile);
            }
            package = newPackage(DATA, username, seq, length, data);
            sendPackage(package, connection);
            seq++;
        }
        fclose(pFile);
    }else{
        bzero(data,DATA_SEGMENT_SIZE);
        package = newPackage(DATA, username, seq, length, data);
        sendPackage(package, connection);
        fprintf(stderr, "Erro na abertura do arquivo \"%s\".\n",file);
    }
}

void receiveFile(Connection *connection, char** buffer, int *file_size){
    Package *package = malloc(sizeof(Package));
    int seqFile = SEQUENCE_SHIFT;
    int offset = 0;
    receivePackage(connection, package, seqFile);
    *buffer = malloc((package->length+1)*DATA_SEGMENT_SIZE);

    while(package->length != package->seq-SEQUENCE_SHIFT){
        memcpy(*buffer+offset, package->data, DATA_SEGMENT_SIZE);
        seqFile += 1;
        receivePackage(connection, package, seqFile);
        offset = (package->seq-SEQUENCE_SHIFT)*DATA_SEGMENT_SIZE;
    }
    
    memcpy(*buffer+offset, package->data, DATA_SEGMENT_SIZE);
    *file_size = package->length*DATA_SEGMENT_SIZE+strlen(package->data);
    
    free(package);
}

void saveFile(char *buffer, int file_size, char *path){

    FILE *fp;
    int bytes_written;

    if(file_size != 0){
        fp = fopen(path, "w");
        if(fp == NULL){
            fprintf(stderr, "Error while writing file %s\n", path);
            return;
        }
        
        for ( bytes_written = 0 ; bytes_written < file_size ; bytes_written += DATA_SEGMENT_SIZE ) {
            if ( (file_size - bytes_written) < DATA_SEGMENT_SIZE ) {
                fwrite(buffer+bytes_written, sizeof(char), (file_size - bytes_written), fp);
            }
            else {
                fwrite(buffer+bytes_written, sizeof(char), DATA_SEGMENT_SIZE, fp);
            }
        }

        fclose(fp);
    }else{
        // error, file didnt exist at client
        return;
    }

    //TODO: after writing the client list function, update user's files after one has been sent

}

int getFileSize(char *path) {
    struct stat attr;
    stat(path, &attr);
    return attr.st_size;
}

char* getUserHome() {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_dir;
    }
    return "";
}

char* makePath(char* user, char* filename){
    char *path = malloc(MAX_FILE_NAME+USER_NAME_SIZE+1);

    strcpy(path, user);
    strcat(path, "/");
    strcat(path, filename);

    return path;
}

char* itoa(int i, char b[]) {
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0) {
        *p++ = '-';
        i *= -1;
    }
    int shifter = i;
    do { //Move to where representation ends
        ++p;
        shifter = shifter/10;
    } while(shifter);
    *p = '\0';
    do { //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    } while(i);
    return b;
}

