#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define PORT 4000


void sendFile(char *file, Connection *connection);
void testeMensagens(int port, char *user);
void selectCommand();
int firstConnection(char *user, char *folder, Connection *connection);
