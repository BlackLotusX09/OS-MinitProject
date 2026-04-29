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

 #define MAX_JOBS 100

typedef struct {
    int id;
    pid_t pid;
    char name[64];
    int status;
    int active;
}Job;
/* Global foreground process group */
extern pid_t fg_pgid;
extern Job jobs[MAX_JOBS];
extern int join_count;
extern pthread_mutex_t jobs_lock;

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
void handle_sigstp(int sig);
void handle_sigchld(int sig);

/* Signal handlers */
void setup_signals();
#endif