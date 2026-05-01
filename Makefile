CC = gcc
CFLAGS = -Wall -Wextra -I include
SRCS = src/main.c src/shell.c src/server.c src/client.c
TARGET = oshell

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) -lpthread

clean:
	rm -f $(TARGET)
