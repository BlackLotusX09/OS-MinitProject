
#include<stdio.h>
#include<string.h>
#include <sys/types.h>   // defines pid_t
#include <unistd.h>      // fork(), execvp()
#include <sys/wait.h>    // wait()
#include<stdlib.h>
#ifndef SHELL_H
#define SHELL_H

/* Global foreground process group */
extern pid_t fg_pgid;

/* Global foreground process group */
void parse_input(char *line, char **args);
int split_pipe(char **args,char *command[][50]);

/* Parsing */
void execute_pipe(char *command[][50],int n);
int execute_command(char **args);

/* Execution */
void handle_sigint(int sig);
void handle_sigstp(int sig);
void handle_sigchld(int sig);

/* Signal handlers */
void setup_signals();
#endif