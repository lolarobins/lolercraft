CC=gcc
CFLAGS=-g -O0 -Wall -Iinclude -std=c23 -pthread -D_THREAD_SAFE `pkgconf openssl --cflags`
LDFLAGS=`pkgconf openssl --libs` `pkgconf --libs libcurl`
OBJS=\
	build/data_json.o \
	build/data_misc.o \
	build/data_nbt.o \
	build/event.o \
	build/main.o \
	build/mod.o \
	build/proto_config.o \
	build/proto_encrypt.o \
	build/proto_login.o \
	build/proto_packet.o \
	build/proto_play.o \
	build/proto_socket.o \
	build/proto_status.o

target: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o lolercraft

build:
	mkdir -p build

build/%.o: build src/%.c
	$(CC) $(CFLAGS) -c $(lastword $^) -o $@

clean:
	rm -rf build/
