#include"server.h"
#include"shell.h"

int main(int argc, char *argv[]) {

    // 🔥 SERVER MODE
    if (argc > 1 && strcmp(argv[1], "--server") == 0) {
        return server();
    }

    // 🔥 NORMAL SHELL MODE
    setup_signals();

    load_users("/Users/jaswanth/Desktop/OS-SHELL/data/users.txt");

    if (!login()) {
        exit(1);
    }

    int status = 1;

    while (status) {

        char prompt[64];
        int len = snprintf(prompt, sizeof(prompt), "%s@oshell> ", current_user);
        write(1, prompt, len);

        char input[1024];

        if (fgets(input, sizeof(input), stdin) == NULL) {
            write(1, "\n", 1);
            break;
        }

        input[strcspn(input, "\n")] = 0;

        char original_input[1024];
        strcpy(original_input, input);

        char *args[100];
        char *command[10][50];
        int is_background = 0;

        parse_input(input, args, &is_background);
        int n = split_pipe(args, command);

        // 🔥 HISTORY LOGGING
        if (args[0] != NULL && strcmp(args[0], "history") != 0) {
            append_history(original_input);
        }

        if (n > 1) {
            execute_pipe(command, n);
        } else {
            status = execute_command(args, is_background);
        }
    }

    return 0;
}