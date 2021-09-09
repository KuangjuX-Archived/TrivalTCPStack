BUILD_DIR = ../build
OBJS = $(BUILD_DIR)/tju_packet.o \
	   $(BUILD_DIR)/kernel.o \
	   $(BUILD_DIR)/tju_tcp.o \


default:all

all: clean receiver test_client rdt_server rdt_client

clean:
	-rm -f receiver test_client rdt_server rdt_client client.log server.log rdt_server.log rdt_client.log

receiver:
	gcc -pthread -g -ggdb -DDEBUG -I../inc ./test_receiver.c -o receiver $(OBJS)
test_client:
	gcc -pthread -g -ggdb -DDEBUG -I../inc ./test_client.c -o test_client $(OBJS)

rdt_server:
	gcc -pthread -g -ggdb -DDEBUG -I../inc ./test_rdt_server.c -o rdt_server $(OBJS)
rdt_client:
	gcc -pthread -g -ggdb -DDEBUG -I../inc ./test_rdt_client.c -o rdt_client $(OBJS)
