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
#include <pwd.h>

#include "definitions.h"

#ifdef DEBUG
  #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG:\t"fmt, ## args)
  #define DEBUG_PRINT_COND(cond, fmt, args...) if(cond) fprintf(stderr, "DEBUG:\t"fmt, ## args)
#else
  #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
  #define DEBUG_PRINT_COND(fmt, args...)
#endif

typedef struct package {
  unsigned short int type;
  unsigned short int seq;
  unsigned short int length;
  char user[USER_NAME_SIZE];
  char data[DATA_SEGMENT_SIZE];
} Package;

typedef struct connection_udp {
  int socket;
  struct sockaddr_in* adress;
} Connection;

void printPackage(Package *package);
Package* newPackage(unsigned short int type, char* user, unsigned short int seq, unsigned short int length, char*data);
int sendPackage(Package *package, Connection *connection);
int getFileSize(char *path);
char* getUserHome();
