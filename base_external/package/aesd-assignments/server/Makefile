CC = gcc
CFLAGS = -Wall -Werror -I ../../../../aesd-char-driver

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm -f aesdsocket *.o
