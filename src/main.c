#include "server.h"
#include "shell.h"
#include <errno.h>
#include <dispatch/dispatch.h>

/* Absolute path of shell launch directory */
char BASE_DIR[PATH_MAX];

/* Counting semaphore to limit background jobs */
dispatch_semaphore_t job_sem;

/* Entry point for shell: handles server mode, init, and REPL loop */
int main(int argc, char *argv[]) {
    /* Check for server mode flag */
    if (argc > 1 && strcmp(argv[1], "--server") == 0) return server();

    /* Initialize shell state and data */
    getcwd(BASE_DIR, sizeof(BASE_DIR));
    setup_signals();
    load_users("data/users.txt");
    job_sem = dispatch_semaphore_create(MAX_BG_JOBS);

    if (!login()) exit(1);

    int status = 1;
    while (status) {
        /* Display prompt and read user input */
        printf("%s@oshell> ", current_user);
        fflush(stdout);
        char input[1024];

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (errno == EINTR) { clearerr(stdin); continue; }
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = 0;
        char original_input[1024];
        strcpy(original_input, input);

        char *args[100], *command[10][50];
        int is_background = 0;

        /* Parse and execute command */
        parse_input(input, args, &is_background);
        int n = split_pipe(args, command);

        if (args[0] && strcmp(args[0], "history") != 0) append_history(original_input);

        if (n > 1) execute_pipe(command, n);
        else status = execute_command(args, is_background);
    }

    return 0;
}