
#include<stdio.h>
#include<string.h>
#include <sys/types.h>   // defines pid_t
#include <unistd.h>      // fork(), execvp()
#include <sys/wait.h>    // wait()
#include<stdlib.h>
#ifndef SHELL_H
#define SHELL_H

void parse_input(char *line, char **args);
void execute_pipe(char *command[][50],int n);
int split_pipe(char **args,char *command[][50]);
int execute_command(char **args);
#endif