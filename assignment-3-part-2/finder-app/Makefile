# TODO: Add your Makefile content for building the writer utility

CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Werror

all: writer

writer: writer.c
	$(CC) $(CFLAGS) writer.c -o writer

clean:
	rm -f writer *.o
