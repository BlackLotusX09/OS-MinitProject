#include "server.h"
#include <signal.h>
#include <errno.h>
#include "shell.h"
#define DELIM "__END__\n"

/* Helper to ensure all bytes are sent over the socket */
void send_all(int sock, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(sock, buf + total, len - total, 0);
        if (sent <= 0) { if (errno == EINTR) continue; break; }
        total += sent;
    }
}

/* Handle authentication and command execution for a single client */
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    send_all(client_socket, "AUTH:\n", 6);
    char authbuf[128];
    int n = recv(client_socket, authbuf, sizeof(authbuf) - 1, 0);
    if (n <= 0) { close(client_socket); return; }
    authbuf[strcspn(authbuf, "\n")] = 0;

    char *username = strtok(authbuf, ":");
    char *password = strtok(NULL, ":");
    Role role = ROLE_GUEST;
    int authenticated = 0;

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) {
            role = get_role_from_string(users[i].role);
            authenticated = 1;
            break;
        }
    }
    if (!authenticated) { send_all(client_socket, "AUTH FAILED\n", 12); close(client_socket); return; }
    send_all(client_socket, "AUTH OK\n", 8);

    while ((n = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (strcmp(buffer, "exit") == 0) break;

        /* Enforce role-based restrictions on remote commands */
        if (role == ROLE_GUEST && strcmp(buffer, "ls") != 0 && strcmp(buffer, "pwd") != 0 && strncmp(buffer, "echo", 4) != 0) {
            send_all(client_socket, "Permission denied\n", 18);
            send_all(client_socket, DELIM, strlen(DELIM));
            continue;
        }
        if (role == ROLE_USER && strncmp(buffer, "kill", 4) == 0) {
            send_all(client_socket, "Permission denied\n", 18);
            send_all(client_socket, DELIM, strlen(DELIM));
            continue;
        }

        /* Execute command using popen and stream results back */
        FILE *fp = popen(buffer, "r");
        if (fp) {
            char result[1024];
            while (fgets(result, sizeof(result), fp)) send_all(client_socket, result, strlen(result));
            pclose(fp);
        }
        send_all(client_socket, DELIM, strlen(DELIM));
    }
    close(client_socket);
}

/* Main server loop: listens for connections and forks handlers */
int server() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);
    load_users("data/users.txt");
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) continue;
        if (fork() == 0) { close(server_socket); handle_client(client_socket); exit(0); }
        close(client_socket);
    }
    return 0;
}