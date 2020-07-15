CC      = gcc
CFLAGS  = -c -Wall
LFLAGS  = -o

all: shell

shell: filesys.o shell.o
	$(CC) $(LFLAGS) shell filesys.o shell.o

filesys.o: filesys.c filesys.h
	$(CC) $(CFLAGS) filesys.c

shell.o: shell.c filesys.h
	$(CC) $(CFLAGS) shell.c

clean:
	rm -f shell
	rm -f *.o
