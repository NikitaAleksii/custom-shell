#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Read user input. Returns 0 if there is an error in reading the input; 1 if read successfully
int read_line(char *line)
{
    printf("myline> ");

    if (fgets(line, MAX_CMD_LETTER_SIZE, stdin) == NULL)
    {
        return 0;
    }
    return 1;
}

char **parse_line(char *line, int *background)
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

        token = strtok(NULL, " \n");
    }

    // If the last entered character is &, set the background flag to 1, and remove the character from the array
    if (strcmp(tokenized[i - 1], "&") == 0)
    {
        printf("%s", tokenized[i - 1]);
        tokenized[i - 1] = NULL;
        *background = 1;
    }

    return tokenized;
}

void add_history(char **tokenized)
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
    for (int i = 0; i < HISTORY_MAX; i++)
    {
        printf("%zu\t%s\n", history_buffer[index].id, history_buffer[index].command);
        index = (index + 1) % HISTORY_MAX;
        if (history_buffer[index].id == 0)
            return;
    }
}

int main()
{
    while (1)
    {
        char line[MAX_CMD_LETTER_SIZE];

        if (read_line(line) == 0)
        {
            printf("Error in Reading Command");
            continue;
        }

        // char line[MAX_CMD_LETTER_SIZE] = "Hello world";

        // Tokenize the input
        int background = 0;
        char **tokenized = parse_line(line, &background);

        if (tokenized == NULL)
            continue;

        add_history(tokenized);
        print_history();

        // Print out tokens
        // printf("======== Print tokens ========\n");
        // int i = 0;
        // while (tokenized[i] != NULL){
        //     printf("Input #%d: %s\n", i+1, tokenized[i]);
        //     i++;
        // }

        // printf("Background Flag: %i\n", background);

        free(tokenized);
    }

    return 0;
}