#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
    char *str;
    struct node *next;
};

static void sanitize(char *s)
{
    char *in = s;
    char *out = s;

    while (*in) {
        unsigned char c = (unsigned char)*in;

        if (c == 0x1B) {
            in++;
            if (*in == '[') {
                in++;
                while (*in && !((unsigned char)*in >= '@'
                                && (unsigned char)*in <= '~'))
                    in++;
                if (*in) in++;
            } else if (*in == 'O') {
                in++;
                if (*in) in++;
            } else {
                if (*in) in++;
            }
            continue;
        }

        if (c < 0x20 && c != '\n' && c != '\t') {
            in++;
            continue;
        }

        if (c == 0x7F) {
            in++;
            continue;
        }

        *out++ = *in++;
    }
    *out = '\0';
}

int main(void)
{
    struct node *head = NULL;
    struct node *tail = NULL;
    char buf[4096];

    printf("Enter lines, '.' at start to finish:\n");

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        sanitize(buf);

        if (buf[0] == '.')
            break;

        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
            len--;
        }

        char *copy = malloc(len + 1);
        if (copy == NULL) {
            perror("malloc");
            return 1;
        }
        memcpy(copy, buf, len + 1);

        struct node *n = malloc(sizeof(*n));
        if (n == NULL) {
            perror("malloc");
            free(copy);
            return 1;
        }
        n->str = copy;
        n->next = NULL;

        if (head == NULL) {
            head = n;
            tail = n;
        } else {
            tail->next = n;
            tail = n;
        }
    }

    printf("\nAll lines:\n");
    for (struct node *p = head; p != NULL; p = p->next)
        printf("%s\n", p->str);

    while (head != NULL) {
        struct node *next = head->next;
        free(head->str);
        free(head);
        head = next;
    }

    return 0;
}