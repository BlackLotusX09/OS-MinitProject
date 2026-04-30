#include "client.h"

int execute_rsh(char **args) {
    if (args[1] == NULL || args[2] == NULL || args[3] == NULL) {
        write(1, "Usage: rsh <host> <port> <command>\n", 36);
        return 1;
    }

    char *host = args[1];
    int port = atoi(args[2]);

    /* 🔥 build command string */
    char command[1024] = "";
    for (int i = 3; args[i] != NULL; i++) {
        strcat(command, args[i]);
        strcat(command, " ");
    }

    strcat(command, "\n");

    /* 🔥 create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return 1;
    }

    /* 🔥 connect */
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sock);
        return 1;
    }

    char buffer[1024];

    /* ================= AUTH PHASE ================= */

    // wait for AUTH prompt
    int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(sock);
        return 1;
    }

    buffer[n] = 0;

    // send credentials
    char auth[128];
    snprintf(auth, sizeof(auth), "admin:admin\n");
    send(sock, auth, strlen(auth), 0);

    // receive auth response
    n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(sock);
        return 1;
    }

    buffer[n] = 0;

    if (strstr(buffer, "AUTH FAILED")) {
        write(1, "Authentication failed\n", 22);
        close(sock);
        return 1;
    }

    /* ================= COMMAND PHASE ================= */

    // NOW send command (correct place)
    send(sock, command, strlen(command), 0);

    // read response
    while (1) {
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;

        buffer[bytes] = 0;

        if (strstr(buffer, "__END__")) {
            break;
        }

        write(STDOUT_FILENO, buffer, bytes);
    }

    close(sock);
    return 1;
}