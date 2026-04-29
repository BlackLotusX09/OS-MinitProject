
#include"shell.h"
int main(){
    setup_signals();
    if(!login()){
        exit(1);
    }
    load_users("/Users/jaswanth/Desktop/OS-SHELL/data/users.txt");
    int status=1;
    while(status){
    char prompt[64];
    int len = snprintf(prompt, sizeof(prompt), "%s@oshell> ", current_user);
    write(1, prompt, len);

    char input[1024];
    if (fgets(input, sizeof(input), stdin) == NULL) {
    write(1, "\n", 1);
    break;
}

    input[strcspn(input,"\n")]=0;

    char *args[100];
    char *command[10][50];

    int is_background=0;
    parse_input(input, args, &is_background);
    int n=split_pipe(args,command);
    if(n>1){
        execute_pipe(command,n);
    }else{
        status = execute_command(args, is_background);
    }
    
    }
    return 0;
}