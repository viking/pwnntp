CFLAGS = -g3 -DDEBUG -D_FILE_OFFSET_BITS=64
#CFLAGS = -O2

all: pwnntp

main.o: main.c main.h conn.h group.h response.h
	gcc $(CFLAGS) -c main.c -o main.o

conn.o: conn.c conn.h
	gcc $(CFLAGS) -c conn.c -o conn.o

group.o: group.c group.h
	gcc $(CFLAGS) -c group.c -o group.o

response.o: response.c response.h
	gcc $(CFLAGS) -c response.c -o response.o

pwnntp: main.o conn.o group.o response.o
	gcc main.o conn.o group.o response.o -o pwnntp -lssl -lsqlite3 -lz

clean:
	rm -f *.o pwnntp
