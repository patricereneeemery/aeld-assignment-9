CC=gcc
CFLAGS=-Wall -Wextra -Werror

all: writer

writer: writer.o
	$(CC) $(CFLAGS) -o writer writer.o

writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c

clean:
	rm -f writer *.o

test:
	./finder-test.sh
