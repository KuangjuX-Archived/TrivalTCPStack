BUILD_DIR = ../build
OBJS = $(BUILD_DIR)/tju_packet.o \
	   $(BUILD_DIR)/kernel.o \
	   $(BUILD_DIR)/tju_tcp.o 

default:all

all: clean receiver test_client

clean:
	-rm -f receiver test_client client.log server.log

receiver:
	gcc -pthread -g -ggdb -DDEBUG -I../include ./test_receiver.c -o receiver $(OBJS)
test_client:
	gcc -pthread -g -ggdb -DDEBUG -I../include ./test_client.c -o test_client $(OBJS)
