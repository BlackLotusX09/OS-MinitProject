#include "client.h"
#include "shell.h"

/* Execute a remote command via the rsh built-in client */
int execute_rsh(char **args) {
    if (!args[1] || !args[2] || !args[3]) {
        write(1, "Usage: rsh <host> <port> <command>\n", 36);
        return 1;
    }

    /* Build command string from remaining arguments */
    char command[1024] = "";
    for (int i = 3; args[i]; i++) { strcat(command, args[i]); strcat(command, " "); }
    strcat(command, "\n");

    /* Create and connect TCP socket to remote server */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(args[2]));
    if (inet_pton(AF_INET, args[1], &server_addr.sin_addr) <= 0 || connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("RSH connection failed");
        if (sock >= 0) close(sock);
        return 1;
    }

    /* Perform simple text-based authentication with the server */
    char buffer[1024];
    int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) { close(sock); return 1; }
    char auth[128];
    snprintf(auth, sizeof(auth), "%s:%s\n", current_user, current_password);
    send(sock, auth, strlen(auth), 0);
    n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0 || strstr(buffer, "AUTH FAILED")) {
        write(1, "Authentication failed\n", 22);
        close(sock);
        return 1;
    }

    /* Send command and print streaming response until END marker */
    send(sock, command, strlen(command), 0);
    while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = 0;
        if (strstr(buffer, "__END__")) break;
        write(STDOUT_FILENO, buffer, n);
    }

    close(sock);
    return 1;
}