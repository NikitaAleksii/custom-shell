#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <editline/readline.h>

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

char **parse_line(char *line, int *background, int *command_size)
{
    char *token = strtok(line, " \n");
    if (token == NULL)
        return NULL;
    char **tokenized = (char **)malloc(MAX_CMD_STRING_NUM * sizeof(char *));

    if (!tokenized)
        return NULL;

    // Parse through user input and append it to the array
    int i = 0;
    while (token != NULL)
    {
        if (i > MAX_CMD_STRING_NUM)
            return NULL;

        tokenized[i] = token;
        i++;
        *command_size = *command_size + 1;
        token = strtok(NULL, " \n");
    }

    // If the last entered character is &, set the background flag to 1, and remove the character from the array
    if (strcmp(tokenized[i - 1], "&") == 0)
    {
        printf("%s", tokenized[i - 1]);
        fflush(stdout);
        tokenized[i - 1] = NULL;
        *background = 1;
        *command_size = *command_size - 1;
    }

    return tokenized;
}

void add_shell_history(char **tokenized)
{
    char command[MAX_CMD_LETTER_SIZE] = "";

    // Combine tokens to get a command
    int i = 0;
    while (tokenized[i] != NULL)
    {
        strcat(command, tokenized[i]);
        strcat(command, " ");
        i++;
    }

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
        printf("Too many variables\n");
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

int main()
{
    while (1)
    {
        char prompt[256];
        build_shell_prompt(prompt, sizeof(prompt));

        char *line = readline(prompt);
        if (!line)
        {
            printf("\n");
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
        }

        free(tokenized);
        free(line);
    }

    return 0;
}