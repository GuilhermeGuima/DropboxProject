#ifndef UTIL_H
#define UTIL_H

//#define DEBUG // Comentar esta linha para desativar DEBUG mode da primeira parte do trabalho.

#define DEBUG2 // Comentar esta linha para desativar DEBUG mode da segunda parte do trabalho.

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
#include <semaphore.h>
#include <arpa/inet.h>

#include "definitions.h"

#ifdef DEBUG
  #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG:\t"fmt, ## args)
  #define DEBUG_PRINT_COND(cond, fmt, args...) if(cond) fprintf(stderr, "DEBUG:\t"fmt, ## args)
#else
  #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
  #define DEBUG_PRINT_COND(fmt, args...)
#endif

#ifdef DEBUG2
  #define DEBUG_PRINT2(fmt, args...) fprintf(stderr, "DEBUG:\t"fmt, ## args)
  #define DEBUG_PRINT_COND2(cond, fmt, args...) if(cond) fprintf(stderr, "DEBUG:\t"fmt, ## args)
#else
  #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
  #define DEBUG_PRINT_COND(fmt, args...)
#endif

// data struct representing an UDP data packet
typedef struct packet {
  unsigned short int type;
  unsigned short int seq;
  unsigned short int length;
  char user[USER_NAME_SIZE];
  char data[DATA_SEGMENT_SIZE];
} Packet;

// data struct for an UDP connection
typedef struct connection_udp {
  int socket;
  struct sockaddr_in* address;
} Connection;

/*******************************************************************
                  PACKET FUNCTIONS
********************************************************************/

/*  Sends a packet (blocking until ACK is received)
  @input packet - the packet to be sent
  @input connection - the connection to be used
  @input limited - whether the request has a limit of tries
  @return FAILURE/SUCCESS
*/
int sendPacket(Packet *packet, Connection *connection, int limited);

/*  Receives a packet (blocking call)
  @input connection - the connection to be used
  @output buffer - the packet buffer in which the received packet will be put
  @input - sequence number for this connection
  @return FAILURE/SUCCESS
*/
int receivePacket(Connection *connection, Packet *buffer, int expectedSeq);

/*  Receives a file
  @input connection - the connection to be used
  @output buffer - the buffer in which the received file will be put
  @output - the file's total size in bytes
*/
void receiveFile(Connection *connection, char** buffer, int *file_size);

/*  Sends a file
  @input file - the path to the file to be sent
  @input connection - the connection to be used
  @input username - the user sending the file
*/
void sendFile(char *file, Connection *connection, char* username);

/*  Writes a buffer into a file
  @input buffer - buffer containing the data to be written
  @input file_size - amount of data to be written
  @input path - file path
*/
void saveFile(char *buffer, int file_size, char *path);

/*  Creates a new packet
  @input type - type of packet (avilable types are found in definitions.h)
  @input user - the user's username
  @input seq - the packet sequence number
  @input length - if the file is part of a multipart message, the length of that message
  @input data - the data the packet is carrying
  @return a pointer to the created packet
*/
Packet* newPacket(unsigned short int type, char* user, unsigned short int seq, unsigned short int length, char *data);

/*******************************************************************
                  FILE SYSTEM FUNCTIONS
********************************************************************/

/* Gets the file size in bytes
  @input bpath - the file path
  @return the file size in bytes
*/
int getFileSize(char *path);

/*  Returns a path <left>/<right>
  @input left - string to be put before /
  @input right - string to be put after /
  @return the path created by appending /<right> to <left>
*/
char* makePath(char* left, char* right);

/*  Lists all files in a directory, with their access time,
    modification time and creation time
  @input dir_path - directory path
  @return a string containing the list of all files and their times
*/
char* listDirectoryContents(char* dir_path);

/*  Find the current OS user's $HOME
  @return the path to the OS user's $HOME
*/
char* getUserHome();

/*******************************************************************
                  CONNECTION FUNCTIONS
********************************************************************/

/*  Sets a timeout as specified in definitions.h to the socket
  @input sockfd - socket identifier
  @return FAILURE/SUCESS
*/
int setTimeout(int sockfd);

/*  Turns an integer into a string
  @input i - integer to convert
  @output b - converted string
  @return b
*/
char* itoa(int i, char b[]);

/*******************************************************************
                  DEBUGGING FUNCTIONS
********************************************************************/

/*  Prints the contents of a packet
  @input packet - packet to be printed
*/
void printPacket(Packet *packet);

#endif
