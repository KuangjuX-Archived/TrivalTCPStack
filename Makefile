TOP_DIR = .
INC_DIR = $(TOP_DIR)/include
SRC_DIR = $(TOP_DIR)/src
BUILD_DIR = $(TOP_DIR)/build

LIBCO = $(TOP_DIR)/libco/lib
SOLIBCO = $(TOP_DIR)/libco/solib

CC = gcc
FLAGS = -pthread -g -ggdb -DDEBUG -I$(INC_DIR) -std=c11

OBJS = $(BUILD_DIR)/tju_packet.o \
	   $(BUILD_DIR)/kernel.o \
	   $(BUILD_DIR)/tju_tcp.o \
	   $(BUILD_DIR)/timer.o \
	   $(BUILD_DIR)/thpool.o \
	   $(BUILD_DIR)/sockqueue.o \
	   $(BUILD_DIR)/utils.o \
	   $(BUILD_DIR)/queue.o \
	   $(BUILD_DIR)/chan.o \
	   $(BUILD_DIR)/api.o



default:all

all: clean server client
.PHONY: clean server client

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c 
	@$(CC) $(FLAGS) -c -o $@ $<

lib:
	@make -C libco

clean:
	@rm -f ./build/*.o $(BUILD_DIR)/client $(BUILD_DIR)/server

server: $(OBJS)
	@$(CC) $(FLAGS) ./src/server.c -o $(BUILD_DIR)/server $(OBJS)
	@$(BUILD_DIR)/server

client: $(OBJS)
	@$(CC) $(FLAGS) ./src/client.c -o $(BUILD_DIR)/client $(OBJS)
	@$(BUILD_DIR)/client

test_receiver: $(OBJS)
	@$(CC) $(FLAGS) ./test/test_receiver.c -o $(BUILD_DIR)/test_receiver $(OBJS)
	@$(BUILD_DIR)/test_receiver

test_client: $(OBJS)
	@$(CC) $(FLAGS) ./test/test_client.c -o $(BUILD_DIR)/test_client $(OBJS)
	@$(BUILD_DIR)/test_client



	
