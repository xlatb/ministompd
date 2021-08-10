CC=gcc
# gdb (Ubuntu 9.2-0ubuntu1~20.04) 9.2 is printing function arguments incorrectly without -fcf-protection=none
# https://stackoverflow.com/questions/64697087/gdb-shows-incorrect-arguments-of-functions-for-stack-frames
# Apparently fixed in gdb 10.1 and later.
CFLAGS=-std=c99 -O0 -g -fcf-protection=none -D_POSIX_C_SOURCE=200809L -Wall
#CFLAGS=-std=c99 -O2 -Wall

OBJS=ministompd.o frame.o frameparser.o frameserializer.o buffer.o bytestring.o \
     bytestring_list.o headerbundle.o connection.o connectionbundle.o listener.o \
     queueconfig.o storage.o storage_memory.o queue.o alloc.o log.o siphash24.o \
     hash.o subscription.o framerouter.o queuebundle.o list.o printbuf.o

ministompd : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o ministompd

ministompd.o : src/ministompd.c src/*.h
	$(CC) $(CFLAGS) -c src/ministompd.c

alloc.o : src/alloc.c src/*.h
	$(CC) $(CFLAGS) -c src/alloc.c

log.o : src/log.c src/*.h
	$(CC) $(CFLAGS) -c src/log.c

frame.o : src/frame.c src/*.h
	$(CC) $(CFLAGS) -c src/frame.c

frameparser.o : src/frameparser.c src/*.h
	$(CC) $(CFLAGS) -c src/frameparser.c

frameserializer.o : src/frameserializer.c src/*.h
	$(CC) $(CFLAGS) -c src/frameserializer.c

buffer.o : src/buffer.c src/*.h
	$(CC) $(CFLAGS) -c src/buffer.c

bytestring.o : src/bytestring.c src/*.h
	$(CC) $(CFLAGS) -c src/bytestring.c

hash.o : src/hash.c src/*.h
	$(CC) $(CFLAGS) -c src/hash.c

bytestring_list.o : src/bytestring_list.c src/*.h
	$(CC) $(CFLAGS) -c src/bytestring_list.c

headerbundle.o : src/headerbundle.c src/*.h
	$(CC) $(CFLAGS) -c src/headerbundle.c

connection.o : src/connection.c src/*.h
	$(CC) $(CFLAGS) -c src/connection.c

connectionbundle.o : src/connectionbundle.c src/*.h
	$(CC) $(CFLAGS) -c src/connectionbundle.c

listener.o : src/listener.c src/*.h
	$(CC) $(CFLAGS) -c src/listener.c

queueconfig.o : src/queueconfig.c src/*.h
	$(CC) $(CFLAGS) -c src/queueconfig.c

storage.o : src/storage.c src/*.h src/storage/*.h
	$(CC) $(CFLAGS) -c src/storage.c

storage_memory.o : src/storage/memory.c src/*.h src/storage/*.h
	$(CC) $(CFLAGS) -c src/storage/memory.c -o storage_memory.o

queue.o : src/queue.c src/*.h
	$(CC) $(CFLAGS) -c src/queue.c

queuebundle.o : src/queuebundle.c src/*.h
	$(CC) $(CFLAGS) -c src/queuebundle.c

subscription.o : src/subscription.c src/*.h
	$(CC) $(CFLAGS) -c src/subscription.c

framerouter.o : src/framerouter.c src/*.h
	$(CC) $(CFLAGS) -c src/framerouter.c

siphash24.o : src/siphash24.c
	$(CC) $(CFLAGS) -c src/siphash24.c

list.o : src/list.c
	$(CC) $(CFLAGS) -c src/list.c

printbuf.o : src/printbuf.c
	$(CC) $(CFLAGS) -c src/printbuf.c

clean :
	rm -f ministompd $(OBJS)
