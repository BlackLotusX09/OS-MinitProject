#include "server.h"
#include <signal.h>
#include <errno.h>
#include"shell.h"
#define DELIM "__END__\n"

/* 🔥 send all bytes safely */
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

/* 🔥 handle one client */
void handle_client(int client_socket) {

    char buffer[BUFFER_SIZE];

    /* ================= AUTH PHASE ================= */

    send_all(client_socket, "AUTH:\n", 6);

    char authbuf[128];
    memset(authbuf, 0, sizeof(authbuf));

    int n = recv(client_socket, authbuf, sizeof(authbuf) - 1, 0);
    if (n <= 0) {
        close(client_socket);
        return;
    }

    authbuf[strcspn(authbuf, "\n")] = 0;

    char *username = strtok(authbuf, ":");
    char *password = strtok(NULL, ":");

    if (!username || !password) {
        send_all(client_socket, "Invalid auth format\n", 21);
        close(client_socket);
        return;
    }

    Role role = ROLE_GUEST;
    int authenticated = 0;

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {

            role = get_role_from_string(users[i].role);
            authenticated = 1;
            break;
        }
    }

    if (!authenticated) {
        send_all(client_socket, "AUTH FAILED\n", 12);
        close(client_socket);
        return;
    }

    send_all(client_socket, "AUTH OK\n", 8);

    /* ================= COMMAND LOOP ================= */

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received == 0) {
            write(1, "Client disconnected\n", 20);
            break;
        }

        if (bytes_received < 0) {
            perror("recv failed");
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strlen(buffer) == 0) continue;

        /* 🔥 exit command */
        if (strcmp(buffer, "exit") == 0) {
            send_all(client_socket, "Connection closed\n", 18);
            break;
        }

        /* 🔥 log */
        char logbuf[256];
        int loglen = snprintf(logbuf, sizeof(logbuf),
                              "Client(%s): %s\n", username, buffer);
        write(STDOUT_FILENO, logbuf, loglen);

        /* 🔥 ROLE-BASED ACCESS CONTROL */
        if (role == ROLE_GUEST) {
            if (strcmp(buffer, "ls") != 0 &&
                strcmp(buffer, "pwd") != 0 &&
                strncmp(buffer, "echo", 4) != 0) {

                send_all(client_socket, "Permission denied\n", 18);
                send_all(client_socket, DELIM, strlen(DELIM));
                continue;
            }
        }

        if (role == ROLE_USER) {
            if (strncmp(buffer, "kill", 4) == 0) {
                send_all(client_socket, "Permission denied\n", 18);
                send_all(client_socket, DELIM, strlen(DELIM));
                continue;
            }
        }

        /* 🔥 execute command */
        FILE *fp = popen(buffer, "r");
        if (!fp) {
            send_all(client_socket, "Execution failed\n", 18);
            send_all(client_socket, DELIM, strlen(DELIM));
            continue;
        }

        char result[1024];

        while (fgets(result, sizeof(result), fp)) {
            send_all(client_socket, result, strlen(result));
        }

        pclose(fp);

        /* 🔥 end marker */
        send_all(client_socket, DELIM, strlen(DELIM));
    }

    close(client_socket);
}

/* 🔥 main server */
int server() {

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    signal(SIGCHLD, SIG_IGN);

    /* 🔥 IMPORTANT: load users */
    load_users("/Users/jaswanth/Desktop/OS-SHELL/data/users.txt");

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    char msg[128];
    int len = snprintf(msg, sizeof(msg),
                       "Server listening on port %d...\n", PORT);
    write(STDOUT_FILENO, msg, len);

    while (1) {

        client_socket = accept(server_socket,
                               (struct sockaddr*)&client_addr,
                               &client_addr_len);

        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        write(1, "Client connected\n", 17);

        pid_t pid = fork();

        if (pid == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        }
        else if (pid > 0) {
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