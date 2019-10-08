#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "dropboxUtil.h"

#define PORT 4000
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024*(EVENT_SIZE + 16))

void testeMensagens(int port, char *user);
void selectCommand();
int connectServer(char *user, struct hostent *server, Connection *connection);
int firstConnection(char *user, Connection *connection);
Connection* getConnection(int port);
void closeConnection();

int uploadFile(char *file_path);
int downloadFile(char *file_path);
int deleteFile(char *file_path);
const char* listServer();
const char* listClient();
int getSyncDir();
int initSyncDirWatcher();
void *sync_thread();
