#define DEBUG // Comentar esta linha para desativar DEBUG mode.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define PACKAGE_SIZE 1024
#define DATA_SEGMENT_SIZE 1000
#define USER_NAME_SIZE 20

#define TRUE 1
#define FALSE 0

#ifdef DEBUG
  #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG:\t"fmt, ## args)
  #define DEBUG_PRINT_COND(cond, fmt, args...) if(cond) fprintf(stderr, "DEBUG:\t"fmt, ## args)
#else
  #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
  #define DEBUG_PRINT_COND(fmt, args...)
#endif

typedef struct package {
  char user[USER_NAME_SIZE];
  unsigned short int seq;
  unsigned short int length;
  char data[DATA_SEGMENT_SIZE];
} Package;

typedef struct connection_udp {
  int socket;
  struct sockaddr_in* adress;
} Connection;

void printPackage(Package *package);
Package* newPackage(char* user, unsigned short int seq, unsigned short int length, char*data);
int sendPackage(int sockfd, struct sockaddr_in *adress, Package *package);
void sendFile(char *file, Connection *connection);
int getFileSize(char *path);
