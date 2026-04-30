#include "server.h"
#include <signal.h>
#include <errno.h>

#define DELIM "__END__\n"

/* send all bytes (handles partial sends) */
void send_all(int sock, const char *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t sent = send(sock, buf + total, len - total, 0);

        if (sent <= 0) {
            if (errno == EINTR) continue;
            break;
        }

        total += sent;
    }
}

/*  handle a single client */
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received == 0) {
            write(STDOUT_FILENO, "Client disconnected\n", 20);
            break;
        }

        if (bytes_received < 0) {
            perror("recv failed");
            break;
        }

        /* remove newline */
        buffer[strcspn(buffer, "\n")] = 0;

        /* ignore empty input */
        if (strlen(buffer) == 0) {
            continue;
        }

        /* exit command */
        if (strcmp(buffer, "exit") == 0) {
            send_all(client_socket, "Connection closed\n", 18);
            break;
        }

        /* log command */
        char logbuf[1024];
        int loglen = snprintf(logbuf, sizeof(logbuf), "Client: %s\n", buffer);
        write(STDOUT_FILENO, logbuf, loglen);

        /* basic safety (prevent dangerous commands) */
        if (strstr(buffer, "rm") || strstr(buffer, "shutdown")) {
            send_all(client_socket, "Command not allowed\n", 20);
            send_all(client_socket, DELIM, strlen(DELIM));
            continue;
        }

        /* execute command */
        FILE *fp = popen(buffer, "r");
        if (!fp) {
            send_all(client_socket, "Execution failed\n", 18);
            send_all(client_socket, DELIM, strlen(DELIM));
            continue;
        }

        /* send output */
        char result[1024];
        while (fgets(result, sizeof(result), fp)) {
            send_all(client_socket, result, strlen(result));
        }

        pclose(fp);

        /* 🔥 mark end of output */
        send_all(client_socket, DELIM, strlen(DELIM));
    }

    close(client_socket);
}

/* main server function */
int server() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    /* prevent zombie processes */
    signal(SIGCHLD, SIG_IGN);

    /* create socket */
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* allow reuse of port */
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* setup address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* bind */
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    /* listen */
    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    /* log server start */
    char msg[128];
    int len = snprintf(msg, sizeof(msg), "Server listening on port %d...\n", PORT);
    write(STDOUT_FILENO, msg, len);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        /* log connection */
        char connmsg[64];
        int len1 = snprintf(connmsg, sizeof(connmsg), "Client connected\n");
        write(STDOUT_FILENO, connmsg, len1);

        pid_t pid = fork();

        if (pid == 0) {
            /* child */
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        }
        else if (pid > 0) {
            /* parent */
            close(client_socket);
        }
        else {
            perror("Fork failed");
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}