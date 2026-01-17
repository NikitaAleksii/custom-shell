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
 * Tokenizes the input line into tokens using strtok().
 * If the final token is exactly "&", sets *background = 1 and removes that token
 *
 * Parameters:
 *   line         - user input
 *   background   - output flag: set to 1 if last token is "&", otherwise unchanged/0
 *   command_size - output count of tokens excluding "&"
 *
 * Returns:
 *   Pointer to a NULL-terminated heap-allocated array of char* tokens, or NULL on failure/empty input
 *   The returned array should be freed by the caller
 */
char **parse_line(char *line, int *background, int *command_size)
{
    char *token = strtok(line, " \n");
    if (token == NULL)
        return NULL;
    char **tokenized = (char **)malloc(MAX_CMD_STRING_NUM * sizeof(char *));

    if (!tokenized)
        return NULL;

    // Parse through user input and append tokens to the array
    int i = 0;
    while (token != NULL)
    {
        if (i >= MAX_CMD_STRING_NUM - 1)
        {
            free(tokenized);
            return NULL;
        }

        tokenized[i] = token;
        i++;
        token = strtok(NULL, " \n");
    }

    tokenized[i] = NULL;
    *command_size = i;

    // If the last entered character is &, set the background flag to 1, and remove the character from the array
    if (strcmp(tokenized[i - 1], "&") == 0)
    {
        fflush(stdout);
        tokenized[i - 1] = NULL;
        *background = 1;
        *command_size = *command_size - 1;
    }

    return tokenized;
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
        printf("Failed executing command\n");
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
        printf("Failed executing command\n");
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

/*
 * pipeline = {{"command 1"}, {"command 2"}}
 */
int split_pipeline(char **tokenized, char ****pipeline)
{
    if (strcmp(tokenized[0], "|") == 0)
        return NULL;

    // Count the number of pipes
    int pipes = 0;
    for (int i = 0; tokenized[i] != NULL; i++)
    {
        if (strcmp(tokenized[0], "|") == 0)
            pipes++;
    }

    // Allocate memmory for pipes + 1 commands
    *pipeline = (char ***)malloc((pipes + 1) * sizeof(char **));

    // Pipeline commands
    int p = 0;
    int start = 0;
    for (int i = 0; tokenized[i] != NULL; i++)
    {
        if (tokenized[i] == NULL || strcmp(tokenized[i], "|") == 0)
        {
            int size = i - start;
            *pipeline[p] = (char **)malloc((size + 1) * sizeof(char *)); // size + 1 for NULL

            for (int j = 0; j < size; j++)
                *pipeline[p][j] = tokenized[start + j];

            *pipeline[p][size] = NULL;
            start = i + 1;
            p++;
        }
        if (tokenized[i] == NULL)
            break;
    }

    return pipes; 
}

void run_pipeline(char ***pipeline, int cmds)

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

        char ***pipeline;

        // exit
        if (strcmp(tokenized[0], "exit") == 0)
        {
            free(tokenized);
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
            free(tokenized);
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

        free(tokenized);
        free(line);
    }
    return 0;
}