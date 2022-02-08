#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>


// struct redirection
//      Data structure to support redirections

typedef struct redirection redirection;

struct redirection {
    int operator;    // '>', '<', or '2>'
    char* filename_1;
    char* filename_2;
    char* filename_3;
};


// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;           // number of arguments
    char** argv;        // arguments, terminated by NULL
    pid_t pid;          // process ID running this command, -1 if none
    int cmd_type;       // store the multiple possible values of TOKEN_
    redirection *redir; // address of redirection data structure
    int status;         // status with which the command exited
    int inpipe;         // save type of pipe
    int outpipe;        // save type of pipe
    command* next_cmd;  // pointer to the next command in the list
    command* prev_command;  // pointer to the previous command in the list
};

// redirections_init(r)
//      Initialize the redirection data structure
void redirection_init(redirection* r) {
    r->operator = 0;
    r->filename_1 = NULL;
    r->filename_2 = NULL;
    r->filename_3 = NULL;
}


// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    c->pid = -1;
    c->inpipe = 0;
    c->outpipe = 0;
    c->cmd_type = 0;
    c->next_cmd = NULL;
    c->prev_command = NULL;
    c->redir = (redirection*) malloc(sizeof(redirection));
    return c;
}


// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
    for (int i = 0; i != c->argc; ++i) {
        free(c->argv[i]);
    }
    free(c->argv);
    free(c);
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND EVALUATION

// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t start_command(command* c, pid_t pgid) {
    
    if (!c || !c->argc) {
        return 0;
    }

    // handle change of directory
    if (!strcmp(c->argv[0], "cd") && c->argv[1]) {
        chdir(c->argv[1]);
    } 

    (void) pgid;
    int pid = 0;
    int pipefd[2];
    pipefd[0] = 0;
    pipefd[1] = 0;

    // handle pipes
    if (c->cmd_type & OPTS_PIPE) {
        pipe(pipefd);
        c->outpipe = pipefd[1];
        if (c->next_cmd) {
            c->next_cmd->inpipe = pipefd[0];
        } 
    }

    if ((pid = fork()) == 0) {
        // set pgid
        setpgid(0, pgid);

        // handle pipes
        if (c->outpipe) {
            close(pipefd[0]);
            dup2(c->outpipe, STDOUT_FILENO);
            close(c->outpipe);
        }   
        if(c->inpipe) {
            close(pipefd[1]);
            dup2(c->inpipe, STDIN_FILENO);
            close(c->inpipe);
        }

        // handle redirections
        if (c->cmd_type & OPTS_REDIRECTION) {
            int fd;

            if (c->redir->operator & OP_STDOUT) {     // operator '>'
                fd = open(c->redir->filename_1, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                if (fd < 0) {
                    fprintf(stderr, "%s: No such file or directory\n", c->redir->filename_1);
                    _exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } 
            if (c->redir->operator & OP_STDIN) {     // operator '<'
                fd = open(c->redir->filename_2, O_RDONLY);
                if (fd < 0) {
                    fprintf(stderr, "%s: No such file or directory\n", c->redir->filename_2);
                    _exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (c->redir->operator & OP_STDERR) {     // operator '2>'
                fd = open(c->redir->filename_3, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                if (fd < 0) {
                    fprintf(stderr, "%s: No such file or directory\n", c->redir->filename_3);
                    _exit(1);
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        if(strcmp(c->argv[0], "cd")) {
            if (execvp(c->argv[0], c->argv) < 0) {
            fprintf(stderr, "%s: Command not found!!!.\n", c->argv[0]);
            _exit(1);
            }
        } 
    } else if (pid < 0) {
        fprintf(stderr, "Could not fork.\n");
        exit(1);
    }
    
    // set child's pgid
    setpgid(pid, pgid);

    // close pipes
    if (c->outpipe != STDOUT_FILENO && (c->cmd_type & OPTS_PIPE)) {
        close(pipefd[1]);
    }
    if (c->inpipe != STDIN_FILENO && (c->cmd_type & OPTS_PIPE)) {
        close(c->inpipe);
    }
    
    c->pid = pid;
    return pid;
}

// check_conditional(c)
    // If the command pointed to by cursor terminated abnormally
    // return NULL. Otherwise, return the address of the command
    // to be executed such that the conditionals are respected. 

command* check_conditional(command* c, int status) {

    if (!c || !c->argc) {
        return NULL;
    }

    if (!(strcmp(c->argv[0], "cd")) && !(c->cmd_type & OPTS_REDIRECTION)){
        return c->next_cmd;
    }

    // check if child exited abnormally
    if (!WIFEXITED(status)) {
        fprintf(stderr, "pid: %d. Waitpid error.\n", c->pid);
        return NULL;
    } else {
        if (WEXITSTATUS(status)) {      // status is false 
            if (c->cmd_type & OPTS_AND) {
                return check_conditional(c->next_cmd, status);
            } else {
                return c->next_cmd;
            }
        } else {                        // status is true
            if (c->cmd_type & OPTS_OR) {
                return check_conditional(c->next_cmd, status);
            } else {
                return c->next_cmd;
            }
        }
    }
}

// is_background(c)
    // if the list pointed to by pointer 'c' is supposed to be run in 
    // background mode, then return the address of the next list. 
    // Otherwise, return a NULL pointer.
command* is_background(struct command* c) {

    while (!(c->cmd_type & OPTS_BACKGROUND)) {
        if (!c->next_cmd) {
            break;
        }

        c = c->next_cmd;
    }

    return c->next_cmd;
}

// run_list(c)
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `claim_foreground(pgid)` before waiting for the pipeline.
//       - Call `claim_foreground(0)` once the pipeline is complete.

void run_list(command* c) {

    // check if it is a valid list
    if (!c || !c->argc) {
        return;
    }

    pid_t pgid = getpgid(0);

    // check if some list is to be run in the background
    command* next_list = is_background(c);

    // support command lists
    if (next_list || c->cmd_type & OPTS_BACKGROUND) {
        pid_t pid;
        // Run background in child process
        if ((pid = fork()) == 0) {

            while (c && c != next_list) {
                start_command(c, 0);

                // support pipes 
                if (c->cmd_type & OPTS_PIPE) {
                    c = c->next_cmd;
                } else {
                    if (waitpid(c->pid, &c->status, 0) < 0) {
                        _exit(1);
                    }

                    // support conditionals
                    c = check_conditional(c, c->status);
                }
            }
            _exit(0);
        } else if (pid < 0) {
            fprintf(stderr, "Could not fork.\n");
            exit(1);
        }
        if (next_list) {
            run_list(next_list);
        }
    } else {

        while (c) {

            // support interrupts
            claim_foreground(pgid);
            start_command(c, pgid);
            // support pipes
            if (c->cmd_type & OPTS_PIPE) {

                run_list(c->next_cmd);
                break;
            } else {
                if (waitpid(c->pid, &c->status, 0) < 0) {
                        _exit(1);
                    }
                // support conditionals
                c = check_conditional(c, c->status);
            }
            claim_foreground(0);
        }
        
    }

}

int set_cmd_type(int cmd_type, int type) {

    switch(type) {

        case TOKEN_NORMAL:
            return cmd_type |= OPTS_NORMAL;
            break;
        case TOKEN_REDIRECTION:
            return cmd_type |= OPTS_REDIRECTION;
            break;
        case TOKEN_SEQUENCE:
            return cmd_type |= OPTS_SEQUENCE;
            break;
        case TOKEN_BACKGROUND:
            return cmd_type |= OPTS_BACKGROUND;
            break;
        case TOKEN_PIPE:
            return cmd_type |= OPTS_PIPE;
            break;
        case TOKEN_AND:
            return cmd_type |= OPTS_AND;
            break;
        case TOKEN_OR:
            return cmd_type |= OPTS_OR;
            break;
        default:
            return cmd_type;
    }
}

// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

void eval_line(const char* s) {
    int type;
    char* token;
    // Your code here!

    // build the command
    command* c = command_alloc();
    command* cursor = c;
    while ((s = parse_shell_token(s, &type, &token)) != NULL) {

        if (type != TOKEN_NORMAL && type != TOKEN_REDIRECTION) {
            cursor->cmd_type = set_cmd_type(cursor->cmd_type, type);
            cursor->next_cmd = command_alloc();
            cursor->next_cmd->prev_command = cursor;
            cursor = cursor->next_cmd;
        } else if (type == TOKEN_REDIRECTION) {

            while (type == TOKEN_REDIRECTION) {


                cursor->cmd_type = set_cmd_type(cursor->cmd_type, type);

                // save specific type of redirection
                if (!strcmp(token, ">")) {

                    cursor->redir->operator |= OP_STDOUT;
                    s = parse_shell_token(s, &type, &cursor->redir->filename_1);
                } else if (!strcmp(token, "<")) {

                    cursor->redir->operator |= OP_STDIN;
                    s = parse_shell_token(s, &type, &cursor->redir->filename_2);
                } else if (!strcmp(token, "2>")) {

                    cursor->redir->operator |= OP_STDERR;
                    s = parse_shell_token(s, &type, &cursor->redir->filename_3);
                } else {
                    fprintf(stderr, "%s: Invalid token\n", token);
                }

                // save the filename
                //s = parse_shell_token(s, &type, &cursor->filename);   

                // append arguments until next command type
                s = parse_shell_token(s, &type, &token);

                while (type == TOKEN_NORMAL) {
                    command_append_arg(cursor, token);
                    s = parse_shell_token(s, &type, &token);
                }
            }

            cursor->cmd_type = set_cmd_type(cursor->cmd_type, type);
            cursor->next_cmd = command_alloc();
            cursor->next_cmd->prev_command = cursor;
            cursor = cursor->next_cmd; 
        } 
        else {

            command_append_arg(cursor, token);
        } 
    }

    // Free empty command allocations
    if(!cursor->argc){
        cursor->prev_command->next_cmd = NULL;
        command_free(cursor);
    }

    if (c->argc) {
        run_list(c);
    }

    command_free(c);
}

static volatile sig_atomic_t SigStatus = 0;

void interrupt_handler(int sig) {
    SigStatus = sig;
}

int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);
    set_signal_handler(SIGINT, interrupt_handler);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {

        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        while(1) {
            if (waitpid(0, NULL, WNOHANG) < 1)
                break;
        }

        if(SigStatus) {
            printf("\n");
            SigStatus = 0;
            needprompt = 1;
            bufpos = 0;
        }

    }
    return 0;
}