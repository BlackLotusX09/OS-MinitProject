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
#include <fcntl.h>   // For fcntl, flock, and open flags
#include <errno.h>
#include <dispatch/dispatch.h>


#define HISTORY_FILE "data/history.log"
#define MAX_JOBS 100
#define MAX_USERS 100
#define MAX_BG_JOBS 5

typedef enum {
    ROLE_ADMIN,
    ROLE_USER,
    ROLE_GUEST
} Role;



typedef struct {
    int id;
    pid_t pid;
    char name[64];
    int status;
    int active;
}Job;


typedef struct {
    char username[32];
    char password[32];
    char role[16];
} User;

extern Role current_role;
extern char current_user[32];
extern char current_password[32];
extern char BASE_DIR[PATH_MAX];

extern User users[MAX_USERS];
extern int user_count;
/* Global foreground process group */
extern pid_t fg_pgid;
extern Job jobs[MAX_JOBS];
extern int job_count;
extern pthread_mutex_t jobs_lock;
extern dispatch_semaphore_t job_sem;



Role get_role_from_string(char *role);
int login();
int is_allowed(char **args);


void append_history(const char *cmd);
void show_history();

void get_history_path(char *path);
void load_users(const char *filename);
void add_job(pid_t pid, char *name);
Job* find_job_by_index(int id);
void remove_job(pid_t pid);
/* Global foreground process group */
void parse_input(char *line, char **args,int *isBackground);
int split_pipe(char **args,char *command[][50]);

/* Parsing */
void execute_pipe(char *command[][50],int n);
int execute_command(char **args,int isBackground);

/* Execution */
void handle_sigint(int sig);
void handle_sigtstp(int sig);
void handle_sigchld(int sig);

/* Signal handlers */
void setup_signals();
#endif