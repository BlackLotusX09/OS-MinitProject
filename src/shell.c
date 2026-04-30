#include "shell.h"
#include "client.h"

/* Global state for user session and job control */
Role current_role;
char current_user[32];
char current_password[32];
pid_t fg_pgid = 0;
Job jobs[MAX_JOBS];
int job_count = 0;
User users[MAX_USERS];
int user_count = 0;
pthread_mutex_t jobs_lock = PTHREAD_MUTEX_INITIALIZER;

/* Map string roles from users.txt to Role enum */
Role get_role_from_string(char *role){
    if(strcmp(role,"admin")==0)return ROLE_ADMIN;
    if(strcmp(role,"user")==0)return ROLE_USER;
    return ROLE_GUEST;
}

/* Construct absolute path for history log file */
void get_history_path(char *path){
    snprintf(path,PATH_MAX,"%s/data/history.log",BASE_DIR);
}

/* Handle user login with up to 3 attempts */
int login() {
    char username[32];
    char password[32];

    for (int attempt = 0; attempt < 3; attempt++) {
        write(STDOUT_FILENO, "Username: ", 10);
        fgets(username, sizeof(username), stdin);
        write(STDOUT_FILENO, "Password: ", 10);
        fgets(password, sizeof(password), stdin);

        username[strcspn(username, "\n")] = 0;
        password[strcspn(password, "\n")] = 0;

        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, username) == 0 &&
                strcmp(users[i].password, password) == 0) {
                strcpy(current_user, username);
                strncpy(current_password, password, 31);
                current_password[31] = '\0';
                current_role = get_role_from_string(users[i].role);
                printf("Login successful as %s\n", current_user);
                return 1;
            }
        }
        write(STDOUT_FILENO, "Invalid credentials\n", 20);
    }
    write(STDOUT_FILENO, "Too many failed attempts\n", 25);
    exit(1);
}

/* Role-based access control check for commands */
int is_allowed(char **args) {
    if (args[0] == NULL) return 1;
    if (current_role == ROLE_ADMIN) return 1;

    if (current_role == ROLE_USER) {
        if (strcmp(args[0], "kill") == 0) return 0;
        return 1;
    }

    if (current_role == ROLE_GUEST) {
        if (strcmp(args[0], "ls") == 0 || strcmp(args[0], "pwd") == 0 ||
            strcmp(args[0], "echo") == 0 || strcmp(args[0], "help") == 0 ||
            strcmp(args[0], "exit") == 0) return 1;
        return 0;
    }
    return 0;
}

/* Load users from a text file into the global users array */
void load_users(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening user file");
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (user_count >= MAX_USERS) break;
        line[strcspn(line, "\n")] = 0;
        char *username = strtok(line, ":");
        char *password = strtok(NULL, ":");
        char *role = strtok(NULL, ":");
        if (username && password && role) {
            strncpy(users[user_count].username, username, 31);
            strncpy(users[user_count].password, password, 31);
            strncpy(users[user_count].role, role, 15);
            user_count++;
        }
    }
    fclose(fp);
}

/* Display the last 20 entries from command history */
void show_history() {
    char path[PATH_MAX];
    get_history_path(path);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return;
    }
    struct flock lock = {F_RDLCK, SEEK_SET, 0, 0, 0};
    fcntl(fd, F_SETLKW, &lock);
    FILE *fp = fdopen(fd, "r");
    char lines[100][256];
    int count = 0;
    while (fgets(lines[count % 100], sizeof(lines[0]), fp)) count++;
    int start = (count > 20) ? count - 20 : 0;
    for (int i = start; i < count; i++) printf("%s", lines[i % 100]);
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(fp);
}

/* Append a command to the history log with file locking */
void append_history(const char *cmd) {
    char path[PATH_MAX];
    get_history_path(path);
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return;
    struct flock lock = {F_WRLCK, SEEK_SET, 0, 0, 0};
    fcntl(fd, F_SETLKW, &lock);
    write(fd, cmd, strlen(cmd));
    write(fd, "\n", 1);
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
}

/* Add a new background job to the tracking list */
void add_job(pid_t pid, char *name) {
    dispatch_semaphore_wait(job_sem, DISPATCH_TIME_FOREVER);
    pthread_mutex_lock(&jobs_lock);
    if (job_count < MAX_JOBS) {
        jobs[job_count].id = job_count + 1;
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].name, name, 63);
        jobs[job_count].active = 1;
        job_count++;
    }
    pthread_mutex_unlock(&jobs_lock);
}

/* Find a job entry by its 1-indexed ID */
Job* find_job_by_index(int id) {
    if (id <= 0 || id > job_count) return NULL;
    return jobs[id - 1].active ? &jobs[id - 1] : NULL;
}

/* Remove a job from the list and release its semaphore slot */
void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            for (int j = i; j < job_count - 1; j++) jobs[j] = jobs[j + 1];
            job_count--;
            break;
        }
    }
    dispatch_semaphore_signal(job_sem);
}

/* Tokenize input string into arguments and detect background flag */
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
        args[i - 1] = NULL;
    } else {
        args[i] = NULL;
    }
}

/* Split arguments into separate commands based on pipe symbols */
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

/* Handle SIGINT (Ctrl+C) to terminate foreground process */
void handle_sigint(int sig) {
    (void)sig;
    if (fg_pgid > 0) kill(-fg_pgid, SIGINT);
    write(1, "\n", 1);
}

/* Handle SIGTSTP (Ctrl+Z) to stop foreground process */
void handle_sigtstp(int sig) {
    (void)sig;
    if (fg_pgid > 0) {
        kill(-fg_pgid, SIGTSTP);
        fg_pgid = 0;
    } else {
        write(STDOUT_FILENO, "\n", 1);
    }
}

/* Reap zombie children in the background */
void handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved_errno;
}

/* Setup signal handlers for INT, TSTP, and CHLD signals */
void setup_signals() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, NULL);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);
}

/* Execute a single command (built-in or external) */
int execute_command(char **args, int isBackground) {
    if (args[0] == NULL) return 1;
    if (!is_allowed(args)) {
        write(STDOUT_FILENO, "Permission denied\n", 18);
        return 1;
    }

    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) fprintf(stderr, "cd: expected argument\n");
        else if (chdir(args[1]) != 0) perror("cd failed");
        return 1;
    }
    if (strcmp(args[0], "history") == 0) { show_history(); return 1; }
    if (strcmp(args[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
        else perror("pwd failed");
        return 1;
    }
    if (strcmp(args[0], "rsh") == 0) return execute_rsh(args);
    if (strcmp(args[0], "jobs") == 0) {
        pthread_mutex_lock(&jobs_lock);
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].active) printf("[%d] Running %s [%d]\n", jobs[i].id, jobs[i].name, jobs[i].pid);
        }
        pthread_mutex_unlock(&jobs_lock);
        return 1;
    }
    if (strcmp(args[0], "fg") == 0) {
        if (args[1] == NULL) { printf("fg: expected job id\n"); return 1; }
        int job_id = atoi(args[1]);
        pthread_mutex_lock(&jobs_lock);
        Job *job = find_job_by_index(job_id);
        pthread_mutex_unlock(&jobs_lock);
        if (!job) { printf("fg: no such job\n"); return 1; }
        fg_pgid = job->pid;
        kill(-job->pid, SIGCONT);
        int status;
        waitpid(job->pid, &status, 0);
        fg_pgid = 0;
        pthread_mutex_lock(&jobs_lock);
        remove_job(job->pid);
        pthread_mutex_unlock(&jobs_lock);
        return 1;
    }
    if (strcmp(args[0], "exit") == 0) return 0;

    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        setpgid(pid, pid);
        if (isBackground) {
            add_job(pid, args[0]);
            printf("[%d] %d\n", job_count, pid);
        } else {
            fg_pgid = pid;
            int status;
            waitpid(pid, &status, WUNTRACED);
            fg_pgid = 0;
            if (WIFSTOPPED(status)) {
                pthread_mutex_lock(&jobs_lock);
                if (job_count < MAX_JOBS) {
                    jobs[job_count].id = job_count + 1;
                    jobs[job_count].pid = pid;
                    strncpy(jobs[job_count].name, args[0], 63);
                    jobs[job_count].active = 1;
                    job_count++;
                }
                printf("\n[%d]+ Stopped   %s\n", job_count, args[0]);
                pthread_mutex_unlock(&jobs_lock);
            }
        }
    } else perror("fork");
    return 1;
}

/* Execute a series of commands connected by pipes */
void execute_pipe(char *commands[][50], int n) {
    int pipes[n-1][2];
    pid_t pgid = 0;
    for (int i = 0; i < n-1; i++) pipe(pipes[i]);
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            if (pgid == 0) pgid = getpid();
            setpgid(0, pgid);
            if (i == 0) dup2(pipes[0][1], STDOUT_FILENO);
            else if (i == n - 1) dup2(pipes[i-1][0], STDIN_FILENO);
            else { dup2(pipes[i-1][0], STDIN_FILENO); dup2(pipes[i][1], STDOUT_FILENO); }
            for (int j = 0; j < n-1; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            execvp(commands[i][0], commands[i]);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);
        }
    }
    fg_pgid = pgid;
    for (int i = 0; i < n-1; i++) { close(pipes[i][0]); close(pipes[i][1]); }
    for (int i = 0; i < n; i++) wait(NULL);
    fg_pgid = 0;
}