#include "shell.h"

pid_t fg_pgid = 0;

/* ---------------- Parsing ---------------- */

void parse_input(char *line, char **args) {
    char *token = strtok(line, " ");
    int i = 0;

    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}

int split_pipe(char **args, char *command[][50]) {
    int cmd_idx = 0, arg_idx = 0;

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            command[cmd_idx][arg_idx] = NULL;
            cmd_idx++;
            arg_idx = 0;
        } else {
            command[cmd_idx][arg_idx++] = args[i];
        }
    }

    command[cmd_idx][arg_idx] = NULL;
    return cmd_idx + 1;
}

/* ---------------- Signal Handlers ---------------- */

void handle_sigint(int sig) {
    if (fg_pgid > 0) {
        kill(-fg_pgid, SIGINT);
    }
    write(1, "\n", 1);
}

void handle_sigtstp(int sig) {
    if (fg_pgid > 0) {
        kill(-fg_pgid, SIGSTOP);
    }
}

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void setup_signals() {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, NULL);

    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);
}

/* ---------------- Execution ---------------- */

int execute_command(char **args) {
    if (args[0] == NULL) return 1;

    /* Built-ins */

    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd failed");
            }
        }
        return 1;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd failed");
        }
        return 1;
    }

    if (strcmp(args[0], "help") == 0) {
        printf("Simple Shell\ncd pwd help exit\n");
        return 1;
    }

    if (strcmp(args[0], "exit") == 0) {
        return 0;
    }

    /* External command */

    pid_t pid = fork();

    if (pid == 0) {
        setpgid(0, 0);
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    }

    else if (pid > 0) {
        setpgid(pid, pid);
        fg_pgid = pid;

        int status;
        waitpid(pid, &status, 0);

        fg_pgid = 0;
    }

    else {
        perror("fork");
    }

    return 1;
}

/* ---------------- Pipes ---------------- */

void execute_pipe(char *commands[][50], int n) {
    int pipes[n-1][2];
    pid_t pgid = 0;

    for (int i = 0; i < n-1; i++) {
        pipe(pipes[i]);
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            if (pgid == 0) pgid = getpid();
            setpgid(0, pgid);

            if (i == 0) {
                dup2(pipes[0][1], STDOUT_FILENO);
            } else if (i == n - 1) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            } else {
                dup2(pipes[i-1][0], STDIN_FILENO);
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < n-1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(commands[i][0], commands[i]);
            perror("execvp");
            exit(1);
        }

        else if (pid > 0) {
            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);
        }
    }

    fg_pgid = pgid;

    for (int i = 0; i < n-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

    fg_pgid = 0;
}