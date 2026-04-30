#include "server.h"
#include "shell.h"
#include <errno.h>
#include <dispatch/dispatch.h>

char BASE_DIR[PATH_MAX];
dispatch_semaphore_t job_sem;
int main(int argc, char *argv[]) {


    // 🔥 SERVER MODE
    if (argc > 1 && strcmp(argv[1], "--server") == 0) {
        return server();
    }
    getcwd(BASE_DIR, sizeof(BASE_DIR));
    // 🔥 NORMAL SHELL MODE
    setup_signals();

    load_users("data/users.txt");
    job_sem = dispatch_semaphore_create(MAX_BG_JOBS);
    if (job_sem == NULL) {
        perror("dispatch_semaphore_create");
        exit(1);
    }

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
        // Check if the "error" was actually just a signal interruption
        if (errno == EINTR) {
            clearerr(stdin); // Reset the EOF/error tags for stdin[cite: 1]
            continue;        // Jump back to the start of the while loop[cite: 1]
        }
        // If it wasn't EINTR, it's a real EOF (like Ctrl+D)[cite: 1]
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

    // cleanup
    // cleanup (dispatch semaphores are reference‑counted; releasing is optional on program exit)
    // No explicit destroy needed for dispatch_semaphore_t


}