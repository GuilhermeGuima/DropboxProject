#include "../include/dropboxUtil.h"

void printPacket(Packet *packet) {
    DEBUG_PRINT("PRINTANDO PACOTE\n");
    printf("%hu\n", packet->type);
    printf("%s\n", packet->user);
    printf("%hu\n", packet->seq);
    printf("%hu\n", packet->length);
    printf("%s\n", packet->data);
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

char* listDirectoryContents(char* dir_path){
    DIR *parent_dir;
    struct dirent *dp;
    struct stat sb;

    char file_path[MAX_PATH];
    char *s = malloc(DATA_SEGMENT_SIZE);
    bzero(s, DATA_SEGMENT_SIZE);
    char *str = malloc(DATA_SEGMENT_SIZE);
    bzero(str, DATA_SEGMENT_SIZE);
    int lengthTot = 0;

    if((parent_dir = opendir(dir_path)) == NULL){
        fprintf(stderr, "Error opening directory %s\n", dir_path);
        return NULL;
    }

    while((dp = readdir(parent_dir)) != NULL){
        strcpy(file_path, dir_path);
        strcat(file_path,"/");
        strcat(file_path, dp->d_name);

        if(stat(file_path, &sb) != -1){
            if((sb.st_mode & S_IFMT) == S_IFDIR){
                // skip directories
                continue;
            }else{
                sprintf(str,"File: %s\n- Last modified: %s - Access time: %s - Changed time: %s", dp->d_name, ctime(&sb.st_mtime), ctime(&sb.st_atime), ctime(&sb.st_ctime));
                memcpy(s+lengthTot,str, strlen(str));
                lengthTot += strlen(str);
            }
        }
    }
    return s;
}

Packet* newPacket(unsigned short int type, char* user, unsigned short int seq, unsigned short int length, char *data) {

    DEBUG_PRINT("CRIANDO NOVO PACOTE\n");
    if (strlen(user) > USER_NAME_SIZE) {
        DEBUG_PRINT("FALHA AO CRIAR NOVO PACOTE, NOME DO USUARIO MUITO GRANDE\n");
        return NULL;
    }

    Packet *packet = malloc(sizeof(*packet));
    strcpy (packet->user, user);
    packet->type = type;
    packet->seq = seq;
    packet->length = length;
    memcpy(packet->data, data, DATA_SEGMENT_SIZE);
    return packet;

}

int sendPacket(Packet *packet, Connection *connection, int limited){
    int n, tries = 0;
    int notACKed = TRUE;
    unsigned int length;
    struct sockaddr_in from;
    Packet *ackBuffer = malloc(PACKET_SIZE);

    do{
        //DEBUG_PRINT("ENVIANDO PACOTE TIPO %d SEQ %d\n", packet->type, packet->seq);
        n = sendto(connection->socket, packet, PACKET_SIZE, 0, (const struct sockaddr *) connection->address, sizeof(struct sockaddr_in));

        if (n < 0) {
            DEBUG_PRINT("ERRO AO ENVIAR PACOTE\n");
    		return FAILURE;
    	}

        length = sizeof(struct sockaddr_in);
        if(recvfrom(connection->socket, ackBuffer, PACKET_SIZE, 0, (struct sockaddr *) &from, &length) < 0 ){
            DEBUG_PRINT("TIMEOUT NO ENVIO DO PACOTE\n");
        } else {
            if(ackBuffer->type == ACK && ackBuffer->seq == packet->seq){
                notACKed = FALSE;
            }else{
                DEBUG_PRINT ("RECEBEU ACK %d FORA DE SEQUENCIA\n", ackBuffer->seq);
            }
        }

        tries++;

        if(tries >= MAX_TRIES && limited) return FAILURE;

    } while(notACKed);

    DEBUG_PRINT("RECEBEU ACK %d DO PACOTE\n", ackBuffer->seq);
    free(ackBuffer);
    return SUCCESS;
}

int receivePacket(Connection *connection, Packet *buffer, int expectedSeq){
    unsigned int length, n;
    struct sockaddr_in *from = malloc(sizeof(struct sockaddr_in));
    Packet *packet = malloc(PACKET_SIZE);

    length = sizeof(struct sockaddr_in);

    // blocking call
    while(1){
        //DEBUG_PRINT("RECEBENDO PACOTE %d port: %d\n", expectedSeq, ntohs(connection->address->sin_port));
        if(recvfrom(connection->socket, buffer, PACKET_SIZE, 0, (struct sockaddr *) from, &length) < 0){
            fprintf(stderr, "ERRO NO RECEBIMENTO DE PACOTE");
        }else{
            if(buffer->seq == expectedSeq){
                DEBUG_PRINT("MANDANDO PACOTE DE ACK %d\n",expectedSeq);
                // send ACK packet
                packet->type = ACK;
                packet->seq = expectedSeq;
                n = sendto(connection->socket, packet, PACKET_SIZE, 0, (const struct sockaddr *) from, sizeof(struct sockaddr_in));

                if (n < 0) {
                    DEBUG_PRINT("ERRO AO ENVIAR PACOTE DE ACK\n");
                    return FAILURE;
                }

                break;  // ACK has been sent
            } else {
                // discard duplicate and resend ACK
                DEBUG_PRINT("RE-ENVIANDO PACOTE DE ACK %d, SEQ RECEBIDA %d\n", expectedSeq, buffer->seq);
                packet->type = ACK;
                packet->seq = expectedSeq;
                n = sendto(connection->socket, packet, PACKET_SIZE, 0, (const struct sockaddr *) &from, sizeof(struct sockaddr_in));
            }
        }
    }

    connection->address = from;

    free(packet);
    return SUCCESS;
}

void sendFile(char *file, Connection *connection, char* username) {
    FILE* pFile;
    Packet *packet;
    int file_size = 0;
    int total_send = 0;
    unsigned short int seq = SEQUENCE_SHIFT; // start at two to avoid clash with comands sequence
    unsigned short int length = 0;
    char data[DATA_SEGMENT_SIZE];

    pFile = fopen(file, "rb");
    file_size = getFileSize(file);

    // sends file size
    itoa(file_size, data);
    packet = newPacket(CMD, username, seq, 0, data);
    sendPacket(packet, connection, NOT_LIMITED);
    seq++;

    DEBUG_PRINT("FILE_SIZE: %d\n",file_size);

    if(pFile) {

        length = floor(file_size/DATA_SEGMENT_SIZE);
        for ( total_send = 0 ; total_send < file_size ; total_send = total_send + DATA_SEGMENT_SIZE ) {
            bzero(data, DATA_SEGMENT_SIZE);
            if ( (file_size - total_send) < DATA_SEGMENT_SIZE ) {
                fread(data, sizeof(char), (file_size - total_send), pFile);
            }
            else {
                fread(data, sizeof(char), DATA_SEGMENT_SIZE, pFile);
            }
            packet = newPacket(DATA, username, seq, length, data);
            sendPacket(packet, connection, NOT_LIMITED);
            seq++;
        }
        fclose(pFile);
        free(packet);
        packet = NULL;
    }else{
       fprintf(stderr,"Erro ao abrir arquivo.\n");
    }
}

void receiveFile(Connection *connection, char** buffer, int *file_size){
    Packet *packet = malloc(sizeof(Packet));
    int seqFile = SEQUENCE_SHIFT;
    int offset = 0;

    // receives packet with file_size
    receivePacket(connection, packet, seqFile);
    seqFile++;
    *file_size = atoi(packet->data);
    DEBUG_PRINT("FILE_SIZE: %d\n",*file_size);

    int written = 0;

    if(*file_size != 0){
        receivePacket(connection, packet, seqFile);
        *buffer = malloc((packet->length+1)*DATA_SEGMENT_SIZE);
        
        while(packet->length != packet->seq-SEQUENCE_SHIFT-1){
            memcpy(*buffer+offset, packet->data, DATA_SEGMENT_SIZE);
            written += DATA_SEGMENT_SIZE;
            seqFile++;
            offset = (packet->seq-SEQUENCE_SHIFT)*DATA_SEGMENT_SIZE;
            receivePacket(connection, packet, seqFile);
        }

        memcpy(*buffer+offset, packet->data, *file_size-written);
        written += *file_size-written;
    }

    free(packet);
    packet = NULL;
}

void saveFile(char *buffer, int file_size, char *path) {

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

char* makePath(char* left, char* right){
    char *path = malloc(MAX_PATH);

    strcpy(path, left);
    strcat(path, "/");
    strcat(path, right);

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
