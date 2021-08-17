TOP_DIR = .
INC_DIR = $(TOP_DIR)/inc
SRC_DIR = $(TOP_DIR)/src
BUILD_DIR = $(TOP_DIR)/build

CC = gcc
FLAGS = -pthread -g -ggdb -DDEBUG -I$(INC_DIR)

OBJS = $(BUILD_DIR)/tju_packet.o \
	   $(BUILD_DIR)/kernel.o \
	   $(BUILD_DIR)/tju_tcp.o 

LIBS = $(BUILD_DIR)/tju_packet.c \
	   $(BUILD_DIR)/kernel.c \
	   $(BUILD_DIR)/tju_tcp.c



default:all

.PHONY: clean build server client

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c 
	$(CC) $(FLAGS) -c -o $@ $<

clean:
	-rm -f ./build/*.o client server

server: $(OBJS)
	$(CC) $(FLAGS) ./src/server.c -o server $(OBJS)

client:
	$(CC) $(FLAGS) ./src/client.c -o client $(OBJS) 



	
