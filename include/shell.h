#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <dispatch/dispatch.h>

#define MAX_JOBS 100
#define MAX_USERS 100
#define MAX_BG_JOBS 5

typedef enum { ROLE_ADMIN, ROLE_USER, ROLE_GUEST } Role;

typedef struct {
    int id;
    pid_t pid;
    char name[64];
    int active;
} Job;

typedef struct {
    char username[32];
    char password[32];
    char role[16];
} User;

/* Shared global state */
extern Role current_role;
extern char current_user[32];
extern char current_password[32];
extern char BASE_DIR[PATH_MAX];
extern User users[MAX_USERS];
extern int user_count;
extern pid_t fg_pgid;
extern Job jobs[MAX_JOBS];
extern int job_count;
extern pthread_mutex_t jobs_lock;
extern dispatch_semaphore_t job_sem;

/* Core shell functions */
Role get_role_from_string(char *role);
int login();
int is_allowed(char **args);
void load_users(const char *filename);
void append_history(const char *cmd);
void show_history();
void add_job(pid_t pid, char *name);
Job* find_job_by_index(int id);
void remove_job(pid_t pid);
void parse_input(char *line, char **args, int *isBackground);
int split_pipe(char **args, char *command[][50]);
int execute_command(char **args, int isBackground);
void execute_pipe(char *commands[][50], int n);
void setup_signals();
int execute_rsh(char **args);

#endif