#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
    char *str;
    struct node *next;
};

#define MAX_LINE 1024

int main(void)
{
    struct node *head = NULL, *tail = NULL;
    char buffer[MAX_LINE];

    printf("Enter strings (a line starting with '.' stops input):\n");

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t len = strlen(buffer);
        int overflow = 0;

        
        if (len == sizeof(buffer) - 1 && buffer[len - 1] != '\n') {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
            overflow = 1;
        }

        
        if (buffer[0] == '.')
            break;

        
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';

        

        
        struct node *new_node = malloc(sizeof *new_node);
        if (!new_node) { perror("malloc"); exit(1); }
        new_node->str = malloc(len + 1);
        if (!new_node->str) { perror("malloc"); free(new_node); exit(1); }
        memcpy(new_node->str, buffer, len + 1);   
        new_node->next = NULL;

        if (!head) head = tail = new_node;
        else       { tail->next = new_node; tail = new_node; }

        if (overflow)
            fprintf(stderr, "warning: line truncated\n");
    }

    printf("\n--- Strings in list ---\n");
    int count = 0;
    for (struct node *cur = head; cur; cur = cur->next) {
        printf("%s\n", cur->str);
        count++;
    }
    printf("--- Total: %d strings ---\n", count);

    while (head) {
        struct node *tmp = head;
        head = head->next;
        free(tmp->str);
        free(tmp);
    }
    return 0;
}