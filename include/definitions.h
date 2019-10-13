//Package Type

#define ACK 0
#define CMD 1
#define DATA 2
#define UPLOAD 3
#define DOWNLOAD 4
#define DELETE 5
#define LISTSERVER 6
#define EXIT 7
#define SYNC 8
#define BROADCAST 9
#define DOWNLOAD_ALL 10

//Size

#define PACKAGE_SIZE 1024
#define DATA_SEGMENT_SIZE 1000
#define USER_NAME_SIZE 18
#define MAX_PATH 256
#define MAX_FILES 20
#define MAX_FILE_NAME 64

//Strings

#define ACCESS_ERROR "access_error"
#define SERVER_FOLDER_NAME "Dropbox_Users"

//Constants

#define TRUE 1
#define FALSE 0
#define SUCCESS 1
#define FAILURE 0
#define INVALID -1
#define LOGGED 1

//Commands

#define CMD_UPLOAD "upload "
#define CMD_DOWNLOAD "download "
#define CMD_DELETE "delete "
#define CMD_LISTSERVER "list_server"
#define CMD_LISTCLIENT "list_client"
#define CMD_GETSYNCDIR "get_sync_dir"
#define CMD_EXIT "exit"
#define CMD_CONNECT "connect"

//Commands Length

#define CMD_UPLOAD_LEN 7
#define CMD_DOWNLOAD_LEN 9
#define CMD_DELETE_LEN 7
#define CMD_LISTSERVER_LEN 11
#define CMD_LISTCLIENT_LEN 11
#define CMD_GETSYNCDIR_LEN 12
#define CMD_EXIT_LEN 4

// Connection

#define TIMEOUT 3 // in seconds
#define SEQUENCE_SHIFT 2 //sequence number shift on data transmission
#define LIST_START_SEQ 2 //sequence number shift for file list transmission
#define MAX_LIST_SIZE 4000
#define CLIENTS_PORT 30000