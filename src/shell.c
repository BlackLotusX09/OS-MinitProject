
#include"shell.h"
typedef struct{
    char **args;
}Command;

void parse_input(char *line, char **args) {

    char *token = strtok(line, " ");
    int i = 0;

    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}
int split_pipe(char **args,char *command[][50]) {
    int pipe_exists = 0;
    int cmd_idx=0;
    int arg_idx=0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_exists=1;
            command[cmd_idx][arg_idx]=NULL;
            cmd_idx++;
            arg_idx=0;
        }else{
            command[cmd_idx][arg_idx++]=args[i];
        }
    }
    command[cmd_idx][arg_idx] = NULL;
    //printf("%d\n",cmd_idx);
    return cmd_idx + 1;
}
void execute_pipe(char *commands[][50], int n) {
    int pipes[n-1][2];

    for (int i = 0; i < n-1; i++) {
        pipe(pipes[i]);
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            if (i == 0) {
                dup2(pipes[0][1], STDOUT_FILENO);
            }
            else if (i == n - 1) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            else {
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
    }

    for (int i = 0; i < n-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
}
int execute_command(char **args) {

    if (args[0] == NULL) return 1;

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

    else if (strcmp(args[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd failed");
        }
        return 1;
    }

    else if (strcmp(args[0], "help") == 0) {
        printf("Simple Shell\n");
        printf("Built-in commands:\n");
        printf("  cd [dir]\n  pwd\n  exit\n  help\n");
        return 1;
    }

    else if (strcmp(args[0], "exit") == 0) {
        return 0;
    }

    else {
        pid_t pid = fork();

        if (pid == 0) {
            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "%s: command not found\n", args[0]);
                exit(1);
            }
        }

        else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
        }

        else {
            perror("fork");
        }
    }

    return 1;
}