
#include"shell.h"
int main(){
    setup_signals();
    int status=1;
    while(status){
    write(1,"oshell> ",8);

    char input[1024];
    if (fgets(input, sizeof(input), stdin) == NULL) {
    write(1, "\n", 1);
    break;
}

    input[strcspn(input,"\n")]=0;

    char *args[100];
    char *command[10][50];

    parse_input(input,args);
    int n=split_pipe(args,command);
    if(n>1){
        execute_pipe(command,n);
    }else{
        status=execute_command(args);
    }
    
    }
    return 0;
}