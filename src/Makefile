CC = gcc
CFLAGS = -Wall -Wextra -I ../include
SRCS = main.c shell.c client.c server.c
TARGET = ../oshell

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) -lpthread

clean:
	rm -f $(TARGET)