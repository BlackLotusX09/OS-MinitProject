#include "shell.h"

pid_t fg_pgid = 0;
Job jobs[MAX_JOBS];
int job_count = 0;
pthread_mutex_t jobs_lock = PTHREAD_MUTEX_INITIALIZER;
/* ---------------- Parsing ---------------- */
void add_job(pid_t pid, char *name) {
    pthread_mutex_lock(&jobs_lock);
    if (job_count < MAX_JOBS) {
        jobs[job_count].id = job_count + 1;
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].name, name, 63);
        jobs[job_count].name[63] = '\0';
        jobs[job_count].active = 1;
        job_count++;
    }
    pthread_mutex_unlock(&jobs_lock);
    //pthread_mutex_t jobs_lock = PTHREAD_MUTEX_INITIALIZER;
}
Job* find_job_by_index(int id) {
    if (id <= 0 || id > job_count) return NULL;

    if (jobs[id - 1].active) {
        return &jobs[id - 1];
    }

    return NULL;
}
void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            break;
        }
    }
}
void parse_input(char *line, char **args, int *isBackground) {

    *isBackground = 0;

    char *token = strtok(line, " ");
    int i = 0;

    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }

    if (i > 0 && strcmp(args[i - 1], "&") == 0) {
        *isBackground = 1;
        args[i - 1] = NULL;   // remove &
    } else {
        args[i] = NULL;       // terminate normally
    }

    // 🔥 ALWAYS ensure termination
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
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

void setup_signals() {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, NULL);

    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);
}

/* ---------------- Execution ---------------- */

int execute_command(char **args,int isBackground) {
    if (args[0] == NULL) return 1;

    /* Built-ins */
    printf("CMD = %s\n", args[0]);
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

    if (strcmp(args[0], "jobs") == 0) {
    printf("job_count = %d\n", job_count);
    pthread_mutex_lock(&jobs_lock);
    
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            printf("[%d] Running %s [%d]\n",
                   jobs[i].id,
                   jobs[i].name,
                   jobs[i].pid);
        }
    }

    pthread_mutex_unlock(&jobs_lock);

    return 1;
}
    if (strcmp(args[0], "fg") == 0) {

    if (args[1] == NULL) {
        printf("fg: expected job id\n");
        return 1;
    }

    int job_id = atoi(args[1]);

    pthread_mutex_lock(&jobs_lock);
    Job *job = find_job_by_index(job_id);
    pthread_mutex_unlock(&jobs_lock);

    if (job == NULL) {
        printf("fg: no such job\n");
        return 1;
    }

    pid_t pid = job->pid;

    // Bring to foreground
    fg_pgid = pid;

    // Continue if stopped
    kill(-pid, SIGCONT);

    int status;
    waitpid(pid, &status, 0);

    fg_pgid = 0;

    // Remove job after completion
    pthread_mutex_lock(&jobs_lock);
    remove_job(pid);
    pthread_mutex_unlock(&jobs_lock);

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
        if(isBackground){
            add_job(pid,args[0]);
            char buffer[50];
            int id=job_count;
            int len = snprintf(buffer, sizeof(buffer), "[%d] %d\n",id, pid);
            write(STDOUT_FILENO, buffer, len);
        }else{
            fg_pgid = pid;
            int status;
            waitpid(pid, &status, 0);
            fg_pgid = 0;
        }
       
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