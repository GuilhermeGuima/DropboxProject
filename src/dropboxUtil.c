#include "../include/dropboxUtil.h"

void print_package(Package *package) {
    printf("%s\n", package->user);
    printf("%hu\n", package->seq);
    printf("%hu\n", package->length);
    printf("%s\n", package->data);
}

Package* new_package(char* user, unsigned short int seq, unsigned short int length, char *data) {
    if (strlen(user) > 20) {
        return NULL;
    }
    if (strlen(user) > 20) {
        return NULL;
    }

    Package *package = malloc(sizeof(*package));
    strcpy (package->user, user);
    package->seq = seq;
    package->length = length;
    strcpy(package->data, data);
    return package;
}

int send_package(int sockfd, struct sockaddr_in* adress, Package *package) {
    int n;
    DEBUG_PRINT("ENVIANDO PACOTE\n");
    n = sendto(sockfd, package, 1024, 0, (const struct sockaddr *) adress, sizeof(struct sockaddr_in));
	printf("\n%d",n);
	if (n < 0) {
        DEBUG_PRINT("ERRO AO ENVIAR PACOTE\n");
		return 0;
	}
    return 1;
}
