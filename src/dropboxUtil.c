#include "../include/dropboxUtil.h"

void printPackage(Package *package) {
    DEBUG_PRINT("PRINTANDO PACOTE\n");
    printf("%hu\n", package->type);
    printf("%s\n", package->user);
    printf("%hu\n", package->seq);
    printf("%hu\n", package->length);
    printf("%s\n", package->data);
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

int sendPackage(Package *package, Connection *connection) {
    int n;
    DEBUG_PRINT("ENVIANDO PACOTE\n");
    n = sendto(connection->socket, package, PACKAGE_SIZE, 0, (const struct sockaddr *) connection->adress, sizeof(struct sockaddr_in));
	if (n < 0) {
        DEBUG_PRINT("ERRO AO ENVIAR PACOTE\n");
		return FAILURE;
	}
    return SUCCESS;
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
