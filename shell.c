#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <editline/readline.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CMD_LETTER_SIZE 1000
#define MAX_CMD_STRING_NUM 100
#define HISTORY_MAX 10

typedef struct
{
    char command[MAX_CMD_LETTER_SIZE];
    size_t id;
} history_t;

history_t *head = NULL;
history_t *tail = NULL;
history_t history_buffer[HISTORY_MAX];
size_t next_id = 0;

/*
 * Parses a raw input line into a NULL-terminated array of tokens.
 *
 * The function:
 *   - Splits the input on whitespace (spaces, tabs, newlines)
 *   - Preserves text enclosed in single ('') or double ("") quotes as a single token
 *   - Each token and the token array are dynamically allocated
 *   - The caller is responsible for freeing the result using free_tokens()
 *
 * Parameters:
 *   line         - user input
 *   background   - output flag: set to 1 if last token is "&", otherwise unchanged/0
 *   command_size - output count of tokens excluding "&"
 *
 * Returns:
 *   A NULL-terminated array of heap-allocated strings representing tokens,
 *   or NULL on allocation failure.
 */
char **parse_line(char *line, int *background, int *command_size)
{
    int pos = 0;
    int i = 0;
    int len = strlen(line);

    char **tokenized = malloc(MAX_CMD_STRING_NUM * sizeof(char *));
    if (!tokenized)
    {
        perror("Parse Line Error (malloc tokenized)");
        return NULL;
    }

    while (pos < len)
    {
        while (pos < len && (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\n'))
            pos++;

        if (pos >= len)
            break;

        char ch = '\0';
        if (line[pos] == '\'' || line[pos] == '"')
        {
            ch = line[pos];
            pos++;
        }

        int start = pos;
        int closed = 0;
        while (pos < len)
        {
            if (ch != '\0')
            {
                if (line[pos] == ch)
                {
                    closed = 1;
                    pos++;
                    break;
                }
            }
            else
            {
                if (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\n')
                    break;
            }
            pos++;
        }

        int token_len = pos - start;
        if (closed)
            token_len--;

        if (token_len > 0)
        {
            char *token = malloc(token_len + 1);
            if (!token)
            {
                for (int j = 0; j < i; j++)
                    free(tokenized[j]);
                free(tokenized);
                perror("Parse Line Error (malloc)");
                return NULL;
            }
            strncpy(token, line + start, token_len);
            token[token_len] = '\0';
            tokenized[i++] = token;
        }
    }

    tokenized[i] = NULL;
    *command_size = i;

    // Background check
    if (i > 0 && strcmp(tokenized[i - 1], "&") == 0)
    {
        free(tokenized[i - 1]);
        tokenized[i - 1] = NULL;
        *background = 1;
        *command_size = i - 1;
    }

    return tokenized;
}

/*
 * Frees a token array produced by parse_line().
 *
 * Parameters:
 *   tokenized     - NULL-terminated array of tokens
 *   command_size  - Number of valid tokens in the array
 */
void free_tokens(char **tokenized, int command_size)
{
    for (int i = 0; i < command_size; i++)
    {
        free(tokenized[i]);
    }
    free(tokenized);
}

/*
 * Reconstructs a command string from a token array and appends it
 * to the circular history buffer. Updates history IDs and head/tail pointers
 *
 * Parameters:
 *   tokenized - array of tokens
 *
 * Side effects:
 *   Modifies global history state: history_buffer, head, tail, next_id
 */
void add_shell_history(char **tokenized)
{
    char command[MAX_CMD_LETTER_SIZE] = "";

    // Reconstruct a command from tokens
    int i = 0;
    while (tokenized[i] != NULL)
    {
        strcat(command, tokenized[i]);
        strcat(command, " ");
        i++;
    }

    // Create a new history entry, copy the command string into it,
    // and assign a unique sequential ID.
    history_t new_element;
    strcpy(new_element.command, command);
    new_element.id = next_id;
    next_id++;

    int index = new_element.id % HISTORY_MAX;
    if (head == NULL)
    {
        history_buffer[index] = new_element;
        head = &history_buffer[index];
        tail = &history_buffer[index + 1];
    }
    else
    {
        if (tail == head)
        {
            int temp = (index + 1) % HISTORY_MAX;
            head = &history_buffer[temp];
        }
        history_buffer[index] = new_element;
        tail = &history_buffer[(index + 1) % HISTORY_MAX];
    }
}

/*
 * Prints the contents of the shell history buffer in chronological order,
 * starting from the oldest entry (head). Iterates through the circular
 * history buffer and stops when an unused entry is encountered or when
 * HISTORY_MAX entries have been printed
 */
void print_history()
{
    if (head == NULL)
        return;

    int index = head->id % HISTORY_MAX;
    printf("ID\tCOMMAND\n");
    fflush(stdout);
    for (int i = 0; i < HISTORY_MAX; i++)
    {
        printf("%zu\t%s\n", history_buffer[index].id, history_buffer[index].command);
        fflush(stdout);
        index = (index + 1) % HISTORY_MAX;
        if (history_buffer[index].id == 0)
            return;
    }
}

/*
 * Implements the built-in `cd` command
 *
 * Behavior:
 *   - With no arguments (command_size == 1), changes to $HOME
 *   - With one argument (command_size == 2):
 *       If the argument is "-", changes to $OLDPWD and prints the target path
 *       Otherwise, changes to the provided path
 *   - With more than one argument, prints an error and does nothing
 *
 * Parameters:
 *   tokenized     - array of tokens
 *   command_size  - number of tokens in tokenized
 */
void builtin_cd(char **tokenized, int command_size)
{
    char curr_cwd[PATH_MAX];
    if (getcwd(curr_cwd, sizeof(curr_cwd)) == NULL)
    {
        curr_cwd[0] = '\0';
    }

    char *target;
    if (command_size == 1)
    {
        target = getenv("HOME");
        if (!target)
        {
            printf("Home not found\n");
            fflush(stdout);
            return;
        }
    }
    else if (command_size == 2)
    {
        if (strcmp(tokenized[1], "-") == 0)
        {
            target = getenv("OLDPWD");
            if (!target)
            {
                printf("Previous path not found\n");
                fflush(stdout);
                return;
            }
            printf("%s\n", target);
            fflush(stdout);
        }
        else
        {
            target = tokenized[1];
        }
    }
    else
    {
        printf("Too many arguments\n");
        fflush(stdout);
        return;
    }

    if (chdir(target) != 0)
    {
        printf("Error in changing the directory\n");
        fflush(stdout);
        return;
    }

    if (curr_cwd[0] != '\0')
    {
        if (setenv("OLDPWD", curr_cwd, 1) == -1)
        {
            printf("Error in changing the old environment\n");
            fflush(stdout);
        }
    }

    char new_cwd[PATH_MAX];
    if (getcwd(new_cwd, sizeof(new_cwd)) != NULL)
    {
        if (setenv("PWD", new_cwd, 1) == -1)
        {
            printf("Error in changing the new environment\n");
            fflush(stdout);
        }
    }
    else
    {
        printf("Error in changing the new environment\n");
        fflush(stdout);
    }
}

/*
 * Builds the interactive shell prompt string
 *
 * The prompt is formatted as:
 *     "<user> <current-directory> > "
 *
 * Parameters:
 *   prompt - output buffer to store the formatted prompt string
 *   size   - size of the output buffer in bytes
 */
void build_shell_prompt(char *prompt, size_t size)
{
    const char *user = getenv("USER");
    if (!user)
        user = "user";

    char *cwd = getcwd(NULL, 0);
    if (!cwd)
        cwd = strdup("?");

    char *slash = strrchr(cwd, '/');
    char *dir = (slash && slash != cwd) ? slash + 1 : cwd;

    snprintf(prompt, size, "%s %s> ", user, dir);
    free(cwd);
}

/*
 * Executes a command in the background
 *
 * This function is intended to be called after a fork().
 * If the process is a child process (pid == 0), it replaces
 * the child’s image with the requested program using execvp()
 *
 * Parameters:
 *   pid       - Process ID returned by fork()
 *   tokenized - Null-terminated array of tokens representing the command
 */
void run_background(pid_t pid, char **tokenized)
{
    if (pid < 0)
    {
        printf("Error in Running in Background\n");
        return;
    }
    else if (pid == 0)
    {
        execvp(tokenized[0], tokenized);
        perror("Failed executing command\n");
        exit(1);
    }
}

/*
 * Executes a command in the foreground
 *
 * This function is intended to be called after a fork().
 * If the process is a child process (pid == 0), it replaces
 * the child’s image with the requested program using execvp().
 * If the process is the parent, it waits for the child process
 * to complete before returning control to the shell prompt
 *
 * Parameters:
 *   pid       - Process ID returned by fork()
 *   tokenized - Null-terminated array of tokens representing the command
 */
void run_foreground(pid_t pid, char **tokenized)
{
    if (pid < 0)
    {
        printf("Error in Running in Foreground\n");
        return;
    }
    else if (pid == 0)
    {
        signal(SIGINT, SIG_DFL);
        execvp(tokenized[0], tokenized);
        perror("Failed executing command\n");
        exit(1);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
    }
}

/*
 * SIGINT (Ctrl-C) handler for the shell
 *
 * Writes a newline to standard output to keep the shell alive and
 * allow the main loop to redisplay the prompt.
 *
 * sig  - Signal number (unused)
 */
static void handle_sigint(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, "\n", 1);
}

/**
 * Splits a tokenized command line into pipeline segments
 *
 * This function scans the token array for pipe symbols ("|") and divides
 * the input into multiple command arrays, one for each stage
 * of the pipeline. Each resulting command array is NULL-terminated.
 *
 * Memory for the pipeline structure and each command segment is dynamically
 * allocated.
 *
 * Parameters:
 *   tokenized - NULL-terminated array of tokens produced by the tokenizer
 *   pipeline  - Output parameter; on success, points to multiple command arrays, one per pipeline stage
 *
 * Returns:
 *   The number of pipe symbols found in the input
 */
int split_pipeline(char **tokenized, char ****pipeline)
{
    // Count the number of pipes
    int pipes = 0;
    for (int i = 0; tokenized[i] != NULL; i++)
    {
        if (strcmp(tokenized[i], "|") == 0)
            pipes++;
    }

    // Allocate memmory for pipes + 1 commands
    *pipeline = malloc((pipes + 1) * sizeof(char **));

    // Pipeline commands
    int p = 0;
    int start = 0;
    for (int i = 0;; i++)
    {
        if (tokenized[i] == NULL || strcmp(tokenized[i], "|") == 0)
        {
            int size = i - start;
            (*pipeline)[p] = malloc((size + 1) * sizeof(char *)); // size + 1 for NULL

            for (int j = 0; j < size; j++)
                (*pipeline)[p][j] = tokenized[start + j];

            (*pipeline)[p][size] = NULL;
            start = i + 1;
            p++;
        }
        if (tokenized[i] == NULL)
            break;
    }

    return pipes;
}

/**
 * Executes a pipeline of commands connected by pipes
 *
 * Each element of the pipeline array represents one command in the pipeline
 * and is a NULL-terminated array.
 * The function creates the required pipes, forks one process per command,
 * and connects standard input and output using dup2() so that the output of
 * each command becomes the input of the next
 *
 * Parameters:
 *   pipeline - Array of command argument vectors (char **), one per pipeline stage
 *   cmds     - Number of commands in the pipeline
 */
void run_pipeline(char ***pipeline, int cmds)
{
    int prev_fd = -1; // read end of previous pipe

    pid_t *pids = malloc(cmds * sizeof(pid_t));
    if (pids == NULL)
    {
        perror("Failed running pipeline (malloc)");
        return;
    }

    for (int i = 0; i < cmds; i++)
    {
        int fd[2] = {-1, -1};
        // Create a pipe for all but the last command
        if (i < cmds - 1)
        {
            if (pipe(fd) == -1)
            {
                perror("Failed running pipeline (pipe)");
                if (prev_fd != -1)
                    close(prev_fd);
                for (int k = 0; k < i; k++)
                    waitpid(pids[k], NULL, 0);
                free(pids);
                return;
            }
        }

        pid_t pid;
        if ((pid = fork()) == -1)
        {
            // Cleanup on fork failure
            perror("Failed running pipeline (fork)");
            if (prev_fd != -1)
                close(prev_fd);
            if (fd[0] != -1)
                close(fd[0]);
            if (fd[1] != -1)
                close(fd[1]);
            for (int k = 0; k < i; k++)
                waitpid(pids[k], NULL, 0);
            free(pids);
            return;
        }
        if (pid == 0)
        {
            // Child
            // Connect stdin to previous pipe
            if (prev_fd != -1)
            {
                if (dup2(prev_fd, STDIN_FILENO) == -1)
                {
                    perror("Failed running pipeline (dup2)");
                    exit(1);
                }
            }

            // Connect stdout to next pipe
            if (i < cmds - 1)
            {
                close(fd[0]);
                if (dup2(fd[1], STDOUT_FILENO) == -1)
                {
                    perror("Failed running pipeline (dup2)");
                    _exit(1);
                }
                close(fd[1]);
            }

            // Close inherited descriptors
            if (prev_fd != -1)
                close(prev_fd);

            execvp(pipeline[i][0], pipeline[i]);
            perror("Failed running pipeline (execvp)");
            exit(1);
        }
        // Parent
        // Track child and close unused FDs
        pids[i] = pid;

        if (prev_fd != -1)
            close(prev_fd);

        if (i < cmds - 1)
        {
            close(fd[1]);
            prev_fd = fd[0];
        }
    }
    if (prev_fd != -1)
        close(prev_fd);

    // Wait for all pipeline processes
    for (int i = 0; i < cmds; i++)
        waitpid(pids[i], NULL, 0);

    free(pids);
}

void free_pipeline(char ***pipeline, int cmds)
{
    for (int i = 0; i < cmds; i++)
    {
        free(pipeline[i]); // free each command (char **)
    }
    free(pipeline); // free pipeline array
}

/*
 * Checks whether the tokenized command contains a pipe symbol ("|")
 *
 * Returns:
 *   1 if at least one pipe is present, 0 otherwise
 */
int contains_pipe(char **tokenized)
{
    int i = 0;
    while (tokenized[i] != NULL)
    {
        if (strcmp(tokenized[i], "|") == 0)
            return 1;
        i++;
    }
    return 0;
}

/*
 * Validates pipe syntax in a tokenized command
 *
 * Ensures the command does not start or end with a pipe and does not
 * contain consecutive pipe symbols
 *
 * Returns:
 *   1 if the pipe syntax is valid, 0 otherwise
 */
int is_valid_pipe(char **tokenized)
{
    if (tokenized == NULL || tokenized[0] == NULL)
        return 0;

    // Cannot start with '|'
    if (strcmp(tokenized[0], "|") == 0)
        return 0;

    int i = 0;
    while (tokenized[i] != NULL)
    {
        // Cannot have consecutive pipes
        if (strcmp(tokenized[i], "|") == 0 &&
            tokenized[i + 1] != NULL &&
            strcmp(tokenized[i + 1], "|") == 0)
            return 0;
        i++;
    }

    // Cannot end with '|'
    if (strcmp(tokenized[i - 1], "|") == 0)
        return 0;

    return 1;
}

/*
 * Main REPL loop for the shell.
 *
 * Repeatedly:
 *   1) Builds and displays a prompt showing USER and the current directory name.
 *   2) Reads a line of input using readline().
 *   3) Tokenizes the input into an argv-style array and detects background requests ('&').
 *   4) Adds the command to the shell's history buffer.
 *   5) Executes supported built-in commands (exit, history, pwd, cd).
 */
int main()
{
    signal(SIGCHLD, SIG_IGN);      // Reap background children
    signal(SIGINT, handle_sigint); // Handle Ctrl-C without terminating the shell

    while (1)
    {
        char prompt[256];
        build_shell_prompt(prompt, sizeof(prompt));

        char *line = readline(prompt);
        if (!line)
        {
            free(line);
            break;
        }

        // Tokenize the input
        int background = 0;
        int command_size = 0;
        char **tokenized = parse_line(line, &background, &command_size);

        if (tokenized == NULL)
        {
            free(line);
            continue;
        }

        // Add the input to history
        add_shell_history(tokenized);

        // Implements piping
        char ***pipeline = NULL;
        if (contains_pipe(tokenized))
        {
            if (!is_valid_pipe(tokenized))
            {
                fprintf(stderr, "Not valid pipe\n");
                free_tokens(tokenized, command_size);
                free(line);
                continue;
            }

            int pipes = split_pipeline(tokenized, &pipeline);
            run_pipeline(pipeline, pipes + 1);
            free_pipeline(pipeline, pipes + 1);

            free_tokens(tokenized, command_size);
            free(line);
            continue;
        }

        // exit
        if (strcmp(tokenized[0], "exit") == 0)
        {
            free_tokens(tokenized, command_size);
            free(line);
            return 0;
        }

        // history
        if (strcmp(tokenized[0], "history") == 0)
        {
            print_history();
        }

        // pwd
        if (strcmp(tokenized[0], "pwd") == 0 && command_size == 1)
        {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL)
            {
                printf("%s\n", cwd);
                fflush(stdout);
            }
            else
            {
                printf("Error in path\n");
                fflush(stdout);
            }
        }
        else if (strcmp(tokenized[0], "pwd") == 0 && command_size > 1)
        {
            printf("Too many arguments\n");
            fflush(stdout);
        }

        // cd
        if (strcmp(tokenized[0], "cd") == 0)
        {
            builtin_cd(tokenized, command_size);
            free_tokens(tokenized, command_size);
            free(line);
            continue;
        }

        // Fork a process and choose whether to execute in background or foreground
        pid_t pid = fork();

        if (background == 1)
        {
            run_background(pid, tokenized);
        }
        else
        {
            run_foreground(pid, tokenized);
        }

        free_tokens(tokenized, command_size);
        free(line);
    }
    return 0;
}