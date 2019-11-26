

CC = clang
CFLAGS = -g -Wall

default: shell

shell: main.o
	$(CC) $(CFLAGS) -o shell main.o shell.o format.o vector.o callbacks.o

main.o: main.c shell.o
	$(CC) $(CFLAGS) -c main.c

shell.o: shell.c shell.h format.o vector.o
	$(CC) $(CFLAGS) -c shell.c

format.o: format.c format.h
	$(CC) $(CFLAGS) -c format.c

vector.o: vector.c vector.h callbacks.o
	$(CC) $(CFLAGS) -c vector.c

callbacks.o: callbacks.c callbacks.h
	$(CC) $(CFLAGS) -c callbacks.c

clean:
	$(RM) main *.o *~

