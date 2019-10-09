#ifndef UTIL_H
#define UTIL_H

//#define DEBUG // Comentar esta linha para desativar DEBUG mode.

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
#include <pthread.h>
#include <time.h>
#include <libgen.h>
#include <dirent.h>

#include "definitions.h"

#ifdef DEBUG
  #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG:\t"fmt, ## args)
  #define DEBUG_PRINT_COND(cond, fmt, args...) if(cond) fprintf(stderr, "DEBUG:\t"fmt, ## args)
#else
  #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
  #define DEBUG_PRINT_COND(fmt, args...)
#endif

char server_folder[MAX_PATH];

typedef struct package {
  unsigned short int type;
  unsigned short int seq;
  unsigned short int length;
  char user[USER_NAME_SIZE];
  char data[DATA_SEGMENT_SIZE];
} Package;

typedef struct connection_udp {
  int socket;
  struct sockaddr_in* address;
} Connection;

int setTimeout(int sockfd);
int sendPackage(Package *package, Connection *connection);
int receivePackage(Connection *connection, Package *buffer, int expectedSeq);
void receiveFile(Connection *connection, char** buffer, int *file_size);
void sendFile(char *file, Connection *connection, char* username);
void saveFile(char *buffer, int file_size, char *path);
void printPackage(Package *package);
Package* newPackage(unsigned short int type, char* user, unsigned short int seq, unsigned short int length, char*data);
int sendPackage(Package *package, Connection *connection);
int getFileSize(char *path);
char* makePath(char* user, char* filename);
char* listDirectoryContents(char* dir_path);
char* getUserHome();
char* itoa(int i, char b[]);

#endif
