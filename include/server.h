#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>


#define PORT 9090
#define BUFFER_SIZE 1024

int server();
void handle_client(int client_socket);

