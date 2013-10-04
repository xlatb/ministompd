CC=gcc
CFLAGS=-std=c99 -g -Wall
#CFLAGS=-std=c99 -O2 -Wall

OBJS=ministompd.o frame.o frameparser.o frameserializer.o buffer.o bytestring.o headerbundle.o connection.o connectionbundle.o listener.o alloc.o

ministompd : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o ministompd

ministompd.o : src/ministompd.c src/*.h
	$(CC) $(CFLAGS) -c src/ministompd.c

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

headerbundle.o : src/headerbundle.c src/*.h
	$(CC) $(CFLAGS) -c src/headerbundle.c

connection.o : src/connection.c src/*.h
	$(CC) $(CFLAGS) -c src/connection.c

connectionbundle.o : src/connectionbundle.c src/*.h
	$(CC) $(CFLAGS) -c src/connectionbundle.c

listener.o : src/listener.c src/*.h
	$(CC) $(CFLAGS) -c src/listener.c

alloc.o : src/alloc.c src/*.h
	$(CC) $(CFLAGS) -c src/alloc.c

clean :
	rm -f ministompd $(OBJS)
