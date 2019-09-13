//#define DEBUG // Comentar esta linha para desativar DEBUG mode.

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define PACKAGE_SIZE 1024

#ifdef DEBUG
  #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG:\t"fmt, ## args)
  #define DEBUG_PRINT_COND(cond, fmt, args...) if(cond) fprintf(stderr, "DEBUG:\t"fmt, ## args)
#else
  #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
  #define DEBUG_PRINT_COND(fmt, args...)
#endif

typedef struct package {
  char user[20];
  unsigned short int seq;
  unsigned short int length;
  char data[1000];
} Package;

void print_package(Package *package);
Package* new_package(char* user, unsigned short int seq, unsigned short int length, char*data);
int send_package(int sockfd, struct sockaddr_in *adress, Package *package);
