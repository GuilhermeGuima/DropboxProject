//Package Type

#define ACK 0
#define CMD 1
#define DATA 2
#define UPLOAD 3
#define DOWNLOAD 4
#define DELETE 5
#define LISTSERVER 6
#define EXIT 7
#define CONNECT 8

//Size

#define PACKAGE_SIZE 1024
#define DATA_SEGMENT_SIZE 1000
#define USER_NAME_SIZE 18
#define MAX_PATH 256
#define MAX_FILES 20
#define MAX_FILE_NAME 64

//Strings

#define ACCESS_ERROR "access_error"

//Constants

#define TRUE 1
#define FALSE 0
#define SUCCESS 1
#define FAILURE 0

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

// Connection

#define TIMEOUT 3 // in seconds
#define SEQUENCE_SHIFT 2 //sequence number shift on data transmission

