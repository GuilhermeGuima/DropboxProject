CC = gcc
INC_DIR = ./include
SRC_DIR = ./src
BIN_DIR = ./bin
DST_DIR = dst/
CFLAGS = -Wall -g -std=gnu11

all: utils client server
	@echo "All files compiled!"

utils:	$(SRC_DIR)/dropboxUtil.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/dropboxUtil.c && mv dropboxUtil.o $(BIN_DIR)

client:	$(SRC_DIR)/dropboxClient.c
	$(CC) $(CFLAGS)  -o $(DST_DIR)/dropboxClient $(SRC_DIR)/dropboxClient.c $(BIN_DIR)/dropboxUtil.o -pthread -lm

server: $(SRC_DIR)/dropboxServer.c
	$(CC) $(CFLAGS) -o $(DST_DIR)/dropboxServer $(SRC_DIR)/dropboxServer.c $(BIN_DIR)/dropboxUtil.o -pthread -lm 

clean:
	rm -f $(DST_DIR)*.*
	rm -f $(BIN_DIR)*.*
	@echo "Every clear"
